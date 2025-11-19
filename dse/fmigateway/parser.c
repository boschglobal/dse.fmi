// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdio.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/collections/hashmap.h>
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


static void _get_exe(YamlNode* doc, YamlNode* node, const char* path,
    WindowsModel* model, const char* type)
{
    if (dse_yaml_get_string(node, path, &model->exe)) {
        model->exe = getenv(type);
        if (model->exe == NULL) {
            YamlNode* e_node = dse_yaml_find_node(doc, "spec/runtime/env");
            if (e_node == NULL) {
                return;
            }
            dse_yaml_get_string(e_node, type, &model->exe);
        }
    }
}


static char* _build_delimited_str(
    char* dest, const char* src, const char* delim)
{
    if (dest == NULL) return strdup(src);

    size_t len = strlen(dest) + strlen(src) + strlen(delim) + 1;
    char*  result = calloc(len, sizeof(char));
    if (strlen(dest) > 0) {
        strncat(result, dest, len - 1);
        strncat(result, delim, len - 1);
    }
    free(dest);
    return strncat(result, src, len - 1);
}


static char* _generate_yaml_files(YamlNode* model_n)
{
    YamlNode* f_node = dse_yaml_find_node(model_n, "runtime/files");
    if (f_node) {
        char* yaml = NULL;
        for (uint32_t i = 0; i < hashlist_length(&f_node->sequence); i++) {
            YamlNode* fi_node = hashlist_at(&f_node->sequence, i);
            char*     file = fi_node->scalar;
            yaml = _build_delimited_str(yaml, file, " ");
        }
        return yaml;
    }
    return NULL;
}


static FmiGatewayEnvvar* _generate_model_envar(YamlNode* model_n)
{
    YamlNode* node = dse_yaml_find_node(model_n, "runtime/env");
    if (node) {
        if (node->node_type != 3) return NULL;  // YAML_MAPPING_NODE
        char**            _keys = hashmap_keys(&node->mapping);
        FmiGatewayEnvvar* envar =
            calloc(node->mapping.used_nodes + 1, sizeof(FmiGatewayEnvvar));
        for (uint64_t i = 0; i < node->mapping.used_nodes; i++) {
            YamlNode* _n = hashmap_get(&node->mapping, _keys[i]);
            if (_n == NULL || _n->node_type != 1) continue;  // YAML_SCALAR_NODE
            envar[i].name = _n->name;
            envar[i].default_value = _n->scalar;
            log_debug("  %s = %s", _n->name, _n->scalar);
        }
        for (uint64_t _ = 0; _ < node->mapping.used_nodes; _++)
            free(_keys[_]);
        free(_keys);
        return envar;
    }
    return NULL;
}


static void _print_model_info(WindowsModel* model)
{
    log_notice("%s", model->name);
    log_notice("  exe: %s", model->exe);
    log_notice("  Yaml: %s", model->yaml);
    log_notice("  Stepsize: %lf", model->step_size);
    log_notice("  Endtime: %lf", model->end_time);
    log_notice("  Timout: %lf", model->timeout);
    log_notice("  Loglevel: %d", model->log_level);
}


static WindowsModel* _gwfmu_model_generator(
    YamlNode* model_n, YamlNode* gw_doc, YamlNode* doc, const char* exe)
{
    YamlNode* n = dse_yaml_find_node((YamlNode*)model_n, "name");
    if (n && n->scalar) {
        WindowsModel* model = calloc(1, sizeof(WindowsModel));

        const char* name = NULL;
        if (dse_yaml_get_string(model_n, "name", &name)) {
            log_error("Name is required for model.");
            free(model);
            return NULL;
        };
        model->name = strdup(name);

        if (dse_yaml_get_double(
                model_n, "annotations/cli/step_size", &model->step_size)) {
            model->step_size = DEFAULT_STEP_SIZE;
        };
        if (dse_yaml_get_double(
                model_n, "annotations/cli/end_time", &model->end_time)) {
            model->end_time = DEFAULT_END_TIME;
        };
        if (dse_yaml_get_int(
                model_n, "annotations/cli/log_level", &model->log_level)) {
            model->log_level = DEFAULT_LOG_LEVEL;
        };
        /* model timeout > stack timeout > default timeout */
        if (dse_yaml_get_double(
                model_n, "annotations/cli/timeout", &model->timeout)) {
            model->timeout = DEFAULT_TIMEOUT;
            if (doc) {
                dse_yaml_get_double(
                    doc, "spec/runtime/env/timeout", &model->timeout);
            }
        };

        /* Check if an exe is given for this model. */
        _get_exe(gw_doc, model_n, "annotations/cli/exe", model, exe);
        model->yaml = _generate_yaml_files(model_n);
        model->envar = _generate_model_envar(model_n);
        _print_model_info(model);
        return model;
    }
    return NULL;
}


static void _parse_script_envar(
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
                hashmap_set_string(&fmu->variables.string.input,  // NOLINT
                    envar->vref, (char*)_str);
                envar->default_value = strdup((char*)_str);
            } else if (strcmp(envar->type, "real") == 0) {
                double value = 0.0;
                dse_yaml_get_double(_env, "default", &value);
                hashmap_set_double(
                    &fmu->variables.scalar.input, envar->vref, value);
                envar->default_value = calloc(NUMERIC_ENVAR_LEN, sizeof(char));
                snprintf(
                    envar->default_value, NUMERIC_ENVAR_LEN, "%d", (int)value);
            }

            hashlist_append(&e_list, envar);
        }
        session->envar = hashlist_ntl(&e_list, sizeof(FmiGatewayEnvvar), true);
    }
}


static void _build_models(uint32_t len, YamlNode* n_models, YamlNode* gw_doc,
    YamlNode* doc, HashList* list)
{
    for (uint32_t i = 0; i < len; i++) {
        YamlNode* model = hashlist_at(&n_models->sequence, i);
        if (model == NULL) continue;
        WindowsModel* w_model =
            _gwfmu_model_generator(model, gw_doc, doc, MODELC_EXE_PATH);
        if (w_model == NULL) continue;
        hashlist_append(list, w_model);
    }
}


static void _build_stacked_model(char* stack_name, uint32_t len,
    YamlNode* n_models, YamlNode* gw_doc, YamlNode* doc, HashList* list)
{
    char* yaml = NULL;
    yaml = _build_delimited_str(yaml, stack_name, " ");

    char* names = NULL;

    for (uint32_t i = 0; i < len; i++) {
        YamlNode* model_n = hashlist_at(&n_models->sequence, i);
        if (model_n == NULL) continue;

        const char* name = NULL;
        if (dse_yaml_get_string(model_n, "name", &name)) {
            log_error("Name is required for model (index: %d).", i);
            continue;
        };
        names = _build_delimited_str(names, name, ",");

        char* model_yaml = NULL;
        model_yaml = _generate_yaml_files(model_n);
        if (model_yaml) {
            yaml = _build_delimited_str(yaml, model_yaml, " ");
            free(model_yaml);
        }
    }

    WindowsModel* model = calloc(1, sizeof(WindowsModel));
    model->name = names;
    model->yaml = yaml;
    model->stacked = true;

    if (dse_yaml_get_double(
            doc, "spec/runtime/env/step_size", &model->step_size)) {
        model->step_size = DEFAULT_STEP_SIZE;
    };
    if (dse_yaml_get_double(
            doc, "spec/runtime/env/end_time", &model->end_time)) {
        model->end_time = DEFAULT_END_TIME;
    };
    if (dse_yaml_get_int(
            doc, "spec/runtime/env/log_level", &model->log_level)) {
        model->log_level = DEFAULT_LOG_LEVEL;
    };
    if (dse_yaml_get_double(doc, "spec/runtime/env/timeout", &model->timeout)) {
        model->timeout = DEFAULT_TIMEOUT;
    };

    _get_exe(gw_doc, doc, "spec/runtime/env/MODELC_EXE_PATH", model,
        MODELC_EXE_PATH);

    hashlist_append(list, model);

    _print_model_info(model);
}


static void _parse_models(char* stack_name, YamlNode* gw_doc, YamlNode* doc,
    HashList* list, bool stacked)
{
    YamlNode* n_models = dse_yaml_find_node(doc, "spec/models");

    /* Node type 2 is sequence. */
    if (n_models == NULL || n_models->node_type != 2) return;
    uint32_t len = hashlist_length(&n_models->sequence);
    if (len == 0) return;

    if (stacked) {
        _build_stacked_model(stack_name, len, n_models, gw_doc, doc, list);
        return;
    }

    _build_models(len, n_models, gw_doc, doc, list);
}


static void _parse_stack(
    FmuInstanceData* fmu, YamlNode* gw_doc, char* stack_name, HashList* list)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    char* model_stack =
        dse_path_cat(fmu->instance.resource_location, stack_name);
    session->model_stack_files = dse_yaml_load_file(model_stack, NULL);
    free(model_stack);
    if (session->model_stack_files == NULL) return;


    for (uint32_t i = 0; i < hashlist_length(session->model_stack_files); i++) {
        YamlNode* doc = hashlist_at(session->model_stack_files, i);
        /* If Document is not of kind Stack, skip. */
        YamlNode* kind = dse_yaml_find_node(doc, "kind");
        if (kind == NULL || kind->scalar == NULL) continue;
        if (strcmp(kind->scalar, "Stack")) continue;

        bool stacked = false;
        dse_yaml_get_bool(doc, "spec/runtime/stacked", &stacked);
        if (stacked) {
            _parse_models(stack_name, gw_doc, doc, list, true);
            continue;
        }
        _parse_models(stack_name, gw_doc, doc, list, false);
    }
}


static void _parse_model_stacks(FmuInstanceData* fmu, YamlNode* gw_doc)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    if (dse_yaml_get_string(gw_doc, "metadata/annotations/model_stack",
            &session->model_stack) == 0) {
        HashList model_list;
        hashlist_init(&model_list, 100);

        char* stack_names = strdup(session->model_stack);
        char* _saveptr = NULL;
        char* _nameptr = strtok_r(stack_names, ";,", &_saveptr);
        while (_nameptr) {
            fmu_log(fmu, 0, "Debug", "Loading stack: %s", _nameptr);
            _parse_stack(fmu, gw_doc, _nameptr, &model_list);
            _nameptr = strtok_r(NULL, ";,", &_saveptr);
        }
        free(stack_names);
        session->w_models =
            hashlist_ntl(&model_list, sizeof(WindowsModel), true);
    }
}


static int _parse_gateway(FmuInstanceData* fmu, YamlNode* doc)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    const char* selector[] = { "name" };
    const char* value[] = { "gateway" };

    /* Model Instance: locate in stack. */
    YamlNode* gateway_node = dse_yaml_find_node_in_seq(
        doc, "spec/models", selector, value, ARRAY_SIZE(selector));
    if (gateway_node == NULL) return EINVAL;

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
            &(session->log_location))) {
        session->log_location = fmu->instance.resource_location;
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


static WindowsModel* _parse_simbus(FmuInstanceData* fmu, YamlNode* root)
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
        _gwfmu_model_generator(simbus_node, root, NULL, SIMBUS_EXE_PATH);

    return simbus;
}


static WindowsModel* _parse_redis(FmuInstanceData* fmu, YamlNode* root)
{
    UNUSED(fmu);
    YamlNode* n_env = dse_yaml_find_node(root, "spec/runtime/env");
    if (n_env == NULL) return NULL;
    WindowsModel* redis_instance = calloc(1, sizeof(WindowsModel));

    /* Parse the Redis port number*/
    redis_instance->args = "6379";
    const char* redis_uri = NULL;
    if (dse_yaml_get_string(root, "spec/connection/transport/redispubsub/uri",
            &redis_uri) != 0) {
        dse_yaml_get_string(
            root, "spec/connection/transport/redis/uri", &redis_uri);
    }
    if (redis_uri) {
        /* Parsing according to redis://host[:port]. */
        const char* p = strpbrk(redis_uri, ":");
        if (p) {
            p = strpbrk(++p, ":");
            if (p) redis_instance->args = ++p;
        }
    }
    log_debug("using redis port: %s", redis_instance->args);

    redis_instance->end_time = DEFAULT_END_TIME;
    redis_instance->name = strdup("transport");
    redis_instance->exe = getenv(REDIS_EXE_PATH);
    if (redis_instance->exe == NULL) {
        if (dse_yaml_get_string(n_env, REDIS_EXE_PATH, &redis_instance->exe)) {
            free(redis_instance);
            return NULL;
        }
    }

    return redis_instance;
}


static void _get_stack_annotations(FmuInstanceData* fmu, YamlNode* doc)
{
    if (doc == NULL || fmu == NULL) return;
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    // Get the annotations node
    YamlNode* ann_node = dse_yaml_find_node(doc, "metadata/annotations");
    if (ann_node == NULL) return;

    ann_node->__inter__ = dse_yaml_interpolate_env;
    dse_yaml_get_uint(
        ann_node, "show_models", (unsigned int*)&session->visibility.models);
    dse_yaml_get_uint(
        ann_node, "show_simbus", (unsigned int*)&session->visibility.simbus);
    dse_yaml_get_uint(
        ann_node, "show_redis", (unsigned int*)&session->visibility.transport);
    dse_yaml_get_uint(
        ann_node, "create_logfiles", (unsigned int*)&session->logging);

    bool start_redis = true;
    dse_yaml_get_uint(doc, "start_redis", (unsigned int*)&start_redis);
    if (start_redis) {
        session->transport = _parse_redis(fmu, doc);
        fmu_log(fmu, 0, "Debug", "Redis will be started by the gateway");
        return;
    }
    fmu_log(fmu, 0, "Debug", "Redis will NOT be started by the gateway");
}


static int _gateway_stack(ModelInstanceSpec* mi, SchemaObject* o)
{
    UNUSED(mi);
    FmuInstanceData* fmu = o->data;
    FmiGateway*      fmi_gw = fmu->data;
    assert(fmi_gw);

    fmi_gw->settings.session = calloc(1, sizeof(FmiGatewaySession));
    FmiGatewaySession* session = fmi_gw->settings.session;

    /* Visibility options. */
    _get_stack_annotations(fmu, o->doc);

    session->simbus = _parse_simbus(fmu, o->doc);

    _parse_gateway(fmu, o->doc);
    _parse_model_stacks(fmu, o->doc);

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
    if (schema_object_search(&mi, &m_sel, _gateway_stack)) {
        fmu_log(fmu, 5, "Fatal", "Could not locate stack.yaml");
    }
}
