%parse-param {xmlDoc ** result} {char ** error}

%{
#include <string.h>
#ifdef __APPLE__
#include "../xml_api.h"
#else
#include "xml_api.h"
#endif

int yylex(void);
void yyerror(xmlDoc ** result, char ** error, const char * str);
%}

%define api.value.type {xmlNode *}

%token T_CREATE T_TABLE T_IDENTIFIER T_DBL_QUOTED T_INT T_UINT T_NUM T_STR T_DROP T_INSERT T_VALUES T_INTO
    T_INT_LITERAL T_UINT_LITERAL T_NUM_LITERAL T_STR_LITERAL T_NULL T_DELETE T_FROM T_WHERE T_JOIN T_ON
    T_EQ_OP T_NE_OP T_LT_OP T_GT_OP T_LE_OP T_GE_OP T_SELECT T_ASTERISK T_OFFSET T_LIMIT T_UPDATE T_SET

%left T_OR_OP
%left T_AND_OP

%%

command_line
    : command semi_non_req YYEOF    { *result = xmlNewDoc(BAD_CAST "1.0");  xmlDocSetRootElement(*result, $1); }
    | YYEOF                         { *result = NULL; }
    ;

semi_non_req
    : /* empty */
    | ';'
    ;

command
    : create_table_command  { $$ = $1; }
    | drop_table_command    { $$ = $1; }
    | insert_command        { $$ = $1; }
    | delete_command        { $$ = $1; }
    | select_command        { $$ = $1; }
//    | update_command        { $$ = $1; }
    ;

create_table_command
    : T_CREATE t_table_non_req name '(' columns_declaration_list ')'    {
        $$ = xmlNewNode(NULL, BAD_CAST "request");

        xmlNodePtr actionNode = xmlNewNode(NULL, BAD_CAST "action");
        xmlNodePtr textActionNode = xmlNewText(BAD_CAST "0");
        xmlAddChild(actionNode, textActionNode);
        xmlAddChild($$, actionNode);

        xmlNodePtr tableNode = xmlNewNode(NULL, BAD_CAST "table");
        xmlAddChild(tableNode, $3);
        xmlAddChild($$, tableNode);

        xmlAddChild($$, $5);
    }
    ;

t_table_non_req
    : /* empty */
    | T_TABLE
    ;

name
    : T_IDENTIFIER  { $$ = $1; }
    | T_DBL_QUOTED  { $$ = $1; }
    ;

columns_declaration_list
    : /* empty */                   { $$ = NULL; }
    | columns_declaration_list_req  { $$ = $1; }
    ;

columns_declaration_list_req
    : column_declaration                                    { $$ = xmlNewNode(NULL, BAD_CAST "columns"); xmlAddChild($$, $1); }
    | columns_declaration_list_req ',' column_declaration   { $$ = $1; xmlAddChild($$, $3); }
    ;

column_declaration
    : name type_node     {
        $$ = xmlNewNode(NULL, BAD_CAST "column");
        xmlAddChild($$, $1);
        xmlAddChild($$, $2);
    }
    ;

type_node
    : type { $$ = xmlNewNode(NULL, BAD_CAST "type"); xmlAddChild($$, $1); }

type
    : T_INT     { $$ = xmlNewText(BAD_CAST "0"); }
    | T_NUM     { $$ = xmlNewText(BAD_CAST "2"); }
    | T_STR     { $$ = xmlNewText(BAD_CAST "3"); }
    ;

drop_table_command
    : T_DROP t_table_non_req name   {
        $$ = xmlNewNode(NULL, BAD_CAST "request");

        xmlNodePtr actionNode = xmlNewNode(NULL, BAD_CAST "action");
        xmlNodePtr textActionNode = xmlNewText(BAD_CAST "1");
        xmlAddChild(actionNode, textActionNode);
        xmlAddChild($$, actionNode);

        xmlNodePtr tableNode = xmlNewNode(NULL, BAD_CAST "table");
        xmlAddChild(tableNode, $3);
        xmlAddChild($$, tableNode);

    }
    ;

insert_command
    : T_INSERT t_into_non_req name braced_names_list_non_req T_VALUES '(' values_list ')'   {
        $$ = xmlNewNode(NULL, BAD_CAST "request");

        xmlNodePtr actionNode = xmlNewNode(NULL, BAD_CAST "action");
        xmlNodePtr textActionNode = xmlNewText(BAD_CAST "2");
        xmlAddChild(actionNode, textActionNode);
        xmlAddChild($$, actionNode);

        xmlNodePtr tableNode = xmlNewNode(NULL, BAD_CAST "table");
        xmlAddChild(tableNode, $3);
        xmlAddChild($$, tableNode);

        xmlAddChild($$, $4);
        xmlAddChild($$, $7);
    }
    ;

t_into_non_req
    : /* empty */
    | T_INTO
    ;

braced_names_list_non_req
    : /* empty */           { $$ = NULL; }
    | braced_names_list     { $$ = $1; }
    ;

braced_names_list
    : '(' names_list_req ')'    { $$ = $2; }
    ;

names_list_req
    : column_name                     { $$ = xmlNewNode(NULL, BAD_CAST "columns"); xmlAddChild($$, $1);  }
    | names_list_req ',' column_name   { $$ = $1; xmlAddChild($$, $3); }
    ;

column_name
    : name { $$ = xmlNewNode(NULL, BAD_CAST "column"); xmlAddChild($$, $1); }

values_list
    : /* empty */       { $$ = NULL; }
    | values_list_req   { $$ = $1; }
    ;

values_list_req
    : value                     { $$ = xmlNewNode(NULL, BAD_CAST "values"); xmlAddChild($$, $1); }
    | values_list_req ',' value { $$ = $1; xmlAddChild($$, $3); }
    ;

value
    : T_INT_LITERAL     { $$ = $1; }
    | T_UINT_LITERAL    { $$ = $1; }
    | T_NUM_LITERAL     { $$ = $1; }
    | T_STR_LITERAL     { $$ = $1; }
    | T_NULL            { $$ = $1; }
    ;

delete_command
    : T_DELETE T_FROM name where_stmt_non_req   {
        $$ = xmlNewNode(NULL, BAD_CAST "request");

        xmlNodePtr actionNode = xmlNewNode(NULL, BAD_CAST "action");
        xmlNodePtr textActionNode = xmlNewText(BAD_CAST "3");
        xmlAddChild(actionNode, textActionNode);
        xmlAddChild($$, actionNode);

        xmlNodePtr tableNode = xmlNewNode(NULL, BAD_CAST "table");
        xmlAddChild(tableNode, $3);
        xmlAddChild($$, tableNode);

        if ($4) {
            xmlAddChild($$, $4);
        }
    }
    ;

where_stmt_non_req
    : /* empty */   { $$ = NULL; }
    | where_stmt    { $$ = $1; }
    ;

where_stmt
    : T_WHERE where_expr    { $$ = $2; }
    ;

where_expr
    : '(' where_expr ')'            { $$ = $2; }
    | name where_value_op value     {
        $$ = xmlNewNode(NULL, BAD_CAST "where");

        xmlAddChild($$, $2);

        xmlNodePtr columnNode = xmlNewNode(NULL, BAD_CAST "column");
        xmlAddChild(columnNode, $1);
        xmlAddChild($$, columnNode);

        xmlAddChild($$, $3);
    }
    | where_expr T_AND_OP where_expr    {
        $$ = xmlNewNode(NULL, BAD_CAST "where");

        xmlNodePtr opNode = xmlNewNode(NULL, BAD_CAST "op");
        xmlAddChild(opNode, xmlNewText(BAD_CAST XML_API_OPERATOR_AND));
        xmlAddChild($$, opNode);

        xmlNodePtr leftNode = xmlNewNode(NULL, BAD_CAST "left");
        xmlAddChild(leftNode, $1);
        xmlAddChild($$, leftNode);

        xmlNodePtr rightNode = xmlNewNode(NULL, BAD_CAST "left");
        xmlAddChild(rightNode, $3);
        xmlAddChild($$, rightNode);
    }
    | where_expr T_OR_OP where_expr     {
        $$ = xmlNewNode(NULL, BAD_CAST "where");

        xmlNodePtr opNode = xmlNewNode(NULL, BAD_CAST "op");
        xmlAddChild(opNode, xmlNewText(BAD_CAST XML_API_OPERATOR_OR));
        xmlAddChild($$, opNode);

        xmlNodePtr leftNode = xmlNewNode(NULL, BAD_CAST "left");
        xmlAddChild(leftNode, $1);
        xmlAddChild($$, leftNode);

        xmlNodePtr rightNode = xmlNewNode(NULL, BAD_CAST "left");
        xmlAddChild(rightNode, $3);
        xmlAddChild($$, rightNode);
    }
    ;

where_value_op
    : T_EQ_OP   { $$ =xmlNewNode(NULL, BAD_CAST "op"); xmlAddChild($$, xmlNewText(BAD_CAST XML_API_OPERATOR_EQ));}
    | T_NE_OP   { $$ =xmlNewNode(NULL, BAD_CAST "op"); xmlAddChild($$, xmlNewText(BAD_CAST XML_API_OPERATOR_NE));}
    | T_LT_OP   { $$ =xmlNewNode(NULL, BAD_CAST "op"); xmlAddChild($$, xmlNewText(BAD_CAST XML_API_OPERATOR_LT));}
    | T_GT_OP   { $$ =xmlNewNode(NULL, BAD_CAST "op"); xmlAddChild($$, xmlNewText(BAD_CAST XML_API_OPERATOR_GT));}
    | T_LE_OP   { $$ =xmlNewNode(NULL, BAD_CAST "op"); xmlAddChild($$, xmlNewText(BAD_CAST XML_API_OPERATOR_LE));}
    | T_GE_OP   { $$ =xmlNewNode(NULL, BAD_CAST "op"); xmlAddChild($$, xmlNewText(BAD_CAST XML_API_OPERATOR_GE));}
    ;

select_command
    : T_SELECT names_list_or_asterisk T_FROM name join_stmts where_stmt_non_req offset_stmt_non_req limit_stmt_non_req {
                $$ = xmlNewNode(NULL, BAD_CAST "request");

                xmlNodePtr actionNode = xmlNewNode(NULL, BAD_CAST "action");
                xmlNodePtr textActionNode = xmlNewText(BAD_CAST "4");
                xmlAddChild(actionNode, textActionNode);
                xmlAddChild($$, actionNode);

                xmlNodePtr tableNode = xmlNewNode(NULL, BAD_CAST "table");
                xmlAddChild(tableNode, $4);
                xmlAddChild($$, tableNode);

        if ($2) {
            xmlAddChild($$, $2);
        }

        if ($5) {
            xmlAddChild($$, $5);
        }

        if ($6) {
            xmlAddChild($$, $6);
        }

        if ($7) {
            xmlAddChild($$, $7);
        }

        if ($8) {
            xmlAddChild($$, $8);
        }
    }
    ;

names_list_or_asterisk
    : names_list_req    { $$ = $1; }
    | T_ASTERISK        { $$ = NULL; }
    ;

join_stmts
    : /* empty */           { $$ = NULL; }
    | join_stmts_non_null   { $$ = $1; }
    ;

join_stmts_non_null
    : join_stmt             { $$ = xmlNewNode(NULL, BAD_CAST "joins"); xmlAddChild($$, $1); }
    | join_stmts join_stmt  { $$ = $1; xmlAddChild($$, $2); }
    ;

join_stmt
    : T_JOIN name T_ON name T_EQ_OP name    {
        $$ = xmlNewNode(NULL, BAD_CAST "join");
        xmlNodePtr tableNode = xmlNewNode(NULL, BAD_CAST "table");
        xmlAddChild(tableNode, $2);
        xmlAddChild($$, tableNode);
        xmlNodePtr tNode = xmlNewNode(NULL, BAD_CAST "t_column");
        xmlAddChild(tNode, $4);
        xmlAddChild($$, tNode);
        xmlNodePtr sNode = xmlNewNode(NULL, BAD_CAST "s_column");
        xmlAddChild(sNode, $6);
        xmlAddChild($$, sNode);
    }
    ;

offset_stmt_non_req
    : /* empty */   { $$ = NULL; }
    | offset_stmt   { $$ = $1; }
    ;

offset_stmt
    : T_OFFSET T_UINT_LITERAL   { $$ = $2; }
    ;

limit_stmt_non_req
    : /* empty */   { $$ = NULL; }
    | limit_stmt    { $$ = $1; }
    ;

limit_stmt
    : T_LIMIT T_UINT_LITERAL    { $$ = $2; }
    ;

//update_command
//: T_UPDATE name T_SET update_values_list_req where_stmt_non_req {
//$$ = xmlNewNode(NULL, BAD_CAST "request");
//
//xmlNodePtr actionNode = xmlNewNode(NULL, BAD_CAST "action");
//xmlNodePtr textActionNode = xmlNewText(BAD_CAST "5");
//xmlAddChild(actionNode, textActionNode);
//xmlAddChild($$, actionNode);
//
//xmlNodePtr tableNode = xmlNewNode(NULL, BAD_CAST "table");
//xmlAddChild(tableNode, $2);
//xmlAddChild($$, tableNode);
//
//
//
//int c = json_object_array_length($4);
//struct json_object * columns = json_object_new_array_ext(c);
//struct json_object * values = json_object_new_array_ext(c);
//for (int i = 0; i < c; ++i) {
//struct json_object * elem = json_object_array_get_idx($4, i);
//
//json_object_array_add(columns, json_object_array_get_idx(elem, 0));
//json_object_array_add(values, json_object_array_get_idx(elem, 1));
//}
//
//json_object_object_add($$, "columns", columns);
//json_object_object_add($$, "values", values);
//
//if ($5) {
//json_object_object_add($$, "where", $5);
//}
//}
//;
//
//update_values_list_req
//: update_value                              { $$ = json_object_new_array(); json_object_array_add($$, $1); }
//| update_values_list_req ',' update_value   { $$ = $1; json_object_array_add($$, $3); }
//;
//
//update_value
//: name T_EQ_OP value    { $$ = json_object_new_array(); json_object_array_add($$, $1); json_object_array_add($$, $3); }
//;

%%

void yyerror(xmlDoc ** result, char ** error, const char * str) {
    free(*error);

    *error = strdup(str);
}
