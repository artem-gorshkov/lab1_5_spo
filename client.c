#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include <inttypes.h>

#include "xml_api.h"
#include "y.tab.h"

void scan_string(const char *str);

static void badAnswer(xmlDoc *response) {
    xmlChar *xmlbuff;
    int buffersize;
    xmlDocDumpFormatMemory(response, &xmlbuff, &buffersize, 1);
    const char *err = (char *) xmlbuff;

    printf("Bad answer: %s\n", err);
}

static bool is_error_response(xmlDoc *response) {
    xmlNode *responseNode = xmlDocGetRootElement(response);
    if (responseNode != NULL && responseNode->children != NULL &&
            strcmp((char *) responseNode->children->name, "error") == 0) {
        printf("Error: %s.\n", responseNode->children->children->content);
        return true;
    }

    return false;
}

static void print_amount_response(xmlDoc *response, const char *action) {
    xmlNode *responseNode = xmlDocGetRootElement(response);
    if (strcmp((char *) responseNode->children->name, "amount") == 0) {
        printf("%s rows was %s.\n", (char *) responseNode->children->children->content, action);
        return;
    }

    badAnswer(response);

}

static void print_table_separator(unsigned int columns_length, const int *columns_width) {
    for (int i = 0; i < columns_length; ++i) {
        putchar('+');

        for (int j = 0; j < columns_width[i] + 2; ++j) {
            putchar('-');
        }
    }

    puts("+");
}

static void print_table_response(xmlDoc *response) {
    xmlNode *responseNode = xmlDocGetRootElement(response);
    xmlNode *columns = NULL;
    xmlNode *values = NULL;

    columns = find_node(responseNode, BAD_CAST "columns");
    values = find_node(responseNode, BAD_CAST "values");

    if (columns == NULL || values == NULL) {
        badAnswer(response);
    }

    unsigned int rows_length = xmlChildElementCount(values);
    unsigned int columns_length = xmlChildElementCount(columns);

    int columns_width[columns_length];
    xmlNodePtr column_node;
    int i = 0;
    for (column_node = columns->children; column_node; column_node = column_node->next) {
        if (column_node->type == XML_ELEMENT_NODE) {
            columns_width[i] = (int) strlen((char *) column_node->children->content);
            i++;
        }
    }

    xmlNodePtr row_node;
    for (row_node = values->children; row_node; row_node = row_node->next) {
        xmlNodePtr value_node;
        int j = 0;
        for (value_node = row_node->children; value_node; value_node = value_node->next) {
            int width = (int) strlen((char *) value_node->children->content);
            columns_width[j] = columns_width[j] > width ? columns_width[j] : width;
            j++;
        }
    }

    print_table_separator(columns_length, columns_width);

    i = 0;
    for (column_node = columns->children; column_node; column_node = column_node->next) {
        printf("| %*s ", columns_width[i], (char *) column_node->children->content);
        i++;
    }

    puts("|");

    for (row_node = values->children; row_node; row_node = row_node->next) {
        print_table_separator(columns_length, columns_width);
        xmlNodePtr value_node;
        int j = 0;
        for (value_node = row_node->children; value_node; value_node = value_node->next) {
            printf("| %*s ", columns_width[j], (char *) value_node->children->content);
            j++;
        }
        puts("|");
    }

    print_table_separator(columns_length, columns_width);
}

static void print_response(enum xml_api_action action, xmlDoc *response) {

    if (is_error_response(response)) {
        return;
    }

    switch (action) {
        case XML_API_TYPE_CREATE_TABLE:
            printf("Table was created.\n");
            break;

        case XML_API_TYPE_DROP_TABLE:
            printf("Table was dropped.\n");
            break;

        case XML_API_TYPE_INSERT:
            printf("Row was inserted.\n");
            break;

        case XML_API_TYPE_DELETE:
            print_amount_response(response, "deleted");
            break;

        case XML_API_TYPE_SELECT:
            print_table_response(response);
            break;

        case XML_API_TYPE_UPDATE:
            print_amount_response(response, "updated");
            break;

        default:
            return;
    }
}

static bool handle_request(int socket, xmlDoc *request) {
    xmlChar *xmlbuff;
    int buffersize;
    xmlDocDumpFormatMemory(request, &xmlbuff, &buffersize, 1);
    const char *request_string = (char *) xmlbuff;

    ssize_t wrote;
    size_t remaining = strlen(request_string);
    while (remaining > 0) {
        wrote = write(socket, request_string, remaining);

        if (wrote < 0) {
            return false;
        }

        request_string += wrote;
        remaining -= wrote;
    }

    char buffer[64 * 1024];
    ssize_t was_read = read(socket, buffer, sizeof(buffer) / sizeof(*buffer));
    if (was_read <= 0) {
        return false;
    }

    if (was_read == sizeof(buffer) / sizeof(*buffer)) {
        buffer[sizeof(buffer) / sizeof(*buffer) - 1] = '\0';
    } else {
        buffer[was_read] = '\0';
    }

    xmlParserCtxtPtr ctxt; /* the parser context */
    xmlDocPtr reponse_doc; /* the resulting document tree */

    /* create a parser context */
    ctxt = xmlNewParserCtxt();
    if (ctxt == NULL) {
        fprintf(stderr, "Failed to allocate parser context\n");
    }
    /* parse, activating the DTD validation option */
    reponse_doc = xmlCtxtReadMemory(ctxt, buffer, (int) strlen(buffer), "response.xml", NULL, XML_PARSE_DTDATTR);
    /* check if parsing succeeded */
    if (reponse_doc == NULL) {
        fprintf(stderr, "Failed to parse\n");
    } else {
        /* check if validation succeeded */
        if (ctxt->valid == 0)
            fprintf(stderr, "Failed to validate\n");
        /* free up the resulting document */
        print_response(xml_api_get_action(request), reponse_doc);
        xmlFreeDoc(reponse_doc);
    }
    /* free up the parser context */
    xmlFreeParserCtxt(ctxt);

    return true;
}

static bool handle_command(int socket, const char *command) {
    xmlDoc *request = NULL;
    char *error = NULL;

    scan_string(command);
    if (yyparse(&request, &error) != 0) {
        printf("Parsing error: %s.\n", error);
        return true;
    }

    if (!request) {
        return true;
    }

    return handle_request(socket, request);
}

int main() {
    // create a socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    // specify an address for the socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9002); // можно параметром
    server_address.sin_addr.s_addr = INADDR_ANY;

    // check for error with the connection
    if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) != 0) {
        perror("There was an error making a connection to the remote socket");
        return -1;
    }

    bool working = true;
    while (working) {
        size_t command_capacity = 0;
        char *command = NULL;

        printf("> ");
        fflush(stdout);

        ssize_t was_read = getline(&command, &command_capacity, stdin);
        if (was_read <= 0) {
            free(command);
            break;
        }

        command[was_read] = '\0';
        working = handle_command(client_socket, command);
    }

    // and then close the socket
    close(client_socket);

    printf("Bye!\n");
    return 0;
}
