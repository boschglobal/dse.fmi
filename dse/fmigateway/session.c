// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x) ((void)x)


/**
fmigateway_session_configure
============================

If session parameters were parsed from the model description, this method
configures and starts the additional models, or executes the given command.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmigateway_session_configure(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    FmiGatewaySession* session = fmi_gw->settings.session;
    if (session == NULL) return;

    if (session->init.file) {
        char buf[256];
        snprintf(
            buf, sizeof(buf), "%s%s", session->init.path, session->init.file);
        system(buf);
    }

    if (session->w_models && strcmp(PLATFORM_OS, "windows") == 0) {
        fmigateway_session_windows_start(fmu);
    }
}


/**
fmigateway_session_end
======================

If session parameters were parsed from the model description, this method
shuts down the additional models, or executes the given command.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmigateway_session_end(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    FmiGatewaySession* session = fmi_gw->settings.session;
    if (session == NULL) return;

    if (session->shutdown.file) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s%s", session->shutdown.path,
            session->shutdown.file);
        system(buf);
    }

    if (session->w_models && strcmp(PLATFORM_OS, "windows") == 0) {
        fmigateway_session_windows_end(fmu);
    }
}


__attribute__((weak)) void fmigateway_session_windows_start(
    FmuInstanceData* fmu)
{
    UNUSED(fmu);
}


__attribute__((weak)) void fmigateway_session_windows_end(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}
