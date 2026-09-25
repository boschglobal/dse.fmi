// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <libxml/xpath.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/fmu/fmu.h>
#include <dse/fmu/xml.h>
#include <dse/fmigateway/fmigateway.h>


static const char* _runtime_name(FmiGatewayRuntimeType type)
{
    switch (type) {
    case FMIGATEWAY_RUNTIME_SIMER:
        return "simer";
    default:
        return "legacy";
    }
}


typedef struct FmiParseConfig {
    const char* runtime_xpath;
    const char* cmd_xpath_fmt;
    const char* logloc_xpath;
    const char* loglevel_xpath;
    const char* scalar_var_xpath;
    const char* string_param_xpath;
    const char* str_envar_xpath;
    const char* float_envar_xpath;
    int         fmi_version;
} FmiParseConfig;


typedef struct FmiParseGenContext {
    FmuInstanceData*      fmu;
    const FmiParseConfig* cfg;
    bool                  is_string;
} FmiParseGenContext;


/* FMI2: annotations live under VendorAnnotations/Tool[@name='dse.fmi.config'].
 * Structure: Tool / <dse:annotations> (namespaced wrapper, satisfies FMI 2.0
 * xs:any maxOccurs=1) / <dse:Annotation name="key">value</dse:Annotation>.
 * Both elements are in the DSE namespace so local-name() is used throughout. */
#define FMI2_ANN(key)                                                          \
    "//VendorAnnotations/Tool[@name='dse.fmi.config']"                         \
    "/*[local-name()='annotations']"                                           \
    "/*[local-name()='Annotation'][@name='dse.fmi.gateway." key "']"
#define FMI2_ANN_FMT(key)                                                      \
    "//VendorAnnotations/Tool[@name='dse.fmi.config']"                         \
    "/*[local-name()='annotations']"                                           \
    "/*[local-name()='Annotation'][starts-with(@name,'dse.fmi.gateway." key    \
    "')]"
#define FMI2_MV(element, key)                                                  \
    "//ModelVariables/ScalarVariable"                                          \
    "[@causality='parameter']"                                                 \
    "[" element "]"                                                            \
    "[Annotations/Tool[@name='dse.fmi.gateway." key "']]"
#define FMI2_SCALAR_VAR                                                        \
    "//ModelVariables/ScalarVariable"                                          \
    "[@causality='input' or @causality='parameter'][Real]"
#define FMI2_STRING_PARAM                                                      \
    "//ModelVariables/ScalarVariable[@causality='parameter'][String]"


/* FMI3: annotations live under fmiModelDescription/Annotations, by @type. */
#define FMI3_ANN(key)                                                          \
    "//fmiModelDescription/Annotations"                                        \
    "/Annotation[@type='dse.fmi.gateway." key "']"
#define FMI3_ANN_FMT(key)                                                      \
    "//fmiModelDescription/Annotations"                                        \
    "/Annotation[starts-with(@type,'dse.fmi.gateway." key "')]"
#define FMI3_MV(element, key)                                                  \
    "//ModelVariables/" element "[@causality='parameter']"                     \
    "[Annotations/Annotation[@type='dse.fmi.gateway." key "']]"
#define FMI3_SCALAR_VAR                                                        \
    "//ModelVariables/Float64[@causality='input' or @causality='parameter']"
#define FMI3_STRING_PARAM "//ModelVariables/String[@causality='parameter']"


static const FmiParseConfig _cfg_fmi2 = {
    .fmi_version = 2,
    .runtime_xpath = FMI2_ANN("runtime"),
    .cmd_xpath_fmt = FMI2_ANN_FMT("%s.cmd."),
    .logloc_xpath = FMI2_ANN("loglocation"),
    .loglevel_xpath = FMI2_ANN("loglevel"),
    .scalar_var_xpath = FMI2_SCALAR_VAR,
    .string_param_xpath = FMI2_STRING_PARAM,
    .str_envar_xpath = FMI2_MV("String", "script.parameter"),
    .float_envar_xpath = FMI2_MV("Real", "script.parameter"),
};


static const FmiParseConfig _cfg_fmi3 = {
    .fmi_version = 3,
    .runtime_xpath = FMI3_ANN("runtime"),
    .cmd_xpath_fmt = FMI3_ANN_FMT("%s.cmd."),
    .logloc_xpath = FMI3_ANN("loglocation"),
    .loglevel_xpath = FMI3_ANN("loglevel"),
    .scalar_var_xpath = FMI3_SCALAR_VAR,
    .string_param_xpath = FMI3_STRING_PARAM,
    .str_envar_xpath = FMI3_MV("String", "script.parameter"),
    .float_envar_xpath = FMI3_MV("Float64", "script.parameter"),
};


static void _parse_simulation_settings(
    FmuInstanceData* fmu, xmlXPathContextPtr ctx, const FmiParseConfig* cfg)
{
    FmiGateway* fmi_gw = fmu->data;

    static const XmlFieldMapSpec _runtime_map[] = {
        { "simer", FMIGATEWAY_RUNTIME_SIMER },
        { NULL },
    };

    const XmlFieldSpec _spec[] = {
        { XmlFieldTypeS, "/*", offsetof(FmiGateway, settings.model_name),
            "modelName", NULL },
        { XmlFieldTypeD, "//DefaultExperiment",
            offsetof(FmiGateway, settings.end_time), "stopTime", NULL },
        { XmlFieldTypeD, "//DefaultExperiment",
            offsetof(FmiGateway, settings.step_size), "stepSize", NULL },
        { XmlFieldTypeI, cfg->runtime_xpath,
            offsetof(FmiGateway, settings.runtime.type), NULL, _runtime_map },
        { XmlFieldTypeS, cfg->logloc_xpath,
            offsetof(FmiGateway, settings.runtime.log_location), NULL, NULL },
        { XmlFieldTypeI, cfg->loglevel_xpath,
            offsetof(FmiGateway, settings.log_level), NULL, NULL },
    };

    fmi_gw->settings.log_level = LOG_ERROR;                    /* default */
    fmi_gw->settings.runtime.type = FMIGATEWAY_RUNTIME_LEGACY; /* default */

    xml_load_object(ctx, fmi_gw, _spec, sizeof(_spec) / sizeof(_spec[0]));

    if (fmi_gw->settings.runtime.log_location == NULL ||
        strlen(fmi_gw->settings.runtime.log_location) == 0) {
        free((void*)fmi_gw->settings.runtime.log_location);
        fmi_gw->settings.runtime.log_location =
            strdup(fmu->instance.resource_location);
    }
}


static void* _cmd_generator(xmlNodePtr node, void* userdata)
{
    FmiGateway* fmi_gw = userdata;
    xmlChar*    text = xmlNodeGetContent(node);
    char*       cmd = text ? strdup((char*)text) : strdup("");
    if (text) xmlFree(text);
    vector_push(&fmi_gw->settings.runtime.cmds, &cmd);
    log_debug("simer cmd: %s", cmd);
    return userdata;
}


static void _index_scalar_variable(
    FmuInstanceData* fmu, uint32_t vr, double value)
{
    FmiGateway* fmi_gw = fmu->data;
    double*     storage = malloc(sizeof(double));
    *storage = value;
    vector_push(&fmi_gw->settings.scalar_storage, &storage);
    fmu_variable_add(&fmu->variables.scalar.input, vr, storage);
}


static void _index_string_variable(
    FmuInstanceData* fmu, uint32_t vr, const char* value)
{
    // NOLINTNEXTLINE(build/include_what_you_use)
    fmu_variable_add(&fmu->variables.string.input, vr, strdup(value));
}


static void _parse_runtime(
    FmuInstanceData* fmu, xmlXPathContextPtr ctx, const FmiParseConfig* cfg)
{
    FmiGateway* fmi_gw = fmu->data;

    if (fmi_gw->settings.runtime.type == FMIGATEWAY_RUNTIME_LEGACY) return;

    const char* rt_name = _runtime_name(fmi_gw->settings.runtime.type);

    /* Collect cmd.N annotations. */
    char cmd_xpath[256];
    snprintf(cmd_xpath, sizeof(cmd_xpath), cfg->cmd_xpath_fmt, rt_name);
    fmi_gw->settings.runtime.cmds = vector_make(sizeof(char*), 0, NULL);
    uint32_t cmd_idx = 0;
    while (xml_object_enumerator(
               ctx, cmd_xpath, &cmd_idx, _cmd_generator, fmi_gw) != NULL) {
    }
}


static xmlChar* _get_start_value(
    xmlNodePtr var, bool is_string, int fmi_version)
{
    if (is_string) {
        if (fmi_version == 3) {
            /* FMI 3: start is a child <Start value="..."> element. */
            for (xmlNodePtr c = var->children; c; c = c->next) {
                if (c->type == XML_ELEMENT_NODE &&
                    xmlStrcmp(c->name, BAD_CAST "Start") == 0)
                    return xmlGetProp(c, BAD_CAST "value");
            }
        } else {
            /* FMI 2: start is an attribute on the <String> child. */
            for (xmlNodePtr c = var->children; c; c = c->next) {
                if (c->type == XML_ELEMENT_NODE &&
                    xmlStrcmp(c->name, BAD_CAST "String") == 0)
                    return xmlGetProp(c, BAD_CAST "start");
            }
        }
    } else {
        if (fmi_version == 3) {
            /* FMI 3: start attribute on the variable element itself. */
            return xmlGetProp(var, BAD_CAST "start");
        } else {
            /* FMI 2: start is an attribute on the <Real> child. */
            for (xmlNodePtr c = var->children; c; c = c->next) {
                if (c->type == XML_ELEMENT_NODE &&
                    xmlStrcmp(c->name, BAD_CAST "Real") == 0)
                    return xmlGetProp(c, BAD_CAST "start");
            }
        }
    }
    return NULL;
}


static void* _variable_generator(xmlNodePtr node, void* userdata)
{
    FmiParseGenContext* gctx = userdata;
    xmlChar*            vref = xmlGetProp(node, BAD_CAST "valueReference");
    if (vref == NULL) return userdata;

    uint32_t vr = (uint32_t)strtoul((char*)vref, NULL, 10);
    xmlChar* start =
        _get_start_value(node, gctx->is_string, gctx->cfg->fmi_version);
    if (gctx->is_string) {
        _index_string_variable(gctx->fmu, vr, start ? (char*)start : "");
    } else {
        _index_scalar_variable(
            gctx->fmu, vr, start ? strtod((char*)start, NULL) : 0.0);
    }
    if (start) xmlFree(start);
    xmlFree(vref);
    return userdata; /* sentinel: non-NULL continues enumeration */
}


static void _parse_variables(
    FmuInstanceData* fmu, xmlXPathContextPtr ctx, const FmiParseConfig* cfg)
{
    const char* xpaths[] = { cfg->string_param_xpath, cfg->scalar_var_xpath };
    for (int i = 0; i < 2; i++) {
        FmiParseGenContext gctx = {
            .fmu = fmu, .cfg = cfg, .is_string = (i == 0)
        };
        uint32_t index = 0;
        while (xml_object_enumerator(ctx, xpaths[i], &index,
                   _variable_generator, &gctx) != NULL) {
        }
    }
}


static void* _envar_generator(xmlNodePtr node, void* userdata)
{
    FmiParseGenContext* gctx = userdata;

    xmlChar* vref = xmlGetProp(node, BAD_CAST "valueReference");
    xmlChar* sv_name = xmlGetProp(node, BAD_CAST "name");
    if (!vref || !sv_name) {
        if (vref) xmlFree(vref);
        if (sv_name) xmlFree(sv_name);
        return NULL;
    }

    /* Value is the FMU variable, indexed with its start value. */
    FmiGatewayParameter* envar = calloc(1, sizeof(FmiGatewayParameter));
    envar->vref = strdup((char*)vref);
    envar->name = strdup((char*)sv_name);
    envar->type = gctx->is_string ? "String" : "Real";

    xmlFree(vref);
    xmlFree(sv_name);

    return envar;
}


static void _parse_xml_script_envar(
    FmuInstanceData* fmu, xmlXPathContextPtr ctx, const FmiParseConfig* cfg)
{
    FmiGateway* fmi_gw = fmu->data;

    HashList e_list;
    hashlist_init(&e_list, 128);

    const char* envar_xpaths[] = { cfg->str_envar_xpath,
        cfg->float_envar_xpath };
    for (int i = 0; i < 2; i++) {
        FmiParseGenContext gctx = {
            .fmu = fmu, .cfg = cfg, .is_string = (i == 0)
        };
        uint32_t             index = 0;
        FmiGatewayParameter* envar;
        while ((envar = xml_object_enumerator(ctx, envar_xpaths[i], &index,
                    _envar_generator, &gctx)) != NULL)
            hashlist_append(&e_list, envar);
    }

    fmi_gw->settings.scripts.envar =
        hashlist_ntl(&e_list, sizeof(FmiGatewayParameter), true);
}


static void fmigateway_parse_xml_config(
    FmuInstanceData* fmu, const FmiParseConfig* cfg)
{
    char xml_path[PATH_MAX];
    snprintf(xml_path, PATH_MAX, "%s/../modelDescription.xml",
        fmu->instance.resource_location);
    xmlInitParser();
    xmlDocPtr doc = xmlParseFile(xml_path);
    if (doc == NULL) {
        fprintf(stderr, "Document not parsed successfully.\n");
        return;
    }

    FmiGateway* fmi_gw = fmu->data;
    fmi_gw->settings.scalar_storage = vector_make(sizeof(double*), 0, NULL);

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (ctx) {
        _parse_simulation_settings(fmu, ctx, cfg);
        _parse_runtime(fmu, ctx, cfg);
        _parse_variables(fmu, ctx, cfg);
        _parse_xml_script_envar(fmu, ctx, cfg);
        xmlXPathFreeContext(ctx);
    }
    vector_sort(&fmu->variables.scalar.input);
    // NOLINTNEXTLINE(build/include_what_you_use)
    vector_sort(&fmu->variables.string.input);

    xmlFreeDoc(doc);
}


/**
fmigateway_parse_xml
====================

Parses the `modelDescription.xml` of the FMU and populates the gateway
settings. The version of the FMI standard (FMI2 or FMI3) is determined from
the FMU instance, which selects the appropriate XPath strategy. The following
information is extracted:

- Simulation settings: model name, end time, step size, log level.
- Runtime configuration: type (simer/legacy), log location, command list.
- Input and parameter variables: Real/Float64 inputs and parameters, and String
  parameters, are indexed with their start value so that values set before
  `fmu_init()` are retained.
- Script environment variables: string and scalar FMU variables annotated as
  script parameters. Their value is taken from the FMU variable (start value,
  or as set by the importer).

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmigateway_parse_xml(FmuInstanceData* fmu)
{
    const FmiParseConfig* cfg =
        (fmu->instance.version == 3) ? &_cfg_fmi3 : &_cfg_fmi2;
    fmigateway_parse_xml_config(fmu, cfg);
}
