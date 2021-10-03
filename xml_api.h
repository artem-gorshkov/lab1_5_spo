#include <libxml/parser.h>
#include <libxml/tree.h>

//<?xml version="1.0" encoding="UTF-8"?>
//<request>
//	<action>{0/1/2/3/4/5}</action>
//	...
//</request>
//CREATE TABLE:
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
//DROP TABLE
//<request>
//	<action>1</action>
//	<table>name</table>
//</request>
//INSERT
//<request>
//	<action>2</action>
//	<table>name</table>
//	<columns>
//		<column>column</column>
//		...
//	</columns>
//	<values>
//		<value>value</value>
//		...
//	</values>
//</request>
//DELETE
//<request>
//	<action>3</action>
//	<table>name</table>
//	<where>where</where>
//</request>
//<response>
//	<amount>int</amount>
//</response>
//SELECT
//<request>
//	<action>4</action>
//	<table>name</table>
//	<where>where</where>
//</request>
//
//<response>
//
//</response>