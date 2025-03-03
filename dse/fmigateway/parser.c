// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/model.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x)            ((void)x)
#define ARRAY_SIZE(x)        (sizeof(x) / sizeof(x[0]))
#define DEFAULT_END_TIME     60 * 60 * 10  // 10 minutes
#define DEFAULT_STEP_SIZE    0.0005
#define DEFAULT_LOG_LEVEL    6
#define DEFAULT_TIMEOUT      60.0
#define GATEWAY_INIT_CMD     "GATEWAY_INIT_CMD"
#define GATEWAY_SHUTDOWN_CMD "GATEWAY_SHUTDOWN_CMD"
#define REDIS_EXE_PATH       "REDIS_EXE_PATH"
#define MODELC_EXE_PATH      "MODELC_EXE_PATH"
#define SIMBUS_EXE_PATH      "SIMBUS_EXE_PATH"


static inline WindowsModel* _gwfmu_model_generator(
    YamlNode* data, YamlNode* gw_doc, const char* exe)
{
    YamlNode* n = dse_yaml_find_node((YamlNode*)data, "name");
    if (n && n->scalar) {
        WindowsModel* w_model = calloc(1, sizeof(WindowsModel));

        if (dse_yaml_get_string(data, "name", &w_model->name)) {
            log_error("Name is required for model.");
            free(w_model);
            return NULL;
        };
        if (dse_yaml_get_double(
                data, "annotations/cli/step_size", &w_model->step_size)) {
            w_model->step_size = DEFAULT_STEP_SIZE;
        };
        if (dse_yaml_get_double(
                data, "annotations/cli/end_time", &w_model->end_time)) {
            w_model->end_time = DEFAULT_END_TIME;
        };
        if (dse_yaml_get_int(
                data, "annotations/cli/log_level", &w_model->log_level)) {
            w_model->log_level = DEFAULT_LOG_LEVEL;
        };
        if (dse_yaml_get_double(
                data, "annotations/cli/timeout", &w_model->timeout)) {
            w_model->timeout = DEFAULT_TIMEOUT;
        };

        /* Check if an exe is given for this model. */
        if (dse_yaml_get_string(data, "annotations/cli/exe", &w_model->exe)) {
            YamlNode* e_node = dse_yaml_find_node(gw_doc, "spec/runtime/env");
            if (e_node == NULL) {
                free(w_model);
                return NULL;
            }
            w_model->exe = getenv(exe);
            if (w_model->exe == NULL) {
                dse_yaml_get_string(e_node, exe, &w_model->exe);
            }
        }

        YamlNode* f_node = dse_yaml_find_node(data, "runtime/files");
        if (f_node) {
            int offset = 0;
            w_model->yaml = malloc(1);
            w_model->yaml[0] = '\0';

            for (uint32_t i = 0; i < hashlist_length(&f_node->sequence); i++) {
                YamlNode* fi_node = hashlist_at(&f_node->sequence, i);
                char*     file = fi_node->scalar;
                size_t buffer_size = strlen(w_model->yaml) + strlen(file) + 2;
                w_model->yaml = realloc(w_model->yaml, buffer_size);
                if (offset) {
                    offset += snprintf(
                        w_model->yaml + offset, buffer_size, " %s", file);
                } else {
                    offset += snprintf(
                        w_model->yaml + offset, buffer_size, "%s", file);
                }
            }
        }

        log_notice("%s", w_model->name);
        if (w_model->exe) {
            log_notice("  exe: %s", w_model->exe);
        } else {
            log_notice("  exe: using environment variable");
        }
        log_notice("  Yaml: %s", w_model->yaml);
        log_notice("  Stepsize: %lf", w_model->step_size);
        log_notice("  Endtime: %lf", w_model->end_time);
        log_notice("  Timout: %lf", w_model->timeout);
        log_notice("  Loglevel: %d", w_model->log_level);

        return w_model;
    }
    return NULL;
}


static inline void _parse_script_envar(
    FmuInstanceData* fmu, YamlNode* node, FmiGatewaySession* session)
{
    UNUSED(fmu);

    YamlNode* n_env = dse_yaml_find_node(node, "annotations/cmd_envvars");
    if (n_env) {
        /* Node type 2 is a yaml sequence. */
        if (n_env == NULL || n_env->node_type != 2) return;
        uint32_t len = hashlist_length(&n_env->sequence);
        if (len == 0) return;

        HashList e_list;
        hashlist_init(&e_list, 128);
        /* Enumerate over the envars. */
        for (uint32_t i = 0; i < len; i++) {
            YamlNode* _env = hashlist_at(&n_env->sequence, i);
            if (_env == NULL) continue;
            FmiGatewayEnvvar* envar = calloc(1, sizeof(FmiGatewayEnvvar));

            /* Convert value reference to string for use as hashmap key. */
            static char vr_idx[NUMERIC_ENVAR_LEN];
            snprintf(vr_idx, sizeof(vr_idx), "%i", i);
            envar->vref = strdup(vr_idx);

            /* Read cmd envar annotations. */
            if (dse_yaml_get_string(_env, "name", &envar->name)) {
                fmu_log(fmu, 4, "Error", "no envvar name for index %d", i);
                continue;
            };
            if (dse_yaml_get_string(_env, "type", &envar->type)) {
                envar->type = "string";
            };

            /* Read and set default values. */
            if (strcmp(envar->type, "string") == 0) {
                const char* _str;
                if (dse_yaml_get_string(_env, "default", &_str)) {
                    _str = "";
                };
                hashmap_set_string(
                    &fmu->variables.string.input, envar->vref, (char*)_str);
                envar->default_value = strdup((char*)_str);
            } else if (strcmp(envar->type, "real") == 0) {
                double value = 0.0;
                dse_yaml_get_double(_env, "default", &value);
                hashmap_set_double(
                    &fmu->variables.scalar.input, envar->vref, value);
                envar->default_value = calloc(NUMERIC_ENVAR_LEN, sizeof(char));
                snprintf((char*)envar->default_value,
                    NUMERIC_ENVAR_LEN, "%d", (int)value);
            }

            hashlist_append(&e_list, envar);
        }
        session->envar = hashlist_ntl(&e_list, sizeof(FmiGatewayEnvvar), true);
    }
}


static inline WindowsModel* _parse_redis(FmuInstanceData* fmu, YamlNode* root)
{
    UNUSED(fmu);
    YamlNode* n_env = dse_yaml_find_node(root, "spec/runtime/env");
    if (n_env == NULL) return NULL;

    WindowsModel* redis_instance = calloc(1, sizeof(WindowsModel));
    redis_instance->name = "connection";

    redis_instance->exe = getenv(REDIS_EXE_PATH);
    if (redis_instance->exe == NULL) {
        if (dse_yaml_get_string(n_env, REDIS_EXE_PATH, &redis_instance->exe)) {
            free(redis_instance);
            return NULL;
        }
    }

    return redis_instance;
}


static inline WindowsModel* _parse_simbus(FmuInstanceData* fmu, YamlNode* root)
{
    const char* selector[] = { "name" };
    const char* value[] = { "simbus" };
    /* Model Instance: locate in stack. */
    YamlNode*   simbus_node = dse_yaml_find_node_in_seq(
          root, "spec/models", selector, value, ARRAY_SIZE(selector));
    if (simbus_node == NULL) {
        fmu_log(fmu, 0, "Notice", "Simbus not running on windows.");
        return NULL;
    }

    WindowsModel* simbus =
        _gwfmu_model_generator(simbus_node, root, SIMBUS_EXE_PATH);

    return simbus;
}


static inline int _parse_gateway(FmuInstanceData* fmu, YamlNode* doc)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    const char* selector[] = { "name" };
    const char* value[] = { "gateway" };

    /* Model Instance: locate in stack. */
    YamlNode* gateway_node = dse_yaml_find_node_in_seq(
        doc, "spec/models", selector, value, ARRAY_SIZE(selector));
    if (gateway_node == NULL) return -1;

    /* Get and set runtime parameters. */
    if (dse_yaml_get_double(gateway_node, "annotations/step_size",
            &(fmi_gw->settings.step_size))) {
        fmi_gw->settings.step_size = DEFAULT_STEP_SIZE;
    }
    if (dse_yaml_get_double(gateway_node, "annotations/end_time",
            &(fmi_gw->settings.end_time))) {
        fmi_gw->settings.end_time = DEFAULT_END_TIME;
    }
    if (dse_yaml_get_int(gateway_node, "annotations/log_level",
            &(fmi_gw->settings.log_level))) {
        fmi_gw->settings.log_level = DEFAULT_LOG_LEVEL;
    }
    if (dse_yaml_get_string(gateway_node, "annotations/log_location",
            &(fmi_gw->settings.log_location))) {
        fmi_gw->settings.log_location = fmu->instance.resource_location;
    }

    session->init_cmd = getenv(GATEWAY_INIT_CMD);
    if (session->init_cmd == NULL) {
        dse_yaml_get_string(
            gateway_node, "runtime/env/GATEWAY_INIT_CMD", &session->init_cmd);
    }

    session->shutdown_cmd = getenv(GATEWAY_SHUTDOWN_CMD);
    if (session->shutdown_cmd == NULL) {
        dse_yaml_get_string(gateway_node, "runtime/env/GATEWAY_SHUTDOWN_CMD",
            &session->shutdown_cmd);
    }

    _parse_script_envar(fmu, gateway_node, session);

    return 0;
}


static inline void _parse_model_stack(FmuInstanceData* fmu, YamlNode* gw_doc)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    char* model_stack = dse_path_cat(
        fmu->instance.resource_location, fmi_gw->settings.session->model_stack);
    session->model_stack_file = dse_yaml_load_single_doc(model_stack);
    free(model_stack);

    if (session->model_stack_file == NULL) return;

    YamlNode* n_models =
        dse_yaml_find_node(session->model_stack_file, "spec/models");
    if (n_models) {
        /* Node type 2 is sequence. */
        if (n_models == NULL || n_models->node_type != 2) return;
        uint32_t len = hashlist_length(&n_models->sequence);
        if (len == 0) return;
        HashList m_list;
        hashlist_init(&m_list, 100);

        /* Enumerate over the envars. */
        for (uint32_t i = 0; i < len; i++) {
            YamlNode* model = hashlist_at(&n_models->sequence, i);
            if (model == NULL) continue;
            WindowsModel* w_model =
                _gwfmu_model_generator(model, gw_doc, MODELC_EXE_PATH);
            if (w_model == NULL) continue;
            hashlist_append(&m_list, w_model);
        }
        session->w_models = hashlist_ntl(&m_list, sizeof(WindowsModel), true);
    }
}


static inline int _stack_match_handler(ModelInstanceSpec* mi, SchemaObject* o)
{
    UNUSED(mi);
    FmuInstanceData* fmu = o->data;
    FmiGateway*      fmi_gw = fmu->data;
    assert(fmi_gw);

    fmi_gw->settings.session = calloc(1, sizeof(FmiGatewaySession));
    FmiGatewaySession* session = fmi_gw->settings.session;

    /* Visibility options. */
    dse_yaml_get_bool(o->doc, "metadata/annotations/redis_show",
        &session->visibility.transport);
    dse_yaml_get_bool(o->doc, "metadata/annotations/simbus_show",
        &session->visibility.simbus);
    dse_yaml_get_bool(o->doc, "metadata/annotations/models_show",
        &session->visibility.models);

    session->transport = _parse_redis(fmu, o->doc);

    session->simbus = _parse_simbus(fmu, o->doc);

    _parse_gateway(fmu, o->doc);

    dse_yaml_get_string(
        o->doc, "metadata/annotations/model_stack", &session->model_stack);
    if (session->model_stack) {
        _parse_model_stack(fmu, o->doc);
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
        .kind = "Stack",
        .name = "gateway",
        .data = fmu,
    };
    ModelInstanceSpec mi = { .yaml_doc_list = fmi_gw->settings.doc_list };
    if (schema_object_search(&mi, &m_sel, _stack_match_handler)) {
        fmu_log(fmu, 5, "Fatal", "Could not locate stack.yaml");
    }
}
