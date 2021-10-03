#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlstring.h>
#include <stdio.h>
#include "xml_api.h"

/*static char *action = "action";

static void
print_element_names(xmlNode *a_node) {
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            printf("node type: Element, name: %s\n", cur_node->name);
            if (xmlStrcmp(xmlCharStrdup(action), cur_node->name) == 0) {
                xmlChar *string = cur_node->children->content;
            }

            print_element_names(cur_node->children);
        }
    }

int xml_api_get_action(char *xml) {
        xmlDoc *doc = NULL;
        doc = xmlReadMemory(xml, (int) strlen(xml), "action.tmp", 0, 0);
        xmlNode *pNode = xmlDocGetRootElement(doc);
        xmlNode *cur_node = NULL;

        for (cur_node = pNode; cur_node; cur_node = cur_node->next) {
            if (cur_node->type == XML_ELEMENT_NODE) {
                printf("node type: Element, name: %s\n", cur_node->name);
            }

            print_element_names(cur_node->children);
        }

        return -1;
    }*/

int main() {
    LIBXML_TEST_VERSION
    char *xml = "<request><action>1</action></request>";
    printf("%d", xml_api_get_action(xmlReadMemory(xml, strlen(xml), "actiob.tmp", 0, 0)));
    return 0;
}
