/*********************************************************
 * Copyright (C) 2011-2019 VMware, Inc. All rights reserved.
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

/**
 * @file prefs.h --
 *
 *    Service preference definitions.
 *
 *    @addtogroup vgauth_prefs
 *    @{
 */

#ifndef _PREFS_H_
#define _PREFS_H_

#include <glib.h>


typedef struct _PrefHandle {
   GKeyFile *keyFile;
   char *fileName;
} *PrefHandle;

PrefHandle Pref_Init(const gchar *configFilename);
void Pref_Shutdown(PrefHandle ph);
gchar *Pref_GetString(PrefHandle ph,
                      const gchar *prefName,
                      const gchar *groupName,
                      const char *defaultVal);
int Pref_GetInt(PrefHandle ph,
                const gchar *prefName,
                const gchar *groupName,
                int defaultVal);
gboolean Pref_GetBool(PrefHandle ph,
                      const gchar *prefName,
                      const gchar *groupName,
                      gboolean defaultVal);

void Pref_LogAllEntries(const PrefHandle ph);


/*
 * Location of the prefs file.  Windows expects to find it in the
 * registry.
 */
#ifdef _WIN32
#define VGAUTH_REGISTRY_KEY "SOFTWARE\\VMware, Inc.\\VMware VGAuth"
#define VGAUTH_REGISTRY_PREFFILE "PreferencesFile"
// fallback value if registry isn't set
#define VGAUTH_PREF_CONFIG_FILENAME "c:\\Program Files\\VMware\\VMware Tools\\vgauth.conf"
#else
#define VGAUTH_PREF_CONFIG_FILENAME "/etc/vmware-tools/vgauth.conf"
// XXX temp til installer tweaks its location
#define VGAUTH_PREF_CONFIG_FILENAME_OLD "/etc/vmware/vgauth.conf"
#endif

/*
 * Preferences
 *
 * Glib configuration is similar to Windows .ini files.
 *
 * It uses a simple name=value syntax, with preferences separated by groups.
 * Groupnames are delineated with '[' and ']'.
 * Unlike Windows, glib requires at least one group.
 *
 * @verbatim
# Sample configuration
# Note -- do not use '"'s around strings, they will be treated as part
# of the string.  Also be sure to use a double '\' in Windows filenames,
# since '\' is the escape character, eg c:\\Program Files\\VMware\\schemas"
# String values must be in UTF8.
#
[service]
logfile=/tmp/log.out
samlSchemaDir=/usr/lib/vmware-vgauth/schemas
aliasStoreDir=/var/lib/vmware/VGAuth/aliasStore
loglevel=normal
enableLogging=true
enableCoreDumps=true
clockSkewAdjustment = 300

[ticket]
ticketTTL=3600

[auditing]
auditSuccessEvents=true

[localization]
msgCatalog = /etc/vmware-tools/vgauth/messages
# EOF
 @endverbatim
 * See http://developer.gnome.org/glib/2.28/glib-Key-value-file-parser.html#glib-Key-value-file-parser.description
 */

/** Service group name. */
#define VGAUTH_PREF_GROUP_NAME_SERVICE     "service"

/*
 * Pref names
 */
/** Whether to log to a file. */
#define VGAUTH_PREF_LOGTOFILE              "enableLogging"
/** Whether to allow core dumps. */
#define VGAUTH_PREF_ALLOW_CORE             "enableCoreDumps"
/** The location of the logfile. */
#define VGAUTH_PREF_NAME_LOGFILE           "logfile"
/** The logging level. */
#define VGAUTH_PREF_NAME_LOGLEVEL          "loglevel"
/** Maxiumum number of old log files to be kept. */
#define VGAUTH_PREF_NAME_MAX_OLD_LOGFILES  "maxOldLogFiles"
/** Maxiumum size in MB of each log file. */
#define VGAUTH_PREF_NAME_MAX_LOGSIZE       "maxLogSize"
/** Number of seconds a specific user's listen connection will go unreferenced until it is discarded. */
#define VGAUTH_PREF_NAME_LISTEN_TTL        "listenTTL"
/** Maximum number of data connections allowed for a non privileged user */
#define VGAUTH_PREF_NAME_MAX_DATA_CONNECTIONS_PER_USER  \
   "maxDataConnectionsPerUser"
/** Where the XML schema files used for SAML parsing were installed. */
#define VGAUTH_PREF_SAML_SCHEMA_DIR        "samlSchemaDir"
/** The location of the idstore */
#define VGAUTH_PREF_ALIASSTORE_DIR         "aliasStoreDir"
/** The number of seconds slack allowed in either direction in SAML token date checks. */
#define VGAUTH_PREF_CLOCK_SKEW_SECS        "clockSkewAdjustment"

/** Ticket group name. */
#define VGAUTH_PREF_GROUP_NAME_TICKET      "ticket"

/** Number of seconds a ticket will go unreferenced until it is discarded. */
#define VGAUTH_PREF_NAME_TICKET_TTL        "ticketTTL"

/** Auditing group name. */
#define VGAUTH_PREF_GROUP_NAME_AUDIT       "auditing"

/** Whether to generate audit events for successful operations. */
#define VGAUTH_PREF_AUDIT_SUCCESS          "auditSuccessEvents"

/** SSPI group name. */
#define VGAUTH_PREF_GROUP_NAME_SSPI        "sspi"

/**
 * Number of seconds within which a SSPI authentication handshake must be
 * completed or it is discarded. Default is ten minutes.
 */
#define VGAUTH_PREF_NAME_SSPI_HANDSHAKE_TTL "sspiHandshakeTTL"


/** Localization group name. */
#define VGAUTH_PREF_GROUP_NAME_LOCALIZATION        "localization"

/** Where the localized version of the messages were installed. */
#define VGAUTH_PREF_LOCALIZATION_DIR        "msgCatalog"

/*
 * Pref values
 */

/** Normal logging level; informational messages and errors. */
#define SERVICE_LOGLEVEL_NORMAL             "normal"
/** Normal logging level plus debug messages. */
#define SERVICE_LOGLEVEL_VERBOSE            "verbose"
#define SERVICE_LOGLEVEL_DEBUG              "debug"

/** @} */

/*
 * Default values for the preferences. These should be defined next to the
 * preference name they refer to, but then they would show up in the generated
 * documentation.
 */

#define VGAUTH_PREF_DEFAULT_SSPI_HANDSHAKE_TTL (10 * 60)

/*
 * Parent directory of 'messages', which has <lang>/<app>.vmsg
 * below that.
 */
#ifdef _WIN32
#define VGAUTH_PREF_DEFAULT_LOCALIZATION_CATALOG "."
#else
#define VGAUTH_PREF_DEFAULT_LOCALIZATION_CATALOG "/etc/vmware-tools"
#endif

#define VGAUTH_PREF_DEFAULT_MAX_DATA_CONNECTIONS_PER_USER 5

#define VGAUTH_PREF_DEFAULT_CLOCK_SKEW_SECS (300)

#endif // _PREFS_H_

