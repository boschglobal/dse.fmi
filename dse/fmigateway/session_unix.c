// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>


char* fmigateway_file_exists(FmuInstanceData* fmu, const char* name)
{
    static const char* const extensions[] = { "sh", NULL };
    char                     path[PATH_MAX];
    for (const char* const* ext = extensions; *ext; ext++) {
        snprintf(path, sizeof(path), "%s/../%s.%s",
            fmu->instance.resource_location, name, *ext);
        if (access(path, F_OK) == 0) return strdup(path);
    }
    return NULL;
}


int fmigateway_setenv(const char* name, const char* value)
{
    if (value == NULL) return unsetenv(name);

    return setenv(name, value, true);
}


void fmigateway_start_models(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}


void fmigateway_shutdown_models(FmuInstanceData* fmu)
{
    FmiGateway*       fmi_gw = fmu->data;
    ModelGatewayDesc* gw = fmi_gw->model;
    model_gw_exit(gw);
    fmu_log(fmu, 0, "Debug", "Gateway exited...");
}


void fmigateway_parallelize(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}


void fmigateway_teardown(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}


int fmigateway_run_cmd(FmuInstanceData* fmu, const char* cmd_string)
{
    fmu_log(fmu, 0, "Debug", "Run cmd: %s (cwd: %s)", cmd_string,
        fmu->instance.resource_location);

    pid_t pid = fork();
    if (pid < 0) {
        fmu_log(
            fmu, 3, "Error", "Could not fork to execute cmd '%s'", cmd_string);
        return EINVAL;
    }
    if (pid == 0) {
        /* Child: change to resource directory, then exec. */
        if (chdir(fmu->instance.resource_location) != 0) {
            _exit(127);
        }
        execl("/bin/sh", "sh", "-c", cmd_string, (char*)NULL);
        _exit(127);
    }
    /* Parent: wait for child. */
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fmu_log(fmu, 3, "Error", "waitpid failed for cmd '%s'", cmd_string);
        return EINVAL;
    }
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (exitCode != 0) {
        fmu_log(fmu, 3, "Error", "Cmd '%s' exited with code %d.", cmd_string,
            exitCode);
        return ECANCELED;
    }
    return 0;
}


void fmigateway_run_simer(FmuInstanceData* fmu)
{
    UNUSED(fmu);  // No-op for now.
    return;
}


void fmigateway_stop_simer(FmuInstanceData* fmu)
{
    UNUSED(fmu);  // No-op for now.
    return;
}
