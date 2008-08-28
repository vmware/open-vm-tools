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
 * ghiProtocolHandler.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    provide information about the guests protocol handlers (applications
 *    that are launched when opening URLS).
 */

/*
 * Neither RFC 1630 or 1738 seem to specify an actual maximum, current
 * usage implies that 64 bytes should be sufficient
 */
const GHI_URL_SCHEME_NAME_MAX_LEN = 64;

/*
 * The Windows MAX_PATH define specifies that paths may be up to 260 character
 * units in length. To allow for expansion when going to UTF8 we multiply that
 * value by 4 here.
 */
const GHI_PROTOCOL_HANDLER_MAX_PATH = 1040;

/*
 * Maximum number of Protocol Handlers that may be encoded in a single
 * XDR array.
 */
const GHI_MAX_NUM_PROTOCOL_HANDLERS = 32;


struct GHIProtocolHandlerDetails {
   /*
    * The scheme name of a URL is typically, http, ftp, mailto, feed etc.
    */
   string schemeName<GHI_URL_SCHEME_NAME_MAX_LEN>;

   /*
    * The action URI is used in conjunction with UNITY_RPC_SHELL_OPEN to
    * instruct the guest to open a specified URL.
    */
   string handlerActionURI<GHI_PROTOCOL_HANDLER_MAX_PATH>;

   /*
    * The executable path can be used as a parameter to
    * UNITY_RPC_GET_BINARY_INFO to retrieve additional binary information such
    * as Icon images.
    */
   string executablePath<GHI_PROTOCOL_HANDLER_MAX_PATH>;
};

struct GHIProtocolHandlerList {
   struct GHIProtocolHandlerDetails handlers<GHI_MAX_NUM_PROTOCOL_HANDLERS>;
};
