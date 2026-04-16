// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0


#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <windows.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/fmigateway/fmigateway.h>


#define MODEL_MAX_TIME 60 * 60 * 100  // 100 hours.
#define UNUSED(x)      ((void)x)
#define MAX_CMD_LENGTH 2048


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
static char* _build_cmd(WindowsModel* w_model, const char* path)
{
    char   cmd[MAX_CMD_LENGTH];
    size_t max_len = sizeof cmd;
    int    offset =
        snprintf(cmd, max_len, "cmd /C cd %s && %s", path, w_model->exe);

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
static HANDLE _create_file(char* name)
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
_gracefully_terminate_process
=============================

Gracefully terminate a windows process by sending a Ctrl C, Sigint
Signal to the process.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.
*/
static void _gracefully_terminate_process(
    FmuInstanceData* fmu, WindowsModel* w_model)
{
    WindowsProcess* w_process = w_model->w_process;
    if (w_process == NULL) return;
    if (w_model->end_time != MODEL_MAX_TIME) return;

    /* Check if process is still running */
    DWORD exitCode;
    if (!GetExitCodeProcess(w_process->p_info.hProcess, &exitCode)) {
        fmu_log(fmu, FmiLogError, "Error",
            "Failed to get exit code for process %s (error %d)", w_model->name,
            GetLastError());
        return;
    }

    if (exitCode != STILL_ACTIVE) {
        fmu_log(fmu, FmiLogOk, "Info", "Process %s already terminated",
            w_model->name);
        return;
    }

    fmu_log(fmu, FmiLogOk, "Debug", "Sending SIGINT to process %s...",
        w_model->name);

    /* Verify process is still valid before console manipulation */
    DWORD result = WaitForSingleObject(w_process->p_info.hProcess, 0);
    if (result == WAIT_OBJECT_0) {
        fmu_log(fmu, FmiLogOk, "Info",
            "Process %s terminated before sending signal", w_model->name);
        return;
    }

    /* Sequence of functions to attach to the running models and send a SIGINT
     */
    FreeConsole();
    if (!AttachConsole(w_process->p_info.dwProcessId)) {
        DWORD error = GetLastError();
        fmu_log(fmu, FmiLogError, "Error",
            "Failed to attach to console of process %s (error %d)",
            w_model->name, error);
        goto cleanup;
    }

    SetConsoleCtrlHandler(NULL, true);
    if (!GenerateConsoleCtrlEvent(
            CTRL_BREAK_EVENT, w_process->p_info.dwProcessId)) {
        fmu_log(fmu, FmiLogError, "Error",
            "Failed to send CTRL_BREAK_EVENT to process %s (error %d)",
            w_model->name, GetLastError());
    }
    FreeConsole();

    /* Wait for the process to handle the SIGINT and reattach to host. */
    Sleep(1000);

    fmu_log(fmu, FmiLogOk, "Debug", "Terminated process %s via SIGINT.",
        w_model->name);

cleanup:
    SetConsoleCtrlHandler(NULL, false);
    if (!AttachConsole(-1)) {
        /* If we can't reattach, try to allocate a new console for safety */
        AllocConsole();
    }
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
static void _start_redis(
    FmuInstanceData* fmu, WindowsModel* w_model, WindowsProcess* w_process)
{
    /* Redis Server. */
    char* file_path =
        dse_path_cat(fmu->instance.resource_location, w_model->exe);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cmd /C %s --port %s", file_path, w_model->args);

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE,
            CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, NULL, NULL,
            &(w_process->s_info), &(w_process->p_info))) {
        fmu_log(fmu, FmiLogError, "Error", "Could not start Redis-Server.exe");
    }

    free(file_path);
}


/*
_build_env
==========

Add model specific environment variables to the parent environment
and return a new environment block as string.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.

Return
------
char*
: A new environment block as a string, double-null terminated.
*/
static char* _build_env(WindowsModel* m)
{
    if (m->envar == NULL) return NULL;

    char*  parentEnv = GetEnvironmentStrings();
    char*  p = parentEnv;
    size_t parentEnvSize = 0;

    /* Calculate size of parent env block (double-null terminated). */
    while (*p) {
        size_t len = strlen(p) + 1;  // +1 for null terminator
        parentEnvSize += len;
        p += len;
    }
    parentEnvSize++;  // for the final extra null

    uint32_t env_size = 0;
    for (FmiGatewayEnvvar* e = m->envar; e && e->name; e++) {
        /* Calculate the size needed for each environment variable
           + 1 for the "=" and + 1 for the Nullterminator. */
        env_size += strlen(e->name) + 1 + strlen(e->default_value) + 1;
    }
    env_size++;  // for the final extra null

    char* envBlock = calloc(env_size, sizeof(char));
    char* ptr = envBlock;

    for (FmiGatewayEnvvar* e = m->envar; e && e->name; e++) {
        int len = snprintf(ptr, env_size, "%s=%s", e->name, e->default_value);
        ptr += len + 1;  // Move pointer past the null terminator
    }

    /* Add the final null terminator to end the block */
    *ptr = '\0';

    char* combinedEnv = calloc(parentEnvSize + env_size, sizeof(char));
    memcpy(combinedEnv, parentEnv, parentEnvSize);
    combinedEnv[parentEnvSize - 1] = '\0';
    memcpy(combinedEnv + parentEnvSize - 1, envBlock, env_size);
    free(envBlock);

    return combinedEnv;
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
static void _start_model(FmuInstanceData* fmu, WindowsModel* m)
{
    FmiGateway*     fmi_gw = fmu->data;
    WindowsProcess* w_process = m->w_process;
    char*           cmd = _build_cmd(m, fmu->instance.resource_location);
    HANDLE          _log;

    if (fmi_gw->settings.session->logging) {
        /* Create logfile for modelC models. */
        char log[PATH_MAX];
        snprintf(log, sizeof(log), "%s/%s_log.txt",
            fmi_gw->settings.session->log_location, m->name);
        _log = _create_file(log);
        if (_log) {
            w_process->s_info.hStdInput = NULL;
            w_process->s_info.hStdError = _log;
            w_process->s_info.hStdOutput = _log;
        }
    }

    /* Build Environment for this model. (NULL if no envar set) */
    char* env = _build_env(m);

    /* ModelC models. */
    if (!CreateProcess(NULL, cmd, NULL, NULL, ((_log) ? TRUE : FALSE),
            CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, env, NULL,
            &(w_process->s_info), &(w_process->p_info))) {
        fmu_log(fmu, FmiLogError, "Error", "Could not start %s (error %d)",
            m->name, GetLastError());
    }

    free(env);
    free(cmd);
}


/*
_configure_process
==================

Initialize the process handles.

Parameters
----------
w_model (WindowsModel)
: Model Descriptor containing parameter information.

visible (bool)
: Indicates whether the process window should be visible.

name (const char*)
: Name of the process, used for the window title.
*/
static void _configure_process(
    WindowsProcess* w_process, bool visible, const char* name)
{
    /* Initialize the process handles and information for
       the windows processes. */
    ZeroMemory(&(w_process->s_info), sizeof(STARTUPINFO));
    ZeroMemory(&(w_process->p_info), sizeof(PROCESS_INFORMATION));

    w_process->s_info.cb = sizeof(STARTUPINFO);
    w_process->s_info.dwFlags = (STARTF_USESTDHANDLES);

    /* Display a terminal window. */
    if (visible) {
        w_process->s_info.lpTitle = (char*)name;
    } else {
        w_process->s_info.dwFlags =
            (STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW);
        w_process->s_info.wShowWindow = SW_HIDE;
    }
}


/*
_check_alive
============

Check if a process is still running.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

w_model (WindowsModel)
: Model Descriptor containing parameter information.

Returns
-------
bool
: true if process is still running, false otherwise.
*/
static bool _check_alive(FmuInstanceData* fmu, WindowsModel* w_model)
{
    WindowsProcess* w_process = w_model->w_process;
    DWORD           exitCode;

    if (w_process == NULL) return false;

    if (!GetExitCodeProcess(w_process->p_info.hProcess, &exitCode)) {
        fmu_log(fmu, FmiLogError, "Error",
            "Failed to get exit code for process %s (error %d)", w_model->name,
            GetLastError());
        return false;
    }

    return (exitCode == STILL_ACTIVE);
}


/*
_check_shutdown
===============

Observe a process and check if it is terminated.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

w_model (WindowsModel)
: Model Descriptor containing parameter information.

sec (integer)
: Time in seconds.
*/
static int32_t _check_shutdown(
    FmuInstanceData* fmu, WindowsModel* w_model, int sec)
{
    WindowsProcess* w_process = w_model->w_process;

    if (w_process == NULL) return 0;

    DWORD result = WaitForSingleObject(w_process->p_info.hProcess, sec * 1000);

    if (result == WAIT_OBJECT_0) {
        fmu_log(fmu, FmiLogOk, "Info", "%s is shut down.", w_model->name);
    } else {
        fmu_log(
            fmu, FmiLogError, "Error", "%s is still active.", w_model->name);
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
void fmigateway_start_models(FmuInstanceData* fmu)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    /* Transport process. */
    if (session->transport) {
        fmu_log(fmu, FmiLogOk, "Debug", "Starting process: %s",
            session->transport->name);
        session->transport->w_process = calloc(1, sizeof(WindowsProcess));
        _configure_process(session->transport->w_process,
            session->visibility.transport, session->transport->name);
        _start_redis(fmu, session->transport, session->transport->w_process);
    }

    /* Simbus process. */
    if (session->simbus) {
        fmu_log(fmu, FmiLogOk, "Debug", "Starting process: %s",
            session->simbus->name);
        session->simbus->w_process = calloc(1, sizeof(WindowsProcess));
        _configure_process(session->simbus->w_process,
            session->visibility.simbus, session->simbus->name);
        _start_model(fmu, session->simbus);
    }

    for (WindowsModel* m = session->w_models; m && m->name; m++) {
        m->w_process = calloc(1, sizeof(WindowsProcess));
        fmu_log(fmu, FmiLogOk, "Debug", "Starting process: %s", m->name);
        _configure_process(m->w_process, session->visibility.models, m->name);
        _start_model(fmu, m);
    }
}


/**
fmigateway_sync_extra_step
==========================

Performs an extra step to shutdown models.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
 */
static void _sync_extra_step(FmuInstanceData* fmu)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;
    ModelGatewayDesc*  gw = fmi_gw->model;

    if (fmi_gw->state < FMIGATEWAY_STATE_INITIALIZED) return;

    double step_size =
        session->simbus ? session->simbus->step_size : MODEL_DEFAULT_STEP_SIZE;
    fmu_log(fmu, FmiLogOk, "Debug",
        "Performing extra step to shutdown models (%f)...",
        session->last_step + (step_size * 1.001));

    model_gw_sync(gw, session->last_step + (step_size * 1.001));
    fmu_log(fmu, FmiLogOk, "Debug",
        "Extra step for shutting down models finished...");
}


/**
fmigateway_shutdown_models
==========================

Terminates all previously started windows processes.
After sending the termination signals, one additional
step is made by the gateway to close the simulation.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
 */
static void _shutdown_models(FmuInstanceData* fmu)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;

    /* Return if there are no models to handle. */
    if (session->w_models == NULL) return;

    /* If not a single step was taken, perform one for proper shutdown. */
    if (fmi_gw->state < FMIGATEWAY_STATE_RUNNING) {
        model_gw_sync(fmi_gw->model, session->last_step);
    }

    for (WindowsModel* m = session->w_models; m && m->name; m++) {
        _gracefully_terminate_process(fmu, m);
    }

    bool extra_step = true;
    /* Check if any model is still alive. */
    for (WindowsModel* m = session->w_models; m && m->name; m++) {
        if (_check_alive(fmu, m)) continue;
        extra_step = false;
        fmu_log(fmu, FmiLogOk, "Debug",
            "Model %s shut down without extra step.", m->name);
    }

    if (extra_step) {
        _sync_extra_step(fmu);
    }
}

/**
fmigateway_shutdown_models
==========================

Terminates all previously started windows processes.
After sending the termination signals, one additional
step is made by the gateway to close the simulation.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
 */
void fmigateway_shutdown_models(FmuInstanceData* fmu)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;
    ModelGatewayDesc*  gw = fmi_gw->model;

    _shutdown_models(fmu);

    if (fmi_gw->state >= FMIGATEWAY_STATE_INITIALIZED) {
        model_gw_exit(gw);
        fmu_log(fmu, FmiLogOk, "Debug", "Gateway exited...");
    }

    /* Loop through processes and confirm that all are closed. */
    for (WindowsModel* m = session->w_models; m && m->name; m++) {
        _check_shutdown(fmu, m, 10);
    }

    if (session->simbus) {
        if (_check_shutdown(fmu, session->simbus, 10) < 0) {
            _gracefully_terminate_process(fmu, session->simbus);
        }
    }

    if (session->transport) {
        _gracefully_terminate_process(fmu, session->transport);
    }
}


/**
fmigateway_setenv
=================

Set an environment variable.

Parameters
----------
name (const char*)
: The name of the environment variable.
value (const char*)
: The value to set for the environment variable.

Returns
-------
int
: Non-zero on success, zero on failure.
*/
int fmigateway_setenv(const char* name, const char* value)
{
    return SetEnvironmentVariable(name, value);
}


/*
fmigateway_run_parallelisation
==============================

Run a PowerShell script and return its first line of stdout as a
heap-allocated string. Caller must free() the returned string.

Parameters
----------
script_path (const char*)
: Absolute path to the .ps1 script to execute.

Returns
-------
char*
: Heap-allocated string with the script output (trailing CRLF stripped).
  Returns NULL if the script could not be executed or produced no output.
*/
static char* fmigateway_run_parallelisation(
    FmuInstanceData* fmu, const char* script_path)
{
    /* Use a unique temp file per call to avoid handle inheritance issues
       when multiple FMU instances call this concurrently. */
    char tmp_dir[MAX_PATH];
    char tmp_file[MAX_PATH];
    char output[MAX_PATH];

    if (!GetTempPathA(MAX_PATH, tmp_dir) ||
        !GetTempFileNameA(tmp_dir, "fgw", 0, tmp_file)) {
        fmu_log(fmu, FmiLogError, "Error",
            "Failed to create temp file for script output (error %d)",
            GetLastError());
        return NULL;
    }

    char cmd[MAX_CMD_LENGTH];
    /* -NonInteractive suppresses prompts; -File runs the script directly
       so Write-Output goes to stdout which is redirected to tmp_file. */
    snprintf(cmd, sizeof cmd,
        "powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass"
        " -File \"%s\" > \"%s\"",
        script_path, tmp_file);

    if (fmigateway_run_cmd(fmu, cmd) != 0) {
        DeleteFileA(tmp_file);
        return NULL;
    }

    FILE* f = fopen(tmp_file, "r");

    if (f) {
        if (fgets(output, sizeof output, f)) {
            output[strcspn(output, "\r\n")] = '\0';
        }
        fclose(f);
    } else {
        fmu_log(fmu, FmiLogError, "Error",
            "Failed to read script output from temp file (error %d)",
            GetLastError());
    }

    DeleteFileA(tmp_file);

    return (*output) ? strdup(output) : NULL;
}

/*
fmigateway_parallelize
======================

Run the parallelisation.ps1 script (if present) and capture its output.
The output is used to update the resource location of the FMU instance.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

*/
void fmigateway_parallelize(FmuInstanceData* fmu)
{
    /* Run parallelisation.ps1 (if present) and capture its output. */
    char* ps1_path =
        dse_path_cat(fmu->instance.resource_location, "parallelisation.ps1");
    char* script_output = NULL;

    if (access(ps1_path, F_OK) == 0) {
        script_output = fmigateway_run_parallelisation(fmu, ps1_path);
        if (script_output) {
            fmu_log(fmu, 0, "Debug", "parallelisation.ps1 returned: %s",
                script_output);
            fmu->instance.resource_location = script_output;
            fmu_log(fmu, 0, "Debug", "Using new resource location: %s",
                fmu->instance.resource_location);
        }
    }

    free(ps1_path);
}


/*
fmigateway_run_cmd
=================

Run a command using CreateProcess and wait for its completion.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
cmd_string (const char*)
: The command to execute.

Returns
-------
int
: 0 on success, non-zero on failure.
*/
int fmigateway_run_cmd(FmuInstanceData* fmu, const char* cmd_string)
{
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    /* Wrap in cmd /C to support shell features (pipes, &&, batch scripts).
       The working directory is set via lpCurrentDirectory, not via 'cd'. */
    char cmd[MAX_CMD_LENGTH + 8];
    int  n = snprintf(cmd, sizeof(cmd), "cmd /C %s", cmd_string);
    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        fmu_log(
            fmu, FmiLogError, "Error", "Command string too long to execute.");
        return EINVAL;
    }

    fmu_log(fmu, FmiLogOk, "Debug", "Run cmd: %s (cwd: %s)", cmd_string,
        fmu->instance.resource_location);

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL,
            fmu->instance.resource_location, &si, &pi)) {
        fmu_log(fmu, FmiLogError, "Error",
            "Could not execute cmd '%s' (error %d)", cmd_string,
            GetLastError());
        return EINVAL;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        fmu_log(fmu, FmiLogError, "Error", "Cmd '%s' exited with code %d.",
            cmd_string, (int)exitCode);
        return ECANCELED;
    }

    return 0;
}
