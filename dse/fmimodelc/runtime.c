// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <dse/fmimodelc/fmimodelc.h>
#include <dse/fmu/fmu.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/schema.h>
#include <dse/clib/util/yaml.h>


static SchemaSignalObject* __signal_match;
static const char*         __signal_match_name;


static void _log(const char* format, ...)
{
    printf("ModelCFmu: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}


static int _signal_group_match_handler(
    ModelInstanceSpec* model_instance, SchemaObject* object)
{
    uint32_t index = 0;

    /* Enumerate over the signals. */
    SchemaSignalObject* so;
    do {
        so = schema_object_enumerator(model_instance, object, "spec/signals",
            &index, schema_signal_object_generator);
        if (so == NULL) break;
        if (strcmp(so->signal, __signal_match_name) == 0) {
            __signal_match = so; /* Caller to free. */
            return 0;
        }
        free(so);
    } while (1);

    return 0;
}


static ChannelSpec* _model_build_channel_spec(
    ModelInstanceSpec* model_instance, const char* channel_name)
{
    _log("Search for channel on MI (%s) by name/alias=%s", model_instance->name,
        channel_name);
    const char* selectors[] = { "name" };
    const char* values[] = { channel_name };
    YamlNode*   c_node = dse_yaml_find_node_in_seq(
        model_instance->spec, "channels", selectors, values, 1);
    if (c_node) {
        _log(" channel found by name");
    } else {
        const char* selectors[] = { "alias" };
        const char* values[] = { channel_name };
        c_node = dse_yaml_find_node_in_seq(
            model_instance->spec, "channels", selectors, values, 1);
        if (c_node) {
            _log("  channel found by alias");
        }
    }
    if (c_node == NULL) {
        _log("Channel node (%s) not found on MI (%s)!", channel_name,
            model_instance->name);
        return NULL;
    }

    ChannelSpec* channel_spec = calloc(1, sizeof(ChannelSpec));
    channel_spec->name = channel_name;
    channel_spec->private = c_node;
    YamlNode* n_node = dse_yaml_find_node(c_node, "name");
    if (n_node && n_node->scalar) channel_spec->name = n_node->scalar;
    YamlNode* a_node = dse_yaml_find_node(c_node, "alias");
    if (a_node && a_node->scalar) channel_spec->alias = a_node->scalar;

    return channel_spec; /* Caller to free. */
}


static const char** _signal_annotation_list(ModelInstanceSpec* mi,
    SignalVector* sv, const char* signal, const char* name)
{
    /* Set the search vars. */
    __signal_match = NULL;
    __signal_match_name = signal;

    /* Select and handle the schema objects. */
    ChannelSpec*          cs = _model_build_channel_spec(mi, sv->name);
    SchemaObjectSelector* selector;
    selector = schema_build_channel_selector(mi, cs, "SignalGroup");
    if (selector) {
        schema_object_search(mi, selector, _signal_group_match_handler);
    }
    schema_release_selector(selector);
    free(cs);

    /* Look for the annotation. */
    if (__signal_match) {
        const char** v = NULL;
        YamlNode*    a_node =
            dse_yaml_find_node(__signal_match->data, "annotations");
        if (a_node) {
            v = dse_yaml_get_array(a_node, name, NULL);
        }
        free(__signal_match);
        __signal_match = NULL;
        return v;
    }

    return NULL;
}


void fmimodelc_index_scalar_signals(
    RuntimeModelDesc* m, HashMap* input, HashMap* output)
{
    for (ModelInstanceSpec* mi = m->model.sim->instance_list; mi && mi->name;
        mi++) {
        for (SignalVector* sv = mi->model_desc->sv; sv && sv->name; sv++) {
            if (sv->is_binary == true) continue;

            for (uint32_t i = 0; i < sv->count; i++) {
                /* Value Reference. */
                const char* vref =
                    signal_annotation(sv, i, "fmi_variable_vref", NULL);
                if (vref == NULL) continue;

                /* Locate the SimBus variable. */
                SimbusVectorIndex index =
                    simbus_vector_lookup(m->model.sim, sv->name, sv->signal[i]);
                if (index.sbv == NULL) continue;
                double* scalar = &index.sbv->scalar[index.vi];
                if (scalar == NULL) continue;

                /* Index based on causality. */
                const char* causality =
                    signal_annotation(sv, i, "fmi_variable_causality", NULL);
                if (strcmp(causality, "output") == 0) {
                    hashmap_set(output, vref, scalar);
                } else if (strcmp(causality, "input") == 0) {
                    hashmap_set(input, vref, scalar);
                }
            }
        }
    }
    _log("  Scalar: input=%lu, output=%lu", input->used_nodes,
        output->used_nodes);
}


void fmimodelc_index_binary_signals(
    RuntimeModelDesc* m, HashMap* rx, HashMap* tx)
{
    for (ModelInstanceSpec* mi = m->model.sim->instance_list; mi && mi->name;
        mi++) {
        for (SignalVector* sv = mi->model_desc->sv; sv && sv->name; sv++) {
            if (sv->is_binary == false) continue;

            for (uint32_t i = 0; i < sv->count; i++) {
                /* Value Reference. */
                const char* vref =
                    signal_annotation(sv, i, "fmi_variable_vref", NULL);
                if (vref == NULL) continue;

                /* Index according to bus topology. */
                // dse.standards.fmi-ls-bus-topology.rx_vref: [2,4,6,8]
                const char** rx_list = _signal_annotation_list(sv->mi, sv,
                    sv->signal[i], "dse.standards.fmi-ls-bus-topology.rx_vref");
                if (rx_list) {
                    for (size_t j = 0; rx_list[j]; j++) {
                        /* Value Reference for the RX variable. */
                        const char* rx_vref = rx_list[j];

                        /* Locate the SimBus variable. */
                        SimbusVectorIndex idx = simbus_vector_lookup(
                            m->model.sim, sv->name, sv->signal[i]);
                        if (idx.sbv == NULL) continue;

                        /* Complete the index entry. */
                        SimbusVectorIndex* _ =
                            calloc(1, sizeof(SimbusVectorIndex));
                        *_ = idx;
                        hashmap_set_alt(rx, rx_vref, _);
                    }
                    free(rx_list);
                }

                // dse.standards.fmi-ls-bus-topology.tx_vref: [3,5,7,9]
                const char** tx_list = _signal_annotation_list(sv->mi, sv,
                    sv->signal[i], "dse.standards.fmi-ls-bus-topology.tx_vref");
                if (tx_list) {
                    for (size_t j = 0; tx_list[j]; j++) {
                        /* Value Reference for the TX variable. */
                        const char* tx_vref = tx_list[j];

                        /* Locate the SimBus variable. */
                        SimbusVectorIndex idx = simbus_vector_lookup(
                            m->model.sim, sv->name, sv->signal[i]);
                        if (idx.sbv == NULL) continue;

                        /* Complete the index entry. */
                        SimbusVectorIndex* _ =
                            calloc(1, sizeof(SimbusVectorIndex));
                        *_ = idx;
                        hashmap_set_alt(tx, tx_vref, _);
                    }
                    free(tx_list);
                }
            }
        }
    }
    _log("  Binary: rx=%lu, tx=%lu", rx->used_nodes, tx->used_nodes);
}


void fmimodelc_index_text_encoding(
    RuntimeModelDesc* m, HashMap* encode_func, HashMap* decode_func)
{
    // dse.standards.fmi-ls-binary-to-text.encoding: ascii85

    for (ModelInstanceSpec* mi = m->model.sim->instance_list; mi && mi->name;
        mi++) {
        for (SignalVector* sv = mi->model_desc->sv; sv && sv->name; sv++) {
            if (sv->is_binary == false) continue;

            for (uint32_t i = 0; i < sv->count; i++) {
                /* Value Reference. */
                const char* _ =
                    signal_annotation(sv, i, "fmi_variable_vref", NULL);
                if (_ == NULL) continue;

                /* Encoding. */
                const char* encoding = signal_annotation(sv, i,
                    "dse.standards.fmi-ls-binary-to-text.encoding", NULL);
                if (strcmp(encoding, "ascii85") != 0) continue;

                /* Index, all with same encoding (for now). */
                // dse.standards.fmi-ls-binary-to-text.vref: [[2,3,4,5,6,7,8,9]
                const char** vref_list = _signal_annotation_list(sv->mi, sv,
                    sv->signal[i], "dse.standards.fmi-ls-binary-to-text.vref");
                if (vref_list) {
                    for (size_t j = 0; vref_list[j]; j++) {
                        /* Value Reference for the RX variable. */
                        const char* vref = vref_list[j];

                        /* Encoding. */
                        hashmap_set(encode_func, vref, dse_ascii85_encode);
                        hashmap_set(decode_func, vref, dse_ascii85_decode);
                    }
                    free(vref_list);
                }
            }
        }
    }
    _log("  Encoding: enc=%lu, dec=%lu", encode_func->used_nodes,
        decode_func->used_nodes);
}


static int envar_iterator(void* value, void* key)
{
    _log(
        "  set envar: name=%s, value=%s", (const char*)key, (const char*)value);
    fmimodelc_setenv((const char*)key, (const char*)value);
    return 0;
}

void fmimodelc_set_model_env(RuntimeModelDesc* m)
{
    HashMap envars;
    hashmap_init(&envars);
    YamlNode* env_node = NULL;

    /* Envars from : stack/spec/runtime/env */
    env_node = dse_yaml_find_node(m->model.sim->spec, "spec/runtime/env");
    if (env_node && hashmap_number_keys(env_node->mapping)) {
        char**   name = hashmap_keys(&env_node->mapping);
        uint32_t count = hashmap_number_keys(env_node->mapping);
        for (size_t i = 0; i < count; i++) {
            YamlNode* value = hashmap_get(&env_node->mapping, name[i]);
            if (value->scalar) {
                hashmap_set(&envars, name[i], value->scalar);
            }
        }
        for (uint32_t _ = 0; _ < count; _++)
            free(name[_]);
        free(name);
    }

    /* Envars from : mi/runtime/env (prefix with model name). */
    for (ModelInstanceSpec* mi = m->model.sim->instance_list; mi && mi->name;
        mi++) {
        env_node = dse_yaml_find_node(mi->spec, "runtime/env");
        if (env_node && hashmap_number_keys(env_node->mapping)) {
            char**   name = hashmap_keys(&env_node->mapping);
            uint32_t count = hashmap_number_keys(env_node->mapping);
            for (size_t i = 0; i < count; i++) {
                YamlNode* value = hashmap_get(&env_node->mapping, name[i]);
                if (value->scalar) {
                    if (strcmp(mi->name, "simbus") == 0) {
                        hashmap_set(&envars, name[i], value->scalar);
                    } else {
                        /* Prefix with the model name. */
                        char buffer[100];
                        snprintf(buffer, sizeof(buffer), "%s__%s", mi->name,
                            name[i]);
                        for (int i = 0; buffer[i]; i++)
                            buffer[i] = toupper(buffer[i]);
                        hashmap_set(&envars, buffer, value->scalar);
                    }
                }
            }
            for (uint32_t _ = 0; _ < count; _++)
                free(name[_]);
            free(name);
        }
    }

    _log("Runtime Environment Variables: ");
    hashmap_kv_iterator(&envars, envar_iterator, true);
    hashmap_destroy(&envars);
}
