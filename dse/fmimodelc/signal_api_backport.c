// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <errno.h>
#include <dse/modelc/model.h>


const char* signal_annotation(
    SignalVector* sv, uint32_t index, const char* name)
{
    if (sv && index < sv->count) {
        if (sv->annotation) {
            return sv->annotation(sv, index, name);
        } else {
            errno = ENOSYS;
            return NULL;
        }
    } else {
        errno = EINVAL;
        return NULL;
    }
}


int signal_append(SignalVector* sv, uint32_t index, void* data, uint32_t len)
{
    if (sv && index < sv->count && sv->is_binary) {
        if (sv->append) {
            return sv->append(sv, index, data, len);
        } else {
            return -ENOSYS;
        }
    } else {
        return -EINVAL;
    }
}


int signal_reset(SignalVector* sv, uint32_t index)
{
    if (sv && index < sv->count && sv->is_binary) {
        if (sv->reset) {
            return sv->reset(sv, index);
        } else {
            return -ENOSYS;
        }
    } else {
        return -EINVAL;
    }
}
