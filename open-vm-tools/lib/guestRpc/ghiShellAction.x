/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * ghiShellAction.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    open files, applications or URLs in either guest or host.
 */


/*
 * Enumerates the different versions of the messages.
 */
enum GHIShellActionVersion {
    GHI_SHELL_ACTION_V1 = 1
};

/*
 * The Windows MAX_PATH define specifies that paths may be up to 260 character
 * units in length. To allow for expansion when going to UTF8 we multiply that
 * value by 4 here.
 */
const GHI_SHELL_ACTION_URI_MAX_SIZE = 1040;

/*
 * Maximum size of a location URI - although RFC2397 doesn't define a max size
 * for a URI some browsers may limit the URI size so we define a hard limit
 * for URI's at 8K in length (this matches a limit in IE8 for example)
 */
const GHI_SHELL_ACTION_LOCATION_MAX_SIZE = 8192;

/*
 * Maximum number of locations that may be encoded in a single
 * XDR array.
 */
const GHI_SHELL_ACTION_MAX_NUM_LOCATIONS = 32;

struct GHIShellActionLocation {
   string location<GHI_SHELL_ACTION_LOCATION_MAX_SIZE>;
};

struct GHIShellActionV1 {
   /*
    * The actionURI - typically something like x-vmware-action:///run or
    * x-vmware-action:///browse.
    */
   string actionURI<GHI_SHELL_ACTION_URI_MAX_SIZE>;

   /*
    * The target of the action - may be a URI encoded path to an executable.
    */
   string targetURI<GHI_SHELL_ACTION_URI_MAX_SIZE>;

   /*
    * A list of locations to be operated on using the actionURI and targetURI.
    */
   struct GHIShellActionLocation locations<GHI_SHELL_ACTION_MAX_NUM_LOCATIONS>;
};

/*
 * This defines the protocol for a 'shellAction' messages. The union allows
 * us to create new versions of the protocol later by creating new values
 * in the GHIShellActionVersion enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail
 * if an unknown version is provided on the wire.
 */
union GHIShellAction switch (GHIShellActionVersion ver) {
case GHI_SHELL_ACTION_V1:
   struct GHIShellActionV1 *actionV1;
};

