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
 * ghiGetBinaryHandlers.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    provide information about the types of files a given binary supports.
 */


/*
 * Enumerates the different versions of the messages.
 */
enum GHIBinaryHandlersVersion {
    GHI_BINARY_HANDLERS_V1 = 1
};

const GHI_HANDLERS_SUFFIX_MAX_LEN = 32;
const GHI_HANDLERS_MIMETYPE_MAX_LEN = 256;
const GHI_HANDLERS_UTI_MAX_LEN = 256;
const GHI_HANDLERS_FRIENDLY_NAME_MAX_LEN = 256;
const GHI_HANDLERS_MAX_NUM_ICONS = 8;
const GHI_HANDLERS_VERB_MAX_LEN = 64;
const GHI_MAX_NUM_ACTION_URI_PAIRS = 16;

/*
 * The Windows MAX_PATH define specifies that paths may be up to 260 character
 * units in length. To allow for expansion when going to UTF8 we multiply that
 * value by 4 here.
 */
const GHI_HANDLERS_ACTIONURI_MAX_PATH = 1040;

/*
 * Maximum number of filetypes that may be encoded in a single
 * XDR array.
 */
const GHI_MAX_NUM_BINARY_HANDLERS = 32;

struct GHIBinaryHandlersIconDetails {
   /*
    * The icon dimensions in pixels
    */
   int width;
   int height;

   /*
    * A string identifier for this icon that can be used to retrieve
    * the specific pixel data using GHI_GET_ICON_DATA
    */
   string identifier<GHI_HANDLERS_ACTIONURI_MAX_PATH>;
};

struct GHIBinaryHandlersActionURIPair {
   /*
    * The verb for the action URI (typically something like run or print).
    */
   string verb<GHI_HANDLERS_VERB_MAX_LEN>;

   /*
    * The executable path to use when launching the binary with this particular
    * filetype and verb. Some filetypes may require additional or different command line
    * arguments for a given verb that can be encoded here.
    */
   string actionURI<GHI_HANDLERS_ACTIONURI_MAX_PATH>;
};

struct GHIBinaryHandlersDetails {
   /*
    * The file suffix (including leading period character).
    */
   string suffix<GHI_HANDLERS_SUFFIX_MAX_LEN>;

   /*
    * A mimetype - if available.
    */
   string mimetype<GHI_HANDLERS_MIMETYPE_MAX_LEN>;

   /*
    * A UTI (universal type identifier) - if available.
    */
   string UTI<GHI_HANDLERS_UTI_MAX_LEN>;

   /*
    * A list of the verbs (run, print etc.) and their matching
    * action URIs.
    */
   struct GHIBinaryHandlersActionURIPair actionURIs<GHI_MAX_NUM_ACTION_URI_PAIRS>;

   /*
    * A friendly name displayed for this document/filetype.
    */
   string friendlyName<GHI_HANDLERS_FRIENDLY_NAME_MAX_LEN>;

   /*
    * A list of the different sized icons for this filetype.
    */
   struct GHIBinaryHandlersIconDetails icons<GHI_HANDLERS_MAX_NUM_ICONS>;
};

struct GHIBinaryHandlersList {
   struct GHIBinaryHandlersDetails handlers<GHI_MAX_NUM_BINARY_HANDLERS>;
};

/*
 * This defines the protocol for a 'get.binary.handlers' message. The union allows
 * us to create new versions of the protocol later by creating new values
 * in the GHIBinaryHandlersVersion enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail
 * if an unknown version is provided on the wire.
 */
union GHIBinaryHandlers switch (GHIBinaryHandlersVersion ver) {
case GHI_BINARY_HANDLERS_V1:
   struct GHIBinaryHandlersList *handlersV1;
};

