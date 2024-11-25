// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <libxml/xpath.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/importer/importer.h>


#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


/**
parse_tool_anno
===============

Parse a specific tool annotation by `Tool name` and `name`.

Parameters
----------
node (xmlNode*)
: Node (ScalarVariable) from where to search for the tool annotation.

tool (const char*)
: The name of the tool annotations container.

name (const char*)
: The name of the annotation to return.

Returns
-------
xmlChar*
: Annotation text (consolidated `XmlElement` text).

NULL
: The annotation was not found.
*/
__attribute__((unused))
static xmlChar* parse_tool_anno(
    xmlNode* node, const char* tool, const char* name)
{
    xmlChar* result = NULL;

    /* Build the XPath query. */
    size_t query_len =
        snprintf(NULL, 0, "Annotations/Tool[@name='%s']/Annotation", tool) + 1;
    char* query = calloc(query_len, sizeof(char));
    snprintf(query, query_len, "Annotations/Tool[@name='%s']/Annotation", tool);
    xmlXPathContext* ctx = xmlXPathNewContext(node->doc);
    ctx->node = node;
    xmlXPathObject* obj = xmlXPathEvalExpression((xmlChar*)query, ctx);

    /* Locate the annotation value (if present). */
    if (obj->type == XPATH_NODESET && obj->nodesetval) {
        /* Annotation. */
        for (int i = 0; i < obj->nodesetval->nodeNr; i++) {
            xmlNode* a_node = obj->nodesetval->nodeTab[i];
            xmlChar* a_name = xmlGetProp(a_node, (xmlChar*)"name");
            xmlChar* a_value =
                xmlNodeListGetString(a_node->doc, a_node->xmlChildrenNode, 1);
            if (strcmp((char*)a_name, name) == 0) {
                result = a_value;
                xmlFree(a_name);
                break;
            } else {
                xmlFree(a_name);
                xmlFree(a_value);
            }
        }
    }

    /* Cleanup and return the result. */
    free(query);
    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctx);
    return result;
}


static inline void _alloc_var(HashMap map, void** vr_ptr, void** val_ptr,
    size_t ptr_val_size, size_t* count, bool is_string)
{
    *count = map.used_nodes;
    *vr_ptr = calloc(*count, sizeof(unsigned int));
    *val_ptr = calloc(*count, ptr_val_size);
    char** keys = hashmap_keys(&map);
    for (uint64_t i = 0; i < map.used_nodes; i++) {
        unsigned int key = (unsigned int)atoi(keys[i]);
        void*        value = hashmap_get(&map, keys[i]);
        memcpy((void*)(*vr_ptr + i * sizeof(unsigned int)), &key,
            sizeof(unsigned int));
        if (value) {
            if (is_string) {
                /* TODO Support for binary/string variables. */
                // char** string_arr = *val_ptr;
                // int   len = strlen(value);
                // char* vr_ptr = (char*)calloc(len + 1, sizeof(char));
                // memcpy(vr_ptr, value, len);
                // string_arr[i] = vr_ptr;
            } else {
                memcpy(
                    (void*)(*val_ptr + i * ptr_val_size), value, ptr_val_size);
            }
        }
        free(keys[i]);
    }

    free(keys);
    hashmap_destroy(&map);
}


modelDescription* parse_model_desc(char* docname)
{
    xmlDocPtr doc;

    xmlInitParser();
    doc = xmlParseFile(docname);
    if (doc == NULL) {
        fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
    }

    xmlXPathContext* ctx = xmlXPathNewContext(doc);
    xmlXPathObject*  xml_sv_obj = xmlXPathEvalExpression(
         (xmlChar*)"/fmiModelDescription/ModelVariables/ScalarVariable", ctx);

    HashMap vr_rx_real;
    HashMap vr_tx_real;
    HashMap vr_rx_binary;
    HashMap vr_tx_binary;
    hashmap_init(&vr_rx_real);
    hashmap_init(&vr_tx_real);
    hashmap_init(&vr_rx_binary);
    hashmap_init(&vr_tx_binary);

    modelDescription* desc = calloc(1, sizeof(modelDescription));

    for (int i = 0; i < xml_sv_obj->nodesetval->nodeNr; i++) {
        xmlNodePtr scalarVariable = xml_sv_obj->nodesetval->nodeTab[i];
        xmlChar*   vr = xmlGetProp(scalarVariable, (xmlChar*)"valueReference");
        xmlChar* causality = xmlGetProp(scalarVariable, (xmlChar*)"causality");
        if (vr == NULL || causality == NULL) goto next;

        for (xmlNodePtr child = scalarVariable->children; child;
             child = child->next) {
            if (child->type != XML_ELEMENT_NODE) continue;

            xmlChar* start = xmlGetProp(child, (xmlChar*)"start");
            if (xmlStrcmp(child->name, (xmlChar*)"Real") == 0 ||
                xmlStrcmp(child->name, (xmlChar*)"Float64") == 0) {
                double _start = atof((char*)start);
                if (xmlStrcmp(causality, (xmlChar*)"input") == 0) {
                    hashmap_set_double(&vr_rx_real, (char*)vr, _start);
                } else if (xmlStrcmp(causality, (xmlChar*)"output") == 0) {
                    hashmap_set_double(&vr_tx_real, (char*)vr, _start);
                }
            } else if (xmlStrcmp(child->name, (xmlChar*)"String") == 0 ||
                       xmlStrcmp(child->name, (xmlChar*)"Binary") == 0) {
                if (strcmp((char*)causality, "input") == 0) {
                    hashmap_set_string(&vr_rx_binary, (char*)vr, (char*)start);
                } else if (strcmp((char*)causality, "output") == 0) {
                    hashmap_set_string(&vr_tx_binary, (char*)vr, (char*)start);
                }

                /* TODO Support for binary/string variables. */
                // xmlChar* a_bus_id = parse_tool_anno(
                //     child->name, "dse.standards.fmi-ls-bus-topology",
                //     "bus_id");
                // xmlChar* encoding = parse_tool_anno(
                //     child->name, "dse.standards.fmi-ls-binary-to-text",
                //     "encoding");
                // xmlChar* encoding = parse_tool_anno(
                //     child->name, "dse.standards.fmi-ls-binary-codec",
                //     "mimetype");
            }
            xmlFree(start);
        }
    /* Cleanup. */
    next:
        xmlFree(vr);
        xmlFree(causality);
    }
    /* Setup the Scalar vr/ val array for the getter/ setter. */
    _alloc_var(vr_rx_real, (void**)&desc->real.vr_rx_real,
        (void**)&desc->real.val_rx_real, sizeof(double), &desc->real.rx_count,
        false);
    _alloc_var(vr_tx_real, (void**)&desc->real.vr_tx_real,
        (void**)&desc->real.val_tx_real, sizeof(double), &desc->real.tx_count,
        false);

    /* Setup the string vr/ val array for the getter/ setter. */
    _alloc_var(vr_rx_binary, (void**)&desc->binary.vr_rx_binary,
        (void**)&desc->binary.val_rx_binary, sizeof(char*),
        &desc->binary.rx_count, true);
    _alloc_var(vr_tx_binary, (void**)&desc->binary.vr_tx_binary,
        (void**)&desc->binary.val_tx_binary, sizeof(char*),
        &desc->binary.tx_count, true);

    /* Cleanup. */
    xmlXPathFreeObject(xml_sv_obj);
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    return desc;
}
