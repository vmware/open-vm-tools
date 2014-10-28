/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * Functions to launch an external process
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ProcessOpaque* ProcessHandle;

typedef enum _ProcessError {
   PROCESS_SUCCESS,
   PROCESS_FAILED
} ProcessError;

/**
 * Process_Create
 *
 * since this file can be included in a c++ file that already has the 
 * namespaced c++ definition of LogFunction defined, we can't use the c 
 * version of LogFunction as an input. Only choice is to make it a raw 
 * pointer and cast it in the processXXX.c file which can use the C 
 * definition of LogFunction.
 */
ProcessError Process_Create(ProcessHandle *h,
                            char* args[],
                            void* log);

ProcessError Process_RunToComplete(ProcessHandle h, unsigned long timeout);

const char* Process_GetStdout(ProcessHandle h);

const char* Process_GetStderr(ProcessHandle h);

int Process_GetExitCode(ProcessHandle h);

ProcessError Process_Destroy(ProcessHandle h);
   
#ifdef __cplusplus
} // extern "C"
#endif
