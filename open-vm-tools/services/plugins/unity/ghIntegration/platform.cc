/*********************************************************
 * Copyright (C) 2008-2010 VMware, Inc. All rights reserved.
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
 * ghIntegrationX11.c --
 *
 *    Guest-host integration implementation for POSIX-compliant platforms that run X11.
 *
 *    The main tasks done by this code are reading in the system's .desktop files to turn
 *    them into an internal representation of available applications on the system
 *    (implemented by GHIPlatformReadAllApplications, GHIPlatformReadApplicationsDir,
 *    GHIPlatformReadDesktopFile, and kin), and feeding portions of that internal
 *    representation to the host upon request
 *    (GHIPlatform{OpenStartMenuTree,GetStartMenuItem,CloseStartMenuTree}).
 */


#include <stdio.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>

#ifndef GTK2
#error "Gtk 2.0 is required"
#endif

#include <gtk/gtk.h>
#include <gtkmm.h>
#include <glibmm.h>
#include <giomm.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf-core.h>

// gdkx.h includes Xlib.h, which #defines Bool.
#include <gdk/gdkx.h>
#undef Bool

#include <gio/gdesktopappinfo.h>

extern "C" {
#include "vmware.h"
#include "vmware/tools/guestrpc.h"
#include "base64.h"
#include "dbllnklst.h"
#include "debug.h"
#include "util.h"
#include "region.h"
#include "unity.h"
#include "unityCommon.h"
#include "system.h"
#include "codeset.h"
#include "imageUtil.h"
#include "str.h"
#include "strutil.h"
#include <paths.h>
#include "vm_atomic.h"
#include "mntinfo.h"
#include "guest_msg_def.h"
#include "Uri.h"
};

#define URI_TEXTRANGE_EQUAL(textrange, str) \
   (((textrange).afterLast - (textrange).first) == (ssize_t) strlen((str))        \
    && !strncmp((textrange).first, (str), (textrange).afterLast - (textrange).first))

#include "appUtil.h"
#include "ghIntegration.h"
#include "ghIntegrationInt.h"
#include "ghiX11.h"
#include "ghiX11icon.h"

#ifdef REDIST_GMENU
#   include "vmware/tools/ghi/menuItemManager.hh"
using vmware::tools::ghi::MenuItemManager;
using vmware::tools::ghi::MenuItem;
#endif

#include "vmware/tools/ghi/pseudoAppMgr.hh"
using vmware::tools::ghi::PseudoAppMgr;
using vmware::tools::ghi::PseudoApp;


using vmware::tools::NotifyIconCallback;

/*
 * These describe possible start menu item flags. It should come from ghiCommon.h
 * eventually.
 */
#define UNITY_START_MENU_ITEM_DIRECTORY (1 << 0)

/*
 * This macro provides an estimate of how much space an icon might take beyond the actual
 * icon data when returned from unity.get.binary.info. This makes space for the
 * width/height/size strings, and adds enough padding to give some breathing room just in
 * case.
 *
 * > This is only an estimate. <
 */
#define ICON_SPACE_PADDING (sizeof "999x999x65535x" + 25)


/*
 * GHI/X11 context object
 */

struct _GHIPlatform {
   GTree *apps; // Tree of GHIMenuDirectory's, keyed & ordered by their dirname
   GHashTable *appsByExecutable; // Translates full executable path to GHIMenuItem
   GHashTable *appsByDesktopEntry; // Translates full .desktop path to GHIMenuItem
   /*
    * Translates arbitrary executable paths as discovered through
    * UnityPlatformGetWindowPaths to a .desktop-ful executable URI.
    *
    * Example:
    * (key)   /usr/lib/firefox-3.6.3/firefox-bin (via Firefox window's _NET_WM_PID)
    * (value) file:///usr/bin/firefox?DesktopEntry=/usr/share/applications/firefox.desktop
    */
   GHashTable *appsByWindowExecutable;

   /* Pre-wrapper script environment.  See @ref System_GetNativeEnviron. */
   std::vector<Glib::ustring> nativeEnviron;

   /* Callbacks to send data (RPCs) to the host */
   GHIHostCallbacks hostCallbacks;

#ifdef REDIST_GMENU
   /* Launch menu item layout generator thing. */
   MenuItemManager *menuItemManager;
#endif
};

/*
 * The GHIMenuItem object represents an individual leaf-node menu item (corresponding to
 * a .desktop file).
 */
typedef struct {
   char *exepath;     // The full exe path for use in GHIPlatform::appsByExecutable
   char *keyfilePath; // Key to GHIPlatform::appsByDesktopEntry, used in %k field code
   GKeyFile *keyfile; // glib data structure representing the parsed .desktop file
} GHIMenuItem;

/*
 * Represents a "start menu folder" so to speak.
 */
typedef struct {
   const char *dirname;         // The .desktop category that this object represents
   const char *prettyDirname;   // (optional) A prettier version of dirname.
   GPtrArray *items;            // Array of pointers to GHIMenuItems
} GHIMenuDirectory;

/*
 * Represents an active handle for traversing a menu.
 */
typedef struct {
   int handleID;
   enum { LAUNCH_FOLDER, FIXED_FOLDER, DIRECTORY_FOLDER } handleType;
   GHIMenuDirectory *gmd; // Only set for DIRECTORY_FOLDER handles
} GHIMenuHandle;

/*
 * This is used to help us find the Nth GHIMenuDirectory node in the GHIPlatform::apps
 * tree, an operation that is needed as part of GHIPlatformGetStartMenuItem...
 */
typedef struct {
   int currentItem;
   int desiredItem;
   GHIMenuDirectory *gmd; // OUT - pointer to the Nth GHIMenuDirectory
} GHITreeTraversal;

static char *GHIPlatformUriPathToString(UriPathSegmentA *path);


/*
 * GHI capabilities for this platform.
 */
/*
 * XXX TODO: re-enable once ShellAction is implemented.
 */
/*
static GuestCapabilities platformGHICaps[] = {
   GHI_CAP_CMD_SHELL_ACTION,
   GHI_CAP_SHELL_ACTION_BROWSE,
   GHI_CAP_SHELL_ACTION_RUN,
   GHI_CAP_SHELL_LOCATION_HGFS
};
*/

#if !defined(OPEN_VM_TOOLS)
/*
 * An empty file type list - a reference to this can be returned by
 * GHIPlatformGetBinaryHandlers() in some circumstances.
 */
static FileTypeList sEmptyFileTypeList;
#endif // OPEN_VM_TOOLS


static bool AppInfoLaunchEnv(GHIPlatform* ghip, GAppInfo* appInfo);


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformIsSupported --
 *
 *      Determine whether this guest supports guest host integration.
 *
 * Results:
 *      TRUE if the guest supports GHI
 *      FALSE otherwise
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformIsSupported(void)
{
   return GHIX11DetectDesktopEnv() != NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformInit --
 *
 *      Sets up the platform-specific GHI state.
 *
 * Results:
 *      Pointer to platform-specific data (may be NULL).
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

GHIPlatform *
GHIPlatformInit(GMainLoop *mainLoop,            // IN
                const char **envp,              // IN
                GHIHostCallbacks hostCallbacks) // IN
{
   GHIPlatform *ghip;
   const char *desktopEnv;

   Gtk::Main::init_gtkmm_internals();

   ghip = (GHIPlatform *) Util_SafeCalloc(1, sizeof *ghip);
   ghip->appsByWindowExecutable =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
   ghip->hostCallbacks = hostCallbacks;
   AppUtil_Init();

   const char** tmp;
   for (tmp = envp; *tmp; tmp++) {
      ghip->nativeEnviron.push_back(*tmp);
   }

   desktopEnv = GHIX11DetectDesktopEnv();
   g_desktop_app_info_set_desktop_env(desktopEnv);

#ifdef REDIST_GMENU
   ghip->menuItemManager = new MenuItemManager(desktopEnv);
#endif

#ifdef VMX86_DEBUG
   std::vector<const char *> folderKeysChanged;
   folderKeysChanged.push_back(UNITY_START_MENU_LAUNCH_FOLDER);
   folderKeysChanged.push_back(UNITY_START_MENU_FIXED_FOLDER);
   if (ghip->hostCallbacks.launchMenuChange) {
      ghip->hostCallbacks.launchMenuChange(folderKeysChanged.size(),
                                           &folderKeysChanged[0]);
   }
#endif

   return ghip;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformRegisterCaps --
 *
 *      Register guest platform specific capabilities with the VMX.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
GHIPlatformRegisterCaps(GHIPlatform *ghip) // IN
{
   ASSERT(ghip);
   //ASSERT(platformGHICaps);

   /*
    * XXX TODO: re-enable once ShellAction is implemented.
    */
   //AppUtil_SendGuestCaps(platformGHICaps, ARRAYSIZE(platformGHICaps), TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformUnregisterCaps --
 *
 *      Register guest platform specific capabilities with the VMX.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
GHIPlatformUnregisterCaps(GHIPlatform *ghip) // IN
{
   ASSERT(ghip);
   //ASSERT(platformGHICaps);

   /*
    * XXX TODO: re-enable once ShellAction is implemented.
    */
   //AppUtil_SendGuestCaps(platformGHICaps, ARRAYSIZE(platformGHICaps), FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformCleanup --
 *
 *      Tears down the platform-specific GHI state.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      GHIPlatform is no longer valid.
 *
 *----------------------------------------------------------------------------
 */

void
GHIPlatformCleanup(GHIPlatform *ghip) // IN
{
   if (!ghip) {
      return;
   }

   g_hash_table_destroy(ghip->appsByWindowExecutable);
   free(ghip);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformRegisterNotifyIconCallback / GHIPlatformUnregisterNotifyIconCallback --
 *
 *      Register/Unregister the NotifyIcon Callback object. Since notification icons
 *      (aka Tray icons) are unsupported on Linux guests this function does nothing.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Adds data into the DynBuf.
 *
 *-----------------------------------------------------------------------------
 */

void
GHIPlatformRegisterNotifyIconCallback(NotifyIconCallback *notifyIconCallback) // IN
{
}

void GHIPlatformUnregisterNotifyIconCallback(NotifyIconCallback *notifyIconCallback)   // IN
{
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformGetBinaryInfo --
 *
 *      Get binary information (app name and icons). We're passed app info in
 *      pathURIUtf8 (in URI format), and we find the app info by looking up the
 *      path in GHIPlatform->appsByExecutable. Once we find it, we can retrieve
 *      info on the app from the .desktop file.
 *
 * Results:
 *      TRUE if everything went ok, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformGetBinaryInfo(GHIPlatform *ghip,         // IN: platform-specific state
                         const char *pathURIUtf8,   // IN: full path to the binary file
                         std::string &friendlyName,               // OUT: Friendly name
                         std::list<GHIBinaryIconInfo> &iconList)  // OUT: Icons
{
   const char *realCmd = NULL;
#if 0
   char *keyfilePath = NULL;
   unsigned long windowID = 0;
   gpointer freeMe = NULL;
#endif
   UriParserStateA state;
   UriUriA uri;

   ASSERT(ghip);
   ASSERT(pathURIUtf8);

   memset(&state, 0, sizeof state);
   memset(&uri, 0, sizeof uri);
   state.uri = &uri;

   if (pathURIUtf8[0] == '/') {
      realCmd = pathURIUtf8;
   } else if (uriParseUriA(&state, pathURIUtf8) == URI_SUCCESS) {
      if (URI_TEXTRANGE_EQUAL(uri.scheme, "file")) {
         gchar* tmp = (gchar*)g_alloca(strlen(pathURIUtf8) + 1);
         uriUriStringToUnixFilenameA(pathURIUtf8, tmp);

         Glib::ustring unixFile;
         unixFile.assign(tmp);

         Glib::ustring contentType;
         bool uncertain;
         contentType = Gio::content_type_guess(unixFile, std::string(""),
                                               uncertain);

         Bool success = FALSE;
         PseudoAppMgr appMgr;
         PseudoApp app;

         /*
          * H'okay.  So we're looking up icons, yeah?
          *
          * 1.  If given a URI for an XDG desktop entry file, search for an icon based
          *     on its Icon key.
          * 2.  If given a pseudo app URI, as identified by appMgr, use the special
          *     icon associated with said pseudo app.
          * 3.  If given a folder, try going with "folder" (per icon-naming-spec).
          * 4.  Else fall back to searching our theme for an icon based on MIME/
          *     content type.
          */

         if (contentType == "application/x-desktop") {
            Glib::RefPtr<Gio::DesktopAppInfo> desktopFileInfo;

            desktopFileInfo = Gio::DesktopAppInfo::create_from_filename(unixFile);
            if (desktopFileInfo) {
               friendlyName = desktopFileInfo->get_name();
               GHIX11IconGetIconsForDesktopFile(unixFile.c_str(), iconList);
               success = TRUE;
            }
         } else if (appMgr.GetAppByUri(pathURIUtf8, app)) {
            friendlyName = app.symbolicName;
            GHIX11IconGetIconsByName(app.iconName.c_str(), iconList);
            success = TRUE;
         } else if (Glib::file_test(unixFile, Glib::FILE_TEST_IS_DIR)) {
            friendlyName = Glib::filename_display_basename(unixFile);
            GHIX11IconGetIconsByName("folder", iconList);
            success = TRUE;
         } else {
            friendlyName = Glib::filename_display_basename(unixFile);
            size_t i = 0;
            while ((i = contentType.find('/', i)) != contentType.npos) {
               contentType.replace(i, 1, "-");
            }
            GHIX11IconGetIconsByName(contentType.c_str(), iconList);
            success = TRUE;
         }

         uriFreeUriMembersA(&uri);
         return success;
#if 0
         UriQueryListA *queryList = NULL;
         int itemCount;

         freeMe = GHIPlatformUriPathToString(uri.pathHead);
         realCmd = (const char *) freeMe;
         if (g_str_has_suffix(realCmd, ".desktop") &&
             (desktopFileInfo = g_desktop_app_info_new_from_filename(realCmd))
              != NULL) {
            Bool success;

            friendlyName = g_app_info_get_name(G_APP_INFO(desktopFileInfo));
            g_object_unref(desktopFileInfo);

            success = GHIX11IconGetIconsForDesktopFile(realCmd, iconList);
            uriFreeUriMembersA(&uri);
            return success;
         } else if (uriDissectQueryMallocA(&queryList, &itemCount,
                                           uri.query.first,
                                           uri.query.afterLast) == URI_SUCCESS) {
            UriQueryListA *cur;

            for (cur = queryList; cur; cur = cur->next) {
               if (!cur->value) {
                  continue;
               }

               if (strcmp(cur->key, "WindowXID") == 0) {
                  sscanf(cur->value, "%lu", &windowID); // Ignore any failures
               } else if (strcmp(cur->key, "DesktopEntry") == 0) {
                  keyfilePath = g_strdup(cur->value);
               }
            }

            uriFreeQueryListA(queryList);
         }
#endif
      } else {
         uriFreeUriMembersA(&uri);
         Debug("Binary URI %s does not have a 'file' scheme\n", pathURIUtf8);
         return FALSE;
      }
   } else {
      uriFreeUriMembersA(&uri);
      return FALSE;
   }

   return FALSE;

#if 0
   /*
    * If for some reason the command we got wasn't a fullly expanded filesystem path,
    * then expand the command into a full path.
    */
   if (realCmd[0] != '/') {
      ctmp = g_find_program_in_path(realCmd);
      if (ctmp && *ctmp) {
         free(freeMe);
         freeMe = ctmp;
         realCmd = ctmp;
      } else {
         free(ctmp);
         free(freeMe);
         return FALSE;
      }
   }

   if (keyfilePath) {
      ghm = (GHIMenuItem *) g_hash_table_lookup(ghip->appsByDesktopEntry, keyfilePath);
      g_free(keyfilePath);
   }

   if (!ghm) {
      /*
       * Now that we have the full path, look it up in our hash table of GHIMenuItems
       */
      ghm = (GHIMenuItem *) g_hash_table_lookup(ghip->appsByExecutable, realCmd);
   }

   if (!ghm) {
      /*
       * To deal with /usr/bin/gimp being a symlink to gimp-2.x, also try symlinks.
       */
      char newPath[PATH_MAX + 1];
      ssize_t linkLen;

      linkLen = readlink(realCmd, newPath, sizeof newPath - 1);
      if (linkLen > 0) {
         char *slashLoc;

         newPath[linkLen] = '\0';
         slashLoc = strrchr(realCmd, '/');
         if (newPath[0] != '/' && slashLoc) {
            ctmp = g_strdup_printf("%.*s%s",
                                   (int)((slashLoc + 1) - realCmd),
                                   realCmd, newPath);
            g_free(freeMe);
            freeMe = ctmp;
            realCmd = (const char *) freeMe;
         } else {
            realCmd = newPath;
         }

         ghm = (GHIMenuItem *) g_hash_table_lookup(ghip->appsByExecutable, realCmd);
      }
   }
   /*
    * Stick the app name into 'friendlyName'.
    */
   if (ghm) {
      ctmp = g_key_file_get_locale_string(ghm->keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                   G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL);
      if (!ctmp) {
         ctmp = g_path_get_basename(realCmd);
      }
      friendlyName = ctmp;
      free(ctmp);
   } else {
      /*
       * If we can't find it, then just tell the host that the app name is the same as
       * the basename of the application's path.
       */
      ctmp = strrchr(realCmd, '/');
      if (ctmp) {
         ctmp++;
      } else {
         ctmp = (char *) realCmd;
      }
      friendlyName = ctmp;
   }

   free(freeMe);
   ctmp = NULL;
   freeMe = NULL;

   GHIPlatformCollectIconInfo(ghip, ghm, windowID, iconList);

#endif
   return TRUE;
}


#if !defined(OPEN_VM_TOOLS)
/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformGetBinaryHandlers --
 *
 *      Get the list of filetypes and URL protocols supported by a binary
 *      (application). We're passed an app path in URI format, and we find
 *      the app info by looking up the path in GHIPlatform->appsByExecutable.
 *      Once we find it, we can retrieve info on the app from the .desktop file.
 *
 * Results:
 *      A Filetype list of the handlers.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

const FileTypeList&
GHIPlatformGetBinaryHandlers(GHIPlatform *ghip,      // IN: platform-specific state
                             const char *pathUtf8)   // IN: full path to the executable
{
   return sEmptyFileTypeList;
}
#endif // OPEN_VM_TOOLS


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformOpenStartMenuTree --
 *
 *      Get start menu item count for a given root. This function should be
 *      called before iterating through the menu item subtree.
 *      To start at the root of the start menu, pass in "" for the root.
 *
 *      The output 'buf' is a string holding two numbers separated by a space:
 *          1. A handle ID for this menu tree iterator.
 *          2. A count of the items in this iterator.
 *
 * Results:
 *      TRUE if we were able to get the count successfully
 *      FALSE otherwise
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformOpenStartMenuTree(GHIPlatform *ghip,        // IN: platform-specific state
                             const char *rootUtf8,     // IN: root of the tree
                             uint32 flags,             // IN: flags
                             DynBuf *buf)              // OUT: number of items
{
   Bool success = FALSE;

#ifdef REDIST_GMENU
   std::pair<uint32,uint32> descriptor;
   if (ghip->menuItemManager->OpenMenuTree(rootUtf8, &descriptor)) {
      char tmp[2 * sizeof MAKESTR(UINT_MAX)];
      Str_Sprintf(tmp, sizeof tmp, "%u %u", descriptor.first, descriptor.second);
      DynBuf_AppendString(buf, tmp);
      success = TRUE;
   }
#endif

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformMenuItemToURI --
 *
 *      Returns the URI that would be used to launch a particular GHI menu item
 *
 * Results:
 *      Newly allocated URI string
 *
 * Side effects:
 *      Allocates memory for the URI.
 *
 *-----------------------------------------------------------------------------
 */

static char *
GHIPlatformMenuItemToURI(GHIPlatform *ghip, // IN
                         GHIMenuItem *gmi)  // IN
{
   gchar **argv;
   gint argc;

   char *ctmp;
   UriQueryListA *queryItems;
   int i;
   int err;
   gboolean res;
   int nchars;
   char *uriString;
   char *queryString;

   ASSERT(ghip);
   ASSERT(gmi);

   ctmp = g_key_file_get_string(gmi->keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);

   res = g_shell_parse_argv(ctmp, &argc, &argv, NULL);
   g_free(ctmp);
   if (!res) {
      return NULL;
   }

   queryItems = (UriQueryListA *) alloca((argc + 1) * sizeof *queryItems);

   for (i = 0; i < (argc - 1); i++) {
      queryItems[i].key = "argv[]";
      queryItems[i].value = argv[i + 1];
      queryItems[i].next = &queryItems[i + 1];
   }
   queryItems[i].key = "DesktopEntry";
   queryItems[i].value = gmi->keyfilePath;
   queryItems[i].next = NULL;

   /*
    * 10 + 3 * len is the formula recommended by uriparser for the maximum URI string
    * length.
    */
   uriString = (char *) alloca(10 + 3 * strlen(gmi->exepath));
   if (uriUnixFilenameToUriStringA(gmi->exepath, uriString)) {
      g_strfreev(argv);
      return NULL;
   }
   if (uriComposeQueryCharsRequiredA(queryItems, &nchars) != URI_SUCCESS) {
      g_strfreev(argv);
      return NULL;
   }
   queryString = (char *) alloca(nchars + 1);
   err = uriComposeQueryA(queryString, queryItems, nchars + 1, &i);
   g_strfreev(argv);
   if (err != URI_SUCCESS) {
      return NULL;
   }

   return g_strdup_printf("%s?%s", uriString, queryString);
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformGetStartMenuItem --
 *
 *      Get start menu item at a given index. This function should be called
 *      in the loop to get all items for a menu sub-tree.
 *      If there are no more items, the function will return FALSE.
 *
 *      Upon returning, 'buf' will hold a nul-delimited array of strings:
 *         1. User-visible item name.
 *         2. UNITY_START_MENU_ITEM_* flag.
 *         3. Executable path.
 *         4. Localized user-visible item name.
 *
 * Results:
 *      TRUE if there's an item at a given index, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformGetStartMenuItem(GHIPlatform *ghip, // IN: platform-specific state
                            uint32 handle,     // IN: tree handle
                            uint32 itemIndex,  // IN: the index of the item in the tree
                            DynBuf *buf)       // OUT: item
{
   Bool success = FALSE;

#ifdef REDIST_GMENU
   const MenuItem* menuItem;
   const Glib::ustring* path;

   if (ghip->menuItemManager->GetMenuItem(handle, itemIndex, &menuItem, &path)) {
      Glib::ustring key = *path + "/" + menuItem->key;
      DynBuf_AppendString(buf, key.c_str());

      char tmp[sizeof MAKESTR(UINT_MAX)];
      Str_Sprintf(tmp, sizeof tmp, "%u", menuItem->isFolder ? 1 : 0);
      DynBuf_AppendString(buf, tmp);

      DynBuf_AppendString(buf, menuItem->execPath.c_str());
      DynBuf_AppendString(buf, menuItem->displayName.c_str());
      success = TRUE;
   }
#endif

   return success;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformCloseStartMenu --
 *
 *      Free all memory associated with this start menu tree and cleanup.
 *
 * Results:
 *      TRUE if the handle is valid
 *      FALSE otherwise
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformCloseStartMenuTree(GHIPlatform *ghip, // IN: platform-specific state
                              uint32 handle)     // IN: handle to the tree to be closed
{
#ifdef REDIST_GMENU
   return ghip->menuItemManager->CloseMenuTree(handle);
#else
   return FALSE;
#endif
}


#if 0 // REMOVE AFTER IMPLEMENTING GHIPlatformShellAction
/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformFindHGFSShare --
 *
 *      Finds the filesystem path to a particular HGFS sharename
 *
 * Results:
 *      Newly heap-allocated path to the top of the specified share.
 *
 * Side effects:
 *      Allocates memory for the return value.
 *
 *-----------------------------------------------------------------------------
 */

static char *
GHIPlatformFindHGFSShare(GHIPlatform *ghip,              // IN
                         const UriTextRangeA *sharename) // IN
{
   FILE *fh;
   struct mntent *ment;

   fh = Posix_Setmntent(_PATH_MOUNTED, "r");
   if (!fh) {
      return NULL;
   }

   while ((ment = Posix_Getmntent(fh))) {
      char *fsSharename;
      if (strcmp(ment->mnt_type, "hgfs") && strcmp(ment->mnt_type, "vmhgfs")) {
         continue;
      }

      if (!StrUtil_StartsWith(ment->mnt_fsname, ".host:")) {
         Warning("HGFS filesystem has an fsname of \"%s\" rather than \".host:...\"\n",
                 ment->mnt_fsname);
         continue;
      }

      if (ment->mnt_fsname[strlen(".host:")] == '/') {
         fsSharename = ment->mnt_fsname + strlen(".host:/");
      } else {
         fsSharename = ment->mnt_fsname + strlen(".host:");
      }

      /*
       * XXX this function's logic could be improved substantially to do deeper matching
       * (e.g. if someone has .host:/foo/bar mounted, but nothing else, and is looking to
       * open the document share://foo/bar/baz). Don't know if HGFS allows that, but
       * that'd require passing in the whole URI rather than just the sharename.
       */
      if (URI_TEXTRANGE_EQUAL(*sharename, fsSharename)) {
         char *retval = g_strdup(ment->mnt_dir);

         fclose(fh);

         return retval;
      } else if (fsSharename == '\0') {
         /*
          * This is a mount of the toplevel HGFS directory, so we know it should work.
          */
         char *retval = g_strdup_printf("%s/%.*s",
                                        ment->mnt_dir,
                                        (int)(sharename->afterLast - sharename->first),
                                        sharename->first);
         fclose(fh);
         return retval;
      }
   }
   fclose(fh);

   return NULL;
}
#endif // REMOVE AFTER IMPLEMENTING GHIPlatformShellAction


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformUriPathToString --
 *
 *      Turns a UriPathSegment sequence into a '/' separated filesystem path.
 *
 * Results:
 *      Newly heap-allocated string containing the FS path.
 *
 * Side effects:
 *      Allocates memory (caller is responsible for freeing it).
 *
 *-----------------------------------------------------------------------------
 */

static char *
GHIPlatformUriPathToString(UriPathSegmentA *path) // IN
{
   GString *str;
   char *retval;
   UriPathSegmentA *cur;

   str = g_string_new("");
   for (cur = path; cur; cur = cur->next) {
      g_string_append_c(str, '/');
      g_string_append_len(str, cur->text.first, cur->text.afterLast - cur->text.first);
   }

   retval = str->str;
   g_string_free(str, FALSE);

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformURIToArgs --
 *
 *      Turns a URI into an array of arguments that are useable for execing...
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      Allocates an array of strings, and returns it in *argv...
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GHIPlatformURIToArgs(GHIPlatform *ghip,     // IN
                     const char *uriString, // IN
                     char ***argv,          // IN/OUT
                     int *argc,             // IN/OUT
                     char **dotDesktopPath) // IN/OUT
{
   UriParserStateA state;
   UriUriA uri;
   Bool parseQueryString = TRUE;
   GPtrArray *newargv;

   ASSERT(ghip);
   ASSERT(uriString);
   ASSERT(argv);
   ASSERT(argc);
   ASSERT(dotDesktopPath);

   memset(&state, 0, sizeof state);
   memset(&uri, 0, sizeof uri);
   state.uri = &uri;
   if (uriParseUriA(&state, uriString) != URI_SUCCESS) {
      uriFreeUriMembersA(&uri);
      return FALSE;
   }

   newargv = g_ptr_array_new();

#if 0 // Temporary until ShellAction is implemented.
   /*
    * This is previous code that was used for mapping x-vmware-share and
    * x-vmware-action URIs, but it's not being used at the moment.
    */
   if (URI_TEXTRANGE_EQUAL(uri.scheme, "x-vmware-share")) {
      UriTextRangeA *sharename;
      UriPathSegmentA *sharepath;
      char *sharedir;
      char *subdir;

      /*
       * Try to find a mounted HGFS filesystem that has the right path...
       * Deals with both share://sharename/baz/baz and share:///sharename/baz/baz
       */
      if (uri.hostText.first) {
         sharename = &uri.hostText;
         sharepath = uri.pathHead;
      } else if (uri.pathHead) {
         sharename = &uri.pathHead->text;
         sharepath = uri.pathHead->next;
      } else {
         NOT_REACHED();
      }

      sharedir = GHIPlatformFindHGFSShare(ghip, sharename);
      if (!sharedir) {
         uriFreeUriMembersA(&uri);
         g_ptr_array_free(newargv, TRUE);
         Debug("Couldn't find a mounted HGFS filesystem for %s\n", uriString);
         return FALSE;
      }

      subdir = GHIPlatformUriPathToString(sharepath);
      g_ptr_array_add(newargv, g_strconcat(sharedir, subdir, NULL));
      g_free(sharedir);
      g_free(subdir);
   } else if (URI_TEXTRANGE_EQUAL(uri.scheme, "x-vmware-action")) {
      if (g_file_test("/usr/bin/gnome-open", G_FILE_TEST_IS_EXECUTABLE)) {
         g_ptr_array_add(newargv, g_strdup("/usr/bin/gnome-open"));
      } else if (g_file_test("/usr/bin/htmlview", G_FILE_TEST_IS_EXECUTABLE)
                 && URI_TEXTRANGE_EQUAL(uri.hostText, "browse")) {
         g_ptr_array_add(newargv, g_strdup("/usr/bin/htmlview"));
      } else {
         Debug("Don't know how to handle URI %s. "
               "We definitely don't have /usr/bin/gnome-open.\n",
               uriString);
         NOT_IMPLEMENTED();
      }
   }
#endif // Temporary until ShellAction is implemented.

   if (URI_TEXTRANGE_EQUAL(uri.scheme, "file")) {
      char *fspath = GHIPlatformUriPathToString(uri.pathHead);
      g_ptr_array_add(newargv, fspath);
   } else {
      /*
       * Just append the unparsed URI as-is onto the command line.
       */
      g_ptr_array_add(newargv, g_strdup(uriString));
      parseQueryString = FALSE;
   }

   *dotDesktopPath = NULL;
   if (parseQueryString) {
      /*
       * We may need additional command-line arguments from the part of the URI after the
       * '?'.
       */

      UriQueryListA *queryList;
      int itemCount;

      if (uriDissectQueryMallocA(&queryList, &itemCount,
                                 uri.query.first, uri.query.afterLast) == URI_SUCCESS) {
         UriQueryListA *cur;

         for (cur = queryList; cur; cur = cur->next) {
            if (!cur->value) {
               continue;
            }

            if (strcmp(cur->key, "argv[]") == 0) {
               g_ptr_array_add(newargv, g_strdup(cur->value));
               cur->value = NULL;
            } else if (strcmp(cur->key, "DesktopEntry")) {
               *dotDesktopPath = g_strdup(cur->value);
            }
         }

         uriFreeQueryListA(queryList);
      } else {
         Warning("Dissection of query string in URI %s failed\n",
                 uriString);
      }
   }

   uriFreeUriMembersA(&uri);

   *argc = newargv->len;
   g_ptr_array_add(newargv, NULL);
   *argv = (char **) g_ptr_array_free(newargv, FALSE);

   return TRUE;
}


#if 0 // REMOVE AFTER IMPLEMENTING GHIPlatformShellAction
/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformStripFieldCodes --
 *
 *      Strip field codes from an argv-style string array.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Modifies the string array, possibly freeing some members.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformStripFieldCodes(char **argv, // IN/OUT
                           int *argc)   // IN/OUT
{
   int i;

   ASSERT(argv);
   ASSERT(argc);

   for (i = 0; i < *argc; i++) {
      if (argv[i][0] == '%'
          && argv[i][1] != '\0'
          && argv[i][2] == '\0') {
         g_free(argv[i]);
         /*
          * This math may look slightly dodgy - just remember that these
          * argv's have a terminating NULL pointer, which is not included in its argc.
          */
         g_memmove(argv + i, argv + i + 1,
                   (*argc - i) * sizeof *argv);
         (*argc)--;
      }
   }
}
#endif // REMOVE AFTER IMPLEMENTING GHIPlatformShellAction


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformCombineArgs --
 *
 *      Takes a target URI and turns it into an argv array that we can actually
 *      exec().
 *
 *      XXX TODO: accept location arguments once ShellAction is implemented.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise. If TRUE, fullArgv/fullArgc will
 *      contain the exec-able argument array.
 *
 * Side effects:
 *      Allocates a string array in fullArgv (owner is responsible for freeing).
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GHIPlatformCombineArgs(GHIPlatform *ghip,            // IN
                       const char *targetUtf8,       // IN
                       char ***fullArgv,             // OUT
                       int *fullArgc)                // OUT
{
   char **targetArgv = NULL;
   int targetArgc = 0;
   char *targetDotDesktop = NULL;
   GPtrArray *fullargs = g_ptr_array_new();
   GHIMenuItem *ghm = NULL;
   int i;

   ASSERT(ghip);
   ASSERT(targetUtf8);
   ASSERT(fullArgv);
   ASSERT(fullArgc);

   if (!GHIPlatformURIToArgs(ghip,
                             targetUtf8,
                             &targetArgv,
                             &targetArgc,
                             &targetDotDesktop)) {
      Debug("Parsing URI %s failed\n", targetUtf8);
      return FALSE;
   }

#if 0 // Temporary until ShellAction is implemented.
   /*
    * This is previous code that was used for combining file and action
    * arguments, but it's not being used at the moment. Our action URI format
    * has changed, so this will need to be updated before it's usable.
    */

   /*
    * In the context of the .desktop spec
    * (http://standards.freedesktop.org/desktop-entry-spec/1.1/ar01s06.html),
    * combining the two is not as simple as just concatenating them.
    *
    * XXX for some random older programs, we may want to do concatenation in the future.
    */
   char **srcArgv;
   int srcArgc;
   char *srcDotDesktop = NULL;

   /*
    * First, figure out which argv[] array is the 'main' one, and which one will serve
    * only to fill in the file/URL argument in the .desktop file...
    */
   if (! *actionArgc) {
      srcArgv = *fileArgv;
      srcArgc = *fileArgc;
      srcDotDesktop = fileDotDesktop;
   } else {
      srcArgv = *actionArgv;
      srcArgc = *actionArgc;
      srcDotDesktop = actionDotDesktop;
      if (fileDotDesktop) {
         GHIPlatformStripFieldCodes(*fileArgv, fileArgc);
      }
   }
#endif // Temporary until ShellAction is implemented.

   for (i = 0; i < targetArgc; i++) {
      const char *thisarg = targetArgv[i];

      if (thisarg[0] == '%' && thisarg[1] != '\0' && thisarg[2] == '\0') {
         switch (thisarg[1]) {
         case 'F': // %F expands to multiple filenames
         case 'f': // %f expands to a filename
            /*
             * XXX TODO: add file location arguments
             */
            //if (srcArgv != *fileArgv && *fileArgc) {
            //   g_ptr_array_add(fullargs, g_strdup((*fileArgv)[0]));
            //}
            break;
         case 'U': // %U expands to multiple URLs
         case 'u': // %u expands to a URL
            /*
             * XXX TODO: add URL location arguments
             */
            //if (srcArgv != *fileArgv && fileUtf8) {
            //   g_ptr_array_add(fullargs, g_strdup(fileUtf8));
            //}
            break;

            /*
             * These three require getting at the .desktop info for the app.
             */
         case 'k':
         case 'i':
         case 'c':
            if (!ghm && targetDotDesktop) {
               ghm = (GHIMenuItem *) g_hash_table_lookup(ghip->appsByDesktopEntry,
                                                         targetDotDesktop);
            }
            if (!ghm) {
               ASSERT (fullargs->len > 0);
               ghm = (GHIMenuItem *) g_hash_table_lookup(ghip->appsByExecutable,
                                                         g_ptr_array_index(fullargs, 0));
            }

            if (ghm) {
               switch (thisarg[1]) {
               case 'c': // %c expands to the .desktop's Name=
                  {
                     char *ctmp =
                        g_key_file_get_locale_string(ghm->keyfile,
                                                     G_KEY_FILE_DESKTOP_GROUP,
                                                     G_KEY_FILE_DESKTOP_KEY_NAME,
                                                     NULL, NULL);
                     if (ctmp) {
                        g_ptr_array_add(fullargs, ctmp);
                     }
                  }
                  break;
               case 'i': // %i expands to "--icon" then the .desktop's Icon=
                  {
                     char *ctmp =
                        g_key_file_get_string(ghm->keyfile,
                                              G_KEY_FILE_DESKTOP_GROUP,
                                              G_KEY_FILE_DESKTOP_KEY_ICON,
                                              NULL);
                     if (ctmp && *ctmp) {
                        g_ptr_array_add(fullargs, g_strdup("--icon"));
                        g_ptr_array_add(fullargs, ctmp);
                     }
                  }
                  break;
               case 'k': // %k expands to the .desktop's path
                  g_ptr_array_add(fullargs, g_strdup(ghm->keyfilePath));
                  break;
               }
            }
            break;
         case '%': // Expands to a literal
            g_ptr_array_add(fullargs, g_strdup("%"));
            break;
         default:
            /*
             * Intentionally ignore an unknown field code.
             */
            break;
         }
      } else {
         g_ptr_array_add(fullargs, g_strdup(thisarg));
      }
   }
   *fullArgc = fullargs->len;
   g_ptr_array_add(fullargs, NULL);
   *fullArgv = (char **) g_ptr_array_free(fullargs, FALSE);

   g_strfreev(targetArgv);
   g_free(targetDotDesktop);

   return *fullArgc ? TRUE : FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformShellOpen --
 *
 *      Open the specified file with the default shell handler (ShellExecute).
 *      Note that the file path may be either a URI (originated with
 *      Tools >= NNNNN), or a regular path (originated with Tools < NNNNN).
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformShellOpen(GHIPlatform *ghip,    // IN
                     const char *fileUtf8) // IN
{
   char **fullArgv = NULL;
   int fullArgc = 0;
   Bool retval = FALSE;

   ASSERT(ghip);
   ASSERT(fileUtf8);

   Debug("%s: file: '%s'\n", __FUNCTION__, fileUtf8);

   /*
    * XXX This is not shippable.  GHIPlatformCombineArgs may still be necessary,
    * and I chose to use if (1) rather than #if 0 it out in order to not have to
    * #if 0 out that function and everything else it calls as well.
    */
   if (1) {
      UriParserStateA upState;
      UriUriA uri;

      memset(&upState, 0, sizeof upState);
      memset(&uri, 0, sizeof uri);
      upState.uri = &uri;

      if (uriParseUriA(&upState, fileUtf8) == URI_SUCCESS &&
          URI_TEXTRANGE_EQUAL(uri.scheme, "file")) {
         Bool success = FALSE;

         gchar* tmp = (gchar*)g_alloca(strlen(fileUtf8) + 1);
         uriUriStringToUnixFilenameA(fileUtf8, tmp);

         Glib::ustring unixFile;
         unixFile.assign(tmp);

         Glib::ustring contentType;
         bool uncertain;
         contentType = Gio::content_type_guess(unixFile, std::string(""), uncertain);

         if (contentType == "application/x-desktop") {
            GDesktopAppInfo* dappinfo;
            dappinfo = g_desktop_app_info_new_from_filename(unixFile.c_str());
            if (dappinfo) {
               GAppInfo *appinfo = (GAppInfo*)G_APP_INFO(dappinfo);
               success = AppInfoLaunchEnv(ghip, appinfo);
               g_object_unref(dappinfo);
            }
         } else if (Glib::file_test(unixFile, Glib::FILE_TEST_IS_REGULAR) &&
                    Glib::file_test(unixFile, Glib::FILE_TEST_IS_EXECUTABLE)) {
            std::vector<Glib::ustring> argv;
            argv.push_back(unixFile);
            try {
               Glib::spawn_async("" /* inherit cwd */, argv, ghip->nativeEnviron, (Glib::SpawnFlags) 0);
               success = TRUE;
            } catch(Glib::SpawnError& e) {
               g_warning("%s: %s: %s\n", __FUNCTION__, unixFile.c_str(), e.what().c_str());
            }
         } else {
            std::vector<Glib::ustring> argv;
            Glib::ustring de = GHIX11DetectDesktopEnv();
            // XXX Really we should just use xdg-open exclusively, but xdg-open
            // as shipped with xdg-utils 1.0.2 is broken.  It is fixed
            // in portland CVS, but we need to import into modsource and
            // redistribute with Tools in order to guarantee a working version.
            if (de == "GNOME") {
               argv.push_back("gnome-open");
            } else if (de == "KDE") {
               argv.push_back("kde-open");
            } else {
               argv.push_back("xdg-open");
            }
            argv.push_back(unixFile);
            try {
               Glib::spawn_async("", argv, ghip->nativeEnviron, Glib::SPAWN_SEARCH_PATH);
               success = TRUE;
            } catch(Glib::SpawnError& e) {
               g_warning("%s: %s: %s\n", __FUNCTION__, unixFile.c_str(), e.what().c_str());
            }
         }

         return success;
      }
   } else if (GHIPlatformCombineArgs(ghip, fileUtf8, &fullArgv, &fullArgc) &&
       fullArgc > 0) {
      // XXX Will fix this soon.
#if 0
      retval = g_spawn_async(NULL, fullArgv,
                            /*
                             * XXX  Please don't hate me for casting off the qualifier
                             * here.  Glib does -not- modify the environment, at
                             * least not in the parent process, but their prototype
                             * does not specify this argument as being const.
                             *
                             * Comment stolen from GuestAppX11OpenUrl.
                             */
                             (char **)ghip->nativeEnviron,
                             (GSpawnFlags) (G_SPAWN_SEARCH_PATH |
                             G_SPAWN_STDOUT_TO_DEV_NULL |
                             G_SPAWN_STDERR_TO_DEV_NULL),
                             NULL, NULL, NULL, NULL);
#endif
   }

   g_strfreev(fullArgv);

   return retval;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformShellAction --
 *      Perform the specified shell action with the optional target and
 *      locations arguments. Note that the target may be either a URI
 *      (originated with Tools >= NNNNN), or a regular path (originated with
 *      Tools < NNNNN).
 *      See the comment at ghIntegration.c::GHITcloShellAction for information
 *      on the command format and supported actions.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformShellAction(GHIPlatform *ghip,          // IN: platform-specific state
                       const char *actionURI,      // IN
                       const char *targetURI,      // IN
                       const char **locations,     // IN
                       int numLocations)           // IN
{
   /*
    * TODO: implement the shell action execution.
    * The GHIPlatformShellUrlOpen() below is left for reference, but is not
    * used right now. Its functionality should be integrated here.
    */
   ASSERT(ghip);
   ASSERT(actionURI);
   ASSERT(targetURI);

   Debug("%s not implemented yet.\n", __FUNCTION__);

   return FALSE;
}


#if 0 // REMOVE AFTER IMPLEMENTING GHIPlatformShellAction
/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformShellUrlOpen --
 *
 *      Run ShellExecute on a given file.
 *
 * Results:
 *      TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformShellUrlOpen(GHIPlatform *ghip,      // IN: platform-specific state
                        const char *fileUtf8,   // IN: command/file
                        const char *actionUtf8) // IN: action
{
#ifdef GTK2
   char **fileArgv = NULL;
   int fileArgc = 0;
   char *fileDotDesktop = NULL;
   char **actionArgv = NULL;
   int actionArgc = 0;
   char *actionDotDesktop = NULL;
   char **fullArgv = NULL;
   int fullArgc = 0;

   Bool retval = FALSE;

   ASSERT(ghip);

   if (!GHIPlatformURIToArgs(ghip, fileUtf8, &fileArgv, &fileArgc,
                             &fileDotDesktop)) {
      Debug("Parsing URI %s failed\n", fileUtf8);
      return FALSE;
   }

   if (actionUtf8 && !GHIPlatformURIToArgs(ghip, actionUtf8, &actionArgv, &actionArgc,
                                           &actionDotDesktop)) {
      Debug("Parsing action URI %s failed\n", actionUtf8);
      g_strfreev(fileArgv);
      g_free(fileDotDesktop);
      return FALSE;
   }

   if (GHIPlatformCombineArgs(ghip,
                              fileUtf8, &fileArgv, &fileArgc, fileDotDesktop,
                              actionUtf8, &actionArgv, &actionArgc, actionDotDesktop,
                              &fullArgv, &fullArgc)) {
      retval = g_spawn_async(NULL, fullArgv, NULL,
                             G_SPAWN_SEARCH_PATH |
                             G_SPAWN_STDOUT_TO_DEV_NULL |
                             G_SPAWN_STDERR_TO_DEV_NULL,
                             NULL, NULL, NULL, NULL);
   }

   g_strfreev(fileArgv);
   g_free(fileDotDesktop);
   g_strfreev(actionArgv);
   g_free(actionDotDesktop);
   g_strfreev(fullArgv);

   return retval;
#else // !GTK2
   return FALSE;
#endif // GTK2
}
#endif // REMOVE AFTER IMPLEMENTING GHIPlatformShellAction


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformSetGuestHandler --
 *
 *      Set the handler for the specified filetype (or URL protocol) to the
 *      given value.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformSetGuestHandler(GHIPlatform *ghip,    // IN: platform-specific state
                           const char *suffix,   // IN/OPT: suffix
                           const char *mimeType, // IN/OPT: MIME Type
                           const char *UTI,      // IN/OPT: UTI
                           const char *actionURI,  // IN:
                           const char *targetURI)  // IN:
{
   ASSERT(ghip);

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformRestoreDefaultGuestHandler --
 *
 *      Restore the handler for a given type to the value in use before any
 *      changes by tools.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformRestoreDefaultGuestHandler(GHIPlatform *ghip,  // IN: platform-specific state
                                      const char *suffix,   // IN/OPT: Suffix
                                      const char *mimetype, // IN/OPT: MIME Type
                                      const char *UTI)      // IN/OPT: UTI
{
   ASSERT(ghip);

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformSetOutlookTempFolder --
 *
 *    Set the temporary folder used by Microsoft Outlook to store attachments
 *    opened by the user.
 *
 *    XXX While we probably won't ever need to implement this for Linux, we
 *        still the definition of this function in the X11 back-end.
 *
 * Results:
 *    TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformSetOutlookTempFolder(GHIPlatform *ghip,       // IN: platform-specific state
                                const char *targetURI)   // IN: Target URI
{
   ASSERT(ghip);
   ASSERT(targetURI);

   return FALSE;
}


/* @brief Send a mouse or keyboard event to a tray icon.
 *
 * @param[in] ghip Pointer to platform-specific GHI data.
 *
 * @retval TRUE Operation Succeeded.
 * @retval FALSE Operation Failed.
 */

Bool
GHIPlatformTrayIconSendEvent(GHIPlatform *ghip,
                             const char *iconID,
                             uint32 event,
                             uint32 x,
                             uint32 y)
{
   ASSERT(ghip);
   ASSERT(iconID);
   return FALSE;
}

/* @brief Start sending tray icon updates to the VMX.
 *
 * @param[in] ghip Pointer to platform-specific GHI data.
 *
 * @retval TRUE Operation Succeeded.
 * @retval FALSE Operation Failed.
 */

Bool
GHIPlatformTrayIconStartUpdates(GHIPlatform *ghip)
{
   ASSERT(ghip);
   return FALSE;
}

/* @brief Stop sending tray icon updates to the VMX.
 *
 * @param[in] ghip Pointer to platform-specific GHI data.
 *
 * @retval TRUE Operation Succeeded.
 * @retval FALSE Operation Failed.
 */

Bool
GHIPlatformTrayIconStopUpdates(GHIPlatform *ghip)
{
   ASSERT(ghip);
   return FALSE;
}

/* @brief Set a window to be focused.
 *
 * @param[in] ghip Pointer to platform-specific GHI data.
 * @param[in] xdrs Pointer to serialized data from the host.
 *
 * @retval TRUE Operation Succeeded.
 * @retval FALSE Operation Failed.
 */

Bool
GHIPlatformSetFocusedWindow(GHIPlatform *ghip,
                            int32 windowId)
{
   ASSERT(ghip);
   return FALSE;
}


/**
 * @brief Get the hash (or timestamp) of information returned by
 * GHIPlatformGetBinaryInfo.
 *
 * @param[in]  ghip     Pointer to platform-specific GHI data.
 * @param[in]  request  Request containing which executable to get the hash for.
 * @param[out] reply    Reply to be filled with the hash.
 *
 * @retval TRUE Operation succeeded.
 * @retval FALSE Operation failed.
 */

Bool GHIPlatformGetExecInfoHash(GHIPlatform *ghip,
                                const char *execPath,
                                char **execInfoHash)
{
   ASSERT(ghip);
   ASSERT(execPath);
   ASSERT(execInfoHash);

   return FALSE;
}


/*
 ******************************************************************************
 * GHIX11FindDesktopUriByExec --                                         */ /**
 *
 * Given an executable path, attempt to generate an "execUri" associated with a
 * corresponding .desktop file.
 *
 * @sa GHIX11_FindDesktopUriByExec
 *
 * @note Returned pointer belongs to the GHI module.  Caller must not free it.
 *
 * @param[in]  ghip     GHI platform-specific context.
 * @param[in]  execPath Input binary path.  May be absolute or relative.
 *
 * @return Pointer to a URI string on success, NULL on failure.
 *
 ******************************************************************************
 */

const gchar *
GHIX11FindDesktopUriByExec(GHIPlatform *ghip,
                           const char *exec)
{
   char pathbuf[MAXPATHLEN];
   gchar *pathname = NULL;
   gchar *uri = NULL;
   GHIMenuItem *gmi;
   gboolean fudged = FALSE;
   gboolean basenamed = FALSE;

   ASSERT(ghip);
   ASSERT(exec);

   /*
    * XXX This is not shippable.  This is to be addressed by milestone 3 with
    * the improved "fuzzy logic for UNITY_RPC_GET_WINDOW_PATH" deliverable.
    */

   return NULL;

   /*
    * Check our hash table first.  Negative entries are also cached.
    */
   if (g_hash_table_lookup_extended(ghip->appsByWindowExecutable,
                                    exec, NULL, (gpointer*)&uri)) {
      return uri;
   }

   /*
    * Okay, execPath may be absolute or relative.
    *
    * We'll search for a matching .desktop entry using the following methods:
    *
    * 1.  Use absolute path of exec.
    * 2.  Use absolute path of basename of exec.  (Resolves /opt/Adobe/Reader9/
    *     Reader/intellinux/bin/acroread to /usr/bin/acroread.)
    * 3.  Consult whitelist of known applications and guess at possible
    *     launchers.  (firefox-bin => firefox, soffice.bin => ooffice.)
    */

   /*
    * Attempt #1:  Start with unmodified input.
    */
   Str_Strcpy(pathbuf, exec, sizeof pathbuf);

tryagain:
   g_free(pathname);    // Placed here rather than at each goto.  I'm lazy.

   pathname = g_find_program_in_path(pathbuf);
   if (pathname) {
      gmi = (GHIMenuItem*)g_hash_table_lookup(ghip->appsByExecutable, pathname);
      if (gmi) {
         uri = GHIPlatformMenuItemToURI(ghip, gmi);
      }
   }

   if (!uri) {
      /*
       * Attempt #2:  Take the basename of exec.
       */
      if (!basenamed) {
         char tmpbuf[MAXPATHLEN];
         char *ctmp;

         basenamed = TRUE;

         /* basename(3) may modify the input buffer, so make a temporary copy. */
         Str_Strcpy(tmpbuf, pathbuf, sizeof tmpbuf);
         ctmp = basename(tmpbuf);
         if (ctmp != NULL) {
            Str_Strcpy(pathbuf, ctmp, sizeof pathbuf);
            goto tryagain;
         }
      }

      /*
       * Attempt #3:  Get our whitelist on.
       */
      if (!fudged) {
         static struct {
            const gchar *pattern;
            const gchar *exec;
         } fudgePatterns[] = {
            /*
             * XXX Worth compiling once?  Consider placing in an external filter
             * file to allow users to update it themselves easily.
             */
            { "*firefox*-bin", "firefox" },
            { "*thunderbird*-bin", "thunderbird" },
            { "*soffice.bin", "ooffice" }
         };
         unsigned int i;

         fudged = TRUE;

         for (i = 0; i < ARRAYSIZE(fudgePatterns); i++) {
            if (g_pattern_match_simple(fudgePatterns[i].pattern,
                                       pathbuf)) {
               Str_Strcpy(pathbuf, fudgePatterns[i].exec, sizeof pathbuf);
               goto tryagain;
            }
         }
      }
   }

   g_free(pathname);

   /*
    * Cache the result, even if it was negative.
    */
   g_hash_table_insert(ghip->appsByWindowExecutable, g_strdup(exec), uri);

   return uri;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIX11DetectDesktopEnv --
 *
 *      Query environment and/or root window properties to determine if we're
 *      under GNOME or KDE.
 *
 *      XXX Consider moving this to another library.
 *      XXX Investigate whether this requires legal review, since it's cribbed
 *      from xdg-utils' detectDE subroutine (MIT license).
 *      XXX Add the _DT_SESSION bit for XFCE detection.
 *
 * Results:
 *      Pointer to a valid string on success, NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

const char *
GHIX11DetectDesktopEnv(void)
{
   static const char *desktopEnvironment = NULL;
   const char *tmp;

   if (desktopEnvironment) {
      return desktopEnvironment;
   }

   if (g_strcmp0(g_getenv("KDE_FULL_SESSION"), "true") == 0) {
      desktopEnvironment = "KDE";
   } else if ((tmp = g_getenv("GNOME_DESKTOP_SESSION_ID")) && *tmp) {
      desktopEnvironment = "GNOME";
   }

   return desktopEnvironment;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AppInfoLaunchEnv --
 *
 *      Wrapper around g_app_info_launch which takes a custom environment into
 *      account.
 *
 *      GHI/X11 should spawn applications using ghip->nativeEnviron, but
 *      g_app_info_launch doesn't taken a custom environment as a parameter.
 *      Rather than reimplement that function, we work around it by doing the
 *      following:
 *
 *         Parent:
 *         1.  Fork a child process.
 *         2.  Block until child terminates, returning true if the child exited
 *             with an exit code of 0.
 *
 *         Child:
 *         1.  Flush the environment and build a new one from nativeEnviron.
 *         2.  Spawn desired application with g_app_info_launch.
 *         3.  Exit with 0 if spawn was successful, otherwise 1.
 *
 * Results:
 *      Returns true if launch succeeded, false otherwise.
 *
 * Side effects:
 *      Creates a child process and blocks until the child exits.  Child lasts
 *      only as long as it takes to call g_app_info_launch.
 *
 *-----------------------------------------------------------------------------
 */

static bool
AppInfoLaunchEnv(GHIPlatform* ghip,     // IN
                 GAppInfo* appInfo)     // IN
{
   bool success = false;

   pid_t myPid = fork();
   switch (myPid) {
   case -1:
      /* Error. */
      g_warning("%s: fork: %s\n", __FUNCTION__, strerror(errno));
      break;

   case 0:
      /* Child:  Exit with _exit() so as to not trigger any atexit() routines. */
      {
         if (clearenv() == 0) {
            std::vector<Glib::ustring>::iterator envp;
            for (envp = ghip->nativeEnviron.begin();
                 envp != ghip->nativeEnviron.end();
                 ++envp) {
               /*
                * The string passed to putenv() becomes part of the environment --
                * it isn't copied.  That's fine, though, because we're running in
                * the context of a very short-lived wrapper process.
                */
               if (putenv((char*)envp->c_str()) != 0) {
                  g_warning("%s: failed to restore native environment\n", __FUNCTION__);
                  _exit(1);
               }
            }
            success = g_app_info_launch(appInfo, NULL, NULL, NULL);
         }
         _exit(success == true ? 0 : 1);
      }
      break;

   default:
      /* Parent:  Hang out until our child terminates. */
      {
         int status = 0;
         int ret = -1;
         while (1) {
            ret = waitpid(myPid, &status, 0);
            if ((ret == -1 && errno != EINTR) ||
                (ret == myPid && (WIFEXITED(status) || WIFSIGNALED(status)))) {
               break;
            }
         }
         success = (ret == myPid) && WIFEXITED(status) && WEXITSTATUS(status) == 0;
      }
   }

   return success;
}
