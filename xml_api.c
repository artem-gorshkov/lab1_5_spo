#include "xml_api.h"
#include <errno.h>

static xmlChar *find_node_value(xmlNodePtr root_node, xmlChar *target_node) {
    xmlNodePtr curr_node;

    for (curr_node = root_node; curr_node; curr_node = curr_node->next) {
        if (curr_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(target_node, curr_node->name) == 0) {
                return curr_node->children->content;
            }
        }

        if (curr_node->children != NULL) {
            xmlChar *result = find_node_value(curr_node->children, target_node);
            if (result != NULL) {
                return result;
            }
        }
    }

    return NULL;
}

xmlNodePtr find_node(xmlNodePtr root_node, xmlChar *target_node) {
    xmlNodePtr curr_node;

    for (curr_node = root_node; curr_node; curr_node = curr_node->next) {
        if (curr_node->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(target_node, curr_node->name) == 0) {
                return curr_node;
            }
        }

        if (curr_node->children != NULL) {
            xmlNodePtr result = find_node(curr_node->children, target_node);
            if (result != NULL) {
                return result;
            }
        }
    }

    return NULL;
}

enum xml_api_action xml_api_get_action(xmlDoc *doc) {
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    xmlNodePtr curr_node;

    for (curr_node = root_node; curr_node; curr_node = curr_node->next) {
        xmlChar *result = find_node_value(root_node, xmlCharStrdup("action"));
        if (result != NULL) {
            return atoi(result);
        }
    }

    return -1;
}

struct xml_api_create_table_request xml_api_to_create_table_request(xmlDoc *doc) {
    struct xml_api_create_table_request request;
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    xmlNodePtr curr_node = root_node;

    request.table_name = strdup((char *) find_node_value(curr_node, BAD_CAST "table"));
    xmlNodePtr columns_node = find_node(curr_node, xmlCharStrdup("columns"));
    int amount = (int) xmlChildElementCount(columns_node);
    request.columns.amount = amount;
    request.columns.columns = malloc(sizeof(*request.columns.columns) * request.columns.amount);
    int i = 0;
    for (curr_node = columns_node->children; curr_node; curr_node = curr_node->next, i++) {
        xmlChar *name = find_node_value(curr_node, xmlCharStrdup("name"));
        xmlChar *type = find_node_value(curr_node, xmlCharStrdup("type"));
        request.columns.columns[i].name = strdup((char *) name);
        request.columns.columns[i].type = atoi((char *) type);
    }

    return request;
}

struct xml_api_drop_table_request xml_api_to_drop_table_request(xmlDoc *doc) {
    struct xml_api_drop_table_request request;
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    xmlNodePtr curr_node = root_node;

    request.table_name = strdup((char *) find_node_value(curr_node, BAD_CAST "table"));

    return request;
}

static struct storage_value *xml_to_storage_value(xmlChar *value) {
    if (xmlStrcmp(value, BAD_CAST "null") == 0) {
        return NULL;
    }

    struct storage_value *st_value;
    regex_t regex;
    int result;

    regcomp(&regex, "[\'\"].*[\'\"]", 0);
    result = regexec(&regex, (char *) value, 0, NULL, 0);
    if (!result) {
        st_value->type = STORAGE_COLUMN_TYPE_STR;
        st_value->value.str = strdup((char *) value);
        return st_value;
    }

    regcomp(&regex, "[-+]?[1-9]\\d*", 0);
    result = regexec(&regex, (char *) value, 0, NULL, 0);
    if (!result) {
        st_value->type = STORAGE_COLUMN_TYPE_INT;
        st_value->value._int = atoi((char *) value);
        return st_value;
    }

    regcomp(&regex, "[+-]?[0-9]\\.[0-9]+", 0);
    result = regexec(&regex, (char *) value, 0, NULL, 0);
    if (!result) {
        st_value->type = STORAGE_COLUMN_TYPE_NUM;
        st_value->value.num = atof((char *) value);
        return st_value;
    }

    errno = EINVAL;
}

struct xml_api_insert_request xml_api_to_insert_request(xmlDoc *doc) {
    struct xml_api_insert_request request;
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    xmlNodePtr curr_node = root_node;

    request.table_name = strdup((char *) find_node_value(curr_node, BAD_CAST "table"));

    xmlNodePtr columns_node = find_node(curr_node, xmlCharStrdup("columns"));
    int columns_amount = (int) xmlChildElementCount(columns_node);
    request.columns.amount = columns_amount;
    request.columns.columns = malloc(sizeof(*request.columns.columns) * request.columns.amount);
    int i = 0;
    for (curr_node = columns_node->children; curr_node; curr_node = curr_node->next, i++) {
        xmlChar *name = find_node_value(curr_node, xmlCharStrdup("column"));
        request.columns.columns[i] = strdup((char *) name);
    }

    xmlNodePtr values_node = find_node(curr_node, xmlCharStrdup("values"));
    int values_amount = (int) xmlChildElementCount(values_node);
    request.values.amount = values_amount;
    request.values.values = malloc(sizeof(struct storage_value *) * request.values.amount);
    i = 0;
    for (curr_node = values_node->children; curr_node; curr_node = curr_node->next, i++) {
        xmlChar *name = find_node_value(curr_node, xmlCharStrdup("value"));
        request.values.values[i] = xml_to_storage_value(name);
    }

    return request;
}

static struct xml_api_where *do_xml_api_to_where(xmlNode *node) {
    struct xml_api_where *where = malloc(sizeof(*where));

    xmlChar *op_value = find_node_value(node, BAD_CAST "op");
    where->op = atoi((char *) op_value);

    switch (where->op) {
        case XML_API_OPERATOR_AND:
        case XML_API_OPERATOR_OR: {
            xmlNodePtr left_node = find_node(node, BAD_CAST "left");
            where->left = do_xml_api_to_where(left_node);

            xmlNodePtr right_node = find_node(node, BAD_CAST "right");
            where->right = do_xml_api_to_where(right_node);
            break;
        }
        default: {
            xmlChar *column_value = find_node_value(node, BAD_CAST "column");
            where->column = strdup((char *) column_value);
            xmlChar *value_value = find_node_value(node, BAD_CAST "value");
            where->value = xml_to_storage_value(value_value);
        }
    }

    return where;
}

static struct xml_api_where *xml_api_to_where(xmlDoc *doc) {
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    xmlNodePtr where_node = find_node(root_node, BAD_CAST "where");
    return do_xml_api_to_where(root_node);
}

struct xml_api_delete_request xml_api_to_delete_request(xmlDoc *doc) {
    struct xml_api_delete_request request;
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    xmlNodePtr curr_node = root_node;

    request.table_name = strdup((char *) find_node_value(curr_node, BAD_CAST "table"));

    xmlNodePtr where_node = find_node(curr_node, BAD_CAST "where");
    request.where = xml_api_to_where(where_node->doc);

    return request;
}

struct xml_api_select_request xml_api_to_select_request(xmlDoc *doc) {
    struct xml_api_select_request request;
    request.columns.amount = 0;
    request.columns.columns = NULL;
    request.joins.amount = 0;
    request.joins.joins = NULL;
    request.where = NULL;
    request.offset = 0;
    request.limit = 10;
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    xmlNodePtr curr_node = root_node;

    request.table_name = strdup((char *) find_node_value(curr_node, BAD_CAST "table"));
    xmlChar *offset = find_node_value(curr_node, BAD_CAST "offset");
    if (offset != NULL) {
        request.offset = atoi((char *) offset);
    }
    xmlChar *limit = find_node_value(curr_node, BAD_CAST "limit");
    if (limit != NULL) {
        request.limit = atoi((char *) limit);
    }

    xmlNodePtr columns_node = find_node(curr_node, xmlCharStrdup("columns"));
    int columns_amount = (int) xmlChildElementCount(columns_node);
    request.columns.amount = columns_amount;
    request.columns.columns = malloc(sizeof(*request.columns.columns) * request.columns.amount);
    int i = 0;
    for (curr_node = columns_node->children; curr_node; curr_node = curr_node->next, i++) {
        xmlChar *name = find_node_value(curr_node, xmlCharStrdup("column"));
        request.columns.columns[i] = strdup((char *) name);
    }

    xmlNodePtr joins_node = find_node(curr_node, xmlCharStrdup("joins"));
    int joins_amount = (int) xmlChildElementCount(columns_node);
    request.joins.amount = joins_amount;
    request.joins.joins = malloc(sizeof(*request.joins.joins) * request.joins.amount);
    i = 0;
    for (curr_node = joins_node->children; curr_node; curr_node = curr_node->next, i++) {
        xmlChar *table_name = find_node_value(curr_node, xmlCharStrdup("table"));
        xmlChar *t_column = find_node_value(curr_node, xmlCharStrdup("t_column"));
        xmlChar *s_column = find_node_value(curr_node, xmlCharStrdup("s_column"));
        request.joins.joins[i].table = strdup((char *) table_name);
        request.joins.joins[i].s_column = strdup((char *) s_column);
        request.joins.joins[i].t_column = strdup((char *) t_column);
    }

    xmlNodePtr where_node = find_node(root_node, BAD_CAST "where");
    request.where = xml_api_to_where(where_node->doc);

    return request;
}

struct xml_api_update_request xml_api_to_update_request(xmlDoc *doc) {
    struct xml_api_update_request request;
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    xmlNodePtr curr_node = root_node;

    request.table_name = strdup((char *) find_node_value(curr_node, BAD_CAST "table"));

    xmlNodePtr columns_node = find_node(curr_node, xmlCharStrdup("columns"));
    int columns_amount = (int) xmlChildElementCount(columns_node);
    request.columns.amount = columns_amount;
    request.columns.columns = malloc(sizeof(*request.columns.columns) * request.columns.amount);
    int i = 0;
    for (curr_node = columns_node->children; curr_node; curr_node = curr_node->next, i++) {
        xmlChar *name = find_node_value(curr_node, xmlCharStrdup("column"));
        request.columns.columns[i] = strdup((char *) name);
    }

    xmlNodePtr values_node = find_node(curr_node, xmlCharStrdup("values"));
    int values_amount = (int) xmlChildElementCount(values_node);
    request.values.amount = values_amount;
    request.values.values = malloc(sizeof(struct storage_value *) * request.values.amount);
    i = 0;
    for (curr_node = values_node->children; curr_node; curr_node = curr_node->next, i++) {
        xmlChar *name = find_node_value(curr_node, xmlCharStrdup("value"));
        request.values.values[i] = xml_to_storage_value(name);
    }

    xmlNodePtr where_node = find_node(root_node, BAD_CAST "where");
    request.where = xml_api_to_where(where_node->doc);

    return request;
}

xmlDoc *xml_api_make_success(xmlNode *answer) {
    xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
    xmlDocSetRootElement(doc, answer);
    return doc;
}

xmlDoc *xml_api_make_success_no_body() {
    xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr responseNode = xmlNewNode(NULL, BAD_CAST "response");
    xmlDocSetRootElement(doc, responseNode);
    return doc;
}

xmlDoc *xml_api_make_error(const char *msg) {
    xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr responseNode = xmlNewNode(NULL, BAD_CAST "response");
    xmlNodePtr errorNode = xmlNewNode(NULL, BAD_CAST "error");
    xmlNodePtr textNode = xmlNewText(BAD_CAST msg);
    xmlAddChild(errorNode, textNode);
    xmlAddChild(responseNode, textNode);
    xmlDocSetRootElement(doc, responseNode);
    return doc;
}