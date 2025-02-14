// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdbool.h>
#include <libxml/xpath.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/fmu/fmu.h>


#define UNUSED(x) ((void)x)


size_t fmu_variable_count(xmlDoc* doc, bool is_binary)
{
    UNUSED(doc);
    UNUSED(is_binary);
    return 0;
}


void fmu_variable_index(
    xmlDoc* doc, FmuInstanceData* fmu, FmuSignalVector* sv, bool is_binary)
{
    UNUSED(doc);
    UNUSED(fmu);
    UNUSED(sv);
    UNUSED(is_binary);
}
