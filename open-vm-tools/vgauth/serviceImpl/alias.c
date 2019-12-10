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
 * @file alias.c --
 *
 *    Functions to support the Alias store.
 */

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <Shlobj.h>
#include "VGAuthLog.h"
#else
#include <unistd.h>
#endif

#include "serviceInt.h"
#include "certverify.h"
#include "VGAuthProto.h"
#include "vmxlog.h"

// puts the identity store in an easy to find place
#undef WIN_TEST_MODE

/*
 * Note that we can't have trailing DIRSEP on these paths, or the
 * file security checks in ServiceAliasInitAliasStore() will fail.
 * A path of "/foo/bar/" is Not the same as "/foo/bar", at least
 * to (l)stat() and g_file_test()
 */
#ifdef _WIN32
#ifdef WIN_TEST_MODE
#define DEFAULT_ALIASSTORE_ROOT_DIR   "C:\\aliasStore"
#else
#define ALIAS_STORE_REL_DIRECTORY   "VMware\\VGAuth\\aliasStore"
#define DEFAULT_ALIASSTORE_ROOT_DIR   "C:\\Documents and Settings\\All Users\\Application Data\\" ALIAS_STORE_REL_DIRECTORY
#endif
#else
#define DEFAULT_ALIASSTORE_ROOT_DIR   "/var/lib/vmware/VGAuth/aliasStore"
#endif

#define ALIASSTORE_MAPFILE_NAME  "mapping.xml"
#define ALIASSTORE_FILE_PREFIX   "user-"
#define ALIASSTORE_FILE_SUFFIX   ".xml"

static gchar *aliasStoreRootDir = DEFAULT_ALIASSTORE_ROOT_DIR;

#ifdef _WIN32
/*
 * Still used to create the Alias directory; passed through to
 * g_mkdir_with_parents()
 */
#define ALIASSTORE_DIR_PERMS     (0)
#else
#define ALIASSTORE_FILE_PERMS    (S_IRUSR | S_IWUSR) // 0600
#define ALIASSTORE_MAPFILE_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) // 0644
#define ALIASSTORE_DIR_PERMS     (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) // 0755
#endif

/*
 * Maximum size of an alias or mapping file in bytes, to prevent DoS attacks.
 */
#define ALIASSTORE_FILE_MAX_SIZE       (10 * 1024 * 1024)

/*
 * Alias store XML details.
 */

/*
per-user file layout:

<userAliases>
 <alias>
   <pemCert> </pemCert>
   <aliasInfos>
      <aliasInfo>
         <subject>foo\bar</subject>
         <comment> this is a comment </comment>
      </aliasInfo>
      <aliasInfo>
         <subject>baz</subject>
         <comment> this is another comment </comment>
      </aliasInfo>
      <aliasInfo>
         <anySubject/>
         <comment> this is a comment for anyUser</comment>
      </aliasInfo>
   </aliasInfos>
 </alias>
</userAliases>
 */

#define ALIASSTORE_XML_PREAMBLE VGAUTH_XML_PREAMBLE

#define ALIASINFO_USER_ALIASES_ELEMENT_NAME "userAliases"

#define ALIASINFO_ALIAS_ELEMENT_NAME "alias"
#define ALIASINFO_PEMCERT_ELEMENT_NAME "pemCertificate"
#define ALIASINFO_ALIASINFOS_ELEMENT_NAME "aliasInfos"
#define ALIASINFO_ALIASINFO_ELEMENT_NAME "aliasInfo"
#define ALIASINFO_SUBJECT_ELEMENT_NAME "subject"
#define ALIASINFO_ANYSUBJECT_ELEMENT_NAME "anySubject"
#define ALIASINFO_COMMENT_ELEMENT_NAME "comment"

static char *AliasStoreXMLUserAliasesStart =
   "<"ALIASINFO_USER_ALIASES_ELEMENT_NAME">\n";

static char *AliasStoreXMLUserAliasesEnd =
   "</"ALIASINFO_USER_ALIASES_ELEMENT_NAME">\n";

static char *AliasStoreXMLFormatAliasStart =
   "<"ALIASINFO_ALIAS_ELEMENT_NAME">\n"
   "   <"ALIASINFO_PEMCERT_ELEMENT_NAME">%s</"ALIASINFO_PEMCERT_ELEMENT_NAME">\n"
   "   <"ALIASINFO_ALIASINFOS_ELEMENT_NAME">\n";

static char *AliasStoreXMLFormatNamedAliasInfo =
   "      <"ALIASINFO_ALIASINFO_ELEMENT_NAME">\n"
   "         <"ALIASINFO_SUBJECT_ELEMENT_NAME">%s</"ALIASINFO_SUBJECT_ELEMENT_NAME">\n"
   "         <"ALIASINFO_COMMENT_ELEMENT_NAME">%s</"ALIASINFO_COMMENT_ELEMENT_NAME">\n"
   "      </"ALIASINFO_ALIASINFO_ELEMENT_NAME">\n";

static char *AliasStoreXMLFormatAnyAliasInfo =
   "      <"ALIASINFO_ALIASINFO_ELEMENT_NAME">\n"
   "         <"ALIASINFO_ANYSUBJECT_ELEMENT_NAME"/>\n"
   "         <"ALIASINFO_COMMENT_ELEMENT_NAME">%s</"ALIASINFO_COMMENT_ELEMENT_NAME">\n"
   "      </"ALIASINFO_ALIASINFO_ELEMENT_NAME">\n";

static char *AliasStoreXMLFormatAliasEnd =
   "   </"ALIASINFO_ALIASINFOS_ELEMENT_NAME">\n"
   "</"ALIASINFO_ALIAS_ELEMENT_NAME">\n";

/*
 * Possible parse states for an alias file.
 */
typedef enum AliasParseState {
   ALIAS_PARSE_STATE_NONE,            // init
   ALIAS_PARSE_STATE_ALIASES,         // working on the list of aliases
   ALIAS_PARSE_STATE_ALIAS,           // working on an alias
   ALIAS_PARSE_STATE_PEMCERT,         // working on a PEM cert
   ALIAS_PARSE_STATE_ALIASINFOS,      // working on a list of aliasInfos
   ALIAS_PARSE_STATE_ALIASINFO,       // working on a aliasInfo
   ALIAS_PARSE_STATE_SUBJECT,         // working on a named subject
   ALIAS_PARSE_STATE_ANYSUBJECT,      // working on an anySubject
   ALIAS_PARSE_STATE_COMMENT,         // working on a comment
} AliasParseState;

/*
 * Used when parsing an alias store.
 */
typedef struct _AliasParseList {
   AliasParseState state;
   int num;
   ServiceAlias *aList;
} AliasParseList;



/*
 * Mapping file XML details.
 */
#define MAP_MAPPINGS_ELEMENT_NAME "mappings"

#define MAP_MAPPING_ELEMENT_NAME "mapping"
#define MAP_PEMCERT_ELEMENT_NAME "pemCertificate"
#define MAP_SUBJECTS_ELEMENT_NAME "subjects"
#define MAP_SUBJECT_ELEMENT_NAME "subject"
#define MAP_ANYSUBJECT_ELEMENT_NAME "anySubject"
#define MAP_USERNAME_ELEMENT_NAME "userName"

static char *MapfileXMLStart =
   "<"MAP_MAPPINGS_ELEMENT_NAME">\n";

static char *MapfileXMLEnd =
   "</"MAP_MAPPINGS_ELEMENT_NAME">\n";

static char *MapfileXMLMappingStart =
   "  <"MAP_MAPPING_ELEMENT_NAME">\n"
   "    <"MAP_PEMCERT_ELEMENT_NAME">%s</"MAP_PEMCERT_ELEMENT_NAME">\n"
   "    <"MAP_USERNAME_ELEMENT_NAME">%s</"MAP_USERNAME_ELEMENT_NAME">\n"
   "    <"MAP_SUBJECTS_ELEMENT_NAME">\n";

static char *MapfileXMLMappingEnd =
   "    </"MAP_SUBJECTS_ELEMENT_NAME">\n"
   "  </"MAP_MAPPING_ELEMENT_NAME">\n";

static char *MapfileXMLNamedSubject =
   "      <"MAP_SUBJECT_ELEMENT_NAME">%s</"MAP_SUBJECT_ELEMENT_NAME">\n";

static char *MapfileXMLAnySubject =
   "      <"MAP_ANYSUBJECT_ELEMENT_NAME"/>\n";

/*
 * Possible parse states for a mapping file.
 */
typedef enum MappedParseState {
   MAP_PARSE_STATE_NONE,           // init
   MAP_PARSE_STATE_MAPPINGS,       // working on the list of mappings
   MAP_PARSE_STATE_MAPPING,        // working on a mapping
   MAP_PARSE_STATE_PEMCERT,        // working on a PEM cert
   MAP_PARSE_STATE_SUBJECTS,       // working on a list of subjects
   MAP_PARSE_STATE_SUBJECT,        // working on a named subject
   MAP_PARSE_STATE_ANYSUBJECT,     // working on an anySubject
   MAP_PARSE_STATE_USERNAME,       // working on a userName
} MappedParseState;


/*
 * Used when parsing a mapping file.
 */
typedef struct _MappedAliasParseList {
   MappedParseState state;
   int num;
   ServiceMappedAlias *maList;
} MappedAliasParseList;

/*
mapping file layout:

<mappings>
 <mapping>
   <pemCert> </pemCert>
   <user>guestusername</user>
   <subjects>
         <subject>baz</subject>
         <anySubject/>
   </subjects>
  </mapping>
</mappings>

 */


/*
 ******************************************************************************
 * ServiceAliasIsSubjectEqual --                                         */ /**
 *
 * Compares two Subjects.
 *
 * @param[in]   t1   Type of the first subject.
 * @param[in]   t2   Type of the second subject.
 * @param[in]   n1   Name of the first subject.
 * @param[in]   n2   Name of the second subject.
 *
 * @return   TRUE if they match.
 *
 ******************************************************************************
 */

gboolean
ServiceAliasIsSubjectEqual(ServiceSubjectType t1,
                           ServiceSubjectType t2,
                           const gchar *n1,
                           const gchar *n2)
{
   gboolean bRet = TRUE;

   if (t1 != t2) {
      return FALSE;
   }

   /*
    * Do a case insenstive comparison.  glib has a pure ascii strcmp,
    * but this data could be non-ascii, so instead use the utf8 function
    * to make it case insensitive and then do a compare.
    */
   if (t1 == SUBJECT_TYPE_NAMED) {
      gchar *n1case;
      gchar *n2case;

      n1case = g_utf8_casefold(n1, -1);
      n2case = g_utf8_casefold(n2, -1);
      bRet = (g_strcmp0(n1case, n2case) == 0) ? TRUE : FALSE;
      g_free(n1case);
      g_free(n2case);
   }

   return bRet;
}


/*
 ******************************************************************************
 * ServiceComparePEMCerts --                                             */ /**
 *
 * Compares two PEM certificates, returns TRUE if they are the same.
 * This does a lot more than just a strcmp() because we may have
 * extraneous whitespace or other junk as in an openssl PEM
 * format which has -----BEGIN CERTIFICATE----- and
 * -----END CERTIFICATE-----.
 *
 * @param[in]   pemCert1      The first cert.
 * @param[in]   pemCert2      The second cert.
 *
 * @return TRUE if the certs are the same.
 *
 ******************************************************************************
 */

gboolean
ServiceComparePEMCerts(const gchar *pemCert1,
                       const gchar *pemCert2)
{
   gchar *cleanCert1;
   gchar *cleanCert2;
   guchar *binCert1;
   guchar *binCert2;
   gsize len1;
   gsize len2;
   gboolean bRet;

   /*
    * First pull off any openssl headers.  The base64 decoder
    * will ignore anything non-base64 (like whitespace) in the
    * text, but it'll treat the 'BEGIN/END' text in the delimiter
    * as real data.
    */
   cleanCert1 = CertVerify_StripPEMCert(pemCert1);
   cleanCert2 = CertVerify_StripPEMCert(pemCert2);

   binCert1 = g_base64_decode(cleanCert1, &len1);
   binCert2 = g_base64_decode(cleanCert2, &len2);

   if (len1 != len2) {
      bRet = FALSE;
      goto done;
   }

   bRet = memcmp(binCert1, binCert2, len1) == 0;

done:
   g_free(cleanCert1);
   g_free(cleanCert2);
   g_free(binCert1);
   g_free(binCert2);

   return bRet;
}


/*
 ******************************************************************************
 * ServiceUserNameToAliasStoreFileName --                                */ /**
 *
 * Map a user name into its VGAuth alias file name,
 * escaping the backslash on Windows.
 *
 * @param[in]   userName       The user name
 *
 * @return   the result ascii string
 *
 ******************************************************************************
 */

static gchar *
ServiceUserNameToAliasStoreFileName(const gchar *userName)
{
   gchar *escapedName = ServiceEncodeUserName(userName);
   gchar *aliasFileName = g_strdup_printf("%s"DIRSEP"%s%s%s",
                                          aliasStoreRootDir,
                                          ALIASSTORE_FILE_PREFIX,
                                          escapedName,
                                          ALIASSTORE_FILE_SUFFIX);

   g_free(escapedName);
   return aliasFileName;
}


/*
 ******************************************************************************
 * ServiceUserNameToTmpAliasStoreFileName --                             */ /**
 *
 * Map a user name into its VGAuth tmp alias file name,
 * escaping the backslash on Windows.
 *
 * @param[in]   userName       The user name
 *
 * @return   the result ascii string
 *
 ******************************************************************************
 */

static gchar *
ServiceUserNameToTmpAliasStoreFileName(const gchar *userName)
{
   gchar *escapedName = ServiceEncodeUserName(userName);
   gchar *tmpAliasFileName = g_strdup_printf("%s"DIRSEP"%sXXXXXX",
                                             aliasStoreRootDir,
                                             escapedName);
   g_free(escapedName);
   return tmpAliasFileName;
}


/*
 ******************************************************************************
 * ServiceLoadAliasFileContents --                                       */ /**
 *
 * Securely loads an alias or mapping file, preventing TOCTOU (Time of Check,
 * Time of Use) bugs.  This also provides a size sanity check, preventing
 * a DoS attack.
 *
 * Steps:
 * 1 - test file for existence and type
 * 2 - size check
 * 3 - verfify ownership and permissions
 * 4 - open file
 * 5 - re-fetch attributes via handle
 * 6 - recheck permissions (by handle or statbuf)
 * 7 - verfiy nothing important changed between the checks
 * 8 - read the file contents
 *
 * Note that it looks like steps 1-3 might be skippable, and we could just
 * check the file after it is opened.  But that would allow symlinks
 * through.  A few comparisons might be optimized out, but those
 * are cheap compared to the syscalls.
 *
 * @param[in]   fileName       name of the file to be read.
 * @param[in]   userName       The user name of the expected owner.
 *                             Can be NULL for map file.
 * @param[out]  contents       The file contents.  Must be g_free()d.
 * @param[out]  fileSize       The file size.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

#ifdef _WIN32
static VGAuthError
ServiceLoadFileContentsWin(const gchar *fileName,
                           const gchar *userName,
                           gchar **contents,
                           gsize *fileSize)
{
   DWORD toRead;
   VGAuthError err = VGAUTH_E_FAIL;
   gchar *buf;
   gchar *bp;
   HANDLE hFile = NULL;
   WIN32_FILE_ATTRIBUTE_DATA fileAttrs;
   BY_HANDLE_FILE_INFORMATION fileInfoHandle;
   gunichar2 *fileNameW = NULL;
   BOOL ok;
   DWORD bytesRead;

   *fileSize = 0;
   *contents = NULL;

   CHK_UTF8_TO_UTF16(fileNameW, fileName, goto done);

   /*
    * Get baseline size and type data.
    */
   if (!GetFileAttributesExW(fileNameW, GetFileExInfoStandard,
                             &fileAttrs)) {
      VGAUTH_LOG_ERR_WIN("failed to get attributes of %s; failing read\n",
              fileName);
      err = VGAUTH_E_FAIL;
      goto done;
   }

   /*
    * For windows, be sure its not a directory, device, or is
    * a reparse point (symlink).
    */
#define FILE_ATTRS_NORMAL(a) (!((a) & (FILE_ATTRIBUTE_DEVICE | \
                                       FILE_ATTRIBUTE_DIRECTORY | \
                                       FILE_ATTRIBUTE_REPARSE_POINT)))

   // verify file type
   if (!FILE_ATTRS_NORMAL(fileAttrs.dwFileAttributes)) {
      if (userName == NULL) {
         Audit_Event(FALSE,
                     SU_(alias.mapping.badfile,
                         "Mapping file '%s' exists but is not a regular file.  "
                         "The Aliases in the mapping file will not be available for "
                         "authentication"),
                     fileName);
         Warning("%s: mapping file %s exists but isn't a regular file (%d), "
                 "punting\n",
                 __FUNCTION__, fileName, fileAttrs.dwFileAttributes);
      } else {
         Audit_Event(FALSE,
                     SU_(alias.alias.badfile,
                         "Alias store '%s' exists but is not a regular file.  "
                         "The Aliases for user '%s' will not be available for "
                         "authentication"),
                     fileName, userName);
         Warning("%s: alias file %s exists but isn't a regular file (%d), "
                 "punting\n",
                 __FUNCTION__, fileName, fileAttrs.dwFileAttributes);
      }
      err = VGAUTH_E_FAIL;
      goto done;
   }

   // sanity check size
   if ((fileAttrs.nFileSizeHigh != 0) ||
       (fileAttrs.nFileSizeLow > ALIASSTORE_FILE_MAX_SIZE)) {
      Warning("%s: size of %s too large %d %d; failing read\n", __FUNCTION__,
              fileName, fileAttrs.nFileSizeHigh, fileAttrs.nFileSizeLow);
      err = VGAUTH_E_FAIL;
      goto done;
   }

   // check ownership
   err = ServiceFileVerifyAdminGroupOwned(fileName);
   if (err == VGAUTH_E_OK) {
      if (userName == NULL) {
         err = ServiceFileVerifyEveryoneReadable(fileName);
      } else {
         err = ServiceFileVerifyUserAccess(fileName,
                                           userName);
      }
   }
   if (err != VGAUTH_E_OK) {
      if (userName == NULL) {
         Audit_Event(FALSE,
                  SU_(alias.mapfile.badperm,
                      "Alias store mapping file '%s' has incorrect"
                      " owner or permissions.  "
                      "The Aliases in the mapping file will not be "
                      "available for authentication"),
                  fileName);
      } else {
         Audit_Event(FALSE,
                     SU_(alias.alias.badperm,
                         "Alias store '%s' has incorrect owner"
                         " or permissions.  "
                         "The Aliases for user '%s' will not be available "
                         "for authentication"),
                     fileName, userName);
      }
      goto done;
   }

   /*
    * Open the file
    */
   hFile = CreateFileW(fileNameW,
                       GENERIC_READ,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_READONLY,
                       NULL);
   if (hFile == INVALID_HANDLE_VALUE) {
      VGAUTH_LOG_ERR_WIN("failed to open file %s", fileName);
      err = VGAUTH_E_FAIL;
      goto done;
   }

   /*
    * Double-check attributes
    */
   if (!GetFileInformationByHandle(hFile, &fileInfoHandle)) {
      VGAUTH_LOG_ERR_WIN("failed to get attributes of %s; failing read\n",
              fileName);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }

   if (fileAttrs.dwFileAttributes != fileInfoHandle.dwFileAttributes) {
      Warning("%s: dwFileAttributes changed mid-read %d %d; failing read\n",
              __FUNCTION__,
              fileAttrs.dwFileAttributes, fileInfoHandle.dwFileAttributes);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }
   if ((fileAttrs.nFileSizeHigh != fileInfoHandle.nFileSizeHigh) ||
       (fileAttrs.nFileSizeLow != fileInfoHandle.nFileSizeLow)) {
      Warning("%s: file size of %s changed mid-read; failing read\n",
              __FUNCTION__, fileName);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }

   /*
    * Double-check ownership
    */
   err = ServiceFileVerifyAdminGroupOwnedByHandle(hFile);
   if (err == VGAUTH_E_OK) {
      if (userName == NULL) {
         err = ServiceFileVerifyEveryoneReadableByHandle(hFile);
      } else {
         err = ServiceFileVerifyUserAccessByHandle(hFile, userName);
      }
   }

   if (err != VGAUTH_E_OK) {
      Warning("%s: file ownsership changed mid-read; failing read\n",
              __FUNCTION__);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }

   /*
    * Now finally read the contents.
    */
   toRead = fileAttrs.nFileSizeLow;
   buf = g_malloc0(toRead + 1);
   bp = buf;
   while (toRead > 0) {
      ok = ReadFile(hFile, bp, toRead, &bytesRead, NULL);
      if (!ok) {
         VGAUTH_LOG_ERR_WIN("ReadFile(%s) failed", fileName);
         err = VGAUTH_E_FAIL;
         g_free(buf);
         goto done;
      }
      toRead -= bytesRead;
      bp += bytesRead;
   }


done:
   if (err == VGAUTH_E_OK) {
      *contents = buf;
      *fileSize = fileAttrs.nFileSizeLow;
   }
   if (hFile) {
      CloseHandle(hFile);
   }
   g_free(fileNameW);

   return err;
}

#else    // !_WIN32

static VGAuthError
ServiceLoadFileContentsPosix(const gchar *fileName,
                             const gchar *userName,
                             gchar **contents,
                             gsize *fileSize)
{
   int ret;
   uid_t uid = -1;
   gid_t gid = -1;
   gsize toRead;
   struct stat lstatBuf;
   struct stat fstatBuf;
   VGAuthError err = VGAUTH_E_FAIL;
   gchar *buf;
   gchar *bp;
   int fd = -1;

   *fileSize = 0;
   *contents = NULL;
   ret = g_lstat(fileName, &lstatBuf);
   if (ret != 0) {
      Warning("%s: lstat(%s) failed (%d %d)\n",
              __FUNCTION__, fileName, ret, errno);
      return VGAUTH_E_FAIL;
   }

   if (lstatBuf.st_size > ALIASSTORE_FILE_MAX_SIZE) {
      Warning("%s: size of %s too large %d; failing read\n", __FUNCTION__,
              fileName, (int) lstatBuf.st_size);
      return VGAUTH_E_FAIL;
   }

   /*
    * Check the file type and verify the permissions.
    * A NULL userName means we're looking at the
    * mapfile.
    */
   if (NULL == userName) {
      if (!S_ISREG(lstatBuf.st_mode)) {
         Audit_Event(FALSE,
                     SU_(alias.mapping.badfile,
                         "Mapping file '%s' exists but is not a regular file.  "
                         "The Aliases in the mapping file will not be available for "
                         "authentication"),
                     fileName);
         Warning("%s: mapping file %s exists but isn't a regular file, punting\n",
                 __FUNCTION__, fileName);
         return VGAUTH_E_FAIL;
      }
      err = ServiceFileVerifyFileOwnerAndPerms(fileName,
                                               SUPERUSER_NAME,
                                               ALIASSTORE_MAPFILE_PERMS,
                                               &uid, &gid);
      if (err != VGAUTH_E_OK) {
         Audit_Event(FALSE,
                     SU_(alias.mapfile.badperm,
                         "Alias store mapping file '%s' has incorrect"
                         " owner or permissions.  "
                         "The Aliases in the mapping file will not be "
                         "available for authentication"),
                  fileName);
         return err;
      }
   } else {
      if (!S_ISREG(lstatBuf.st_mode)) {
         Audit_Event(FALSE,
                     SU_(alias.alias.badfile,
                         "Alias store '%s' exists but is not a regular file.  "
                         "The Aliases for user '%s' will not be available for "
                         "authentication"),
                     fileName, userName);
         Warning("%s: file %s exists but isn't a regular file, punting\n",
                 __FUNCTION__, fileName);
         return VGAUTH_E_FAIL;
      }


      err = ServiceFileVerifyFileOwnerAndPerms(fileName,
                                               userName,
                                               ALIASSTORE_FILE_PERMS,
                                               &uid, &gid);
      if (err != VGAUTH_E_OK) {
         Audit_Event(FALSE,
                     SU_(alias.alias.badperm,
                         "Alias store '%s' has incorrect owner"
                         " or permissions.  "
                         "The Aliases for user '%s' will not be available "
                         "for authentication"),
                     fileName, userName);
         return err;
      }
   }

   /*
    * Now open the file.
    */
   fd = g_open(fileName, O_RDONLY);
   if (fd < 0) {
      Warning("%s: failed to open %s for read (%d)\n",
              __FUNCTION__, fileName, errno);
      return VGAUTH_E_FAIL;
   }

   /*
    * fstat() to make sure it wasn't changed between the first check
    * and the open().
    */
   ret = fstat(fd, &fstatBuf);
   if (ret != 0) {
      Warning("%s: fstat(%s) failed (%d %d)\n",
              __FUNCTION__, fileName, ret, errno);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }

   /*
    * Now the sanity checks.
    */
   if (lstatBuf.st_size != fstatBuf.st_size) {
      Warning("%s: size of %s changed (%d vs %d)\n", __FUNCTION__,
              fileName, (int) lstatBuf.st_size, (int) fstatBuf.st_size);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }

   if (lstatBuf.st_mode != fstatBuf.st_mode) {
      Warning("%s: mode of %s changed (%d vs %d)\n", __FUNCTION__,
              fileName, (int) lstatBuf.st_mode, (int) fstatBuf.st_mode);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }

   if ((lstatBuf.st_uid != fstatBuf.st_uid) ||
       (uid != fstatBuf.st_uid)) {
      Warning("%s: uid of %s changed (%d vs %d vs %d)\n", __FUNCTION__,
              fileName, (int) lstatBuf.st_uid, (int) fstatBuf.st_uid, (int) uid);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }

   if ((lstatBuf.st_gid != fstatBuf.st_gid) ||
       (gid != fstatBuf.st_gid)) {
      Warning("%s: gid of %s changed (%d vs %d vs %d)\n", __FUNCTION__,
              fileName, (int) lstatBuf.st_gid, (int) fstatBuf.st_gid, (int) gid);
      // XXX audit this?
      err = VGAUTH_E_FAIL;
      goto done;
   }

   /*
    * All sanity checks passed; read the bits.
    */
   toRead = lstatBuf.st_size;
   buf = g_malloc0(toRead + 1);
   bp = buf;
   while (toRead > 0) {
      ret = read(fd, bp, toRead);
      if (ret < 0) {
         Warning("%s: failed to read from file %s (%d)\n",
                 __FUNCTION__, fileName, errno);
         // XXX audit this?
         err = VGAUTH_E_FAIL;
         g_free(buf);
         goto done;
      } else if (ret == 0) {     // eof
         break;
      }

      bp += ret;
      toRead -= ret;
   }

done:
   if (VGAUTH_E_OK == err) {
      *contents = buf;
      *fileSize = lstatBuf.st_size;
   }
   if (fd >= 0) {
      close(fd);
   }

   return err;
}
#endif   // !_WIN32


static VGAuthError
ServiceLoadFileContents(const gchar *fileName,
                        const gchar *userName,
                        gchar **contents,
                        gsize *fileSize)
{
#ifdef _WIN32
   return ServiceLoadFileContentsWin(fileName,
                                     userName,
                                     contents,
                                     fileSize);
#else
   return ServiceLoadFileContentsPosix(fileName,
                                       userName,
                                       contents,
                                       fileSize);
#endif
}


/*
 ******************************************************************************
 * AliasDumpAliases --                                                   */ /**
 *
 * Dumps a list of Aliases to the given FILE*.
 *
 * @param[in]   fp           The FILE to dump to.
 * @param[in]   num          The number of entries.
 * @param[in]   aList        The list.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
AliasDumpAliases(FILE *fp,
                 int num,
                 ServiceAlias *aList)
{
   int i;
   int j;
   gchar *text;
   int ret;
   VGAuthError err = VGAUTH_E_OK;
   ServiceAliasInfo *si;

   ASSERT(fp);
   ASSERT((0 == num) || (NULL != aList));

   ret = fputs(ALIASSTORE_XML_PREAMBLE "\n", fp);
   if (ret == EOF) {
      Warning("%s: fputs() failed\n", __FUNCTION__);
      err = VGAUTH_E_FAIL;
      goto done;
   }

   ret = fputs(AliasStoreXMLUserAliasesStart, fp);
   if (ret == EOF) {
      Warning("%s: fputs() failed\n", __FUNCTION__);
      err = VGAUTH_E_FAIL;
      goto done;
   }

   for (i = 0; i < num; i++) {
      text = g_markup_printf_escaped(AliasStoreXMLFormatAliasStart,
                                     g_strstrip(aList[i].pemCert));
      ret = fputs(text, fp);
      g_free(text);
      if (ret == EOF) {
         Warning("%s: fputs() failed\n", __FUNCTION__);
         err = VGAUTH_E_FAIL;
         goto done;
      }

      for (j = 0; j < aList[i].num; j++) {
         si = &(aList[i].infos[j]);
         if (si->type == SUBJECT_TYPE_NAMED) {
            text = g_markup_printf_escaped(AliasStoreXMLFormatNamedAliasInfo,
                                           si->name,
                                           si->comment);
         } else {
            text = g_markup_printf_escaped(AliasStoreXMLFormatAnyAliasInfo,
                                           si->comment);
         }
         ret = fputs(text, fp);
         g_free(text);
         if (ret == EOF) {
            Warning("%s: fputs() failed\n", __FUNCTION__);
            err = VGAUTH_E_FAIL;
            goto done;
         }
      }

      ret = fputs(AliasStoreXMLFormatAliasEnd, fp);
      if (ret == EOF) {
         Warning("%s: fputs() failed\n", __FUNCTION__);
         err = VGAUTH_E_FAIL;
         goto done;
      }
   }

   ret = fputs(AliasStoreXMLUserAliasesEnd, fp);
   if (ret == EOF) {
      Warning("%s: fputs() failed\n", __FUNCTION__);
      err = VGAUTH_E_FAIL;
      goto done;
   }

done:

   return err;
}


/*
 ******************************************************************************
 * AliasDumpMappedAliasesFile --                                         */ /**
 *
 * Dumps a list of MappedAliases to a FILE *.
 *
 * @param[in]   fp           The FILE to dump to.
 * @param[in]   num          The number of entries.
 * @param[in]   maList       The list.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
AliasDumpMappedAliasesFile(FILE *fp,
                           int num,
                           ServiceMappedAlias *maList)
{
   int i;
   int j;
   gchar *text;
   int ret;
   VGAuthError err = VGAUTH_E_OK;

   ret = fputs(ALIASSTORE_XML_PREAMBLE "\n", fp);
   if (ret == EOF) {
      Warning("%s: fputs() failed\n", __FUNCTION__);
      err = VGAUTH_E_FAIL;
      goto done;
   }

   ret = fputs(MapfileXMLStart, fp);
   if (ret == EOF) {
      Warning("%s: fputs() failed\n", __FUNCTION__);
      err = VGAUTH_E_FAIL;
      goto done;
   }

   for (i = 0; i < num; i++) {
      text = g_markup_printf_escaped(MapfileXMLMappingStart,
                                     g_strstrip(maList[i].pemCert),
                                     maList[i].userName);
      ret = fputs(text, fp);
      g_free(text);
      if (ret == EOF) {
         Warning("%s: fputs() failed\n", __FUNCTION__);
         err = VGAUTH_E_FAIL;
         goto done;
      }

      for (j = 0; j < maList[i].num; j++) {
         if (maList[i].subjects[j].type == SUBJECT_TYPE_ANY) {
            ret = fputs(MapfileXMLAnySubject, fp);
         } else {
            text = g_markup_printf_escaped(MapfileXMLNamedSubject,
                                           maList[i].subjects[j].name);
            ret = fputs(text, fp);
            g_free(text);
         }
         if (ret == EOF) {
            Warning("%s: fputs() failed\n", __FUNCTION__);
            err = VGAUTH_E_FAIL;
            goto done;
         }
      }

      ret = fputs(MapfileXMLMappingEnd, fp);
      if (ret == EOF) {
         Warning("%s: fputs() failed\n", __FUNCTION__);
         err = VGAUTH_E_FAIL;
         goto done;
      }
   }

   ret = fputs(MapfileXMLEnd, fp);
   if (ret == EOF) {
      Warning("%s: fputs() failed\n", __FUNCTION__);
      err = VGAUTH_E_FAIL;
      goto done;
   }

done:
   return err;
}


/*
 ******************************************************************************
 * AliasStartElement --                                                  */ /**
 *
 * Used by the per-user Alias parser.
 *
 * Called by the XML parser when it sees the start of a new
 * element.  Used to update the current parser state, and allocate
 * any space that may be needed for processing that state.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  elementName         The name of the element being started.
 * @param[in]  attributeNames      The names of any attributes on the element.
 * @param[in]  attributeValues     The values of any attributes on the element.
 * @param[in]  userData            The AliasParseList as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
AliasStartElement(GMarkupParseContext *parseContext,
                  const gchar *elementName,
                  const gchar **attributeNames,
                  const gchar **attributeValues,
                  gpointer userData,
                  GError **error)
{
   AliasParseList *list = (AliasParseList *) userData;
   ServiceAliasInfo *infos;
   int n;

   ASSERT(list);

   switch (list->state) {
   case ALIAS_PARSE_STATE_NONE:
      if (g_strcmp0(elementName, ALIASINFO_USER_ALIASES_ELEMENT_NAME) == 0) {
         list->state = ALIAS_PARSE_STATE_ALIASES;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      break;
   case ALIAS_PARSE_STATE_ALIASES:
      /*
       * Starting a new Alias, alloc and grow.
       */
      if (g_strcmp0(elementName, ALIASINFO_ALIAS_ELEMENT_NAME) == 0) {
         list->num++;
         list->aList = g_realloc_n(list->aList,
                                   list->num, sizeof(ServiceAlias));
         list->aList[list->num - 1].pemCert = NULL;
         list->aList[list->num - 1].num = 0;
         list->aList[list->num - 1].infos = NULL;
         list->state = ALIAS_PARSE_STATE_ALIAS;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      break;
   case ALIAS_PARSE_STATE_ALIAS:
      if (g_strcmp0(elementName, ALIASINFO_PEMCERT_ELEMENT_NAME) == 0) {
         list->state = ALIAS_PARSE_STATE_PEMCERT;
      } else if (g_strcmp0(elementName, ALIASINFO_ALIASINFOS_ELEMENT_NAME) == 0) {
         list->state = ALIAS_PARSE_STATE_ALIASINFOS;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      break;
   case ALIAS_PARSE_STATE_ALIASINFOS:
      if (g_strcmp0(elementName, ALIASINFO_ALIASINFO_ELEMENT_NAME) == 0) {
         list->state = ALIAS_PARSE_STATE_ALIASINFO;

         // grow
         infos = list->aList[list->num - 1].infos;
         n = ++(list->aList[list->num - 1].num);
         infos = g_realloc_n(infos, n, sizeof(ServiceAliasInfo));
         infos[n - 1].type = -1;
         infos[n - 1].name = NULL;
         infos[n - 1].comment = NULL;
         list->aList[list->num - 1].infos = infos;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      break;
   case ALIAS_PARSE_STATE_ALIASINFO:
      if (g_strcmp0(elementName, ALIASINFO_SUBJECT_ELEMENT_NAME) == 0) {
         list->state = ALIAS_PARSE_STATE_SUBJECT;
      } else if (g_strcmp0(elementName, ALIASINFO_ANYSUBJECT_ELEMENT_NAME) == 0) {
         list->state = ALIAS_PARSE_STATE_ANYSUBJECT;
         /*
          * For empty-tag elements, the Contents code is never called, so set
          * the type here.
          */
         list->aList[list->num - 1].infos[list->aList[list->num - 1].num - 1].type =
            SUBJECT_TYPE_ANY;
      } else if (g_strcmp0(elementName, ALIASINFO_COMMENT_ELEMENT_NAME) == 0) {
         list->state = ALIAS_PARSE_STATE_COMMENT;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      break;
   default:
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Unexpected element '%s' in parse state %d",
                  elementName, list->state);
      break;
   }
}


/*
 ******************************************************************************
 * AliasTextContents --                                                  */ /**
 *
 * Used by the per-user Alias parser.
 *
 * Called by the parser with the contents of an element.
 * Used to store the values.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  text                The contents of the current element
 *                                 (not NUL terminated)
 * @param[in]  textSize            The length of the text.
 * @param[in]  userData            The AliasParseList as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
AliasTextContents(GMarkupParseContext *parseContext,
                  const gchar *text,
                  gsize textSize,
                  gpointer userData,
                  GError **error)
{
   AliasParseList *list = (AliasParseList *) userData;
   gchar *val;
   ServiceAliasInfo *infos;
   int idx;
   gboolean duplicate_found = FALSE;

   ASSERT(list);

   val = g_strndup(text, textSize);

   switch (list->state) {
   case ALIAS_PARSE_STATE_NONE:
   case ALIAS_PARSE_STATE_ALIASES:
   case ALIAS_PARSE_STATE_ALIAS:
   case ALIAS_PARSE_STATE_ALIASINFOS:
   case ALIAS_PARSE_STATE_ALIASINFO:
   case ALIAS_PARSE_STATE_ANYSUBJECT:
      // no-op -- should just be whitespace
      break;
   case ALIAS_PARSE_STATE_PEMCERT:
      if (list->aList[list->num - 1].pemCert != NULL) {
         duplicate_found = TRUE;
         break;
      }
      /*
       * Any extra whitespace in the PEM will confuse the uniqueness
       * check.
       */
      list->aList[list->num - 1].pemCert = g_strstrip(val);
      val = NULL;
      break;
   case ALIAS_PARSE_STATE_COMMENT:
      infos = list->aList[list->num - 1].infos;
      ASSERT(infos);
      idx = list->aList[list->num - 1].num - 1;
      if (infos[idx].comment != NULL) {
         duplicate_found = TRUE;
         break;
      }
      infos[idx].comment = val;
      val = NULL;
      break;
   case ALIAS_PARSE_STATE_SUBJECT:
      infos = list->aList[list->num - 1].infos;
      ASSERT(infos);
      idx = list->aList[list->num - 1].num - 1;
      if (infos[idx].name != NULL) {
         duplicate_found = TRUE;
         break;
      }
      infos[idx].name = val;
      infos[idx].type = SUBJECT_TYPE_NAMED;
      val = NULL;
      break;
   }

   if (duplicate_found) {
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Unexpected contents '%s' in parse state %d",
                  val, list->state);
   }
   g_free(val);
}


/*
 ******************************************************************************
 * AliasEndElement --                                                    */ /**
 *
 * Used by the per-user Alias parser.
 *
 * Called by the XML parser when the end of an element is reached.
 * Used here to pop the parse state.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  elementName         The name of the element being finished.
 * @param[in]  userData            The AliasParseState as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
AliasEndElement(GMarkupParseContext *parseContext,
                const gchar *elementName,
                gpointer userData,
                GError **error)
{
   AliasParseList *list = (AliasParseList *) userData;

   ASSERT(list);

   switch (list->state) {
   case ALIAS_PARSE_STATE_ALIASES:
      list->state = ALIAS_PARSE_STATE_NONE;
      break;
   case ALIAS_PARSE_STATE_ALIAS:
      list->state = ALIAS_PARSE_STATE_ALIASES;
      break;
   case ALIAS_PARSE_STATE_PEMCERT:
   case ALIAS_PARSE_STATE_ALIASINFOS:
      list->state = ALIAS_PARSE_STATE_ALIAS;
      break;
   case ALIAS_PARSE_STATE_ALIASINFO:
      list->state = ALIAS_PARSE_STATE_ALIASINFOS;
      break;
   case ALIAS_PARSE_STATE_SUBJECT:
   case ALIAS_PARSE_STATE_ANYSUBJECT:
   case ALIAS_PARSE_STATE_COMMENT:
      list->state = ALIAS_PARSE_STATE_ALIASINFO;
      break;
   case ALIAS_PARSE_STATE_NONE:
      // XXX error this?
      ASSERT(0);
   }
}


/*
 ******************************************************************************
 * AliasCheckAliasFilePerms --                                           */ /**
 *
 * Verifies the alias file permissions are as expected.
 *
 * @param[in]   fileName        The file to check.
 * @param[in]   userName        Expected owner of the file.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
AliasCheckAliasFilePerms(const gchar *fileName,
                         const gchar *userName)
{
   VGAuthError err;

#ifdef _WIN32
   err = ServiceFileVerifyAdminGroupOwned(fileName);
   if (err == VGAUTH_E_OK) {
      err = ServiceFileVerifyUserAccess(fileName,
                                        userName);
   }
#else
   err = ServiceFileVerifyFileOwnerAndPerms(fileName,
                                            userName,
                                            ALIASSTORE_FILE_PERMS,
                                            NULL, NULL);
#endif

   if (VGAUTH_E_OK != err) {
      Audit_Event(FALSE,
                  SU_(alias.alias.badperm,
                      "Alias store '%s' has incorrect owner"
                      " or permissions.  "
                      "The Aliases for user '%s' will not be available "
                      "for authentication"),
                  fileName, userName);
   }
   return err;
}


/*
 ******************************************************************************
 * AliasCheckMapFilePerms --                                             */ /**
 *
 * Verifies the mapping file permissions are as expected.
 *
 * @param[in]   fileName        The file to check.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
AliasCheckMapFilePerms(const gchar *fileName)
{
   VGAuthError err;

#ifdef _WIN32
   err = ServiceFileVerifyAdminGroupOwned(fileName);
   if (err == VGAUTH_E_OK) {
      err = ServiceFileVerifyEveryoneReadable(fileName);
   }
#else
   err = ServiceFileVerifyFileOwnerAndPerms(fileName,
                                            SUPERUSER_NAME,
                                            ALIASSTORE_MAPFILE_PERMS,
                                            NULL, NULL);
#endif
   if (VGAUTH_E_OK != err) {
      Audit_Event(FALSE,
                  SU_(alias.mapfile.badperm,
                      "Alias store mapping file '%s' has incorrect"
                      " owner or permissions.  "
                      "The Aliases in the mapping file will not be "
                      "available for authentication"),
                  fileName);
   }

   return err;
}


/*
 ******************************************************************************
 * AliasLoadAliases --                                                   */ /**
 *
 * Reads and parses the Alias file for userName.
 *
 * @param[in]   userName        The user whose store is to be loaded.
 * @param[out]  num             The number of certs read.
 * @param[out]  aList           The Aliases read.  The caller should
 *                              call ServiceAliasFreeAliasList() when done.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
AliasLoadAliases(const gchar *userName,
                 int *num,
                 ServiceAlias **aList)
{
   static const GMarkupParser aliasParser = {
      AliasStartElement,
      AliasEndElement,
      AliasTextContents,
      NULL,
      NULL,
   };
   GMarkupParseContext *context = NULL;
   gboolean bRet;
   gchar *fileContents = NULL;
   gsize fileSize;
   gchar *aliasFilename;
   AliasParseList list;
   VGAuthError err;
   GError *gErr = NULL;

   ASSERT(num);
   ASSERT(aList);

   *num = 0;
   *aList = NULL;

   list.state = ALIAS_PARSE_STATE_NONE;
   list.aList = NULL;
   list.num = 0;

   context = g_markup_parse_context_new(&aliasParser, 0, &list, NULL);

   aliasFilename = ServiceUserNameToAliasStoreFileName(userName);

   /*
    * If it's not there, then we have nothing to read.
    */
   if (!g_file_test(aliasFilename, G_FILE_TEST_EXISTS)) {
      goto done;
   }

   err = ServiceLoadFileContents(aliasFilename, userName,
                                 &fileContents, &fileSize);
   if (err != VGAUTH_E_OK) {
      goto done;
   }

   /*
    * Now parse it.  If all goes well, 'list' will be properly
    * populated with the results.
    */
   bRet = g_markup_parse_context_parse(context, fileContents, fileSize, &gErr);
   if (!bRet) {
      Warning("%s: Unable to parse contents of '%s': %s\n", __FUNCTION__,
              aliasFilename, gErr->message);
      g_error_free(gErr);
      err = VGAUTH_E_FAIL;
      goto cleanup;
   }

done:
   /*
    * We're just transferring the data to the caller to free.
    */
   *num = list.num;
   *aList = list.aList;
   err = VGAUTH_E_OK;

cleanup:
   if (err != VGAUTH_E_OK) {
      ServiceAliasFreeAliasList(list.num, list.aList);
   }
   g_markup_parse_context_free(context);
   g_free(aliasFilename);
   g_free(fileContents);
   return err;
}


/*
 ******************************************************************************
 * MappedStartElement --                                                 */ /**
 *
 * Used by the mapping file parser.
 *
 * Called by the XML parser when it sees the start of a new
 * element.  Used to update the current parser state, and allocate
 * any space that may be needed for processing that state.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  elementName         The name of the element being started.
 * @param[in]  attributeNames      The names of any attributes on the element.
 * @param[in]  attributeValues     The values of any attributes on the element.
 * @param[in]  userData            The MappedAliasParseList as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
MappedStartElement(GMarkupParseContext *parseContext,
                   const gchar *elementName,
                   const gchar **attributeNames,
                   const gchar **attributeValues,
                   gpointer userData,
                   GError **error)
{
   MappedAliasParseList *list = (MappedAliasParseList *) userData;
   ServiceSubject *subjs;
   ServiceSubjectType sType = -1;
   int n;

   ASSERT(list);

   switch (list->state) {
   case MAP_PARSE_STATE_NONE:
      if (g_strcmp0(elementName, MAP_MAPPINGS_ELEMENT_NAME) == 0) {
         list->state = MAP_PARSE_STATE_MAPPINGS;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      break;
   case MAP_PARSE_STATE_MAPPINGS:
      /*
       * Starting a new Mapping, alloc and grow.
       */
      if (g_strcmp0(elementName, MAP_MAPPING_ELEMENT_NAME) == 0) {
         list->num++;
         list->maList = g_realloc_n(list->maList,
                                    list->num, sizeof(ServiceMappedAlias));
         list->maList[list->num - 1].pemCert = NULL;
         list->maList[list->num - 1].userName = NULL;
         list->maList[list->num - 1].num = 0;
         list->maList[list->num - 1].subjects = NULL;
         list->state = MAP_PARSE_STATE_MAPPING;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      break;
   case MAP_PARSE_STATE_MAPPING:
      if (g_strcmp0(elementName, ALIASINFO_PEMCERT_ELEMENT_NAME) == 0) {
         list->state = MAP_PARSE_STATE_PEMCERT;
      } else if (g_strcmp0(elementName, MAP_SUBJECTS_ELEMENT_NAME) == 0) {
         list->state = MAP_PARSE_STATE_SUBJECTS;
      } else if (g_strcmp0(elementName, MAP_USERNAME_ELEMENT_NAME) == 0) {
         list->state = MAP_PARSE_STATE_USERNAME;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      break;
   case MAP_PARSE_STATE_SUBJECTS:
      if (g_strcmp0(elementName, MAP_SUBJECT_ELEMENT_NAME) == 0) {
         list->state = MAP_PARSE_STATE_SUBJECT;
         sType = SUBJECT_TYPE_NAMED;
      } else if (g_strcmp0(elementName, MAP_ANYSUBJECT_ELEMENT_NAME) == 0) {
         list->state = MAP_PARSE_STATE_ANYSUBJECT;
         sType = SUBJECT_TYPE_ANY;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, list->state);
      }
      // grow
      n = ++(list->maList[list->num - 1].num);
      subjs = list->maList[list->num - 1].subjects;
      subjs = g_realloc_n(subjs, n, sizeof(ServiceSubject));

      subjs[n - 1].type = sType;
      subjs[n - 1].name = NULL;
      list->maList[list->num - 1].subjects = subjs;
      break;
   default:
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Unexpected element '%s' in parse state %d",
                  elementName, list->state);
      break;
   }
}


/*
 ******************************************************************************
 * MappedTextContents --                                                 */ /**
 *
 * Used by the mapping file parser.
 *
 * Called by the parser with the contents of an element.
 * Used to store the values.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  text                The contents of the current element
 *                                 (not NUL terminated)
 * @param[in]  textSize            The length of the text.
 * @param[in]  userData            The MappedAliasParseList as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
MappedTextContents(GMarkupParseContext *parseContext,
                   const gchar *text,
                   gsize textSize,
                   gpointer userData,
                   GError **error)
{
   MappedAliasParseList *list = (MappedAliasParseList *) userData;
   gchar *val;
   int idx;
   gboolean duplicate_found = FALSE;

   ASSERT(list);

   val = g_strndup(text, textSize);

   switch (list->state) {
   case MAP_PARSE_STATE_NONE:
   case MAP_PARSE_STATE_MAPPINGS:
   case MAP_PARSE_STATE_MAPPING:
   case MAP_PARSE_STATE_SUBJECTS:
   case MAP_PARSE_STATE_ANYSUBJECT:
      // no-op -- should just be whitespace
      break;
   case MAP_PARSE_STATE_PEMCERT:
      if (list->maList[list->num - 1].pemCert != NULL) {
         duplicate_found = TRUE;
         break;
      }
      /*
       * Any extra whitespace in the PEM will confuse the uniqueness
       * check.
       */
      list->maList[list->num - 1].pemCert = g_strstrip(val);
      val = NULL;
      break;
   case MAP_PARSE_STATE_USERNAME:
      if (list->maList[list->num - 1].userName != NULL) {
         duplicate_found = TRUE;
         break;
      }
      list->maList[list->num - 1].userName = val;
      val = NULL;
      break;
   case MAP_PARSE_STATE_SUBJECT:
      idx = list->maList[list->num - 1].num;
      if (list->maList[list->num - 1].subjects[idx - 1].name != NULL) {
         duplicate_found = TRUE;
         break;
      }
      list->maList[list->num - 1].subjects[idx - 1].name = val;
      list->maList[list->num - 1].subjects[idx - 1].type = SUBJECT_TYPE_NAMED;
      val = NULL;
      break;
   }

   if (duplicate_found) {
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Unexpected contents '%s' in parse state %d",
                  val, list->state);
   }
   g_free(val);
}


/*
 ******************************************************************************
 * MappedEndElement --                                                   */ /**
 *
 * Used by the mapping file parser.
 *
 * Called by the XML parser when the end of an element is reached.
 * Used here to pop the parse state.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  elementName         The name of the element being finished.
 * @param[in]  userData            The MappedAliasParseList as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
MappedEndElement(GMarkupParseContext *parseContext,
                 const gchar *elementName,
                 gpointer userData,
                 GError **error)
{
   MappedAliasParseList *list = (MappedAliasParseList *) userData;

   ASSERT(list);

   switch (list->state) {
   case MAP_PARSE_STATE_MAPPINGS:
      list->state = MAP_PARSE_STATE_NONE;
      break;
   case MAP_PARSE_STATE_MAPPING:
      list->state = MAP_PARSE_STATE_MAPPINGS;
      break;
   case MAP_PARSE_STATE_PEMCERT:
   case MAP_PARSE_STATE_SUBJECTS:
   case MAP_PARSE_STATE_USERNAME:
      list->state = MAP_PARSE_STATE_MAPPING;
      break;
   case MAP_PARSE_STATE_SUBJECT:
   case MAP_PARSE_STATE_ANYSUBJECT:
      list->state = MAP_PARSE_STATE_SUBJECTS;
      break;
   case MAP_PARSE_STATE_NONE:
      // XXX set an error here
      ASSERT(0);
   }
}


/*
 ******************************************************************************
 * AliasLoadMapped --                                                    */ /**
 *
 * Reads and parses the mapping file.
 *
 * @param[out]  num             The number of entries read.
 * @param[out]  maList          The ServiceMappedAliases read.  The caller
 *                              should call ServiceAliasFreeMappedAliasList()
 *                              when done.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
AliasLoadMapped(int *num,
                ServiceMappedAlias **maList)
{
   static const GMarkupParser mappedIdParser = {
      MappedStartElement,
      MappedEndElement,
      MappedTextContents,
      NULL,
      NULL,
   };
   GMarkupParseContext *context = NULL;
   gboolean bRet;
   gchar *fileContents = NULL;
   gsize fileSize;
   gchar *mapFilename;
   MappedAliasParseList list;
   VGAuthError err;
   GError *gErr = NULL;

   ASSERT(num);
   ASSERT(maList);

   *num = 0;
   *maList = NULL;

   list.state = MAP_PARSE_STATE_NONE;
   list.maList = NULL;
   list.num = 0;

   context = g_markup_parse_context_new(&mappedIdParser, 0, &list, NULL);

   mapFilename = g_strdup_printf("%s"DIRSEP"%s",
                                 aliasStoreRootDir,
                                 ALIASSTORE_MAPFILE_NAME);

   /*
    * If its not there, then we have nothing to read.
    */
   if (!g_file_test(mapFilename, G_FILE_TEST_EXISTS)) {
      goto done;
   }

   err = ServiceLoadFileContents(mapFilename, NULL,
                                 &fileContents, &fileSize);
   if (err != VGAUTH_E_OK) {
      goto done;
   }

   /*
    * Now parse it.  If all goes well, 'list' will be properly
    * populated with the results.
    */
   bRet = g_markup_parse_context_parse(context, fileContents, fileSize, &gErr);
   if (!bRet) {
      Warning("%s: Unable to parse contents of '%s': %s\n", __FUNCTION__,
              mapFilename, gErr->message);
      g_error_free(gErr);
      err = VGAUTH_E_FAIL;
      goto cleanup;
   }

done:
   /*
    * We're just transferring the certs to the caller to free.
    */
   *num = list.num;
   *maList = list.maList;
   err = VGAUTH_E_OK;

cleanup:
   if (err != VGAUTH_E_OK) {
      ServiceAliasFreeMappedAliasList(list.num, list.maList);
   }
   g_markup_parse_context_free(context);
   g_free(mapFilename);
   g_free(fileContents);
   return err;

   return VGAUTH_E_OK;
}

/*
 ******************************************************************************
 * AliasSafeRenameFiles --                                               */ /**
 *
 * Renames the alias and map files, making sure we don't lose the originals
 * in an error situation.  Since the two files must be in sync, we need
 * to do both at once to allow for restoration on an error.  Note that
 * updating either file is optional, and the caller indicates the need
 * based on a non-NULL filename.
 *
 * @param[in]   srcAliasFile   The alias file being saved.
 * @param[in]   srcMapFile     Any mapping file being saved.
 * @param[in]   userName       The user whose alias is being modified.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
AliasSafeRenameFiles(const gchar *srcAliasFilename,
                     const gchar *srcMapFilename,
                     const gchar *userName)
{
   VGAuthError err;
   gchar *aliasFilename;
   gchar *aliasBackupFilename = NULL;
   gchar *mapFilename = NULL;
   gchar *mapBackupFilename = NULL;


   aliasFilename = ServiceUserNameToAliasStoreFileName(userName);


   /*
    * Do some sanity checks on the files.
    */
   if (NULL != srcAliasFilename) {
      if (g_file_test(aliasFilename, G_FILE_TEST_EXISTS) &&
          !g_file_test(aliasFilename, G_FILE_TEST_IS_REGULAR)) {
         Audit_Event(FALSE,
                     SU_(alias.alias.badfile,
                         "Alias store '%s' exists but is not a regular file.  "
                         "The Aliases for user '%s' will not be available for "
                         "authentication"),
                     aliasFilename, userName);
         Warning("%s: alias store file %s exists but isn't a regular file, punting\n",
                 __FUNCTION__, aliasFilename);
         err = VGAUTH_E_FAIL;
         goto done;
      }
   }

   if (NULL != srcMapFilename) {
      mapFilename = g_strdup_printf("%s"DIRSEP"%s",
                                    aliasStoreRootDir,
                                    ALIASSTORE_MAPFILE_NAME);

      if (g_file_test(mapFilename, G_FILE_TEST_EXISTS) &&
          !g_file_test(mapFilename, G_FILE_TEST_IS_REGULAR)) {
         Audit_Event(FALSE,
                     SU_(alias.mapping.badfile,
                         "Mapping file '%s' exists but is not a regular file.  "
                         "The Aliases in the mapping file will not be "
                         "available for authentication"),
                     mapFilename);
         Warning("%s: map file %s exists but isn't a regular file, punting\n",
                 __FUNCTION__, mapFilename);
         err = VGAUTH_E_FAIL;
         goto done;
      }
   }


   /*
    * Back up the real files so we can recover on an error.
    */
   if (NULL != srcAliasFilename) {
      if (g_file_test(aliasFilename, G_FILE_TEST_EXISTS)) {
         aliasBackupFilename = g_strdup_printf("%s.bak", aliasFilename);
         if (ServiceFileRenameFile(aliasFilename, aliasBackupFilename) < 0) {
            err = VGAUTH_E_FAIL;
            goto done;
         }
      }
   }

   if (NULL != srcMapFilename) {
      if (g_file_test(mapFilename, G_FILE_TEST_EXISTS)) {
         mapBackupFilename = g_strdup_printf("%s.bak", mapFilename);
         if (ServiceFileRenameFile(mapFilename, mapBackupFilename) < 0) {
            err = VGAUTH_E_FAIL;
            goto restore;
         }
      }
   }


   /*
    * Now rename the passed-in files as the official.
    */

   if (NULL != srcAliasFilename) {
      if (ServiceFileRenameFile(srcAliasFilename, aliasFilename) < 0) {
         err = VGAUTH_E_FAIL;
         goto restore;
      }
   }
   if (NULL != srcMapFilename) {
      if (ServiceFileRenameFile(srcMapFilename, mapFilename) < 0) {
         err = VGAUTH_E_FAIL;
         goto restore;
      }
   }

   /*
    * Clean up backups.
    */
   if (aliasBackupFilename && ServiceFileUnlinkFile(aliasBackupFilename) < 0) {
      /* XXX not much to do -- ServiceFileUnlinkFile() spewed error */
   }

   if (mapBackupFilename && ServiceFileUnlinkFile(mapBackupFilename) < 0) {
      /* XXX not much to do -- ServiceFileUnlinkFile() spewed error */
   }

   err = VGAUTH_E_OK;

   /*
    * It all worked, skip the restore section to the cleanup.
    */
   goto done;

restore:
   /*
    * Something went wrong, try to recover.  If this fails,
    * all we can do is whine about it.
    */
   Warning("%s: trying to restore files\n", __FUNCTION__);
   if (ServiceFileRenameFile(aliasBackupFilename, aliasFilename) < 0) {
      Warning("%s: failed to restore %s\n", __FUNCTION__, aliasFilename);
   }

   if (NULL != srcMapFilename) {
      if (ServiceFileRenameFile(mapBackupFilename, mapFilename) < 0) {
         Warning("%s: failed to restore %s\n", __FUNCTION__, mapFilename);
      }
   }

done:
   g_free(aliasFilename);
   g_free(mapFilename);
   g_free(aliasBackupFilename);
   g_free(mapBackupFilename);

   return err;
}


/*
 ******************************************************************************
 * AliasSaveAliasesAndMapped --                                          */ /**
 *
 * Store a list of Aliases in a store.  The mapping file
 * is modified at the same time, since it must stay in sync.
 * Ensures permissions are correct.
 *
 * @param[in]   userName        The user whose store is to be used.
 * @param[in]   num             The number of entries to store.
 * @param[in]   aList           The list of Aliases to store.
 * @param[in]   updateMap       True if the mapfile needs to be updated.
 * @param[in]   numMapped       The number of mapfile entries.
 * @param[in]   maList          The list of MappedAliases to store.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
AliasSaveAliasesAndMapped(const gchar *userName,
                          int num,
                          ServiceAlias *aList,
                          gboolean updateMap,
                          int numMapped,
                          ServiceMappedAlias *maList)
{
   VGAuthError err = VGAUTH_E_OK;
   FILE *fp = NULL;
   int fd;
   gchar *tmpAliasFilename = NULL;
   gchar *tmpMapFilename = NULL;
   int rc;
   gboolean emptyAliasFile = (num == 0);

   /*
    * Special case for empty lists.
    *
    * If the aList is empty, and we can't look up the user,
    * just remove it.
    *
    * This solves an issue with the 'deleted user' unit test.
    * Otherwise the tmp user can have a different uid/Sid
    * (especially on Windows) and think there's a security issue.
    */
   if (emptyAliasFile) {
      gchar *aliasFilename = ServiceUserNameToAliasStoreFileName(userName);
      if (ServiceFileUnlinkFile(aliasFilename)) {
         /* XXX not much to do -- ServiceFileUnlinkFile() spewed error */
      }
      Debug("%s: removed empty alias file '%s'\n", __FUNCTION__, aliasFilename);
      g_free(aliasFilename);
      goto updateMap;
   }

   /*
    * Make a tmpfile for the new alias file, so we can
    * recover if something goes wrong.
    */
   tmpAliasFilename = ServiceUserNameToTmpAliasStoreFileName(userName);

#ifdef _WIN32
   /*
    * Create file with the right permissions.
    * It seemed simpler to let glib open the file then set the permission.
    * However, that would open a security hole, where malicious user
    * could have a window to modify the file.
    */
   {
      UserAccessControl uac;
      /* The default access only allows self and administrators */
      if (!UserAccessControl_Default(&uac)) {
         err = VGAUTH_E_FAIL;
         goto cleanup;
      }

      fd = ServiceFileWinMakeTempfile(&tmpAliasFilename, &uac);
      UserAccessControl_Destroy(&uac);
   }
#else
   fd = ServiceFilePosixMakeTempfile(tmpAliasFilename,
                                     ALIASSTORE_FILE_PERMS);
#endif

   if (fd < 0) {
      err = VGAUTH_E_FAIL;
      goto cleanup;
   }

   /*
    * We want to be sure this file (and the mapping below) is flushed
    * to disk asap, to minmize file corruption issues due to unexpected
    * OS failure.
    *
    * fflush()/fsync() should do it on Linux, but Windows wants a M$-private
    * magic flag plus fflush().  Windows flags are:
    * - 'w' write
    * - 'b' binary (don't do CR/LF translation)
    * - 'c' commit to disk on fflush or _flushall
    *
    * Another glib-ish option would be to use g_set_file_contents(),
    * which also uses fsync() when it can.  However, this doesn't
    * seem to use the 'c'/fflush for Windows, so it appears to be
    * a poor option.  Also, it uses its own tmpfile which it sets
    * to mode 0666.
    */
#ifdef WIN32
   fp = fdopen(fd, "wbc");
#else
   fp = fdopen(fd, "w");
#endif
   if (NULL == fp) {
      VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
      Warning("%s: fdopen() failed\n", __FUNCTION__);
#ifdef _WIN32
      _close(fd);
#else
      close(fd);
#endif
      goto cleanup;
   }
   err = AliasDumpAliases(fp, num, aList);
   if (err != VGAUTH_E_OK) {
      goto cleanup;
   }

   if (fflush(fp) != 0) {
      VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
      Warning("%s: fflush() failed\n", __FUNCTION__);
      goto cleanup;
   }
#ifndef WIN32
   if (fsync(fileno(fp)) != 0) {
      VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
      Warning("%s: fsync() failed\n", __FUNCTION__);
      goto cleanup;
   }
#endif
   rc = fclose(fp);
   fp = NULL;
   if (rc != 0) {
      VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
      Warning("%s: fclose() failed\n", __FUNCTION__);
      goto cleanup;
   }

   /*
    * Make the file owned by the user so they can hand-edit.
    * Note that this may fail for a couple reasons -- the user
    * may have been deleted, or we just can't look it up because
    * of a network issue when using LDAP/NIS/etc.
    *
    * If the user is deleted, we could just leave the file owned
    * by SUPERUSER.  But the user may just be inaccesible, so copy
    * the perms from the current version of the file.
    */

#ifdef _WIN32
   /*
    * On Windows, the cert files are owned by the Administrators group.
    * We only need to grant the user in the ACL.
    */
   if (UsercheckUserExists(userName)) {
      UserAccessControl uac;
      BOOL ok;

      if (!UserAccessControl_GrantUser(&uac, userName, GENERIC_ALL)) {
         err = VGAUTH_E_FAIL;
         goto cleanup;
      }

      ok = WinUtil_SetFileAcl(tmpAliasFilename,
                              UserAccessControl_GetAcl(&uac));
      UserAccessControl_Destroy(&uac);

      if (!ok) {
         err = VGAUTH_E_FAIL;
         goto cleanup;
      }

   } else {
      BOOL ok;
      gchar *origAliasFilename = ServiceUserNameToAliasStoreFileName(userName);

      ok = WinUtil_CopyFileAcl(origAliasFilename, tmpAliasFilename);
      g_free(origAliasFilename);

      if (!ok) {
         err = VGAUTH_E_FAIL;
         goto cleanup;
      }

   }
#else
   /*
    * On Linux we have to set the ownership of the file to be the user.
    */

   if (UsercheckUserExists(userName)) {
      err = ServiceFileSetOwner(tmpAliasFilename, userName);
      if (err != VGAUTH_E_OK) {
         goto cleanup;
      }
   } else {
      gchar *origAliasFilename = ServiceUserNameToAliasStoreFileName(userName);

      err = ServiceFileCopyOwnership(origAliasFilename, tmpAliasFilename);
      g_free(origAliasFilename);

      if (VGAUTH_E_OK != err) {
         goto cleanup;
      }
   }
#endif

updateMap:
   if (updateMap) {
      /*
       * Special case for empty map files -- if its empty, just remove it.
       */
      if (numMapped == 0) {
         gchar *mapFilename = g_strdup_printf("%s"DIRSEP"%s",
                                              aliasStoreRootDir,
                                              ALIASSTORE_MAPFILE_NAME);
         if (ServiceFileUnlinkFile(mapFilename)) {
            /* XXX not much to do -- ServiceFileUnlinkFile() spewed error */
         }
         Debug("%s: removed empty map file '%s'\n", __FUNCTION__, mapFilename);
         g_free(mapFilename);

         goto done;
      }

      tmpMapFilename = g_strdup_printf("%s"DIRSEP"%sXXXXXX",
                                       aliasStoreRootDir,
                                       ALIASSTORE_MAPFILE_NAME);
#ifdef _WIN32
      {
         UserAccessControl uac;
         /* The default access only allows self and administrators */
         if(!UserAccessControl_Default(&uac)) {
            err = VGAUTH_E_FAIL;
            goto cleanup;
         }
         fd = ServiceFileWinMakeTempfile(&tmpMapFilename, &uac);
         UserAccessControl_Destroy(&uac);
      }
#else
      fd = ServiceFilePosixMakeTempfile(tmpMapFilename,
                                        ALIASSTORE_MAPFILE_PERMS);
#endif

      if (fd < 0) {
         err = VGAUTH_E_FAIL;
         goto cleanup;
      }

#ifdef WIN32
      fp = fdopen(fd, "wbc");
#else
      fp = fdopen(fd, "w");
#endif
      if (NULL == fp) {
         VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
         Warning("%s: fdopen() failed\n", __FUNCTION__);
#ifdef _WIN32
         _close(fd);
#else
         close(fd);
#endif
         goto cleanup;
      }
      err = AliasDumpMappedAliasesFile(fp, numMapped, maList);
      if (err != VGAUTH_E_OK) {
         goto cleanup;
      }

      if (fflush(fp) != 0) {
         VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
         Warning("%s: fflush() failed\n", __FUNCTION__);
         goto cleanup;
      }
#ifndef WIN32
      if (fsync(fileno(fp)) != 0) {
         VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
         Warning("%s: fsync() failed\n", __FUNCTION__);
         goto cleanup;
      }
#endif
      rc = fclose(fp);
      fp = NULL;
      if (rc != 0) {
         VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
         Warning("%s: fclose() failed\n", __FUNCTION__);
         goto cleanup;
      }

#ifdef _WIN32
      /*
       * On Windows, the map file is owned by the Administrators group.
       * We make it world readable to match the Unix group/other readable
       * permission.
       */
      {
         UserAccessControl uac;
         BOOL ok;

         if (!UserAccessControl_GrantEveryone(&uac, GENERIC_READ)) {
            err = VGAUTH_E_FAIL;
            goto cleanup;
         }

         ok = WinUtil_SetFileAcl(tmpMapFilename,
                                 UserAccessControl_GetAcl(&uac));
         UserAccessControl_Destroy(&uac);

         if (!ok) {
            err = VGAUTH_E_FAIL;
            goto cleanup;
         }
      }
#endif
   }

   /*
    * Make the tmpfiles become the real in a way we can try to recover from.
    */
   err = AliasSafeRenameFiles(tmpAliasFilename, tmpMapFilename, userName);
   if (err != VGAUTH_E_OK) {
      goto cleanup;
   }

   goto done;

cleanup:
   if (fp != NULL) {
      fclose(fp);
   }

   /*
    * If we fail somwhere, all we can do is try to clean up.
    * If this fails, there's nothing we can do.
    */
   if (ServiceFileUnlinkFile(tmpAliasFilename)) {
      /* XXX not much to do -- ServiceFileUnlinkFile() spewed error */
   }
   if (tmpMapFilename && ServiceFileUnlinkFile(tmpMapFilename)) {
      /* XXX not much to do -- ServiceFileUnlinkFile() spewed error */
   }

done:
   g_free(tmpAliasFilename);
   g_free(tmpMapFilename);
   return err;
}


/*
 ******************************************************************************
 * ServiceAliasCopyAliasInfoContents --                                  */ /**
 *
 * Copies the contents of a ServiceAliasInfo *.
 *
 * @param[in]   src    The source ServiceAliasInfo.
 * @param[in]   dst    The dest ServiceAliasInfo.
 ******************************************************************************
 */

void
ServiceAliasCopyAliasInfoContents(const ServiceAliasInfo *src,
                                  ServiceAliasInfo *dst)
{
   dst->type = src->type;
   dst->name = g_strdup(src->name);
   dst->comment = g_strdup(src->comment);
}


/*
 ******************************************************************************
 * ServiceAliasFreeAliasInfoContents --                                  */ /**
 *
 * Frees the contents of a ServiceAliasInfo *.
 *
 * @param[in]   si     The ServiceAliasInfo to free.
 ******************************************************************************
 */

void
ServiceAliasFreeAliasInfoContents(ServiceAliasInfo *ai)
{
   g_free(ai->name);
   g_free(ai->comment);
}


/*
 ******************************************************************************
 * ServiceAliasFreeAliasInfo --                                          */ /**
 *
 * Frees a ServiceAliasInfo *.
 *
 * @param[in]   ai     The ServiceAliasInfo to free.
 ******************************************************************************
 */

void
ServiceAliasFreeAliasInfo(ServiceAliasInfo *ai)
{
   if (NULL == ai) {
      return;
   }
   ServiceAliasFreeAliasInfoContents(ai);

   g_free(ai);
}


/*
 ******************************************************************************
 * ServiceAliasFreeAliasListContents --                                  */ /**
 *
 * Frees the contents of a ServiceAlias.
 *
 * @param[in]   sa     The ServiceAlias.
 ******************************************************************************
 */

void
ServiceAliasFreeAliasListContents(ServiceAlias *sa)
{
   int i;

   g_free(sa->pemCert);
   for (i = 0; i < sa->num; i++) {
      g_free(sa->infos[i].name);
      g_free(sa->infos[i].comment);
   }
   g_free(sa->infos);
}


/*
 ******************************************************************************
 * ServiceAliasFreeAliasList --                                          */ /**
 *
 * Frees an array of ServiceAlias.
 *
 * @param[in]   num     The size of the array.
 * @param[in]   aList   The list of ServiceAlias.
 ******************************************************************************
 */

void
ServiceAliasFreeAliasList(int num,
                          ServiceAlias *aList)
{
   int i;

   for (i = 0; i < num; i++) {
      ServiceAliasFreeAliasListContents(&(aList[i]));
   }
   g_free(aList);
}


/*
 ******************************************************************************
 * ServiceAliasFreeMappedAliasListContents --                            */ /**
 *
 * Frees the contents of a ServiceMappedAlias.
 *
 * @param[in]   ma      The ServiceMappedAlias.
 ******************************************************************************
 */

void
ServiceAliasFreeMappedAliasListContents(ServiceMappedAlias *ma)
{
   int i;

   g_free(ma->pemCert);
   for (i = 0; i < ma->num; i++) {
      if (SUBJECT_TYPE_NAMED == ma->subjects[i].type) {
         g_free(ma->subjects[i].name);
      }
   }
   g_free(ma->subjects);
   g_free(ma->userName);
}


/*
 ******************************************************************************
 * ServiceAliasFreeMappedAliasList --                                    */ /**
 *
 * Frees an array of ServiceMappedAlias.
 *
 * @param[in]   num     The size of the array.
 * @param[in]   ma      The list of ServiceMappedAlias.
 ******************************************************************************
 */

void
ServiceAliasFreeMappedAliasList(int num,
                                ServiceMappedAlias *ma)
{
   int i;

   for (i = 0; i < num; i++) {
      ServiceAliasFreeMappedAliasListContents(&(ma[i]));
   }
   g_free(ma);
}


/*
 ******************************************************************************
 * ServiceAliasAddAlias --                                               */ /**
 *
 * Adds a certificate and AliasInfo to userName's store.
 *
 * @param[in]   reqUserName     The user making the request.
 * @param[in]   userName        The user whose store is to be used.
 * @param[in]   addMapped       Set if a link is also to be made in the
 *                              mapping file.
 * @param[in]   pemCert         The cert to be stored.
 * @param[in]   ai              The associated AliasInfo.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceAliasAddAlias(const gchar *reqUserName,
                     const gchar *userName,
                     gboolean addMapped,
                     const gchar *pemCert,
                     ServiceAliasInfo *ai)
{
   VGAuthError err;
   int num;
   ServiceAlias *aList = NULL;
   ServiceAliasInfo *newAi;
   int numMapped = 0;
   ServiceMappedAlias *maList = NULL;
   gboolean updatedMapList = FALSE;
   int matchCertIdx = -1;
   int i;
   int j;

   if (!UsercheckUserExists(userName)) {
      Debug("%s: no such user '%s'\n", __FUNCTION__, userName);
      return VGAUTH_E_NO_SUCH_USER;
   }

   if (!CertVerify_IsWellFormedPEMCert(pemCert)) {
      return VGAUTH_E_INVALID_CERTIFICATE;
   }

   /*
    * Load any that already exist.
    */
   err = AliasLoadAliases(userName, &num, &aList);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   /*
    * Do a dup check.  This may be overly simplistic, and it can be
    * tricked by whitespace changes in some situations.
    */
   for (i = 0; i < num; i++) {
      if (ServiceComparePEMCerts(pemCert, aList[i].pemCert)) {
         matchCertIdx = i;
         for (j = 0; j < aList[i].num; j++) {
            if (ServiceAliasIsSubjectEqual(aList[i].infos[j].type, ai->type,
                                           aList[i].infos[j].name, ai->name)) {
               Debug("%s: client tried to add a duplicate subject '%s' for user '%s'\n",
                     __FUNCTION__,
                     (ai->type == SUBJECT_TYPE_ANY) ? "<ANY>" : ai->name,
                     userName);
               // this is a no-op, but we may have a mapFile change
               err = VGAUTH_E_OK;
               if (addMapped) {
                  goto check_map;
               } else {
                  goto done;
               }
            }
         }
      }
   }

   /*
    * If the cert already exists, just add the subject to it.
    */
   if (matchCertIdx != -1) {
      aList[matchCertIdx].num++;
      aList[matchCertIdx].infos = g_realloc_n(aList[matchCertIdx].infos,
                                              aList[matchCertIdx].num,
                                              sizeof(ServiceAliasInfo));
      newAi = &(aList[matchCertIdx].infos[aList[matchCertIdx].num - 1]);
      newAi->type = ai->type;
      newAi->name = g_strdup(ai->name);
      newAi->comment = g_strdup(ai->comment);
   } else {
      /*
       * If its a new cert, add the incoming cert and subject to our list.
       */
      aList = g_realloc_n(aList, num + 1, sizeof(ServiceAlias));
      aList[num].pemCert = g_strdup(pemCert);
      aList[num].num = 1;
      aList[num].infos = g_malloc0(sizeof(ServiceAliasInfo));
      newAi = &(aList[num].infos[0]);
      num++;
      newAi->type = ai->type;
      newAi->name = g_strdup(ai->name);
      newAi->comment = g_strdup(ai->comment);
   }

check_map:
   if (addMapped) {
      err = AliasLoadMapped(&numMapped, &maList);

      /*
       * Do a dup check -- be sure the cert/subject combo
       * doesn't already exist.
       */
      for (i = 0; i < numMapped; i++) {
         if (ServiceComparePEMCerts(pemCert, maList[i].pemCert))  {
            for (j = 0; j < maList[i].num; j++) {
               if (ServiceAliasIsSubjectEqual(maList[i].subjects[j].type, ai->type,
                                              maList[i].subjects[j].name, ai->name)) {
                  Debug("%s: client tried to add a duplicate mapping entry for "
                        "subject '%s' and cert '%s'\n",
                        __FUNCTION__,
                        (ai->type == SUBJECT_TYPE_ANY) ? "<ANY>" : ai->name,
                        maList[i].pemCert);
                  err = VGAUTH_E_MULTIPLE_MAPPINGS;
                  goto done;
               }
            }
            // if its a new subject for the same user, add it
            if (g_strcmp0(maList[i].userName, userName) == 0) {
               // additional subject for cert/username pair
               maList[i].num++;
               maList[i].subjects = g_realloc_n(maList[i].subjects,
                                                maList[i].num,
                                                sizeof(ServiceSubject));
               maList[i].subjects[maList[i].num - 1].type = ai->type;
               maList[i].subjects[maList[i].num - 1].name = g_strdup(ai->name);
               updatedMapList = TRUE;
            }
         }
      }

      /*
       * If new, add it to the array.
       */
      if (!updatedMapList) {
         maList = g_realloc_n(maList,
                              numMapped + 1,
                              sizeof(ServiceMappedAlias));
         maList[numMapped].pemCert = g_strdup(pemCert);
         maList[numMapped].userName = g_strdup(userName);
         maList[numMapped].num = 1;
         maList[numMapped].subjects = g_malloc0(sizeof(ServiceSubject));
         maList[numMapped].subjects[0].type = ai->type;
         maList[numMapped].subjects[0].name = g_strdup(ai->name);
         numMapped++;
      }
   }

   err = AliasSaveAliasesAndMapped(userName, num, aList,
                                   addMapped, numMapped, maList);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to save Aliases\n", __FUNCTION__);
   } else {
      Audit_Event(TRUE,
                 SU_(alias.addid,
                     "Alias added to Alias store owned by '%s' by user '%s'"),
                 userName, reqUserName);
      // security -- don't expose username
      VMXLog_Log(VMXLOG_LEVEL_WARNING,
                 "%s: alias added with Subject '%s'",
                 __FUNCTION__,
                 (ai->type == SUBJECT_TYPE_ANY) ? "<ANY>" : ai->name);
   }

done:

   ServiceAliasFreeAliasList(num, aList);
   if (addMapped) {
      ServiceAliasFreeMappedAliasList(numMapped, maList);
   }

   return err;
}


/*
 ******************************************************************************
 * AliasShrinkAliasList --                                               */ /**
 *
 * Helper function for shrinking a ServiceAlias list.
 *
 * @param[in]    aList          The list of ServiceAliases.
 * @param[in]    num            The new size.
 * @param[in]    idx            Where to start the shrink.
 ******************************************************************************
 */

static void
AliasShrinkAliasList(ServiceAlias *aList,
                     int num,
                     int idx)
{
   ASSERT(idx <= num);

   memmove(&(aList[idx]), &(aList[idx+1]), sizeof(ServiceAlias) *
                          (num - idx));
#ifdef VMX86_DEBUG
   memset(&(aList[num]), '\0', sizeof(ServiceAlias));
#endif
}


/*
 ******************************************************************************
 * AliasShrinkMapList --                                                 */ /**
 *
 * Helper function for shrinking a ServiceMappedAlias list.
 *
 * @param[in]    maList         The list of ServiceMappedAlias.
 * @param[in]    num            The new size.
 * @param[in]    idx            Where to start the shrink.
 ******************************************************************************
 */

static void
AliasShrinkMapList(ServiceMappedAlias *maList,
                   int num,
                   int idx)
{
   ASSERT(idx <= num);

   memmove(&(maList[idx]), &(maList[idx+1]), sizeof(ServiceMappedAlias) *
                          (num - idx));
#ifdef VMX86_DEBUG
   memset(&(maList[num]), '\0', sizeof(ServiceMappedAlias));
#endif
}


/*
 ******************************************************************************
 * ServiceAliasRemoveAlias --                                            */ /**
 *
 * Removes a cert/Subject from userName's store.
 *
 * @param[in]   reqUserName     The user making the request.
 * @param[in]   userName        The user whose store is to be used.
 * @param[in]   pemCert         The cert to be removed in PEM format.
 * @param[in]   subj            The subject.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceAliasRemoveAlias(const gchar *reqUserName,
                        const gchar *userName,
                        const gchar *pemCert,
                        ServiceSubject *subj)
{
   VGAuthError err;
   VGAuthError savedErr = VGAUTH_E_OK;
   int numIds;
   ServiceAlias *aList = NULL;
   gboolean removeAll = (subj->type == SUBJECT_TYPE_UNSET);
   int numMapped = 0;
   ServiceMappedAlias *maList = NULL;
   ServiceAliasInfo *ai;
   ServiceSubject *s;
   int i;
   int j;
   int userIdIdx = -1;
   int userSubjIdx = -1;
   gboolean updateMap = FALSE;

   /*
    * We don't verify the user exists in a Remove operation, to allow
    * cleanup of deleted user's stores.
    */

   if (!CertVerify_IsWellFormedPEMCert(pemCert)) {
      return VGAUTH_E_INVALID_CERTIFICATE;
   }

   /*
    * Load user's store.
    */
   err = AliasLoadAliases(userName, &numIds, &aList);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   if (removeAll) {
      for (i = 0; i < numIds; i++) {
         if (ServiceComparePEMCerts(pemCert, aList[i].pemCert)) {
            userIdIdx = i;
            break;
         }
      }
      // nothing found
      if (-1 == userIdIdx) {
         err = VGAUTH_E_INVALID_ARGUMENT;
         goto done;
      }

      ServiceAliasFreeAliasListContents(&(aList[userIdIdx]));
      numIds--;
      AliasShrinkAliasList(aList, numIds, userIdIdx);
   } else {
      // removing a specific subject
      for (i = 0; i < numIds; i++) {
         if (ServiceComparePEMCerts(pemCert, aList[i].pemCert)) {
            userIdIdx = i;
            for (j = 0; j < aList[i].num; j++) {
               ai = &(aList[i].infos[j]);
               if (ServiceAliasIsSubjectEqual(ai->type, subj->type,
                                              ai->name, subj->name)) {
                  userSubjIdx = j;
                  break;
               }
            }

            if (userSubjIdx >= 0) {
               g_free(aList[userIdIdx].infos[userSubjIdx].name);
               g_free(aList[userIdIdx].infos[userSubjIdx].comment);
               aList[userIdIdx].num--;
               ASSERT(aList[userIdIdx].num >= 0);
               memmove(&(aList[userIdIdx].infos[userSubjIdx]),
                       &(aList[userIdIdx].infos[userSubjIdx+1]),
                       sizeof(ServiceAliasInfo) *
                              (aList[userIdIdx].num - userSubjIdx));
#ifdef VMX86_DEBUG
               memset(&(aList[userIdIdx].infos[aList[userIdIdx].num]), '\0',
                         sizeof(ServiceAliasInfo));
#endif
            } else {
               err = VGAUTH_E_INVALID_ARGUMENT;
               goto done;
            }
            // if all the subjects are gone, toss the whole thing
            if (aList[i].num == 0) {
               numIds--;
               ServiceAliasFreeAliasListContents(&(aList[i]));
               AliasShrinkAliasList(aList, numIds, userIdIdx);
            }
            break;
         }
      }
      if (-1 == userIdIdx) {
         /*
          * No match, but continue on through the mapped code
          * in case we have a orphaned mapped alias from a buggy
          * earlier version.
          */
         savedErr = VGAUTH_E_INVALID_ARGUMENT;
      }
   }

   /*
    * Now clear out any mapped alias.  This may fail to find a match.
    */
   err = AliasLoadMapped(&numMapped, &maList);
   if (VGAUTH_E_OK != err) {
      goto done;
   }

   if (numMapped == 0) {
      goto update;
   }

   if (removeAll) {
      userIdIdx = -1;
      for (i = 0; i < numMapped; i++) {
         if (ServiceComparePEMCerts(pemCert, maList[i].pemCert) &&
             (g_strcmp0(userName, maList[i].userName) == 0)) {
            userIdIdx = i;
            break;
         }
      }
      // nothing found -- not an error for mapping, but be sure to update
      if (-1 == userIdIdx) {
         goto update;
      }

      ServiceAliasFreeMappedAliasListContents(&(maList[userIdIdx]));
      numMapped--;
      AliasShrinkMapList(maList, numMapped, userIdIdx);
      updateMap = TRUE;
   } else {
      // removing a specific subject
      for (i = 0; i < numMapped; i++) {
         userIdIdx = -1;
         userSubjIdx = -1;
         if (ServiceComparePEMCerts(pemCert, maList[i].pemCert)) {
            userIdIdx = i;
            for (j = 0; j < maList[i].num; j++) {
               s = &(maList[i].subjects[j]);
               if (ServiceAliasIsSubjectEqual(s->type, subj->type,
                                              s->name, subj->name)) {
                  userSubjIdx = j;
                  break;
               }
            }

            if (userSubjIdx >= 0) {
               g_free(maList[userIdIdx].subjects[userSubjIdx].name);
               maList[userIdIdx].num--;
               ASSERT(maList[userIdIdx].num >= 0);
               memmove(&(maList[userIdIdx].subjects[userSubjIdx]),
                       &(maList[userIdIdx].subjects[userSubjIdx+1]),
                       sizeof(ServiceSubject) *
                              (maList[userIdIdx].num - userSubjIdx));
#ifdef VMX86_DEBUG
               memset(&(maList[userIdIdx].subjects[maList[userIdIdx].num]), '\0',
                         sizeof(ServiceSubject));
#endif
               // if all the subjects are gone, toss the whole thing
               if (maList[i].num == 0) {
                  numMapped--;
                  ServiceAliasFreeMappedAliasListContents(&(maList[i]));
                  AliasShrinkMapList(maList, numMapped, userIdIdx);
               }
               updateMap = TRUE;
               break;
            }
         }
      }
   }

update:
   err = AliasSaveAliasesAndMapped(userName, numIds, aList,
                                   updateMap, numMapped, maList);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to save Aliases\n", __FUNCTION__);
   } else {
      Audit_Event(TRUE,
                 SU_(alias.removeid,
                     "Alias removed from Alias store owned by '%s' by user '%s'"),
                 userName, reqUserName);
      if (removeAll) {
         // security -- don't expose username
         VMXLog_Log(VMXLOG_LEVEL_WARNING,
                    "%s: all aliases removed for requested username",
                    __FUNCTION__);
      } else {
         // security -- don't expose username
         VMXLog_Log(VMXLOG_LEVEL_WARNING,
                    "%s: alias removed with Subject '%s'",
                    __FUNCTION__,
                    (subj->type == SUBJECT_TYPE_ANY) ? "<ANY>" : subj->name);
      }
   }

done:
   ServiceAliasFreeAliasList(numIds, aList);
   ServiceAliasFreeMappedAliasList(numMapped, maList);

   /*
    * If we couldn't find the alias, but dropped though to the mapped
    * code anyways to catch orphans, return that error.
    */
   if (savedErr != VGAUTH_E_OK) {
      return savedErr;
   } else {
      return err;
   }
}


/*
 ******************************************************************************
 * ServiceAliasQueryAliases --                                           */ /**
 *
 * Queries all aliases from userName's store.
 *
 * @param[in]   userName        The user whose store is to be used.
 * @param[out]  num             The number of entries being returned.
 * @param[out]  aList           The entries being returned.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceAliasQueryAliases(const gchar *userName,
                         int *num,
                         ServiceAlias **aList)
{
   VGAuthError err;

   *num = 0;
   *aList = NULL;

#if 0
   /*
    * XXX
    * This is tricky.  The first reaction is that we should return an error.
    * But this breaks the "admin cleaning up after a deleted user"
    * scenario.  See bug 920481.
    *
    * One possible compromise is to return VGAUTH_E_NO_SUCH_USER
    * IFF there's no data, but that feels sleazy too.
    */
   if (!UsercheckUserExists(userName)) {
      Debug("%s: no such user '%s'\n", __FUNCTION__, userName);
      return VGAUTH_E_NO_SUCH_USER;
   }
#endif

   err = AliasLoadAliases(userName, num, aList);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to load Aliases for '%s'\n", __FUNCTION__, userName);
   }

   return err;
}


/*
 ******************************************************************************
 * ServiceAliasQueryMappedAliases --                                     */ /**
 *
 * Returns the contents of the mapping file.
 *
 * @param[out]  num             The number of entries being returned.
 * @param[out]  maList          The ServiceMappedAliases being returned.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceAliasQueryMappedAliases(int *num,
                               ServiceMappedAlias **maList)
{
   VGAuthError err;

   *num = 0;
   *maList = NULL;

   err = AliasLoadMapped(num, maList);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to load mapped aliases\n", __FUNCTION__);
   }

   return err;
}


/*
 ******************************************************************************
 * ServiceIDVerifyStoreContents --                                       */ /**
 *
 * Looks at every file in the alias store, validating ownership and permissions.
 * If a file fails the validation check, its renamed to file.bad.
 *
 * If we fail to rename a bad file, the function returns an error.
 *
 * XXX add contents/XML sanity check too?
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
ServiceIDVerifyStoreContents(void)
{
   VGAuthError err;
   GDir *dir;
   GError *gErr;
   const gchar *fileName;
   gchar *fullFileName;
   gboolean saveBadFile = FALSE;

   dir = g_dir_open(aliasStoreRootDir, 0, &gErr);
   if (NULL == dir) {
      Warning("%s: failed to open alias store %s: %s\n",
              __FUNCTION__, aliasStoreRootDir, gErr->message);
      g_error_free(gErr);
      return VGAUTH_E_FAIL;
   }

   while ((fileName = g_dir_read_name(dir)) != NULL) {
      fullFileName = g_strdup_printf("%s"DIRSEP"%s",
                                     aliasStoreRootDir,
                                     fileName);
      // mapping file is special
      if (g_strcmp0(ALIASSTORE_MAPFILE_NAME, fileName) == 0) {
         err = AliasCheckMapFilePerms(fullFileName);
         if (VGAUTH_E_OK != err) {
            saveBadFile = TRUE;
         }
      } else if (g_str_has_prefix(fileName, ALIASSTORE_FILE_PREFIX) &&
                 g_str_has_suffix(fileName, ALIASSTORE_FILE_SUFFIX)) {
         gchar *userName;
         gchar *startUserName;
         gchar *suffix;
         gchar *decodedUserName;

         startUserName = g_strdup(fileName);
         userName = startUserName;
         userName += strlen(ALIASSTORE_FILE_PREFIX);
         suffix = g_strrstr(userName, ALIASSTORE_FILE_SUFFIX);
         ASSERT(suffix);
         *suffix = '\0';

         decodedUserName = ServiceDecodeUserName(userName);
         err = AliasCheckAliasFilePerms(fullFileName, decodedUserName);
         if (VGAUTH_E_OK != err) {
            saveBadFile = TRUE;
         }
         g_free(decodedUserName);
         g_free(startUserName);
      }

      if (saveBadFile) {
         gchar *badFileName = g_strdup_printf("%s.bad",
                                              fullFileName);
         err = ServiceFileRenameFile(fullFileName, badFileName);
         if (VGAUTH_E_OK != err) {
            Audit_Event(FALSE,
                        SU_(alias.alias.renamefail,
                            "Failed to rename suspect Alias store '%s' to '%s'"),
                        fullFileName, badFileName);
            /*
             * XXX the best way to handle this would be to add it to
             * a blacklist of bad files and keep going.  but that's
             * a lot of risky work that's very hard to test, so punt for now.
             */
            return VGAUTH_E_FAIL;
         } else {
            Audit_Event(TRUE,
                        SU_(alias.alias.rename,
                            "Suspect Alias store '%s' renamed to '%s'"),
                        fullFileName, badFileName);
         }
         g_free(badFileName);
         saveBadFile = FALSE;
      }
      g_free(fullFileName);
   }

   g_dir_close(dir);

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * ServiceValidateAliases --                                             */ /**
 *
 * Looks at the alias store, and flags any orphaned mapped alias (have no
 * associated per-user alias) that could have been left by bugs in
 * previous releases.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
ServiceValidateAliases(void)
{
   VGAuthError err;
   int numMapped = 0;
   ServiceMappedAlias *maList = NULL;
   int numIds;
   ServiceAlias *aList = NULL;
   ServiceSubject *mappedSubj;
   ServiceSubject *badSubj;
   ServiceAliasInfo *ai;
   int i;
   int j;
   int k;
   int l;
   gboolean foundMatch;

   err = AliasLoadMapped(&numMapped, &maList);
   if (VGAUTH_E_OK != err) {
      goto done;
   }

   if (numMapped == 0) {
      err = VGAUTH_E_OK;
      goto done;
   }

   for (i = 0; i < numMapped; i++) {
      foundMatch = FALSE;
      badSubj = NULL;
      err = AliasLoadAliases(maList[i].userName, &numIds, &aList);
      if (err != VGAUTH_E_OK) {
         Warning("%s: Failed to load alias for user '%s'\n",
                 __FUNCTION__, maList[i].userName);
         continue;
      }

      // iterate over subjects
      for (j = 0; j < maList[i].num; j++) {
         mappedSubj = &(maList[i].subjects[j]);
         badSubj = mappedSubj;
         for (k = 0; k < numIds; k++) {
            if (ServiceComparePEMCerts(maList[i].pemCert, aList[k].pemCert)) {
               for (l = 0; l < aList[k].num; l++) {
                  ai = &(aList[k].infos[l]);
                  if (ServiceAliasIsSubjectEqual(mappedSubj->type, ai->type,
                                              mappedSubj->name, ai->name)) {
                     foundMatch = TRUE;
                     badSubj = NULL;
                     goto next;
                  }
               }
            }
         }
      }
next:
      ServiceAliasFreeAliasList(numIds, aList);
      if (!foundMatch) {
         /* badSubj should always have a value because maList won't be empty */
         /* coverity[var_deref_op] */
         Warning("%s: orphaned mapped alias: user %s subj %s cert %s\n",
                 __FUNCTION__, maList[i].userName,
                 (badSubj->type == SUBJECT_TYPE_NAMED ? badSubj->name : "ANY"),
                 maList[i].pemCert);
#if 0
         /*
          * This could clear the orphaned alias, but a) that could
          * confuse a user and b) if its buggy, its made things worse.
          */
         err = ServiceAliasRemoveAlias("SERVICE-SANITY-CHECK",
                                        maList[i].userName,
                                        maList[i].pemCert,
                                        badSubj);
#endif
      }
   }

done:
   ServiceAliasFreeMappedAliasList(numMapped, maList);

   return err;
}


/*
 ******************************************************************************
 * ServiceAliasInitAliasStore --                                         */ /**
 *
 * Initializes the alias store, creating directory tree.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceAliasInitAliasStore(void)
{
   int ret;
   VGAuthError err = VGAUTH_E_OK;
   gboolean saveBadDir = FALSE;
   char *defaultDir = NULL;

#ifdef _WIN32
   {
      WCHAR pathW[MAX_PATH];
      HRESULT hRes;

      hRes = SHGetFolderPathW(NULL,
                              CSIDL_APPDATA,
                              NULL,
                              0,
                              pathW);
      if (hRes == S_OK) {
         char *pathA = Convert_Utf16ToUtf8(__FUNCTION__,
                                           __FILE__, __LINE__,
                                           pathW);
         if (NULL == pathA) {
            Warning("%s: out of memory converting filePath\n", __FUNCTION__);
            return VGAUTH_E_FAIL;
         }
         defaultDir = g_strdup_printf("%s"DIRSEPS"%s",
                                      pathA, ALIAS_STORE_REL_DIRECTORY);
         g_free(pathA);
      } else {
         defaultDir = g_strdup(DEFAULT_ALIASSTORE_ROOT_DIR);
      }
   }
#else
   defaultDir = g_strdup(DEFAULT_ALIASSTORE_ROOT_DIR);
#endif

   /*
    * Find the alias store directory.  This allows an installer to put
    * it somewhere else if necessary.
    *
    * Note that this pref is not re-read on a signal unlike others.
    * It should be a one-off at startup, and doesn't need to be dynamically
    * changeable.
    */
   aliasStoreRootDir = Pref_GetString(gPrefs,
                                      VGAUTH_PREF_ALIASSTORE_DIR,
                                      VGAUTH_PREF_GROUP_NAME_SERVICE,
                                      defaultDir);

   Log("Using '%s' for alias store root directory\n", aliasStoreRootDir);

   g_free(defaultDir);

   /*
    * Security check -- if the alias store already exists, be sure
    * the file perms are correct.  If not, we may have been hacked,
    * so throw an audit event.
    *
    * If we think the directory is bad, save it off if we can,
    * but keep going so that tickets can still be used.
    *
    */
   if (g_file_test(aliasStoreRootDir, G_FILE_TEST_EXISTS)) {

      /*
       * g_file_test() on a symlink will return true for IS_DIR
       * if it's a link pointing to a directory.  Seems pretty bogus to me,
       * but they use stat() instead of lstat().
       */
      if (g_file_test(aliasStoreRootDir, G_FILE_TEST_IS_DIR) &&
          !g_file_test(aliasStoreRootDir, G_FILE_TEST_IS_SYMLINK)) {
#ifdef _WIN32
         err = ServiceFileVerifyAdminGroupOwned(aliasStoreRootDir);
#else
         err = ServiceFileVerifyFileOwnerAndPerms(aliasStoreRootDir,
                                                  SUPERUSER_NAME,
                                                  ALIASSTORE_DIR_PERMS,
                                                  NULL, NULL);
#endif
         if (VGAUTH_E_OK != err) {
            Audit_Event(FALSE,
                        SU_(alias.dir.badperm,
                            "Alias store directory '%s' has incorrect owner or "
                            "permissions.  "
                            "Any Aliases currently stored in '%s' will not be "
                            "available for authentication"),
                        aliasStoreRootDir, aliasStoreRootDir);
            saveBadDir = TRUE;
         }
         err = ServiceIDVerifyStoreContents();
         if (VGAUTH_E_OK != err) {
            Warning("%s: alias store had invalid contents\n", __FUNCTION__);
            /*
             * At this time, an error means we couldn't rename away
             * a suspect file, and that means something really screwy
             * is going on, so failing is the best option.
             */
            return err;
         }
      } else {
         Audit_Event(FALSE,
                     SU_(alias.dir.notadir,
                         "Alias store directory '%s' exists but is not a directory"),
                     aliasStoreRootDir);
         saveBadDir = TRUE;
      }

      /*
       * Sanity check the alias store.
       */
      err = ServiceValidateAliases();
   }

   if (saveBadDir) {
      gchar *badRootDirName = g_strdup_printf("%s.bad", aliasStoreRootDir);
      err = ServiceFileRenameFile(aliasStoreRootDir, badRootDirName);
      if (VGAUTH_E_OK != err) {
         Audit_Event(FALSE,
                     SU_(alias.dir.renamefail,
                         "Failed to rename suspect Alias store directory '%s' to '%s'"),
                     aliasStoreRootDir, badRootDirName);
         // XXX making this fatal for now.  can we do anything better?
         return VGAUTH_E_FAIL;
      }
      g_free(badRootDirName);
   }


   /*
    * Create the alias store -- do it here rather than depend on the
    * installer, so we can survive something changing it post-install.
    */
   ret = ServiceFileMakeDirTree(aliasStoreRootDir, ALIASSTORE_DIR_PERMS);
   if (ret < 0) {
      Warning("%s: failed to set up Alias store directory tree\n", __FUNCTION__);
      return VGAUTH_E_FAIL;
   }

   return err;
}
