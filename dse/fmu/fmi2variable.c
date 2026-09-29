// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <dlfcn.h>
#include <libxml/xpath.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/ncodec/codec.h>
#include <dse/fmu/fmu.h>


#define FMI2_SCALAR_XPATH "/fmiModelDescription/ModelVariables/ScalarVariable"


static bool __is_scalar_var(xmlNodePtr child)
{
    return (xmlStrcmp(child->name, (xmlChar*)"Real") == 0 ||
            xmlStrcmp(child->name, (xmlChar*)"Integer") == 0 ||
            xmlStrcmp(child->name, (xmlChar*)"Boolean") == 0 ||
            xmlStrcmp(child->name, (xmlChar*)"Float64") == 0);
}


static bool __is_binary_var(xmlNodePtr child)
{
    return (xmlStrcmp(child->name, (xmlChar*)"String") == 0 ||
            xmlStrcmp(child->name, (xmlChar*)"Binary") == 0);
}


static xmlChar* __parse_tool_anno(
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


size_t fmu_variable_count(xmlDoc* doc, bool is_binary)
{
    size_t           count = 0;
    xmlXPathContext* ctx = xmlXPathNewContext(doc);
    xmlXPathObject*  obj =
        xmlXPathEvalExpression((xmlChar*)FMI2_SCALAR_XPATH, ctx);
    if (obj == NULL) goto cleanup;

    for (int i = 0; i < obj->nodesetval->nodeNr; i++) {
        xmlNodePtr scalarVariable = obj->nodesetval->nodeTab[i];
        for (xmlNodePtr child = scalarVariable->children; child;
            child = child->next) {
            if (child->type != XML_ELEMENT_NODE) continue;
            if (is_binary) {
                if (__is_binary_var(child)) {
                    count += 1;
                }
            } else {
                if (__is_scalar_var(child)) {
                    count += 1;
                }
            }
        }
    }

cleanup:
    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctx);
    return count;
}


static void __index_scalar_variable(FmuInstanceData* fmu, FmuSignalVector* sv,
    uint32_t sv_idx, xmlChar* vr, xmlChar* causality)
{
    uint32_t vref = (uint32_t)strtoul((char*)vr, NULL, 10);
    if (xmlStrcmp(causality, (xmlChar*)"output") == 0) {
        fmu_variable_add(
            &(fmu->variables.scalar.output), vref, &(sv->scalar[sv_idx]));
    } else if (xmlStrcmp(causality, (xmlChar*)"input") == 0) {
        fmu_variable_add(
            &(fmu->variables.scalar.input), vref, &(sv->scalar[sv_idx]));
    }
}


static void __index_binary_variable(FmuInstanceData* fmu, FmuSignalVector* sv,
    uint32_t sv_idx, xmlNode* node, xmlChar* vr, xmlChar* causality)
{
    FmuSignalVectorIndex* idx = calloc(1, sizeof(FmuSignalVectorIndex));
    idx->sv = sv;
    idx->vi = sv_idx;
    uint32_t vref = (uint32_t)strtoul((char*)vr, NULL, 10);
    if (xmlStrcmp(causality, (xmlChar*)"output") == 0) {
        fmu_variable_add(&(fmu->variables.binary.tx), vref, idx);
    } else if (xmlStrcmp(causality, (xmlChar*)"input") == 0) {
        fmu_variable_add(&(fmu->variables.binary.rx), vref, idx);
    }

    /*
    fmi-ls-binary-to-text
    ---------------------
    Tool name: dse.standards.fmi-ls-binary-to-text
    Annotation name: encoding
    Annotation value:
        * ascii85
    */
    xmlChar* encoding = __parse_tool_anno(
        node, "dse.standards.fmi-ls-binary-to-text", "encoding");
    if (encoding) {
        if (strcmp((char*)encoding, "ascii85") == 0) {
            fmu_variable_add(
                &fmu->variables.binary.encode_func, vref, dse_ascii85_encode);
            fmu_variable_add(
                &fmu->variables.binary.decode_func, vref, dse_ascii85_decode);
        }
        xmlFree(encoding);
    }

    /*
    fmi-ls-binary-codec
    -------------------
    Tool name: dse.standards.fmi-ls-binary-codec
    Annotation name: mimetype
    Annotation value: <mimetype string>
    */
    sv->mime_type[sv_idx] = (char*)__parse_tool_anno(
        node, "dse.standards.fmi-ls-binary-codec", "mimetype");

    sv->ncodec[sv_idx] = fmu_ncodec_open(fmu, sv->mime_type[sv_idx], idx);
}


void fmu_variable_index(
    xmlDoc* doc, FmuInstanceData* fmu, FmuSignalVector* sv, bool is_binary)
{
    xmlXPathContext* ctx = xmlXPathNewContext(doc);
    xmlXPathObject*  obj =
        xmlXPathEvalExpression((xmlChar*)FMI2_SCALAR_XPATH, ctx);
    if (obj == NULL) goto cleanup;

    /* Scan all variables. */
    uint32_t sv_idx = 0;
    for (int i = 0; i < obj->nodesetval->nodeNr; i++) {
        /* ScalarVariable. */
        xmlNodePtr scalarVariable = obj->nodesetval->nodeTab[i];
        xmlChar*   name = xmlGetProp(scalarVariable, (xmlChar*)"name");
        xmlChar*   vr = xmlGetProp(scalarVariable, (xmlChar*)"valueReference");
        xmlChar* causality = xmlGetProp(scalarVariable, (xmlChar*)"causality");

        /* Search for a supported variable type. */
        for (xmlNodePtr child = scalarVariable->children; child;
            child = child->next) {
            if (child->type != XML_ELEMENT_NODE) continue;
            if (is_binary) {
                if (!__is_binary_var(child)) goto next;
            } else {
                if (!__is_scalar_var(child)) goto next;
            }

            /* Index this variable. */
            assert(sv_idx < sv->count);
            sv->signal[sv_idx] = strdup((char*)name);
            if (is_binary) {
                __index_binary_variable(
                    fmu, sv, sv_idx, scalarVariable, vr, causality);
            } else {
                __index_scalar_variable(fmu, sv, sv_idx, vr, causality);
            }

            /* Only increment the index if a variable was indexed. */
            sv_idx += 1;
        }
    next:
        /* Cleanup. */
        xmlFree(name);
        xmlFree(vr);
        xmlFree(causality);
    }

    if (is_binary) {
        vector_sort(&(fmu->variables.binary.rx));
        vector_sort(&(fmu->variables.binary.tx));
        vector_sort(&(fmu->variables.binary.encode_func));
        vector_sort(&(fmu->variables.binary.decode_func));
    } else {
        vector_sort(&(fmu->variables.scalar.input));
        vector_sort(&(fmu->variables.scalar.output));
    }

cleanup:
    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctx);
}
