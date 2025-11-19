// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/clib/util/yaml.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/gateway.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>


static SchemaSignalObject* __signal_match;
static const char*         __signal_match_name;


ChannelSpec* _model_build_channel_spec(
    ModelInstanceSpec* model_instance, const char* channel_name)
{
    const char* selectors[] = { "name" };
    const char* values[] = { channel_name };
    YamlNode*   c_node = dse_yaml_find_node_in_seq(
        model_instance->spec, "channels", selectors, values, 1);
    /* No channel was found with matching name. */
    if (c_node == NULL) {
        const char* selectors[] = { "alias" };
        const char* values[] = { channel_name };
        c_node = dse_yaml_find_node_in_seq(
            model_instance->spec, "channels", selectors, values, 1);
    }
    /* No channel was found with matching alias. */
    if (c_node == NULL) {
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


void fmigateway_index_scalar_signals(
    FmuInstanceData* fmu, ModelGatewayDesc* m, HashMap* input, HashMap* output)
{
    for (SignalVector* sv = m->sv; sv && sv->name; sv++) {
        if (sv->is_binary) continue;

        for (uint32_t i = 0; i < sv->count; i++) {
            /* Value Reference. */
            const char* vref =
                signal_annotation(sv, i, "fmi_variable_vref", NULL);
            if (vref == NULL) continue;

            /* Locate the variable. */
            ModelSignalIndex idx = signal_index(
                (ModelDesc*)m->mi->model_desc, sv->alias, sv->signal[i]);
            if (idx.scalar == NULL) continue;

            /* Index based on causality. */
            const char* causality =
                signal_annotation(sv, i, "fmi_variable_causality", NULL);
            if (strcmp(causality, "output") == 0) {
                hashmap_set(output, vref, idx.scalar);
            } else if (strcmp(causality, "input") == 0) {
                hashmap_set(input, vref, idx.scalar);
            }
        }
    }

    fmu_log(fmu, 0, "Debug", "  Scalar: input=%lu, output=%lu",
        input->used_nodes, output->used_nodes);
}


static inline void _set_binary_variable(ModelDesc* m, SignalVector* sv,
    uint32_t index, HashMap* map, const char* vref)
{
    /* Locate the variable. */
    ModelSignalIndex idx = signal_index(m, sv->alias, sv->signal[index]);
    if (idx.binary == NULL) return;

    FmuSignalVectorIndex* fmu_idx = calloc(1, sizeof(FmuSignalVectorIndex));
    fmu_idx->sv = calloc(1, sizeof(FmuSignalVector));
    fmu_idx->sv->binary = idx.sv->binary;
    fmu_idx->sv->signal = (char**)idx.sv->signal;
    fmu_idx->sv->length = idx.sv->length;
    fmu_idx->sv->buffer_size = idx.sv->buffer_size;
    fmu_idx->vi = idx.signal;
    hashmap_set_alt(map, vref, fmu_idx);
}


void fmigateway_index_binary_signals(
    FmuInstanceData* fmu, ModelGatewayDesc* m, HashMap* rx, HashMap* tx)
{
    for (ModelInstanceSpec* mi = m->sim->instance_list; mi && mi->name; mi++) {
        for (SignalVector* sv = mi->model_desc->sv; sv && sv->name; sv++) {
            if (sv->is_binary == false) continue;

            for (uint32_t i = 0; i < sv->count; i++) {
                /* Value Reference. */
                const char* vref =
                    signal_annotation(sv, i, "fmi_variable_vref", NULL);
                if (vref == NULL) continue;

                /* FMU binary input variables. */
                /* Index according to bus topology. */
                const char** rx_list = _signal_annotation_list(sv->mi, sv,
                    sv->signal[i], "dse.standards.fmi-ls-bus-topology.rx_vref");
                if (rx_list) {
                    for (size_t j = 0; rx_list[j]; j++) {
                        /* Value Reference for the RX variable. */
                        const char* rx_vref = rx_list[j];

                        /* Create a FmuSignalVector and add it to the map. */
                        _set_binary_variable(
                            m->mi->model_desc, sv, i, rx, rx_vref);
                    }
                    free(rx_list);
                }

                /* FMU binary output variables. */
                /* Index according to bus topology. */
                const char** tx_list = _signal_annotation_list(sv->mi, sv,
                    sv->signal[i], "dse.standards.fmi-ls-bus-topology.tx_vref");
                if (tx_list) {
                    for (size_t j = 0; tx_list[j]; j++) {
                        /* Value Reference for the TX variable. */
                        const char* tx_vref = tx_list[j];

                        /* Create a FmuSignalVector and add it to the map. */
                        _set_binary_variable(
                            m->mi->model_desc, sv, i, tx, tx_vref);
                    }
                    free(tx_list);
                }
            }
        }
    }

    fmu_log(fmu, 0, "Debug", "  Binary: rx=%lu, tx=%lu", rx->used_nodes,
        tx->used_nodes);
}


void fmigateway_index_text_encoding(FmuInstanceData* fmu, ModelGatewayDesc* m,
    HashMap* encode_func, HashMap* decode_func)
{
    for (ModelInstanceSpec* mi = m->sim->instance_list; mi && mi->name; mi++) {
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
    fmu_log(fmu, 0, "Debug", "  Encoding: enc=%lu, dec=%lu",
        encode_func->used_nodes, decode_func->used_nodes);
}
