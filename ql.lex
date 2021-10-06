%option noyywrap case-insensitive

%{
#include <inttypes.h>

#ifdef __APPLE__
#include "../xml_api.h"
#else
#include "xml_api.h"
#endif
#include "y.tab.h"

static xmlNode * create_node() {
    char* text = malloc(sizeof(char) * (yyleng + 1));

    memcpy(text, yytext, yyleng);
    text[yyleng] = '\0';

    return xmlNewText(BAD_CAST text);
}

static xmlNode * create_value_node(char * type) {
    xmlNode * textNode = create_node();
    xmlNode * valueNode = xmlNewNode(NULL, BAD_CAST "value");
    xmlAddChild(valueNode, textNode);
    xmlNewProp(valueNode, BAD_CAST "type", BAD_CAST type);
    return valueNode;
}

static xmlNode * create_null_node() {
    xmlNode * valueNode = xmlNewNode(NULL, BAD_CAST "value");
    xmlNewProp(valueNode, BAD_CAST "null", BAD_CAST "");
    return valueNode;
}

static int64_t int_literal() {
    char * str = malloc(sizeof(*str) * (yyleng + 1));

    memcpy(str, yytext, yyleng);
    str[yyleng] = '\0';

    int64_t val;
    sscanf(str, "%"SCNi64, &val);
    free(str);

    return val;
}

static uint64_t uint_literal() {
    char * str = malloc(sizeof(*str) * (yyleng + 1));

    memcpy(str, yytext, yyleng);
    str[yyleng] = '\0';

    uint64_t val;
    sscanf(str, "%"SCNu64, &val);
    free(str);

    return val;
}

static double num_literal() {
    char * str = malloc(sizeof(*str) * (yyleng + 1));

    memcpy(str, yytext, yyleng);
    str[yyleng] = '\0';

    double val;
    sscanf(str, "%lf", &val);
    free(str);

    return val;
}
%}

S [ \n\b\t\f\r]
W [a-zA-Z_]
D [0-9]

I {W}({W}|{D})*

%%

{S}     ;

create      return T_CREATE;
table       return T_TABLE;
int         return T_INT;
num         return T_NUM;
str         return T_STR;
drop        return T_DROP;
insert      return T_INSERT;
values      return T_VALUES;
into        return T_INTO;
delete      return T_DELETE;
from        return T_FROM;
where       return T_WHERE;
select      return T_SELECT;
from        return T_FROM;
offset      return T_OFFSET;
limit       return T_LIMIT;
update      return T_UPDATE;
set         return T_SET;
join        return T_JOIN;
on          return T_ON;
\*          return T_ASTERISK;
"="         return T_EQ_OP;
"<>"        return T_NE_OP;
"!="        return T_NE_OP;
"<"         return T_LT_OP;
">"         return T_GT_OP;
"<="        return T_LE_OP;
">="        return T_GE_OP;
and         return T_AND_OP;
"&&"        return T_AND_OP;
or          return T_OR_OP;
"||"        return T_OR_OP;

null    yylval = create_null_node(); return T_NULL;
{I}     yylval = create_node(); return T_IDENTIFIER;

-?{D}+               yylval = create_value_node("0"); return T_INT_LITERAL;
{D}*\.{D}+          yylval = create_value_node("2"); return T_NUM_LITERAL;
\'(\\.|[^'\\])*\'   yylval = create_value_node("3"); return T_STR_LITERAL;
\"(\\.|[^"\\])*\"   yylval = create_node(); return T_DBL_QUOTED;

.       return yytext[0];

%%

void scan_string(const char * str) {
    yy_switch_to_buffer(yy_scan_string(str));
}
