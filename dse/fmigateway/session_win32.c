// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0


#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <windows.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/fmigateway/fmigateway.h>


#define MODEL_MAX_TIME 60 * 60 * 10  // 10 minutes


typedef struct WindowsProcess {
    STARTUPINFO         s_info;
    PROCESS_INFORMATION p_info;
} WindowsProcess;


/*
_build_cmd
==========

Build the command for the modelC process from the yaml parameters.
Endtime and stepsize are optional.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.

Returns
-------
string
: string containing the cmd to start a windows model
*/
static inline char* _build_cmd(WindowsModel* w_model, const char* path)
{
    char   cmd[1024];
    size_t max_len = sizeof cmd;
    int    offset =
        snprintf(cmd, max_len, "cmd /C cd %s && %s", path, w_model->file);
    offset += snprintf(cmd + offset, max_len, " --name %s", w_model->name);
    offset +=
        snprintf(cmd + offset, max_len, " --endtime %lf", w_model->end_time);
    offset +=
        snprintf(cmd + offset, max_len, " --stepsize %lf", w_model->step_size);
    offset +=
        snprintf(cmd + offset, max_len, " --logger %d", w_model->log_level);
    offset +=
        snprintf(cmd + offset, max_len, " --timeout %lf", w_model->timeout);
    if (w_model->yaml) {
        offset += snprintf(cmd + offset, max_len, " %s", w_model->yaml);
    }
    return strdup(cmd);
}


/*
_create_file
============

Creates a file on a windows operating system.

Parameters
----------
name (char*)
: name of the file to be created.

Returns
-------
HANDLE
: open handle to the specified file
*/
static inline HANDLE _create_file(char* name)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    return CreateFile(name, FILE_WRITE_DATA, FILE_SHARE_WRITE | FILE_SHARE_READ,
        &sa,                    // default security
        CREATE_ALWAYS,          // create new file always
        FILE_ATTRIBUTE_NORMAL,  // normal file
        NULL                    // attribute template
    );
}


/*
_terminate_process
==================

Terminate a windows process.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.
*/
static inline void _terminate_process(WindowsModel* w_model)
{
    WindowsProcess* w_process = w_model->w_process;

    TerminateProcess(w_process->p_info.hProcess, 0);
    CloseHandle(w_process->p_info.hProcess);
    CloseHandle(w_process->p_info.hThread);
    free(w_process);
}


/*
_gracefully_termiante_process
=============================

Gracefully terminate a windows process by sending a Ctrl C, Sigint
Signal to the process.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.
*/
static inline void _gracefully_termiante_process(WindowsModel* w_model)
{
    WindowsProcess* w_process = w_model->w_process;
    if (w_model->end_time != MODEL_MAX_TIME) return;

    /* Sequence of functions to attach to the running models and sent
       a SIGINT */
    FreeConsole();
    AttachConsole(w_process->p_info.dwProcessId);
    SetConsoleCtrlHandler(NULL, true);
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, w_process->p_info.dwProcessId);
    FreeConsole();

    /* wait for the processes to process the SIGINT and reattach to
       host. */
    Sleep(1000);
    SetConsoleCtrlHandler(NULL, false);
    AttachConsole(-1);
}


/*
_start_redis
============

Create and start a new redis process.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.

w_process (WindowsProcess)
: Process Descriptor, references various data.

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
static inline void _start_redis(
    WindowsModel* w_model, WindowsProcess* w_process, FmuInstanceData* fmu)
{
    /* Redis Server. */
    char* file_path =
        dse_path_cat(fmu->instance.resource_location, w_model->file);

    if (!CreateProcess(file_path, NULL, NULL, NULL, FALSE,
            CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, NULL, NULL,
            &(w_process->s_info), &(w_process->p_info))) {
        log_fatal("Could not start Redis-Server.exe");
    }
    free(file_path);
}


/*
_start_model
============

Create and start a new modelC process.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.

w_process (WindowsProcess)
: Process Descriptor, references various data.

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
static inline void _start_model(
    WindowsModel* w_model, WindowsProcess* w_process, FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;
    char*       cmd = _build_cmd(w_model, fmu->instance.resource_location);

    HANDLE _log;
    if (w_model->log_level >= 0) {
        /* Create logfile for modelC models. */
        char log[128];
        snprintf(log, sizeof(log), "%s/%s_log.txt",
            fmi_gw->settings.log_location, w_model->name);
        _log = _create_file(log);
        if (_log) {
            w_process->s_info.hStdInput = NULL;
            w_process->s_info.hStdError = _log;
            w_process->s_info.hStdOutput = _log;
        }
    }

    /* ModelC models. */
    if (!CreateProcess(NULL, cmd, NULL, NULL, ((_log) ? TRUE : FALSE),
            CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, NULL, NULL,
            &(w_process->s_info), &(w_process->p_info))) {
        log_fatal(
            "Could not start %s (error %d)", w_model->name, GetLastError());
    }
    free(cmd);
}


/*
_start_process
==============

Start a modelC windows process based on information from
a yaml parameter configuration.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
static inline void _start_process(WindowsModel* w_model, FmuInstanceData* fmu)
{
    WindowsProcess* w_process = w_model->w_process;

    log_notice("Starting process: %s", w_model->name);

    /* Initialize the process handles and information for
       the windows processes. */
    ZeroMemory(&(w_process->s_info), sizeof(STARTUPINFO));
    ZeroMemory(&(w_process->p_info), sizeof(PROCESS_INFORMATION));

    w_process->s_info.cb = sizeof(STARTUPINFO);
    w_process->s_info.dwFlags = (STARTF_USESTDHANDLES);

    /* Display a terminal window. */
    if (w_model->show_process) {
        w_process->s_info.lpTitle = (char*)w_model->name;
    } else {
        w_process->s_info.dwFlags =
            (STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW);
        w_process->s_info.wShowWindow = SW_HIDE;
    }

    /* Redis requires a different startup. */
    if (strcasecmp(w_model->name, "redis") == 0) {
        _start_redis(w_model, w_process, fmu);
        return;
    }
    _start_model(w_model, w_process, fmu);
}


/*
_check_shutdown
===============

Observe a process and check if it is terminated.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.

sec (integer)
: Time in seconds.
*/
inline static int32_t _check_shutdown(WindowsModel* w_model, int sec)
{
    WindowsProcess* w_process = w_model->w_process;
    DWORD result = WaitForSingleObject(w_process->p_info.hProcess, sec * 1000);
    if (result == WAIT_OBJECT_0) {
        log_notice("%s is shut down.", w_model->name);
    } else {
        log_error("%s is still active.", w_model->name);
        return -1;
    }
    /* Close the model handle. */
    CloseHandle(w_process->p_info.hProcess);
    CloseHandle(w_process->p_info.hThread);
    free(w_model->w_process);
    return 0;
}


/**
fmigateway_session_windows_start
================================

Creates windows processes based on the parameters
configured in a yaml file. Process informations are
stored for later termination.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
 */
void fmigateway_session_windows_start(FmuInstanceData* fmu)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    for (WindowsModel* w_m = session->w_models; w_m && w_m->name; w_m++) {
        w_m->w_process = calloc(1, sizeof(WindowsProcess));
        _start_process(w_m, fmu);
    }
}


/**
fmigateway_session_windows_end
==============================

Termiantes all previously started windows processes.
After sending the termination signals, one additionally
step is made by the gateway to close the simulation.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
 */
void fmigateway_session_windows_end(FmuInstanceData* fmu)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;
    ModelGatewayDesc*  gw = fmi_gw->model;

    WindowsModel* redis = NULL;
    WindowsModel* simbus = NULL;

    for (WindowsModel* w_m = session->w_models; w_m && w_m->name; w_m++) {
        if (strcasecmp(w_m->name, "simbus") == 0) {
            simbus = w_m;
            continue;
        } else if (strcasecmp(w_m->name, "redis") == 0) {
            redis = w_m;
            continue;
        }
        _gracefully_termiante_process(w_m);
    }

    model_gw_sync(
        gw, session->last_step + (fmi_gw->settings.step_size * 1.001));
    fmu_log(fmu, 0, "Debug", "Extra step for shutting down models finished...");

    model_gw_exit(gw);
    fmu_log(fmu, 0, "Debug", "Gateway exited...");

    /* Loop through processes and confirm that all are closed. */
    for (WindowsModel* w_m = session->w_models; w_m && w_m->name; w_m++) {
        if (strcasecmp(w_m->name, "redis") == 0 ||
            strcasecmp(w_m->name, "simbus") == 0) {
            continue;
        }
        _check_shutdown(w_m, 10);
    }

    if (simbus) {
        if (_check_shutdown(simbus, 10) < 0) {
            _gracefully_termiante_process(simbus);
        }
    }
    if (redis) _terminate_process(redis);
}
