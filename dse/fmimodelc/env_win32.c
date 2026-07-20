// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0


#include <windows.h>


int fmimodelc_setenv(const char* name, const char* value)
{
    /* _putenv_s updates the CRT environment cache (read by getenv()).
       SetEnvironmentVariable updates the Win32 process environment block
       (inherited by child processes). Both are needed. */
    _putenv_s(name, value);
    return SetEnvironmentVariable(name, value) ? 0 : -1;
}
