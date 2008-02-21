/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/


/*
 * cpNameUtil.h
 *
 *    Utility functions for cross-platform name format.
 *
 */

#ifndef __CP_NAME_UTIL_H__
#define __CP_NAME_UTIL_H__

#include "vm_basic_types.h"
#include "cpName.h"

char *CPNameUtil_Strrchr(char const *cpNameIn,
                         size_t cpNameInSize,
                         char searchChar);

int CPNameUtil_ConvertToRoot(char const *nameIn,
                             size_t bufOutSize,
                             char *bufOut);
int CPNameUtil_LinuxConvertToRoot(char const *nameIn,
                                  size_t bufOutSize,
                                  char *bufOut);
int CPNameUtil_WindowsConvertToRoot(char const *nameIn,
                                    size_t bufOutSize,
                                    char *bufOut);
/* 
 * Convert a set of files or directories CP names from current to form C.
 * In/out name lengths include a final nul-terminator to ensure
 * all the final name component is converted.
 */
Bool CPNameUtil_Utf8FormHostToUtf8FormC(const char *cpNameToConvert,
                                        size_t cpNameToConvertLen,
                                        char **cpUtf8FormCName,
                                        size_t *cpUtf8FormCNameLen);

/* 
 * Convert a set of files or directories CP names from current from form C.
 * In/out name lengths include a final nul-terminator to ensure
 * all the final name component is converted.
 */
Bool CPNameUtil_Utf8FormCToUtf8FormHost(const char *cpUtf8FormCName,
                                        size_t cpUtf8FormCNameLen,
                                        char **cpConvertedName,
                                        size_t *cpConvertedNameLen);


#endif /* __CP_NAME_UTIL_H__ */
