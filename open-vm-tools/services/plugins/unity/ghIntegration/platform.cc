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
#include <sigc++/sigc++.h>

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
#include "xdg.h"
};

#define URI_TEXTRANGE_EQUAL(textrange, str) \
   (((textrange).afterLast - (textrange).first) == (ssize_t) strlen((str))        \
    && !strncmp((textrange).first, (str), (textrange).afterLast - (textrange).first))

#include "appUtil.h"
#include "ghIntegration.h"
#include "ghIntegrationInt.h"
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
static void OnMenusChanged(GHIPlatform* ghip);


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
   const char *desktopEnv = Xdg_DetectDesktopEnv();
   Bool supported = (g_strcmp0(desktopEnv, "GNOME") == 0) ||
                    (g_strcmp0(desktopEnv, "KDE") == 0) ||
                    (g_strcmp0(desktopEnv, "XFCE") == 0);
   if (!supported) {
      g_message("GHI not available under unsupported desktop environment %s\n",
                desktopEnv ? desktopEnv : "(nil)");
   }
   return supported;
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

   if (!GHIPlatformIsSupported()) {
      /*
       * Don't bother allocating resources if running under an unsupported
       * desktop environment.
       */
      return NULL;
   }

   ghip = (GHIPlatform *) Util_SafeCalloc(1, sizeof *ghip);
   ghip->appsByWindowExecutable =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
   ghip->hostCallbacks = hostCallbacks;
   AppUtil_Init();

   const char** tmp;
   for (tmp = envp; *tmp; tmp++) {
      /*
       * PR 685881: DESKTOP_AUTOSTART_ID was proposed on the xdg@freedesktop.org
       * mailing list, but doesn't seem like it made it to a final spec.
       *
       * http://lists.freedesktop.org/archives/xdg/2007-January/007436.html
       *
       * It refers to a XSMP session manager client ID which shouldn't be
       * passed to children.  Having this environment variable breaks launching
       * nautilus w/o arguments.  (Aside, GNOME fixed this upstream in response
       * to https://bugzilla.gnome.org/show_bug.cgi?id=649063.)
       */
      if (g_str_has_prefix(*tmp, "DESKTOP_AUTOSTART_ID=")) {
         continue;
      }

      ghip->nativeEnviron.push_back(*tmp);
   }

   /*
    * PR 698958: Unity: There can be only one.  (Disable Ubuntu global application menu.)
    * See https://wiki.ubuntu.com/DesktopExperienceTeam/ApplicationMenu#Troubleshooting
    */
   ghip->nativeEnviron.push_back("UBUNTU_MENUPROXY=");

   desktopEnv = Xdg_DetectDesktopEnv();
   ASSERT(desktopEnv); // Asserting based on GHIPlatformIsSupported check above.
   g_desktop_app_info_set_desktop_env(desktopEnv);

#ifdef REDIST_GMENU
   ghip->menuItemManager = new MenuItemManager(desktopEnv);
   sigc::slot<void,GHIPlatform*> menuSlot = sigc::ptr_fun(&OnMenusChanged);
   ghip->menuItemManager->menusChanged.connect(sigc::bind(menuSlot, ghip));
   OnMenusChanged(ghip);
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

#ifdef REDIST_GMENU
   delete ghip->menuItemManager;
#endif
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

   /* Strip query component. */
   size_t uriSize = strlen(pathURIUtf8) + 1;
   gchar *uriSansQuery = (gchar*)g_alloca(uriSize);
   memcpy(uriSansQuery, pathURIUtf8, uriSize);
   gchar *tmp = strchr(uriSansQuery, '?');
   if (tmp) { *tmp = '\0'; }

   if (uriSansQuery[0] == '/') {
      realCmd = uriSansQuery;
   } else if (uriParseUriA(&state, uriSansQuery) == URI_SUCCESS) {
      if (URI_TEXTRANGE_EQUAL(uri.scheme, "file")) {
         gchar* tmp = (gchar*)g_alloca(strlen(uriSansQuery) + 1);
         uriUriStringToUnixFilenameA(uriSansQuery, tmp);

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

         if (g_str_has_suffix(unixFile.c_str(), ".desktop")) {
            Glib::RefPtr<Gio::DesktopAppInfo> desktopFileInfo;

            desktopFileInfo = Gio::DesktopAppInfo::create_from_filename(unixFile);
            if (desktopFileInfo) {
               friendlyName = desktopFileInfo->get_name();
               GHIX11IconGetIconsForDesktopFile(unixFile.c_str(), iconList);
               success = TRUE;
            }
         } else if (appMgr.GetAppByUri(uriSansQuery, app)) {
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
   ASSERT(ghip);
   ASSERT(fileUtf8);

   Debug("%s: file: '%s'\n", __FUNCTION__, fileUtf8);

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
         Glib::ustring de = Xdg_DetectDesktopEnv();
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

   return FALSE;
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


/*
 *-----------------------------------------------------------------------------
 *
 * OnMenusChanged --
 *
 *      Signal handler for updates to launch or fixed menus.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Calls transport's launchMenuChange callback, if set.
 *
 *-----------------------------------------------------------------------------
 */

static void
OnMenusChanged(GHIPlatform *ghip)       // IN
{
   if (ghip->hostCallbacks.launchMenuChange) {
      std::vector<const char *> folderKeysChanged;
      folderKeysChanged.push_back(UNITY_START_MENU_LAUNCH_FOLDER);
      folderKeysChanged.push_back(UNITY_START_MENU_FIXED_FOLDER);
      ghip->hostCallbacks.launchMenuChange(folderKeysChanged.size(),
                                           &folderKeysChanged[0]);
   }
}
