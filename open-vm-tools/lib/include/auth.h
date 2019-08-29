/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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

#ifndef _AUTH_H_
#define _AUTH_H_

/*
 * auth.h --
 *
 *	Defines for the authorization library.
 */

#include "vm_basic_types.h"
#include "unicodeTypes.h"

#if _WIN32
#  include <windows.h>
   /*
    * This is quite possibly wrong, but fixes compile errors
    * for now.  Come back to this when authentication is
    * properly implemented.
    */
   typedef HANDLE AuthToken;
#else
#  include <pwd.h> //for getpwent, etc.
#  include <grp.h> //for initgroups
   typedef const struct passwd * AuthToken;
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if _WIN32

BOOL Auth_StoreAccountInformation(const char *username, const char *password);
BOOL Auth_DeleteAccountInformation();
BOOL Auth_RetrieveAccountInformation(char **username, char **password);

#define AUTH_ATTRIBUTE_RUNAS_LOCAL_SYSTEM    0x1
#define AUTH_LOCAL_SYSTEM_USER "SYSTEM"

BOOL Auth_StoreAccountInformationForVM(const char *filename, uint32 attributes,
				       const char *username, const char *password);
BOOL Auth_DeleteAccountInformationForVM(const char *filename);
BOOL Auth_AccountInformationIsStoredForVMs();
BOOL Auth_DeleteAccountInformationStoredForVMs();
uint32 Auth_RetrieveAccountInformationForVM(const char *filename, uint32 *attributes,
					    char **username, char **password);

#else

AuthToken Auth_GetPwnam(const char *user);
AuthToken Auth_AuthenticateSelf(void);
AuthToken Auth_AuthenticateUserPAM(const char *user, const char *pass,
                                   const char *service);

#endif

AuthToken Auth_AuthenticateUser(const char *user, const char *pass);

void Auth_CloseToken(AuthToken token);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
