/*********************************************************
 * Copyright (C) 2014-2016 VMware, Inc. All rights reserved.
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

#ifndef _CERT_KEY_H_
#define _CERT_KEY_H_

#include <glib.h>
#include <glib/gstdio.h>
#include <openssl/evp.h>

gchar *
CertKey_ComputeCertPemFileHash(const gchar *certPemFile); // IN

gboolean
CertKey_GenerateKeyCert(int bits,                         // IN
                        int days,                         // IN
                        const gchar *confFile,            // IN
                        const gchar *keyFile,             // IN
                        const gchar *certFile);           // IN

void
CertKey_InitOpenSSLLib(void);

gboolean
WritePemFile(EVP_PKEY *pkey,                     // IN
             const gchar *keyFile,               // IN
             X509 *cert,                         // IN
             const gchar *certFile);             // IN

gchar *
GetSSLError(gchar **errorStr);                   // OUT
#endif // #ifndef _CERT_KEY_H_
