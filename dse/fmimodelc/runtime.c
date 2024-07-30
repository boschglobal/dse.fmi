// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdlib.h>
#include <dse/fmimodelc/fmimodelc.h>
#include <dse/modelc/schema.h>
#include <dse/clib/util/yaml.h>


extern uint8_t __log_level__;


static SchemaSignalObject* __signal_match;
static const char*         __signal_match_name;

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
    ChannelSpec*          cs = model_build_channel_spec(mi, sv->name);
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
    for (SignalVector* sv = m->model.sv; sv && sv->name; sv++) {
        if (sv->is_binary == true) continue;

        for (uint32_t i = 0; i < sv->count; i++) {
            /* Value Reference. */
            const char* vref = signal_annotation(sv, i, "fmi_value_reference");
            if (vref == NULL) continue;

            /* Index based on causality. */
            const char* causality =
                signal_annotation(sv, i, "fmi_variable_causality");
            if (strcmp(causality, "output") == 0) {
                hashmap_set(output, vref, &sv->scalar[i]);
            } else if (strcmp(causality, "input") == 0) {
                hashmap_set(input, vref, &sv->scalar[i]);
            }
        }
    }
}


void fmimodelc_index_binary_signals(
    RuntimeModelDesc* m, HashMap* rx, HashMap* tx)
{
    for (SignalVector* sv = m->model.sv; sv && sv->name; sv++) {
        if (sv->is_binary == false) continue;

        for (uint32_t i = 0; i < sv->count; i++) {
            /* Value Reference. */
            const char* vref = signal_annotation(sv, i, "fmi_value_reference");
            if (vref == NULL) continue;

            /* Index according to bus topology. */
            // dse.standards.fmi-ls-bus-topology.rx_vref: [2,4,6,8]
            const char** rx_list = _signal_annotation_list(sv->mi, sv,
                sv->signal[i], "dse.standards.fmi-ls-bus-topology.rx_vref");
            if (rx_list) {
                for (size_t j = 0; rx_list[j]; j++) {
                    /* Value Reference for the RX variable. */
                    const char* rx_vref = rx_list[j];

                    /* Binary signal index object. */
                    ModelSignalIndex idx =
                        m->model.index((ModelDesc*)m, sv->alias, sv->signal[i]);
                    if (idx.binary == NULL) continue;

                    /* Complete the index entry. */
                    ModelSignalIndex* _ = calloc(1, sizeof(ModelSignalIndex));
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

                    /* Binary signal index object. */
                    ModelSignalIndex idx =
                        m->model.index((ModelDesc*)m, sv->alias, sv->signal[i]);
                    if (idx.binary == NULL) continue;

                    /* Complete the index entry. */
                    ModelSignalIndex* _ = calloc(1, sizeof(ModelSignalIndex));
                    *_ = idx;
                    hashmap_set_alt(tx, tx_vref, _);
                }
                free(tx_list);
            }
        }
    }
}


void fmimodelc_index_text_encoding(
    RuntimeModelDesc* m, HashMap* encode_func, HashMap* decode_func)
{
    // dse.standards.fmi-ls-text-encoding.encoding: ascii85

    for (SignalVector* sv = m->model.sv; sv && sv->name; sv++) {
        if (sv->is_binary == false) continue;

        for (uint32_t i = 0; i < sv->count; i++) {
            /* Value Reference. */
            const char* _ = signal_annotation(sv, i, "fmi_value_reference");
            if (_ == NULL) continue;

            /* Encoding. */
            const char* encoding = signal_annotation(
                sv, i, "dse.standards.fmi-ls-text-encoding.encoding");
            if (strcmp(encoding, "ascii85") != 0) continue;

            /* Index, all with same encoding (for now). */
            // dse.standards.fmi-ls-text-encoding.vref: [[2,3,4,5,6,7,8,9]
            const char** vref_list = _signal_annotation_list(sv->mi, sv,
                sv->signal[i], "dse.standards.fmi-ls-text-encoding.vref");
            if (vref_list) {
                for (size_t j = 0; vref_list[j]; j++) {
                    /* Value Reference for the RX variable. */
                    const char* vref = vref_list[j];

                    /* Encoding. */
                    hashmap_set(encode_func, vref, ascii85_encode);
                    hashmap_set(decode_func, vref, ascii85_decode);
                }
                free(vref_list);
            }
        }
    }
}


void fmimodelc_reset_binary_signals(RuntimeModelDesc* m)
{
    for (SignalVector* sv = m->model.sv; sv && sv->name; sv++) {
        if (sv->is_binary == false) continue;

        for (uint32_t i = 0; i < sv->count; i++) {
            if (sv->reset_called[i] == false) {
                signal_reset(sv, i);
            }
        }
    }
}


void fmimodelc_clear_reset_called(RuntimeModelDesc* m)
{
    for (SignalVector* sv = m->model.sv; sv && sv->name; sv++) {
        if (sv->is_binary == false) continue;

        for (uint32_t i = 0; i < sv->count; i++) {
            sv->reset_called[i] = false;
        }
    }
}
