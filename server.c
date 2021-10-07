#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>

#include "storage.h"
#include "xml_api.h"

static volatile bool closing = false;

static void close_handler(int sig, siginfo_t *info, void *context) {
    closing = true;
}

static xmlDoc *handle_request_create_table(struct xml_api_create_table_request request, struct storage *storage) {
    struct storage_table *table = malloc(sizeof(*table));

    table->storage = storage;
    table->position = 0;
    table->next = 0;
    table->first_row = 0;
    table->name = strdup(request.table_name);
    table->columns.amount = request.columns.amount;
    table->columns.columns = malloc(sizeof(*table->columns.columns) * request.columns.amount);

    for (int i = 0; i < request.columns.amount; ++i) {
        table->columns.columns[i].name = strdup(request.columns.columns[i].name);
        table->columns.columns[i].type = request.columns.columns[i].type;
    }

    errno = 0;
    storage_table_add(table);
    bool error = errno != 0;

    storage_table_delete(table);

    if (error) {
        return xml_api_make_error("a table with the same name is already exists");
    } else {
        return xml_api_make_success_no_body();
    }
}

static xmlDoc *handle_request_drop_table(struct xml_api_drop_table_request request, struct storage *storage) {
    struct storage_table *table = storage_find_table(storage, request.table_name);

    if (!table) {
        return xml_api_make_error("table with the specified name is not exists");
    }

    storage_table_remove(table);
    storage_table_delete(table);
    return xml_api_make_success_no_body();
}

static xmlDoc *map_columns_to_indexes(unsigned int request_columns_amount, char **request_columns_names,
                                      struct storage_joined_table *table, unsigned int *columns_amount,
                                      unsigned int **columns_indexes) {
    unsigned int columns_count = request_columns_amount;

    uint16_t table_columns_amount = storage_joined_table_get_columns_amount(table);

    if (columns_count == 0) {
        columns_count = table_columns_amount;
    }

    *columns_indexes = malloc(sizeof(**columns_indexes) * columns_count);
    if (request_columns_amount == 0) {
        for (unsigned int i = 0; i < columns_count; ++i) {
            (*columns_indexes)[i] = i;
        }
    } else {
        for (unsigned int i = 0; i < columns_count; ++i) {
            bool found = false;

            for (unsigned int j = 0; j < table_columns_amount; ++j) {
                if (strcmp(request_columns_names[i], storage_joined_table_get_column(table, j).name) == 0) {
                    (*columns_indexes)[i] = j;
                    found = true;
                    break;
                }
            }

            if (!found) {
                size_t msg_length = 41 + strlen(request_columns_names[i]);

                char msg[msg_length];
                snprintf(msg, msg_length, "column with name %s is not exists in table", request_columns_names[i]);

                return xml_api_make_error(msg);
            }
        }
    }

    *columns_amount = columns_count;
    return NULL;
}

static xmlDoc *check_values(unsigned int request_values_amount, struct storage_value **request_values_values,
                            struct storage_table *table, unsigned int columns_amount,
                            const unsigned int *columns_indexes) {

    if (request_values_amount != columns_amount) {
        return xml_api_make_error("values amount is not equals to columns amount");
    }

    for (unsigned int i = 0; i < columns_amount; ++i) {
        if (request_values_values[i] == NULL) {
            continue;
        }

        struct storage_column column = table->columns.columns[columns_indexes[i]];
        if (request_values_values[i]->type == column.type) {
            continue;
        }

        switch (request_values_values[i]->type) {
            case STORAGE_COLUMN_TYPE_INT:
                if (column.type == STORAGE_COLUMN_TYPE_UINT) {
                    if (request_values_values[i]->value._int >= 0) {
                        request_values_values[i]->type = STORAGE_COLUMN_TYPE_UINT;
                        request_values_values[i]->value.uint = (uint64_t) request_values_values[i]->value._int;
                        continue;
                    }
                }

                break;

            case STORAGE_COLUMN_TYPE_UINT:
                if (column.type == STORAGE_COLUMN_TYPE_INT) {
                    if (request_values_values[i]->value.uint <= INT64_MAX) {
                        request_values_values[i]->type = STORAGE_COLUMN_TYPE_INT;
                        request_values_values[i]->value._int = (int64_t) request_values_values[i]->value.uint;
                        continue;
                    }
                }

                break;

            default:
                break;
        }

        const char *col_type = storage_column_type_to_string(column.type);
        const char *val_type = storage_column_type_to_string(request_values_values[i]->type);
        size_t msg_length = 47 + strlen(column.name) + strlen(col_type) + strlen(val_type);

        char msg[msg_length];
        snprintf(msg, msg_length, "value for column with name %s (%s) has wrong type %s",
                 column.name, col_type, val_type);
        return xml_api_make_error(msg);
    }

    return NULL;
}

static xmlDoc *handle_request_insert(struct xml_api_insert_request request, struct storage *storage) {
    struct storage_table *table = storage_find_table(storage, request.table_name);

    if (!table) {
        return xml_api_make_error("table with the specified name is not exists");
    }

    unsigned int columns_amount;
    unsigned int *columns_indexes;
    struct storage_joined_table *joined_table = storage_joined_table_wrap(table);

    {
        xmlDoc *error = map_columns_to_indexes(request.columns.amount, request.columns.columns,
                                               joined_table, &columns_amount, &columns_indexes);

        if (error) {
            storage_joined_table_delete(joined_table);
            return error;
        }
    }

    {
        xmlDoc *error = check_values(request.values.amount, request.values.values, table, columns_amount,
                                     columns_indexes);

        if (error) {
            free(columns_indexes);
            storage_joined_table_delete(joined_table);
            return error;
        }
    }

    struct storage_row *row = storage_table_add_row(table);
    for (unsigned int i = 0; i < columns_amount; ++i) {
        storage_row_set_value(row, columns_indexes[i], request.values.values[i]);
    }

    free(columns_indexes);
    storage_row_delete(row);
    storage_joined_table_delete(joined_table);
    return xml_api_make_success_no_body();
}

static xmlDoc *is_where_correct(struct storage_joined_table *table, struct xml_api_where *where) {
    uint16_t table_columns_amount = storage_joined_table_get_columns_amount(table);

    switch (where->op) {
        case XML_API_OPERATOR_EQ:
        case XML_API_OPERATOR_NE:
            if (where->value == NULL) {
                return NULL;
            }

        case XML_API_OPERATOR_LT:
        case XML_API_OPERATOR_GT:
        case XML_API_OPERATOR_LE:
        case XML_API_OPERATOR_GE:
            if (where->value == NULL) {
                return xml_api_make_error("NULL value is not comparable");
            }

            for (unsigned int i = 0; i < table_columns_amount; ++i) {
                struct storage_column column = storage_joined_table_get_column(table, i);

                if (strcmp(column.name, where->column) == 0) {
                    switch (column.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                        case STORAGE_COLUMN_TYPE_UINT:
                        case STORAGE_COLUMN_TYPE_NUM:
                            switch (where->value->type) {
                                case STORAGE_COLUMN_TYPE_INT:
                                case STORAGE_COLUMN_TYPE_UINT:
                                case STORAGE_COLUMN_TYPE_NUM:
                                    return NULL;

                                case STORAGE_COLUMN_TYPE_STR:
                                    break;
                            }

                            break;

                        case STORAGE_COLUMN_TYPE_STR:
                            if (where->value->type == STORAGE_COLUMN_TYPE_STR) {
                                return NULL;
                            }

                            break;
                    }

                    const char *column_type = storage_column_type_to_string(column.type);
                    const char *value_type = storage_column_type_to_string(where->value->type);
                    size_t msg_length = 31 + strlen(column_type) + strlen(value_type);
                    char msg[msg_length];

                    snprintf(msg, msg_length, "types %s and %s are not comparable", column_type, value_type);
                    return xml_api_make_error(msg);
                }
            }

            {
                size_t msg_length = 41 + strlen(where->column);

                char msg[msg_length];
                snprintf(msg, msg_length, "column with name %s is not exists in table", where->column);

                return xml_api_make_error(msg);
            }

        case XML_API_OPERATOR_AND:
        case XML_API_OPERATOR_OR: {
            xmlDoc *left = is_where_correct(table, where->left);
            if (left != NULL) {
                return left;
            }

            return is_where_correct(table, where->right);
        }
    }
}

static bool compare_values_not_null(enum xml_api_operator op, struct storage_value left, struct storage_value right) {
    switch (op) {
        case XML_API_OPERATOR_EQ:
            switch (left.type) {
                case STORAGE_COLUMN_TYPE_INT:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            return left.value._int == right.value._int;

                        case STORAGE_COLUMN_TYPE_UINT:
                            if (left.value._int < 0) {
                                return false;
                            }

                            return ((uint64_t) left.value._int) == right.value.uint;

                        case STORAGE_COLUMN_TYPE_NUM:
                            return ((double) left.value._int) == right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_UINT:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            if (right.value._int < 0) {
                                return false;
                            }

                            return left.value.uint == ((uint64_t) right.value._int);

                        case STORAGE_COLUMN_TYPE_UINT:
                            return left.value.uint == right.value.uint;

                        case STORAGE_COLUMN_TYPE_NUM:
                            return ((double) left.value.uint) == right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_NUM:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            return left.value.num == ((double) right.value._int);

                        case STORAGE_COLUMN_TYPE_UINT:
                            return left.value.num == ((double) right.value.uint);

                        case STORAGE_COLUMN_TYPE_NUM:
                            return left.value.num == right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_STR:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                        case STORAGE_COLUMN_TYPE_UINT:
                        case STORAGE_COLUMN_TYPE_NUM:
                            return false;

                        case STORAGE_COLUMN_TYPE_STR:
                            return strcmp(left.value.str, right.value.str) == 0;
                    }
            }

        case XML_API_OPERATOR_NE:
            return !compare_values_not_null(XML_API_OPERATOR_EQ, left, right);

        case XML_API_OPERATOR_LT:
            switch (left.type) {
                case STORAGE_COLUMN_TYPE_INT:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            return left.value._int < right.value._int;

                        case STORAGE_COLUMN_TYPE_UINT:
                            if (left.value._int < 0) {
                                return true;
                            }

                            return ((uint64_t) left.value._int) < right.value.uint;

                        case STORAGE_COLUMN_TYPE_NUM:
                            return ((double) left.value._int) < right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_UINT:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            if (right.value._int < 0) {
                                return false;
                            }

                            return left.value.uint < ((uint64_t) right.value._int);

                        case STORAGE_COLUMN_TYPE_UINT:
                            return left.value.uint < right.value.uint;

                        case STORAGE_COLUMN_TYPE_NUM:
                            return ((double) left.value.uint) < right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_NUM:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            return left.value.num < ((double) right.value._int);

                        case STORAGE_COLUMN_TYPE_UINT:
                            return left.value.num < ((double) right.value.uint);

                        case STORAGE_COLUMN_TYPE_NUM:
                            return left.value.num < right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_STR:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                        case STORAGE_COLUMN_TYPE_UINT:
                        case STORAGE_COLUMN_TYPE_NUM:
                            return false;

                        case STORAGE_COLUMN_TYPE_STR:
                            return strcmp(left.value.str, right.value.str) < 0;
                    }
            }

        case XML_API_OPERATOR_GT:
            switch (left.type) {
                case STORAGE_COLUMN_TYPE_INT:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            return left.value._int > right.value._int;

                        case STORAGE_COLUMN_TYPE_UINT:
                            if (left.value._int < 0) {
                                return false;
                            }

                            return ((uint64_t) left.value._int) > right.value.uint;

                        case STORAGE_COLUMN_TYPE_NUM:
                            return ((double) left.value._int) > right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_UINT:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            if (right.value._int < 0) {
                                return true;
                            }

                            return left.value.uint > ((uint64_t) right.value._int);

                        case STORAGE_COLUMN_TYPE_UINT:
                            return left.value.uint > right.value.uint;

                        case STORAGE_COLUMN_TYPE_NUM:
                            return ((double) left.value.uint) > right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_NUM:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                            return left.value.num > ((double) right.value._int);

                        case STORAGE_COLUMN_TYPE_UINT:
                            return left.value.num > ((double) right.value.uint);

                        case STORAGE_COLUMN_TYPE_NUM:
                            return left.value.num > right.value.num;

                        case STORAGE_COLUMN_TYPE_STR:
                            return false;
                    }

                case STORAGE_COLUMN_TYPE_STR:
                    switch (right.type) {
                        case STORAGE_COLUMN_TYPE_INT:
                        case STORAGE_COLUMN_TYPE_UINT:
                        case STORAGE_COLUMN_TYPE_NUM:
                            return false;

                        case STORAGE_COLUMN_TYPE_STR:
                            return strcmp(left.value.str, right.value.str) > 0;
                    }
            }

        case XML_API_OPERATOR_LE:
            return !compare_values_not_null(XML_API_OPERATOR_GT, left, right);

        case XML_API_OPERATOR_GE:
            return !compare_values_not_null(XML_API_OPERATOR_LT, left, right);

        default:
            return false;
    }
}

static bool compare_values(enum xml_api_operator op, struct storage_value *left, struct storage_value *right) {
    switch (op) {
        case XML_API_OPERATOR_EQ:
            if (left == NULL || right == NULL) {
                return left == NULL && right == NULL;
            }

            break;

        case XML_API_OPERATOR_NE:
            if (left == NULL || right == NULL) {
                return (left == NULL) != (right == NULL);
            }

            break;

        case XML_API_OPERATOR_LT:
        case XML_API_OPERATOR_GT:
        case XML_API_OPERATOR_LE:
        case XML_API_OPERATOR_GE:
            if (left == NULL || right == NULL) {
                return false;
            }

            break;

        default:
            return false;
    }

    return compare_values_not_null(op, *left, *right);
}

static bool eval_where(struct storage_joined_row *row, struct xml_api_where *where) {
    uint16_t table_columns_amount = storage_joined_table_get_columns_amount(row->table);

    switch (where->op) {
        case XML_API_OPERATOR_EQ:
        case XML_API_OPERATOR_NE:
        case XML_API_OPERATOR_LT:
        case XML_API_OPERATOR_GT:
        case XML_API_OPERATOR_LE:
        case XML_API_OPERATOR_GE:
            for (unsigned int i = 0; i < table_columns_amount; ++i) {
                if (strcmp(storage_joined_table_get_column(row->table, i).name, where->column) == 0) {
                    return compare_values(where->op, storage_joined_row_get_value(row, i), where->value);
                }
            }

            errno = EINVAL;
            return false;

        case XML_API_OPERATOR_AND:
            return eval_where(row, where->left) && eval_where(row, where->right);

        case XML_API_OPERATOR_OR:
            return eval_where(row, where->left) || eval_where(row, where->right);
    }
}

static xmlDoc *handle_request_delete(struct xml_api_delete_request request, struct storage *storage) {
    struct storage_table *table = storage_find_table(storage, request.table_name);

    if (!table) {
        return xml_api_make_error("table with the specified name is not exists");
    }

    struct storage_joined_table *joined_table = storage_joined_table_wrap(table);

    if (request.where != NULL) {
        xmlDoc *error = is_where_correct(joined_table, request.where);

        if (error) {
            storage_joined_table_delete(joined_table);
            return error;
        }
    }

    unsigned long long amount = 0;
    for (struct storage_joined_row *row = storage_joined_table_get_first_row(
            joined_table); row; row = storage_joined_row_next(row)) {
        if (request.where == NULL || eval_where(row, request.where)) {
            storage_row_remove(row->rows[0]);
            ++amount;
        }
    }

    storage_joined_table_delete(joined_table);
    xmlNodePtr responseNode = xmlNewNode(NULL, BAD_CAST "response");
    xmlNodePtr amountNode = xmlNewNode(NULL, BAD_CAST "amount");
    char str_amount[21];
    sprintf(str_amount, "%llu", amount);
    xmlNodePtr textNode = xmlNewText(BAD_CAST str_amount);
    xmlAddChild(amountNode, textNode);
    xmlAddChild(responseNode, amountNode);
    return xml_api_make_success(responseNode);
}

static xmlDoc *handle_request_select(struct xml_api_select_request request, struct storage *storage) {
    if (request.limit > 1000) {
        return xml_api_make_error("limit is too high");
    }

    struct storage_table *table = storage_find_table(storage, request.table_name);

    if (!table) {
        return xml_api_make_error("table with the specified name is not exists");
    }

    struct storage_joined_table *joined_table = storage_joined_table_new(request.joins.amount + 1);
    joined_table->tables.tables[0].table = table;
    joined_table->tables.tables[0].t_column_index = 0;
    joined_table->tables.tables[0].s_column_index = 0;

    for (int i = 0; i < request.joins.amount; ++i) {
        joined_table->tables.tables[i + 1].table = storage_find_table(storage, request.joins.joins[i].table);

        if (!joined_table->tables.tables[i + 1].table) {
            storage_joined_table_delete(joined_table);
            return xml_api_make_error("table with the specified name is not exists");
        }

        joined_table->tables.tables[i + 1].t_column_index = (uint16_t) -1;
        for (int j = 0; j < joined_table->tables.tables[i + 1].table->columns.amount; ++j) {
            if (strcmp(request.joins.joins[i].t_column,
                       joined_table->tables.tables[i + 1].table->columns.columns[j].name) == 0) {
                joined_table->tables.tables[i + 1].t_column_index = j;
                break;
            }
        }

        if (joined_table->tables.tables[i + 1].t_column_index >=
            joined_table->tables.tables[i + 1].table->columns.amount) {
            storage_joined_table_delete(joined_table);
            return xml_api_make_error("column with the specified name is not exists in table");
        }

        uint16_t slice_columns = 0;
        joined_table->tables.tables[i + 1].s_column_index = (uint16_t) -1;
        for (int tbl_index = 0, col_index = 0; tbl_index <= i; ++tbl_index) {
            for (int tbl_col_index = 0; tbl_col_index <
                                        joined_table->tables.tables[tbl_index].table->columns.amount; ++tbl_col_index, ++col_index) {
                if (strcmp(request.joins.joins[i].s_column,
                           joined_table->tables.tables[tbl_index].table->columns.columns[tbl_col_index].name) == 0) {
                    joined_table->tables.tables[i + 1].s_column_index = col_index;
                    break;
                }
            }

            slice_columns += joined_table->tables.tables[tbl_index].table->columns.amount;
            if (joined_table->tables.tables[i + 1].s_column_index < slice_columns) {
                break;
            }
        }

        if (joined_table->tables.tables[i + 1].s_column_index >= slice_columns) {
            storage_joined_table_delete(joined_table);
            return xml_api_make_error("column with the specified name is not exists in the join slice");
        }
    }

    if (request.where) {
        xmlDoc *error = is_where_correct(joined_table, request.where);

        if (error) {
            storage_joined_table_delete(joined_table);
            return error;
        }
    }

    unsigned int columns_amount;
    unsigned int *columns_indexes;

    {
        xmlDoc *error = map_columns_to_indexes(request.columns.amount, request.columns.columns,
                                               joined_table, &columns_amount, &columns_indexes);

        if (error) {
            storage_joined_table_delete(joined_table);
            return error;
        }
    }

    xmlNodePtr responseNode = xmlNewNode(NULL, BAD_CAST "response");
    {
        xmlNodePtr columnsNode = xmlNewNode(NULL, BAD_CAST "columns");

        for (unsigned int i = 0; i < columns_amount; ++i) {
            xmlNodePtr textNode = xmlNewText(
                    BAD_CAST storage_joined_table_get_column(joined_table, columns_indexes[i]).name);
            xmlNodePtr columnNode = xmlNewNode(NULL, BAD_CAST "column");
            xmlAddChild(columnNode, textNode);
            xmlAddChild(columnsNode, columnNode);
        }

        xmlAddChild(responseNode, columnsNode);
    }

    {
        xmlNodePtr valuesNode = xmlNewNode(NULL, BAD_CAST "values");

        unsigned int offset = 0, amount = 0;
        for (struct storage_joined_row *row = storage_joined_table_get_first_row(
                joined_table); row; row = storage_joined_row_next(row)) {
            if (request.where == NULL || eval_where(row, request.where)) {
                if (offset < request.offset) {
                    ++offset;
                    continue;
                }

                if (amount == request.limit) {
                    break;
                }

                xmlNodePtr rowNode = xmlNewNode(NULL, BAD_CAST "row");

                for (unsigned int i = 0; i < columns_amount; ++i) {
                    struct storage_value *val = storage_joined_row_get_value(row, columns_indexes[i]);
                    xmlNodePtr textNode = xml_api_from_value(val);
                    xmlNodePtr valueNode = xmlNewNode(NULL, BAD_CAST "value");
                    xmlNewProp(valueNode, BAD_CAST "type", BAD_CAST val->type);
                    xmlAddChild(valueNode, textNode);
                    xmlAddChild(rowNode, valueNode);
                }
                xmlAddChild(valuesNode, rowNode);
                ++amount;
            }
        }

        xmlAddChild(responseNode, valuesNode);
    }

    free(columns_indexes);
    storage_joined_table_delete(joined_table);
    return xml_api_make_success(responseNode);
}

static xmlDoc *handle_request_update(struct xml_api_update_request request, struct storage *storage) {
    struct storage_table *table = storage_find_table(storage, request.table_name);

    if (!table) {
        return xml_api_make_error("table with the specified name is not exists");
    }

    struct storage_joined_table *joined_table = storage_joined_table_wrap(table);

    if (request.where) {
        xmlDoc *error = is_where_correct(joined_table, request.where);

        if (error) {
            storage_joined_table_delete(joined_table);
            return error;
        }
    }

    unsigned int columns_amount;
    unsigned int *columns_indexes;

    {
        xmlDoc *error = map_columns_to_indexes(request.columns.amount, request.columns.columns,
                                               joined_table, &columns_amount, &columns_indexes);

        if (error) {
            storage_joined_table_delete(joined_table);
            return error;
        }
    }

    {
        xmlDoc *error = check_values(request.values.amount, request.values.values, table, columns_amount,
                                     columns_indexes);

        if (error) {
            free(columns_indexes);
            storage_joined_table_delete(joined_table);
            return error;
        }
    }

    unsigned long long amount = 0;
    for (struct storage_joined_row *row = storage_joined_table_get_first_row(
            joined_table); row; row = storage_joined_row_next(row)) {
        if (request.where == NULL || eval_where(row, request.where)) {
            for (unsigned int i = 0; i < columns_amount; ++i) {
                storage_row_set_value(row->rows[0], columns_indexes[i], request.values.values[i]);
            }

            ++amount;
        }
    }

    free(columns_indexes);
    storage_joined_table_delete(joined_table);
    xmlNodePtr responseNode = xmlNewNode(NULL, BAD_CAST "response");
    xmlNodePtr amountNode = xmlNewNode(NULL, BAD_CAST "amount");
    char str_amount[21];
    sprintf(str_amount, "%llu", amount);
    xmlNodePtr textNode = xmlNewText(BAD_CAST str_amount);
    xmlAddChild(amountNode, textNode);
    xmlAddChild(responseNode, amountNode);
    return xml_api_make_success(responseNode);
}

static xmlDoc *handle_request(xmlDoc *request, struct storage *storage) {
    enum xml_api_action action = xml_api_get_action(request);

    switch (action) {
        case XML_API_TYPE_CREATE_TABLE:
            return handle_request_create_table(xml_api_to_create_table_request(request), storage);

        case XML_API_TYPE_DROP_TABLE:
            return handle_request_drop_table(xml_api_to_drop_table_request(request), storage);

        case XML_API_TYPE_INSERT:
            return handle_request_insert(xml_api_to_insert_request(request), storage);

        case XML_API_TYPE_DELETE:
            return handle_request_delete(xml_api_to_delete_request(request), storage);

        case XML_API_TYPE_SELECT:
            return handle_request_select(xml_api_to_select_request(request), storage);

        case XML_API_TYPE_UPDATE:
            return handle_request_update(xml_api_to_update_request(request), storage);

        default:
            return NULL;
    }
}

static void handle_client(int socket, struct storage *storage) {
    printf("Connected\n");

    while (!closing) {
        char buffer[64 * 1024];

        ssize_t was_read = read(socket, buffer, sizeof(buffer) / sizeof(*buffer));
        if (was_read <= 0) {
            break;
        }

        if (was_read == sizeof(buffer) / sizeof(*buffer)) {
            buffer[sizeof(buffer) / sizeof(*buffer) - 1] = '\0';
        } else {
            buffer[was_read] = '\0';
        }
        xmlDoc *request = xmlReadMemory(buffer, (int) strlen(buffer), "action.tmp", 0, 0);
        printf("Request:\n %s\n", buffer);

        xmlDoc *response_doc = NULL;

        if (request) {
            response_doc = handle_request(request, storage);
        }
        xmlChar *xmlbuff;
        int buffersize;
        xmlDocDumpFormatMemory(response_doc, &xmlbuff, &buffersize, 1);
        const char *response = (char *) xmlbuff;
        printf("Response: \n %s\n", response);

        size_t response_length = strlen(response);
        while (response_length > 0) {
            ssize_t wrote = write(socket, response, response_length);

            if (wrote <= 0) {
                break;
            }

            response_length -= wrote;
            response += wrote;
        }
    }

    close(socket);
    printf("Disconnected\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return 0;
    }

    int fd = open(argv[1], O_RDWR);
    struct storage *storage;

    if (fd < 0 && errno != ENOENT) {
        perror("Error while opening file");
        return errno;
    }

    if (fd < 0 && errno == ENOENT) {
        fd = open(argv[1], O_CREAT | O_RDWR, 0644);
        storage = storage_init(fd);
    } else {
        storage = storage_open(fd);
    }

    // create the server socket
    int server_socket;
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    // define the server address
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9002);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // bind the socket to our specified IP and port
    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) != 0) {
        perror("Cannot start server");
        return 0;
    }

    // second argument is a backlog - how many connections can be waiting for this socket simultaneously
    listen(server_socket, 1);

    {
        struct sigaction sa;

        sa.sa_sigaction = close_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;

        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
    }

    while (!closing) {
        int ret = accept(server_socket, NULL, NULL);

        if (ret < 0) {
            break;
        }

        handle_client(ret, storage);
    }

    close(server_socket);
    storage_delete(storage);
    close(fd);

    printf("Bye!\n");
    return 0;
}
