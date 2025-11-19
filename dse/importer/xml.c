// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <libxml/xpath.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/ncodec/codec.h>
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
static xmlChar* _parse_fmi2_tool_anno(
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


static xmlChar* _parse_fmi3_tool_anno(
    xmlNode* node, const char* tool, const char* name)
{
    xmlChar* result = NULL;
    /* Build the XPath query. */
    size_t   query_len =
        snprintf(NULL, 0, "Annotations/Annotation[@type='%s']", tool) + 1;
    char* query = calloc(query_len, sizeof(char));
    snprintf(query, query_len, "Annotations/Annotation[@type='%s']", tool);
    xmlXPathContext* ctx = xmlXPathNewContext(node->doc);
    ctx->node = node;
    xmlXPathObject* obj = xmlXPathEvalExpression((xmlChar*)query, ctx);

    /* Locate the annotation value (if present). */
    if (obj->type == XPATH_NODESET && obj->nodesetval) {
        /* Annotation. */
        for (int i = 0; i < obj->nodesetval->nodeNr; i++) {
            xmlNode* a_node = obj->nodesetval->nodeTab[i];
            for (xmlNode* cur_node = a_node->children; cur_node;
                cur_node = cur_node->next) {
                if (cur_node->type == XML_ELEMENT_NODE &&
                    cur_node->name != NULL) {
                    if (strcmp((const char*)cur_node->name, name) == 0) {
                        result = xmlNodeGetContent(cur_node);
                        break;
                    }
                }
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
    size_t ptr_size, size_t** val_size, size_t* count,
    BinaryData*** binary_info, bool is_binary)
{
    *count = map.used_nodes;
    *vr_ptr = calloc(*count, sizeof(unsigned int));
    *val_ptr = calloc(*count, ptr_size);

    if (val_size) {
        *val_size = calloc(*count, sizeof(size_t));
    }

    if (is_binary) {
        *binary_info = calloc(*count, sizeof(BinaryData*));
    }

    char** keys = hashmap_keys(&map);
    for (uint64_t i = 0; i < map.used_nodes; i++) {
        unsigned int key = (unsigned int)atoi(keys[i]);
        void*        value = hashmap_get(&map, keys[i]);
        memcpy((void*)(*vr_ptr + i * sizeof(unsigned int)), &key,
            sizeof(unsigned int));
        if (value) {
            if (is_binary) {
                (*binary_info)[i] = (BinaryData*)value;
                /* TODO Support for binary/string variables. */
                // char** string_arr = *val_ptr;
                // int   len = strlen(value);
                // char* vr_ptr = (char*)calloc(len + 1, sizeof(char));
                // memcpy(vr_ptr, value, len);
                // string_arr[i] = vr_ptr;
            } else {
                memcpy((void*)(*val_ptr + i * ptr_size), value, ptr_size);
            }
        }
        free(keys[i]);
    }

    free(keys);
    hashmap_destroy(&map);
}


static inline void _parse_fmi2_scalar(xmlNodePtr child, xmlChar* vr,
    xmlChar* causality, xmlChar* start, HashMap* vr_rx_real,
    HashMap* vr_tx_real)
{
    if (xmlStrcmp(child->name, (xmlChar*)"Real")) return;

    double _start = 0.0;
    if (start != NULL) _start = atof((char*)start);

    if (xmlStrcmp(causality, (xmlChar*)"input") == 0) {
        hashmap_set_double(vr_rx_real, (char*)vr, _start);
    } else if (xmlStrcmp(causality, (xmlChar*)"output") == 0) {
        hashmap_set_double(vr_tx_real, (char*)vr, _start);
    }
}


static void _parse_fmi2_string(xmlNode* variable, xmlNode* child, xmlChar* vr,
    xmlChar* causality, xmlChar* start, HashMap* vr_rx_binary,
    HashMap* vr_tx_binary)
{
    if (xmlStrcmp(child->name, (xmlChar*)"String")) return;

    BinaryData* data = calloc(1, sizeof(BinaryData));
    if (start != NULL) data->start = strdup((char*)start);

    xmlChar* mime_type = _parse_fmi2_tool_anno(
        variable, "dse.standards.fmi-ls-binary-codec", "mimetype");
    if (mime_type) {
        data->mime_type = strdup((char*)mime_type);
        data->type = network_mime_type_value((char*)mime_type, "type");
    }
    xmlFree(mime_type);

    if (strcmp((char*)causality, "input") == 0) {
        hashmap_set(vr_rx_binary, (char*)vr, data);
    } else if (strcmp((char*)causality, "output") == 0) {
        hashmap_set(vr_tx_binary, (char*)vr, data);
    }

    /* TODO Support for binary/string variables. */
    // xmlChar* a_bus_id = parse_tool_anno(
    //     child->name, "dse.standards.fmi-ls-bus-topology",
    //     "bus_id");
    // xmlChar* encoding = parse_tool_anno(
    //     child->name, "dse.standards.fmi-ls-binary-to-text",
    //     "encoding");
}


void _parse_fmi2_model_desc(HashMap* vr_rx_real, HashMap* vr_tx_real,
    HashMap* vr_rx_binary, HashMap* vr_tx_binary, xmlXPathContext* ctx)
{
    xmlXPathObject* xml_sv_obj = xmlXPathEvalExpression(
        (xmlChar*)"/fmiModelDescription/ModelVariables/ScalarVariable", ctx);
    for (int i = 0; i < xml_sv_obj->nodesetval->nodeNr; i++) {
        xmlNodePtr scalarVariable = xml_sv_obj->nodesetval->nodeTab[i];
        xmlChar*   vr = xmlGetProp(scalarVariable, (xmlChar*)"valueReference");
        xmlChar* causality = xmlGetProp(scalarVariable, (xmlChar*)"causality");
        xmlChar* start = NULL;

        if (vr == NULL || causality == NULL) goto next;

        for (xmlNodePtr child = scalarVariable->children; child;
            child = child->next) {
            if (child->type != XML_ELEMENT_NODE) continue;

            start = xmlGetProp(child, (xmlChar*)"start");
            _parse_fmi2_scalar(
                child, vr, causality, start, vr_rx_real, vr_tx_real);
            _parse_fmi2_string(scalarVariable, child, vr, causality, start,
                vr_rx_binary, vr_tx_binary);

            if (start) xmlFree(start);
        }
    /* Cleanup. */
    next:
        if (vr) xmlFree(vr);
        if (causality) xmlFree(causality);
    }
    xmlXPathFreeObject(xml_sv_obj);
}


static inline void _parse_fmi3_scalar(xmlNodePtr child, xmlChar* vr,
    xmlChar* causality, HashMap* vr_rx_real, HashMap* vr_tx_real)
{
    if (xmlStrcmp(child->name, (xmlChar*)"Float64")) return;

    xmlChar* start = xmlGetProp(child, (xmlChar*)"start");
    double   _start = 0.0;
    if (start) _start = atof((char*)start);

    if (xmlStrcmp(causality, (xmlChar*)"input") == 0) {
        hashmap_set_double(vr_rx_real, (char*)vr, _start);
    } else if (xmlStrcmp(causality, (xmlChar*)"output") == 0) {
        hashmap_set_double(vr_tx_real, (char*)vr, _start);
    }

    /* Cleanup. */
    if (start) xmlFree(start);
}


static inline void _parse_fmi3_binary(xmlNodePtr child, xmlChar* vr,
    xmlChar* causality, HashMap* vr_rx_binary, HashMap* vr_tx_binary)
{
    if (xmlStrcmp(child->name, (xmlChar*)"Binary")) return;

    xmlChar* start = NULL;
    for (xmlNodePtr _child = child->children; _child; _child = _child->next) {
        if (_child->type != XML_ELEMENT_NODE) continue;
        if (strcmp((char*)_child->name, "Start") == 0) {
            start = xmlGetProp(_child, (xmlChar*)"value");
        }
    }

    BinaryData* data = calloc(1, sizeof(BinaryData));

    data->start = strdup((char*)start);

    xmlChar* mime_type = _parse_fmi3_tool_anno(
        child, "dse.standards.fmi-ls-binary-codec", "Mimetype");
    if (mime_type) {
        data->mime_type = strdup((char*)mime_type);
        data->type = network_mime_type_value((char*)mime_type, "type");
    }
    xmlFree(mime_type);

    if (strcmp((char*)causality, "input") == 0) {
        hashmap_set(vr_rx_binary, (char*)vr, data);
    } else if (strcmp((char*)causality, "output") == 0) {
        hashmap_set(vr_tx_binary, (char*)vr, data);
    }

    /* Cleanup. */
    if (start) xmlFree(start);
}


static inline void _parse_fmi3_model_desc(HashMap* vr_rx_real,
    HashMap* vr_tx_real, HashMap* vr_rx_binary, HashMap* vr_tx_binary,
    xmlXPathContext* ctx)
{
    xmlXPathObject* xml_sv_obj = xmlXPathEvalExpression(
        (xmlChar*)"/fmiModelDescription/ModelVariables", ctx);
    for (int i = 0; i < xml_sv_obj->nodesetval->nodeNr; i++) {
        xmlNodePtr model_variables = xml_sv_obj->nodesetval->nodeTab[i];
        for (xmlNodePtr child = model_variables->children; child;
            child = child->next) {
            if (child->type != XML_ELEMENT_NODE) continue;

            xmlChar* vr = xmlGetProp(child, (xmlChar*)"valueReference");
            xmlChar* causality = xmlGetProp(child, (xmlChar*)"causality");

            if (vr == NULL || causality == NULL) goto next;

            _parse_fmi3_scalar(child, vr, causality, vr_rx_real, vr_tx_real);
            _parse_fmi3_binary(
                child, vr, causality, vr_rx_binary, vr_tx_binary);

        /* Cleanup. */
        next:
            if (vr) xmlFree(vr);
            if (causality) xmlFree(causality);
        }
    }
    xmlXPathFreeObject(xml_sv_obj);
}


static char* _get_fmu_binary_path(
    xmlXPathContext* ctx, const char* platform, int version)
{
    char*       path = NULL;
    const char* dir = "linux64";
    const char* extension = "so";

    /* Find the model_identifier. */
    xmlXPathObject* obj = xmlXPathEvalExpression(
        (xmlChar*)"/fmiModelDescription/CoSimulation", ctx);
    xmlNode* node = obj->nodesetval->nodeTab[0];
    xmlChar* model_identifier = xmlGetProp(node, (xmlChar*)"modelIdentifier");

    /* Determine the OS/Arch path segment. */
    char* _platform = strdup(platform);
    char* os = _platform;
    char* arch = strchr(_platform, '-');
    if (arch) {
        *arch = '\0';
        arch++;
    }
    switch (version) {
    case 2:
        if (strcmp(os, "linux") == 0) {
            if (strcmp(arch, "amd64") == 0) dir = "linux64";
            if (strcmp(arch, "x86") == 0) dir = "linux32";
            if (strcmp(arch, "i386") == 0) dir = "linux32";
        } else if (strcmp(os, "windows") == 0) {
            extension = "dll";
            if (strcmp(arch, "x64") == 0) dir = "win64";
            if (strcmp(arch, "x86") == 0) dir = "win32";
        }
        break;
    case 3:
        if (strcmp(os, "linux") == 0) {
            if (strcmp(arch, "amd64") == 0) dir = "x86_64-linux";
            if (strcmp(arch, "x86") == 0) dir = "x86_32-linux";
            if (strcmp(arch, "i386") == 0) dir = "x86_32-linux";
        } else if (strcmp(os, "windows") == 0) {
            extension = "dll";
            if (strcmp(arch, "x64") == 0) dir = "x86_64-windows";
            if (strcmp(arch, "x86") == 0) dir = "x86_64-windows";
        }
        break;
    default:
        break;
    }

    /* Build the binary path. */
    const char* format = "binaries/%s/%s.%s";
    size_t len = snprintf(path, 0, format, dir, model_identifier, extension);
    if (len) {
        path = malloc(len + 1);
        snprintf(path, len + 1, format, dir, model_identifier, extension);
    }

    /* Cleanup. */
    free(_platform);
    xmlFree(model_identifier);
    xmlXPathFreeObject(obj);

    return path;
}


static inline char* _get_fmu_version(xmlXPathContext* ctx)
{
    char* result = NULL;

    xmlXPathObject* obj =
        xmlXPathEvalExpression((xmlChar*)"/fmiModelDescription", ctx);
    xmlNodePtr node = obj->nodesetval->nodeTab[0];
    xmlChar*   version = xmlGetProp(node, (xmlChar*)"fmiVersion");
    if (version) {
        result = strdup((char*)version);
        xmlFree(version);
    }

    /* Cleanup. */
    xmlXPathFreeObject(obj);

    return result;
}

modelDescription* parse_model_desc(const char* docname, const char* platform)
{
    xmlDocPtr doc;

    xmlInitParser();
    doc = xmlParseFile(docname);
    if (doc == NULL) {
        fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
    }

    xmlXPathContext* ctx = xmlXPathNewContext(doc);

    HashMap vr_rx_real;
    HashMap vr_tx_real;
    HashMap vr_rx_binary;
    HashMap vr_tx_binary;
    hashmap_init(&vr_rx_real);
    hashmap_init(&vr_tx_real);
    hashmap_init(&vr_rx_binary);
    hashmap_init(&vr_tx_binary);

    modelDescription* desc = calloc(1, sizeof(modelDescription));

    /* Parse the Model Desc based on version.*/
    desc->version = _get_fmu_version(ctx);
    switch (atoi(desc->version)) {
    case 2:
        _parse_fmi2_model_desc(
            &vr_rx_real, &vr_tx_real, &vr_rx_binary, &vr_tx_binary, ctx);
        desc->fmu_lib_path = _get_fmu_binary_path(ctx, platform, 2);
        break;
    case 3:
        _parse_fmi3_model_desc(
            &vr_rx_real, &vr_tx_real, &vr_rx_binary, &vr_tx_binary, ctx);
        desc->fmu_lib_path = _get_fmu_binary_path(ctx, platform, 3);
        break;
    default:
        return NULL;
    }

    /* Setup the Scalar vr/ val array for the getter/ setter. */
    _alloc_var(vr_rx_real, (void**)&desc->real.vr_rx_real,
        (void**)&desc->real.val_rx_real, sizeof(double), NULL,
        &desc->real.rx_count, NULL, false);
    _alloc_var(vr_tx_real, (void**)&desc->real.vr_tx_real,
        (void**)&desc->real.val_tx_real, sizeof(double), NULL,
        &desc->real.tx_count, NULL, false);

    /* Setup the string vr/ val array for the getter/ setter. */
    _alloc_var(vr_rx_binary, (void**)&desc->binary.vr_rx_binary,
        (void**)&desc->binary.val_rx_binary, sizeof(char*),
        &desc->binary.val_size_rx_binary, &desc->binary.rx_count,
        &desc->binary.rx_binary_info, true);
    _alloc_var(vr_tx_binary, (void**)&desc->binary.vr_tx_binary,
        (void**)&desc->binary.val_tx_binary, sizeof(char*),
        &desc->binary.val_size_tx_binary, &desc->binary.tx_count,
        &desc->binary.tx_binary_info, true);

    /* Cleanup. */
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    return desc;
}
