// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/platform.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimcl/fmimcl.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


static void _sort_by_marshal_group(HashList* signal_list)
{
    /*
    Sort the signal list according the marshal group.
        MarshalKind - ascending.
        MarshalDir - TX, RXTX, RX
        MarshalType - ascending.
    */
    MarshalDir mDirOrder[] = {
        MARSHAL_DIRECTION_NONE,
        MARSHAL_DIRECTION_LOCAL,
        MARSHAL_DIRECTION_RXONLY,
        MARSHAL_DIRECTION_TXRX,
        MARSHAL_DIRECTION_TXONLY,
        MARSHAL_DIRECTION_PARAMETER,
    };

    size_t    count = hashlist_length(signal_list);
    HashList* sorted = calloc(1, sizeof(HashList));
    hashlist_init(sorted, count);

    /* Sort the signal list. */
    for (MarshalKind kind = 0; kind < __MARSHAL_KIND_SIZE__; kind++) {
        for (MarshalType type = 0; type < __MARSHAL_TYPE_SIZE__; type++) {
            for (size_t dir = 0; dir < ARRAY_SIZE(mDirOrder); dir++) {
                for (size_t i = 0; i < count; i++) {
                    FmuSignal* s = hashlist_at(signal_list, i);
                    if (s->variable_kind == kind &&
                        s->variable_dir == mDirOrder[dir] &&
                        s->variable_type == type) {
                        hashlist_append(sorted, s);
                    }
                }
            }
        }
    }

    /* Delete the original hashlist structure, but not its contents. */
    hashlist_destroy(signal_list);

    /* Shallow copy the sorted hash list. */
    memcpy(signal_list, sorted, sizeof(HashList));
    free(sorted);
}


static MarshalKind _decode_var_kind(const char* t)
{
    if (t == NULL) return MARSHAL_KIND_NONE;

    if (strcmp(t, "Real") == 0) return MARSHAL_KIND_PRIMITIVE;
    if (strcmp(t, "Integer") == 0) return MARSHAL_KIND_PRIMITIVE;
    if (strcmp(t, "Boolean") == 0) return MARSHAL_KIND_PRIMITIVE;
    if (strcmp(t, "String") == 0) return MARSHAL_KIND_BINARY;

    return MARSHAL_KIND_NONE;
}


static MarshalType _decode_var_type(const char* t)
{
    if (t == NULL) return MARSHAL_TYPE_NONE;

    if (strcmp(t, "Real") == 0) return MARSHAL_TYPE_DOUBLE;
    if (strcmp(t, "Integer") == 0) return MARSHAL_TYPE_INT32;
    if (strcmp(t, "Boolean") == 0) return MARSHAL_TYPE_BOOL;
    if (strcmp(t, "String") == 0) return MARSHAL_TYPE_STRING;

    return MARSHAL_TYPE_NONE;
}


static MarshalDir _decode_var_dir(const char* t)
{
    if (t == NULL) return MARSHAL_DIRECTION_TXRX;

    if (strcmp(t, "input") == 0) return MARSHAL_DIRECTION_TXONLY;
    if (strcmp(t, "output") == 0) return MARSHAL_DIRECTION_RXONLY;
    if (strcmp(t, "inout") == 0) return MARSHAL_DIRECTION_TXRX;
    if (strcmp(t, "parameter") == 0) return MARSHAL_DIRECTION_PARAMETER;
    if (strcmp(t, "local") == 0) return MARSHAL_DIRECTION_LOCAL;

    return MARSHAL_DIRECTION_NONE;
}


static void* _fmu_signal_generator(ModelInstanceSpec* mi, void* data)
{
    UNUSED(mi);
    YamlNode* n = dse_yaml_find_node((YamlNode*)data, "signal");
    if (n && n->scalar) {
        FmuSignal*  s = calloc(1, sizeof(FmuSignal));
        const char* v_type = NULL;
        const char* v_dir = NULL;

        s->name = n->scalar;
        dse_yaml_get_uint(
            data, "annotations/fmi_variable_vref", &s->variable_vref);
        dse_yaml_get_string(
            data, "annotations/fmi_variable_name", &s->variable_name);
        dse_yaml_get_string(data, "annotations/fmi_variable_type", &v_type);
        dse_yaml_get_string(data, "annotations/fmi_variable_causality", &v_dir);
        dse_yaml_get_string(data,
            "annotations/fmi_annotations/"
            "dse.standards.fmi-ls-binary-to-text.encoding",
            &s->variable_annotation_encoding);
        s->variable_kind = _decode_var_kind(v_type);
        s->variable_type = _decode_var_type(v_type);
        s->variable_dir = _decode_var_dir(v_dir);

        return s;
    }
    return NULL;
}


static int _variable_match_handler(ModelInstanceSpec* mi, SchemaObject* o)
{
    HashList* s_list = o->data;

    /* Enumerate over the variables (signals). */
    uint32_t   index = 0;
    FmuSignal* s;
    do {
        s = schema_object_enumerator(
            mi, o, "spec/signals", &index, _fmu_signal_generator);
        if (s == NULL) break;
        if (s->name) {
            log_trace("  %s (vref = %u, name = %s, type = %u)", s->name,
                s->variable_vref, s->variable_name, s->variable_type);
            hashlist_append(s_list, s);
        } else {
            free(s);
        }
    } while (1);

    /* Stop parsing after first match. */
    return 1;
}


static int _model_match_handler(ModelInstanceSpec* mi, SchemaObject* o)
{
    UNUSED(mi);
    FmuModel* m = o->data;
    m->m_doc = o->doc;

    /* Annotations. */
    // clang-format off
    dse_yaml_get_string(m->m_doc, "metadata/annotations/mcl_adapter", &m->mcl.adapter);
    dse_yaml_get_string(m->m_doc, "metadata/annotations/mcl_version", &m->mcl.version);
    dse_yaml_get_bool(m->m_doc, "metadata/annotations/fmi_model_cosim", &m->cosim);
    dse_yaml_get_string(m->m_doc, "metadata/annotations/fmi_model_version", &m->version);
    dse_yaml_get_double(m->m_doc, "metadata/annotations/fmi_stepsize", &m->mcl.step_size);
    dse_yaml_get_string(m->m_doc, "metadata/annotations/fmi_guid", &m->guid);
    dse_yaml_get_string(m->m_doc, "metadata/annotations/fmi_resource_dir", &m->resource_dir);
    // clang-format on

    /* Make sure that a resource dir is set. */
    if (m->resource_dir == NULL) {
        m->resource_dir = "/tmp";
    }

    /* FMU Library. */
    const char* selectors[] = { "os", "arch" };
    const char* values[] = { PLATFORM_OS, PLATFORM_ARCH };
    YamlNode*   n = dse_yaml_find_node_in_seq(
        m->m_doc, "spec/runtime/mcl", selectors, values, ARRAY_SIZE(selectors));
    dse_yaml_get_string(n, "path", &m->path);

    /* Logging. */
    log_notice("FMU Model:");
    log_notice("  Name = %s", m->name);
    log_notice("  MCL Adapter = %s", m->mcl.adapter);
    log_notice("  MCL Version = %s", m->mcl.version);
    log_notice("  CoSim = %s", m->cosim ? "true" : "false");
    log_notice("  Model Version = %s", m->version);
    log_notice("  Model Stepsize = %.6f", m->mcl.step_size);
    log_notice("  Model GUID = %s", m->guid);
    log_notice("  Model Resource Directory = %s", m->resource_dir);
    log_notice("  Path = %s (%s/%s)", m->path, PLATFORM_OS, PLATFORM_ARCH);

    /* Stop parsing after first match. */
    return 1;
}


/**
fmimcl_parse
============

This function parses the given yaml files into a FMU Model descriptor object
and into a mapping list between the signals and FMU variables.

Parameters
----------
fmu_model (FmuModel*)
: FMU Model descriptor object.
*/
void fmimcl_parse(FmuModel* m)
{
    /* Parse the FMU Model. */
    SchemaObjectSelector m_sel = {
        .kind = "Model",
        .name = m->name,
        .data = m,
    };
    schema_object_search(m->mcl.model.mi, &m_sel, _model_match_handler);

    /* Parse the FMU Variables. */
    log_trace("FMU Variables:");
    HashList s_list;
    hashlist_init(&s_list, 1000);

    SchemaLabel scalar_v_labels[] = {
        { .name = "model", .value = m->name },
        { .name = "channel", .value = "signal_vector" },
    };
    SchemaObjectSelector scalar_v_sel = {
        .kind = "SignalGroup",
        .labels = scalar_v_labels,
        .labels_len = ARRAY_SIZE(scalar_v_labels),
        .data = &s_list,
    };
    schema_object_search(
        m->mcl.model.mi, &scalar_v_sel, _variable_match_handler);

    SchemaLabel network_v_labels[] = {
        { .name = "model", .value = m->name },
        { .name = "channel", .value = "network_vector" },
    };
    SchemaObjectSelector network_v_sel = {
        .kind = "SignalGroup",
        .labels = network_v_labels,
        .labels_len = ARRAY_SIZE(network_v_labels),
        .data = &s_list,
    };
    schema_object_search(
        m->mcl.model.mi, &network_v_sel, _variable_match_handler);

    /* Sort the signals by marshal grouping. */
    _sort_by_marshal_group(&s_list);

    /* Convert to a NTL. */
    size_t count = hashlist_length(&s_list);
    m->signals = calloc(count + 1, sizeof(FmuSignal));
    for (uint32_t i = 0; i < count; i++) {
        memcpy(&m->signals[i], hashlist_at(&s_list, i), sizeof(FmuSignal));
        free(hashlist_at(&s_list, i));
    }
    hashlist_destroy(&s_list);
}
