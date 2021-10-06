#pragma once

#include <string.h>
#include <regex.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>

#include "storage.h"

//<?xml version="1.0" encoding="UTF-8"?>
//<request>
//	<action>{0/1/2/3/4/5}</action>
//	...
//</request>
//
//CREATE TABLE
//<request>
//	<action>0</action>
//	<table>name</table>
//	<columns>
//		<column>
//			<name>name</name>
//			<type>{0/1/2/3}</type>
//		</column>
//		...
//	</columns>
//</request>
//
//DROP TABLE
//<request>
//	<action>1</action>
//	<table>name</table>
//</request>
//
//INSERT
//<request>
//	<action>2</action>
//	<table>name</table>
//	<columns>
//		<column>column</column>
//		...
//	</columns>
//	<values>
//		<value type={0/1/2/3} null="true">value</value>
//		...
//	</values>
//</request>
//
//DELETE
//<request>
//	<action>3</action>
//	<table>name</table>
//	<where>where</where>
//</request>
//
//<response>
//	<amount>int</amount>
//</response>
//
//SELECT
//<request>
//	<action>4</action>
//	<table>name</table>
//	<columns>
//		<column>name</column>
//	</columns>
//	<where>where</where>
//	<offset>int</offset>
//	<limit>10</limit>
//	<joins>
//		<join>
//			<table>name</table>
//			<t_column>column</t_column>
//			<s_column>column</s_column>
//		</join>
//	</joins>
//</request>
//
//<response>
//	<columns>
//		<column>column</column>
//		...
//	</columns>
//	<values>
//      <row>
//		    <value>value</value>
//		    ...
//      </row>
//	</values>
//</response>
//
//UPDATE
//<request>
//	<action>5</action>
//	<table>name</table>
//		<columns>
//		<column>column</column>
//		...
//	</columns>
//	<values>
//		<value type={0/1/2/3} null="true">value</value>
//		...
//	</values>
//	<where>where</where>
//</request>
//
//<response>
//	<amount>int</amount>
//</response>
//
//WHERE
//<where>
//	<op>{0/1/2/3/4/5}</op>
//	<column>name</column>
//	<value type={0,1,2,3} null="true">value</value>
//</where>
//
//<where>
//	<op>{6/7}</op>
//	<left>where</left>
//	<right>where</right>
//</where>

enum xml_api_action {
    XML_API_TYPE_CREATE_TABLE = 0,
    XML_API_TYPE_DROP_TABLE = 1,
    XML_API_TYPE_INSERT = 2,
    XML_API_TYPE_DELETE = 3,
    XML_API_TYPE_SELECT = 4,
    XML_API_TYPE_UPDATE = 5,
};

struct xml_api_create_table_request {
    char *table_name;
    struct {
        unsigned int amount;
        struct {
            char *name;
            enum storage_column_type type;
        } *columns;
    } columns;
};

struct xml_api_drop_table_request {
    char *table_name;
};

struct xml_api_insert_request {
    char *table_name;
    struct {
        unsigned int amount;
        char **columns;
    } columns;
    struct {
        unsigned int amount;
        struct storage_value **values;
    } values;
};

enum xml_api_operator {
    XML_API_OPERATOR_EQ = 0,
    XML_API_OPERATOR_NE = 1,
    XML_API_OPERATOR_LT = 2,
    XML_API_OPERATOR_GT = 3,
    XML_API_OPERATOR_LE = 4,
    XML_API_OPERATOR_GE = 5,
    XML_API_OPERATOR_AND = 6,
    XML_API_OPERATOR_OR = 7,
};

struct xml_api_where {
    enum xml_api_operator op;

    union {
        struct {
            char *column;
            struct storage_value *value;
        };

        struct {
            struct xml_api_where *left;
            struct xml_api_where *right;
        };
    };
};

struct xml_api_delete_request {
    char *table_name;
    struct xml_api_where *where;
};

struct xml_api_select_request {
    char *table_name;
    struct {
        unsigned int amount;
        char **columns;
    } columns;
    struct xml_api_where *where;
    unsigned int offset;
    unsigned int limit;
    struct {
        unsigned int amount;
        struct {
            char *table;
            char *t_column;
            char *s_column;
        } *joins;
    } joins;
};

struct xml_api_update_request {
    char *table_name;
    struct {
        unsigned int amount;
        char **columns;
    } columns;
    struct {
        unsigned int amount;
        struct storage_value **values;
    } values;
    struct xml_api_where *where;
};

enum xml_api_action xml_api_get_action(xmlDoc *doc);

struct xml_api_create_table_request xml_api_to_create_table_request(xmlDoc *doc);

struct xml_api_drop_table_request xml_api_to_drop_table_request(xmlDoc *doc);

struct xml_api_insert_request xml_api_to_insert_request(xmlDoc *doc);

struct xml_api_delete_request xml_api_to_delete_request(xmlDoc *doc);

struct xml_api_select_request xml_api_to_select_request(xmlDoc *doc);

struct xml_api_update_request xml_api_to_update_request(xmlDoc *doc);

xmlDoc * xml_api_make_success(xmlNode *answer);

xmlDoc * xml_api_make_success_no_body();

xmlDoc * xml_api_make_error(const char *msg);

xmlNode * xml_api_from_value(struct storage_value *value);

xmlNodePtr find_node(xmlNodePtr root_node, xmlChar *target_node);
