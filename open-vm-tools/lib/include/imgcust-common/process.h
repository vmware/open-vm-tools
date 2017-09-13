/*********************************************************
 * Copyright (C) 2007-2016 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * process.h --
 *
 *      Functions to launch an external process.
 */

#ifndef IMGCUST_COMMON_PROCESS_H
#define IMGCUST_COMMON_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ProcessOpaque *ProcessHandle;

typedef enum _ProcessError {
   PROCESS_SUCCESS,
   PROCESS_FAILED
} ProcessError;


/*
 *------------------------------------------------------------------------------
 *
 * Process_Create --
 *
 *      Creates a process and returns result of the operation.
 *
 *      Since this file can be included in a c++ file that already has the
 *      namespaced c++ definition of LogFunction defined, we can't use the c
 *      version of LogFunction as an input. Only choice is to make it a raw
 *      pointer and cast it in the processXXX.c file which can use the C
 *      definition of LogFunction.
 *
 *------------------------------------------------------------------------------
 */

ProcessError
Process_Create(ProcessHandle *h, char *args[], void *log);


/*
 *------------------------------------------------------------------------------
 *
 * Process_RunToComplete --
 *
 *      Runs the process to completion and returns its result.
 *
 *------------------------------------------------------------------------------
 */

ProcessError
Process_RunToComplete(ProcessHandle h, unsigned long timeout);


/*
 *------------------------------------------------------------------------------
 *
 * Process_GetStdout --
 *
 *      Returns process's standard output.
 *
 *------------------------------------------------------------------------------
 */

const char *
Process_GetStdout(ProcessHandle h);


/*
 *------------------------------------------------------------------------------
 *
 * Process_GetStderr --
 *
 *      Returns process's standard error output.
 *
 *------------------------------------------------------------------------------
 */

const char *
Process_GetStderr(ProcessHandle h);


/*
 *------------------------------------------------------------------------------
 *
 * Process_GetExitCode --
 *
 *      Returns process's exit code.
 *
 *------------------------------------------------------------------------------
 */

int
Process_GetExitCode(ProcessHandle h);


/*
 *------------------------------------------------------------------------------
 *
 * Process_Destroy --
 *
 *      Destroys the process and returns result of the operation.
 *
 *------------------------------------------------------------------------------
 */

ProcessError
Process_Destroy(ProcessHandle h);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // IMGCUST_COMMON_PROCESS_H
