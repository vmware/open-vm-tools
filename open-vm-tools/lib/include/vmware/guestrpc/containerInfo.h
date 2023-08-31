/*********************************************************
 * Copyright (c) 2021 VMware, Inc. All rights reserved.
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

#ifndef _CONTAINERINFO_H_
#define _CONTAINERINFO_H_

/**
 * @file containerInfo.h
 *
 * Common declarations that aid in sending container information
 * from 'containerInfo' plugin in 'VMware Tools' to the host.
 */

/*
 Sample JSON published to the guestinfo variable.
 $ vmtoolsd --cmd "info-get guestinfo.vmtools.containerInfo" | jq
 {
  "version": "1",
  "updateCounter": "11",
  "publishTime": "2021-10-27T18:18:00.855Z",
  "containerinfo": {
    "k8s.io": [
      {
        "i": "k8s.gcr.io/pause"
      }
    ]
  }
}
*/

/* clang-format off */

#define CONTAINERINFO_KEY                  "containerinfo"
#define CONTAINERINFO_GUESTVAR_KEY         "vmtools." CONTAINERINFO_KEY
#define CONTAINERINFO_VERSION_1            1
#define CONTAINERINFO_KEY_VERSION          "version"
#define CONTAINERINFO_KEY_UPDATE_COUNTER   "updateCounter"
#define CONTAINERINFO_KEY_PUBLISHTIME      "publishTime"
#define CONTAINERINFO_KEY_IMAGE            "i"

/* clang-format on */

#endif