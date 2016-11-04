/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * mspackWrapper.h --
 *
 *      Definitions of the mspack wrapper.
 */

#ifndef _MSPACKWRAPPER_H_
#define _MSPACKWRAPPER_H_

/*
 * This include takes care of configuring the header files to suite the library
 * compilation.
 */

#include "mspackConfig.h"
#include "imgcust-common/log.h"

/*
 * The objective of this header file is to abstract the c99 standard 
 * of mspack.h which does not compile with C++ due to usage of keywords
 * like 'this' and 'class' in the code. "extern C" is more of a linker 
 * provision and does not handle this scenario.
 *
 * This header file abstracts away the mspack.h and exposes the same 
 * functionalities.
 *
 */

/*
 * Constant Declaration
 */

/* 
 * Error codes 
 */

#define LINUXCAB_SUCCESS 0          // Success
#define LINUXCAB_ERROR 1            // General error
#define LINUXCAB_ERR_EXTRACT 2      // Extraction error
#define LINUXCAB_ERR_DECOMPRESSOR 3 // Decompression error
#define LINUXCAB_ERR_OPEN 4         // Open error
#define LINUXCAB_ERR_SEEK 5         // Seek error 

// .....................................................................................

/**
 *
 * Set the logging function.
 *
 * @param   [in]  log   Logging function to be used.
 * @returns None
 *
 **/
void
MspackWrapper_SetLogger(LogFunction log);

//......................................................................................

/**
 * 
 * Expands all files in the cabinet into the specified directory. Also returns
 * the command that is specified in the VMware defined header.
 *
 * @param cabFileName      IN:   Cabinet file name
 * @param destDirectory    IN:   Destination directory to uncab
 *
 * @return
 *  On success          LINUXCAB_SUCCESS
 *  On Error            LINUXCAB_ERROR, LINUXCAB_ERR_OPEN, LINUXCAB_ERR_DECOMPRESS,
 *                      LINUXCAB_EXTRACT
 **/
unsigned int
ExpandAllFilesInCab(const char* cabFileName,    
                    const char* destDirectory);

//......................................................................................

/**
 * Does a self check on the library parameters to make sure that the library
 * compilation is compatible with the client compilation. This is funny scenario
 * and is put in to support different flavours of UNIX operating systems.
 * Essentially the library checks of off_t size.
 *
 * @param         None
 * @return  
 *  On Success    LINUXCAB_SUCCESS
 *  On Error      LINUXCAB_ERR_SEEK, LINUXCAB_ERROR
 *
 **/
unsigned int 
SelfTestMspack(void);

//......................................................................................

/**
 * 
 * Get a string error message for the given error code.
 *
 * @param   error  IN:  Error  number
 * @return  error as a string message
 * 
 **/
const char*
GetLinuxCabErrorMsg (const unsigned int error);

// .....................................................................................

/**
 * 
 * Sets up the path for exracting file. For e.g. if the file is /a/b/c/d.abc
 * then it creates /a/b/c (skips if any of the directories along the path
 * exists)
 *
 * @param path  IN: Complete path of the file
 * @return
 *  On Success  LINUXCAB_SUCCESS
 *  On Error    LINUXCAB_ERROR
 *  
 **/
unsigned int
SetupPath (char* path);

#endif

