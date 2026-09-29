// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0


#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <windows.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x)      ((void)x)
#define MAX_CMD_LENGTH 2048


typedef struct WindowsProcess {
    STARTUPINFO         s_info;
    PROCESS_INFORMATION p_info;
} WindowsProcess;


/**
_build_cmd
==========

Builds the command line used to start a modelC process from the model
configuration and resource path.

Parameters
----------
w_model (WindowsModel*)
: Model descriptor containing parameter information.
path (const char*)
: Working directory for the modelC process.

Returns
-------
char*
: Heap-allocated command string. The caller must free the returned string.
*/
static char* _build_cmd(WindowsModel* w_model, const char* path)
{
    char   cmd[MAX_CMD_LENGTH];
    size_t max_len = sizeof cmd;
    int    offset =
        snprintf(cmd, max_len, "cmd /C cd %s && %s", path, w_model->exe);

    offset +=
        snprintf(cmd + offset, max_len - offset, " --name %s", w_model->name);
    offset += snprintf(
        cmd + offset, max_len - offset, " --endtime %lf", w_model->end_time);
    offset += snprintf(
        cmd + offset, max_len - offset, " --stepsize %lf", w_model->step_size);
    offset += snprintf(
        cmd + offset, max_len - offset, " --logger %d", w_model->log_level);
    offset += snprintf(
        cmd + offset, max_len - offset, " --timeout %lf", w_model->timeout);

    if (w_model->yaml) {
        offset +=
            snprintf(cmd + offset, max_len - offset, " %s", w_model->yaml);
    }

    return strdup(cmd);
}


/**
fmigateway_file_exists
======================

Searches the FMU resource directory for a batch or PowerShell script with
the supplied base name.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU descriptor containing the resource directory.
name (const char*)
: The script base name without its extension.

Returns
-------
char*
: Heap-allocated path to the first existing script, or NULL if none exists.
The caller must free the returned path.
*/
char* fmigateway_file_exists(FmuInstanceData* fmu, const char* name)
{
    static const char* const extensions[] = { "bat", "ps1", NULL };
    char                     path[PATH_MAX];
    for (const char* const* ext = extensions; *ext; ext++) {
        snprintf(path, sizeof(path), "%s\\%s.%s",
            fmu->instance.resource_location, name, *ext);
        DWORD attr = GetFileAttributesA(path);
        if (attr != INVALID_FILE_ATTRIBUTES &&
            !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return strdup(path);
        }
    }
    return NULL;
}


/**
_create_file
===========

Creates or replaces a writable inheritable file handle.

Parameters
----------
name (char*)
: Path of the file to create.

Returns
-------
HANDLE
: The created Windows file handle, or INVALID_HANDLE_VALUE on failure.
*/
static HANDLE _create_file(char* name)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    return CreateFile(name, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
        &sa,                    // default security
        CREATE_ALWAYS,          // create new file always
        FILE_ATTRIBUTE_NORMAL,  // normal file
        NULL                    // attribute template
    );
}


/**
_gracefully_terminate_process
=============================

Gracefully terminates a Windows process by attaching to its console and
sending a CTRL_BREAK_EVENT signal.

Parameters
----------
w_model (WindowsModel*)
: Model descriptor containing parameter information.
*/
static void _gracefully_terminate_process(
    FmuInstanceData* fmu, WindowsModel* w_model)
{
    WindowsProcess* w_process = w_model->w_process;
    if (w_process == NULL) return;

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
    if (WaitForSingleObject(w_process->p_info.hProcess, 0) == WAIT_OBJECT_0) {
        fmu_log(fmu, FmiLogOk, "Info",
            "Process %s terminated before sending signal", w_model->name);
        return;
    }

    /* Attach to the target's console group and raise CTRL_BREAK_EVENT. */
    FreeConsole();
    if (!AttachConsole(w_process->p_info.dwProcessId)) {
        fmu_log(fmu, FmiLogError, "Error",
            "Failed to attach to console of process %s (error %d)",
            w_model->name, GetLastError());
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

    /* Wait for the process to handle the signal. */
    Sleep(1000);

    fmu_log(fmu, FmiLogOk, "Debug", "Terminated process %s via SIGINT.",
        w_model->name);

cleanup:
    SetConsoleCtrlHandler(NULL, false);
    /* Reattach to the parent console if there is one. If the parent
       (e.g. MATLAB) has no console, AttachConsole(-1) returns FALSE and
       we MUST NOT call AllocConsole(): doing so would create a brand-new
       console owned by this process which is then inherited by every
       subsequent child (init_cmd / shutdown_cmd), producing the spurious
       "MATLAB-attached" terminal window. */
    AttachConsole(ATTACH_PARENT_PROCESS);
}


/**
_start_redis
============

Creates and starts a Redis process.

Parameters
----------
w_model (WindowsModel*)
: Model Descriptor containing parameter information.

w_process (WindowsProcess*)
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


/**
_build_env
==========

Adds model-specific environment variables to the parent environment and
returns a new double-null-terminated environment block.

Parameters
----------
m (WindowsModel*)
: Model descriptor containing parameter information.

Returns
-------
char*
: A heap-allocated environment block, double-null terminated, or NULL when
    no model environment variables are configured. The caller must free it.
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
    for (FmiGatewayParameter* e = m->envar; e && e->name; e++) {
        /* Calculate the size needed for each environment variable
           + 1 for the "=" and + 1 for the Nullterminator. */
        env_size += strlen(e->name) + 1 + strlen(e->default_value) + 1;
    }
    env_size++;  // for the final extra null

    char* envBlock = calloc(env_size, sizeof(char));
    char* ptr = envBlock;

    for (FmiGatewayParameter* e = m->envar; e && e->name; e++) {
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
    FreeEnvironmentStrings(parentEnv);

    return combinedEnv;
}


/**
_start_model
============

Creates and starts a modelC process, optionally redirecting its standard
output and error streams to a log file.

Parameters
----------
m (WindowsModel*)
: Model descriptor containing parameter information.

w_process (WindowsProcess*)
: Process Descriptor, references various data.

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
static void _start_model(FmuInstanceData* fmu, WindowsModel* m)
{
    FmiGateway*     fmi_gw = fmu->data;
    WindowsProcess* w_process = m->w_process;
    char*           cmd = _build_cmd(m, fmu->instance.resource_location);
    HANDLE          _log = INVALID_HANDLE_VALUE;

    if (fmi_gw->settings.session->logging &&
        fmi_gw->settings.runtime.log_location &&
        strlen(fmi_gw->settings.runtime.log_location) > 0) {
        /* Create logfile for modelC models. */
        char log[PATH_MAX];
        snprintf(log, sizeof(log), "%s/%s_log.txt",
            fmi_gw->settings.runtime.log_location, m->name);
        _log = _create_file(log);
        if (_log != INVALID_HANDLE_VALUE) {
            w_process->s_info.hStdInput = NULL;
            w_process->s_info.hStdError = _log;
            w_process->s_info.hStdOutput = _log;
        }
    }

    /* Build Environment for this model. (NULL if no envar set) */
    char* env = _build_env(m);

    /* ModelC models. */
    if (!CreateProcess(NULL, cmd, NULL, NULL,
            ((_log != INVALID_HANDLE_VALUE) ? TRUE : FALSE),
            CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, env, NULL,
            &(w_process->s_info), &(w_process->p_info))) {
        fmu_log(fmu, FmiLogError, "Error", "Could not start %s (error %d)",
            m->name, GetLastError());
    }

    free(env);
    free(cmd);
}


/**
_configure_process
==================

Initializes the Windows process startup and process information structures.

Parameters
----------
w_process (WindowsProcess*)
: Process descriptor to initialize.

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


/**
_check_alive
============

Checks whether a Windows process is still running.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

w_model (WindowsModel*)
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


/**
_check_shutdown
===============

Waits for a process to terminate, closes its handles, and releases its
process descriptor.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

w_model (WindowsModel*)
: Model Descriptor containing parameter information.

sec (int)
: Maximum wait time in seconds.

Returns
-------
int32_t
: 0 if the process terminated, or -1 if it remained active.
*/
static int32_t _check_shutdown(
    FmuInstanceData* fmu, WindowsModel* w_model, int sec)
{
    WindowsProcess* w_process = w_model->w_process;

    if (w_process == NULL) return 0;

    DWORD result = WaitForSingleObject(w_process->p_info.hProcess, sec * 1000);

    if (result == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        GetExitCodeProcess(w_process->p_info.hProcess, &exitCode);
        fmu_log(fmu, FmiLogOk, "Info", "%s is shut down (exit code: %lu).",
            w_model->name, exitCode);
    } else {
        fmu_log(
            fmu, FmiLogError, "Error", "%s is still active.", w_model->name);
        return -1;
    }

    /* Close the model handle. */
    CloseHandle(w_process->p_info.hProcess);
    CloseHandle(w_process->p_info.hThread);
    free(w_model->w_process);
    w_model->w_process = NULL;
    return 0;
}


/**
fmigateway_start_models
=======================

Creates Windows processes based on the configured session models and stores
their process information for later termination.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
 */
void fmigateway_start_models(FmuInstanceData* fmu)
{
    FmiGateway*        fmi_gw = fmu->data;
    FmiGatewaySession* session = fmi_gw->settings.session;
    if (session == NULL) return;

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
_sync_extra_step
===============

Performs one additional gateway synchronization step so models can shut down
after receiving their termination signal.

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
_shutdown_models
================

Signals all configured model processes to terminate and performs an extra
gateway step when the models have not shut down yet.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU descriptor containing the gateway session and model state.
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

Terminates all previously started Windows processes, including the transport
and SIMER processes. An additional gateway step may be made for model
shutdown before process handles are closed.

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

    if (session == NULL) return;

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

Sets an environment variable for the current process.

Parameters
----------
name (const char*)
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


/**
fmigateway_run_parallelisation
==============================

Runs a PowerShell or batch script and returns its first line of standard
output as a heap-allocated string. The caller must free the returned string.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU descriptor used for logging and command execution.
script_path (const char*)
: Absolute path to the script to execute.

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
    char tmp_dir[MAX_PATH] = { 0 };
    char tmp_file[MAX_PATH] = { 0 };
    char output[MAX_PATH] = { 0 };

    if (!GetTempPathA(MAX_PATH, tmp_dir) ||
        !GetTempFileNameA(tmp_dir, "fgw", 0, tmp_file)) {
        fmu_log(fmu, FmiLogError, "Error",
            "Failed to create temp file for script output (error %d)",
            GetLastError());
        return NULL;
    }

    char        cmd[MAX_CMD_LENGTH] = { 0 };
    const char* ext = strrchr(script_path, '.');
    if (ext && _stricmp(ext, ".ps1") == 0) {
        /* PowerShell: -File runs the script, stdout redirected to tmp_file. */
        snprintf(cmd, sizeof cmd,
            "powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass"
            " -File \"%s\" > \"%s\"",
            script_path, tmp_file);
    } else {
        /* Batch file: run directly via cmd, redirect stdout to tmp_file. */
        snprintf(cmd, sizeof cmd, "\"%s\" > \"%s\"", script_path, tmp_file);
    }

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


/**
fmigateway_parallelize
======================

Runs the parallelisation batch or PowerShell script, if present, and uses its
output to update the FMU instance resource location.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

*/
void fmigateway_parallelize(FmuInstanceData* fmu)
{
    char* script_path = fmigateway_file_exists(fmu, "parallelisation");
    if (script_path == NULL) {
        fmu_log(fmu, 0, "Debug", "No parallelisation script found.");
        return;
    }

    char* script_output = fmigateway_run_parallelisation(fmu, script_path);
    if (script_output) {
        fmu_log(fmu, 0, "Debug", "parallelisation script returned: %s",
            script_output);
        fmu->instance.resource_location = script_output;
        fmu_log(fmu, 0, "Debug", "Using new resource location: %s",
            fmu->instance.resource_location);
    }

    free(script_path);
}


/**
fmigateway_teardown
===================

Runs the optional teardown batch script from the FMU resource directory.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU descriptor containing the resource directory.
*/
void fmigateway_teardown(FmuInstanceData* fmu)
{
    char* path = dse_path_cat(fmu->instance.resource_location, "teardown.bat");
    if (access(path, F_OK) == 0) {
        int rc = fmigateway_run_cmd(fmu, path);
        if (rc) {
            fmu_log(fmu, 0, "Debug", "teardown.bat failed: %d", rc);
        }
    }
}


/**
fmigateway_run_cmd
=================

Runs a command using CreateProcess and waits for it to complete.

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
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

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

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL,
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


/**
fmigateway_run_simer
====================

Starts the SIMER process using the selected runtime command and stores its
process descriptor in the gateway runtime settings.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU descriptor containing the gateway runtime configuration.
*/
void fmigateway_run_simer(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    // NOLINTNEXTLINE(build/include_what_you_use)
    const char* simer_cmd = fmu_variable_find(&fmu->variables.string.input, 0);
    if (simer_cmd == NULL || strlen(simer_cmd) == 0) {
        double* simer_cmd_sel =
            fmu_variable_find(&fmu->variables.scalar.input, 1);
        size_t idx = (simer_cmd_sel && *simer_cmd_sel >= 0)
                         ? (size_t)(*simer_cmd_sel)
                         : 0;
        if (idx < vector_len(&fmi_gw->settings.runtime.cmds)) {
            simer_cmd =
                *(char**)vector_at(&fmi_gw->settings.runtime.cmds, idx, NULL);
        }
    }

    WindowsProcess* w_process = calloc(1, sizeof(WindowsProcess));
    _configure_process(w_process, true, "SIMER");

    /* Create logfile for SIMER (only if log_location is configured). */
    HANDLE _log = INVALID_HANDLE_VALUE;
    char   log[PATH_MAX];
    if (fmi_gw->settings.runtime.log_location &&
        strlen(fmi_gw->settings.runtime.log_location) > 0) {
        snprintf(log, sizeof(log), "%s/SIMER_log.txt",
            fmi_gw->settings.runtime.log_location);
        _log = _create_file(log);
        if (_log == INVALID_HANDLE_VALUE) {
            fmu_log(fmu, FmiLogError, "Error",
                "Could not create SIMER log file (error %d)", GetLastError());
        } else {
            w_process->s_info.hStdInput = NULL;
            w_process->s_info.hStdError = _log;
            w_process->s_info.hStdOutput = _log;
        }
    }

    char work_dir[PATH_MAX];
    snprintf(
        work_dir, sizeof(work_dir), "%s/sim", fmu->instance.resource_location);

    /* Build an absolute path for the exe so CreateProcess can find it,
       while keeping work_dir as lpCurrentDirectory so SIMER runs from there.
       lpApplicationName resolves the exe; lpCommandLine is passed as-is
       so SIMER receives the original argv. p_info refers directly to SIMER. */
    char exe_path[PATH_MAX];
    snprintf(exe_path, sizeof(exe_path), "%s\\bin\\simer.exe", work_dir);

    /* CreateProcess requires a mutable command-line buffer.
       argv[0] is the exe name (lpApplicationName handles the actual lookup). */
    char cmd[MAX_CMD_LENGTH];
    snprintf(cmd, sizeof(cmd), "simer.exe %s", simer_cmd);

    fmu_log(fmu, FmiLogOk, "Debug", "Starting SIMER: %s ", exe_path);
    fmu_log(fmu, FmiLogOk, "Debug", "  SIMER cmd: %s", cmd);
    fmu_log(fmu, FmiLogOk, "Debug", "  SIMER work dir: %s", work_dir);
    if (_log != INVALID_HANDLE_VALUE) {
        fmu_log(fmu, FmiLogOk, "Debug", "  SIMER log file: %s", log);
    }

    if (!CreateProcess(exe_path, cmd, NULL, NULL,
            ((_log != INVALID_HANDLE_VALUE) ? TRUE : FALSE),
            CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, NULL, work_dir,
            &(w_process->s_info), &(w_process->p_info))) {
        fmu_log(fmu, FmiLogError, "Error", "Could not start SIMER (error %d)",
            GetLastError());
        free(w_process);
        return;
    }

    fmi_gw->settings.runtime.simer_process = w_process;
    fmu_log(fmu, FmiLogOk, "Debug", "SIMER started (PID %lu)",
        w_process->p_info.dwProcessId);
}


/**
fmigateway_stop_simer
=====================

Stops the SIMER process by sending CTRL_BREAK_EVENT, falling back to forced
termination when the process console cannot be attached, and releases its
process handles.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU descriptor containing the SIMER process.
*/
void fmigateway_stop_simer(FmuInstanceData* fmu)
{
    FmiGateway*     fmi_gw = fmu->data;
    WindowsProcess* w_process = fmi_gw->settings.runtime.simer_process;

    if (w_process == NULL) return;

    /* Check if already exited. */
    DWORD exitCode;
    if (GetExitCodeProcess(w_process->p_info.hProcess, &exitCode) &&
        exitCode != STILL_ACTIVE) {
        fmu_log(fmu, FmiLogOk, "Info", "SIMER already exited.");
        goto cleanup;
    }

    /* Send CTRL_BREAK_EVENT (SIGINT equivalent) to the process group. */
    fmu_log(fmu, FmiLogOk, "Debug", "Sending SIGINT to SIMER (PID %lu)...",
        w_process->p_info.dwProcessId);

    FreeConsole();
    if (AttachConsole(w_process->p_info.dwProcessId)) {
        SetConsoleCtrlHandler(NULL, TRUE);
        if (!GenerateConsoleCtrlEvent(
                CTRL_BREAK_EVENT, w_process->p_info.dwProcessId)) {
            fmu_log(fmu, FmiLogError, "Error",
                "Failed to send CTRL_BREAK_EVENT to SIMER (error %d)",
                GetLastError());
        }
        FreeConsole();
        Sleep(1000);
    } else {
        /* Fallback: forcefully terminate if we cannot attach. */
        fmu_log(fmu, FmiLogError, "Error",
            "Could not attach to SIMER console (error %d), terminating.",
            GetLastError());
        TerminateProcess(w_process->p_info.hProcess, 1);
    }

    SetConsoleCtrlHandler(NULL, FALSE);
    if (!AttachConsole((DWORD)-1)) AllocConsole();

    /* Wait up to 10 s for graceful exit, then force-kill. */
    if (WaitForSingleObject(w_process->p_info.hProcess, 10000) !=
        WAIT_OBJECT_0) {
        fmu_log(fmu, FmiLogError, "Error",
            "SIMER did not exit in time, terminating forcefully.");
        TerminateProcess(w_process->p_info.hProcess, 1);
    }

    fmu_log(fmu, FmiLogOk, "Debug", "SIMER stopped.");

cleanup:
    CloseHandle(w_process->p_info.hProcess);
    CloseHandle(w_process->p_info.hThread);
    free(w_process);
    fmi_gw->settings.runtime.simer_process = NULL;
}
