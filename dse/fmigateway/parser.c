// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/model.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x)      ((void)x)
#define ARRAY_SIZE(x)  (sizeof(x) / sizeof(x[0]))
#define MODEL_MAX_TIME 60 * 60 * 10  // 10 minutes
#define STEP_SIZE      0.0005


static inline void* _gwfmu_model_generator(ModelInstanceSpec* mi, void* data)
{
    UNUSED(mi);
    YamlNode* n = dse_yaml_find_node((YamlNode*)data, "model");
    if (n && n->scalar) {
        WindowsModel* w_model = calloc(1, sizeof(WindowsModel));

        w_model->name = n->scalar;
        if (dse_yaml_get_double(
                data, "annotations/stepsize", &w_model->step_size)) {
            w_model->step_size = STEP_SIZE;
        };
        if (dse_yaml_get_double(
                data, "annotations/endtime", &w_model->end_time)) {
            w_model->end_time = MODEL_MAX_TIME;
        };
        if (dse_yaml_get_int(
                data, "annotations/loglevel", &w_model->log_level)) {
            w_model->log_level = -1;
        };
        if (dse_yaml_get_double(
                data, "annotations/timeout", &w_model->timeout)) {
            w_model->timeout = 60.0;
        };
        dse_yaml_get_string(data, "annotations/path", &w_model->path);
        dse_yaml_get_string(data, "annotations/file", &w_model->file);
        dse_yaml_get_string(data, "annotations/yaml", &w_model->yaml);
        dse_yaml_get_bool(data, "annotations/show", &w_model->show_process);

        log_notice("%s", w_model->name);
        log_notice("  Path: %s", w_model->path);
        log_notice("  File: %s", w_model->file);
        log_notice("  Yaml: %s", w_model->yaml);
        log_notice("  Stepsize: %lf", w_model->step_size);
        log_notice("  Endtime: %lf", w_model->end_time);
        log_notice("  Timout: %lf", w_model->timeout);
        log_notice("  Loglevel: %d", w_model->log_level);
        log_notice("  Show: %d", w_model->show_process);
        return w_model;
    }
    return NULL;
}


static inline void _parse_session_script(FmuInstanceData* fmu, YamlNode* node,
    const char* node_path, const char** path, const char** file)
{
    YamlNode* _node = dse_yaml_find_node(node, node_path);
    if (_node) {
        if (dse_yaml_get_string(_node, "path", path)) {
            *path = fmu->instance.resource_location;
        }
        dse_yaml_get_string(_node, "file", file);
    }
}


static inline void _gwfmu_parse_session(ModelInstanceSpec* mi, SchemaObject* o,
    FmuInstanceData* fmu, YamlNode* s_node)
{
    FmiGateway* fmi_gw = fmu->data;
    assert(fmi_gw);

    fmi_gw->settings.session = calloc(1, sizeof(FmiGatewaySession));
    FmiGatewaySession* session = fmi_gw->settings.session;

    _parse_session_script(fmu, s_node, "initialization", &session->init.path,
        &session->init.file);
    _parse_session_script(fmu, s_node, "shutdown", &session->shutdown.path,
        &session->shutdown.file);

    YamlNode* w_node = dse_yaml_find_node(s_node, "windows");
    if (w_node) {
        /* Enumerate over the models. */
        uint32_t      index = 0;
        WindowsModel* w_model;
        HashList      m_list;
        hashlist_init(&m_list, 100);
        do {
            w_model = schema_object_enumerator(mi, o,
                "metadata/annotations/session/windows", &index,
                _gwfmu_model_generator);
            if (w_model == NULL) break;
            if (w_model->name) {
                hashlist_append(&m_list, w_model);
            } else {
                free(w_model);
            }
        } while (1);
        /* Convert to a NTL. */
        session->w_models = hashlist_ntl(&m_list, sizeof(WindowsModel), true);
    }
}


static inline int _model_match_handler(ModelInstanceSpec* mi, SchemaObject* o)
{
    FmuInstanceData* fmu = o->data;
    FmiGateway*      fmi_gw = fmu->data;
    assert(fmi_gw);

    /* Get and set runtime parameters. */
    if (dse_yaml_get_double(o->doc, "metadata/annotations/fmi_stepsize",
            &(fmi_gw->settings.step_size))) {
        fmi_gw->settings.step_size = STEP_SIZE;
    }
    if (dse_yaml_get_double(o->doc, "metadata/annotations/fmi_endtime",
            &(fmi_gw->settings.end_time))) {
        fmi_gw->settings.end_time = MODEL_MAX_TIME;
    }
    if (dse_yaml_get_int(o->doc, "metadata/annotations/loglevel",
            &(fmi_gw->settings.log_level))) {
        fmi_gw->settings.log_level = 6;
    }
    if (dse_yaml_get_string(o->doc, "metadata/annotations/loglocation",
            &(fmi_gw->settings.log_location))) {
        fmi_gw->settings.log_location = ".";
    }


    /* Create Session if specified in the doc. */
    YamlNode* session_node =
        dse_yaml_find_node(o->doc, "metadata/annotations/session");
    if (session_node) {
        _gwfmu_parse_session(mi, o, fmu, session_node);
    }

    return 0;
}


/**
fmigateway_parse
================

This method loads the required yaml files from the resource location of the fmu.
The loaded yaml files are parsed into the fmu descriptor object.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmigateway_parse(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;
    assert(fmi_gw);

    /* Load yaml files. */
    for (size_t i = 0; fmi_gw->settings.yaml_files[i]; i++) {
        fmi_gw->settings.doc_list = dse_yaml_load_file(
            fmi_gw->settings.yaml_files[i], fmi_gw->settings.doc_list);
    }

    /* Parse the FMU Model. */
    SchemaObjectSelector m_sel = {
        .kind = "Model",
        .name = "Gateway",
        .data = fmu,
    };
    ModelInstanceSpec mi = { .yaml_doc_list = fmi_gw->settings.doc_list };
    schema_object_search(&mi, &m_sel, _model_match_handler);
}
