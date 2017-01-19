/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * guestQuiesce.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    provide information about guest quiescing settings.
 */

enum GuestQuiesceParamsVersion {
   GUESTQUIESCEPARAMS_V1 = 1,
   GUESTQUIESCEPARAMS_V2 = 2
};

const GUESTQUIESCE_SCRIPTARG_MAX_LEN = 256;
const GUESTQUIESCE_DISKUUID_MAX_LEN = 3200;   /* (UUID_MAXLEN + 1) * 64 disks */


/*  Guest Quiescing parameters. */
struct GuestQuiesceParamsV1 {
   Bool createManifest;     /* Create manifest describing the operations */
   Bool quiesceApps;        /* Allow application quiescing */
   Bool quiesceFS;          /* Allow file system quiescing */
   Bool writableSnapshot;   /* Assume writable snapshot is allowed */
   Bool execScripts;        /* Run custom scripts created by the users */
   string scriptArg<GUESTQUIESCE_SCRIPTARG_MAX_LEN>;  /* Argument to  scripts */
   uint32 timeout;          /* Time out for the quiesce operation*/
   string diskUuids<GUESTQUIESCE_DISKUUID_MAX_LEN>;   /* disk Uuids */
};

/*  Guest Quiescing parameters V2. */
struct GuestQuiesceParamsV2 {
   struct GuestQuiesceParamsV1 paramsV1;
   uint32 vssBackupContext;
   uint32 vssBackupType;
   Bool vssBootableSystemState;
   Bool vssPartialFileSupport;
};


union GuestQuiesceParams switch (GuestQuiesceParamsVersion ver) {
case GUESTQUIESCEPARAMS_V1:
   struct GuestQuiesceParamsV1 *guestQuiesceParamsV1;
case GUESTQUIESCEPARAMS_V2:
   struct GuestQuiesceParamsV2 *guestQuiesceParamsV2;
};
