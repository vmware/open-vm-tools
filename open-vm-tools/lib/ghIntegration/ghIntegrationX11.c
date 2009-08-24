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

#define _BSD_SOURCE 1 // Needed on Linux to get the DT_* values for dirent->d_type
#include <dirent.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef GTK2
#error "Gtk 2.0 is required"
#endif

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf-core.h>

// gdkx.h includes Xlib.h, which #defines Bool.
#include <gdk/gdkx.h>
#undef Bool

#include "vmware.h"
#include "base64.h"
#include "rpcin.h"
#include "dbllnklst.h"
#include "debug.h"
#include "util.h"
#include "region.h"
#include "unity.h"
#include "unityCommon.h"
#include "system.h"
#include "codeset.h"
#include "imageUtil.h"
#include "strutil.h"
#include <paths.h>
#include "vm_atomic.h"
#include "mntinfo.h"
#include "ghIntegration.h"
#include "ghIntegrationInt.h"
#include "guest_msg_def.h"
#include "guestCaps.h"
#include "Uri.h"
#define URI_TEXTRANGE_EQUAL(textrange, str) \
   (((textrange).afterLast - (textrange).first) == strlen((str))        \
    && !strncmp((textrange).first, (str), (textrange).afterLast - (textrange).first))

#include "appUtil.h"

/*
 * The following defines appear in newer versions of glib 2.x, so
 * we define them for backwards compat.
 */
#ifndef G_KEY_FILE_DESKTOP_GROUP
#define G_KEY_FILE_DESKTOP_GROUP                "Desktop Entry"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_NAME
#define G_KEY_FILE_DESKTOP_KEY_NAME             "Name"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_ICON
#define G_KEY_FILE_DESKTOP_KEY_ICON             "Icon"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_EXEC
#define G_KEY_FILE_DESKTOP_KEY_EXEC             "Exec"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_TRY_EXEC
#define G_KEY_FILE_DESKTOP_KEY_TRY_EXEC         "TryExec"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_CATEGORIES
#define G_KEY_FILE_DESKTOP_KEY_CATEGORIES       "Categories"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY
#define G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY       "NoDisplay"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_HIDDEN
#define G_KEY_FILE_DESKTOP_KEY_HIDDEN           "Hidden"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN
#define G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN     "OnlyShowIn"
#endif
#ifndef G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN
#define G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN      "NotShowIn"
#endif

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
 * The GHIDirectoryWatch object represents a watch on a directory to be notified of
 * added/removed/changed .desktop files.
 *
 * XXX Watching directories for added/changed/removed .desktop files is not yet
 * implemented. We need to figure out whether we want to use inotify, dnotify, gamin,
 * etc. and work through all the backwards compat issues.
 */
typedef struct {
   char *directoryPath;
} GHIDirectoryWatch;

struct _GHIPlatform {
   GTree *apps; // Tree of GHIMenuDirectory's, keyed & ordered by their dirname
   GHashTable *appsByExecutable; // Translates full executable path to GHIMenuItem
   GHashTable *appsByDesktopEntry; // Translates full .desktop path to GHIMenuItem

   Bool trackingEnabled;
   GArray *directoriesTracked;

   int nextMenuHandle;
   GHashTable *menuHandles;

   /** Pre-wrapper script environment.  See @ref System_GetNativeEnviron. */
   const char **nativeEnviron;
};

#ifdef GTK2

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

static void GHIPlatformSetMenuTracking(GHIPlatform *ghip,
                                       Bool isEnabled);
static char *GHIPlatformUriPathToString(UriPathSegmentA *path);


/*
 * This is a list of directories that we search for .desktop files by default.
 */
static const char *desktopDirs[] = {
   "/usr/share/applications",
   "/opt/gnome/share/applications",
   "/opt/kde3/share/applications",
   "/opt/kde4/share/applications",
   "/opt/kde/share/applications",
   "/usr/share/applnk",
   "~/.local/share/applications"
};


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


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformDestroyMenuItem --
 *
 *      Frees a menu item object (which right now is just a GKeyFile).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The specified GHIMenuItem is no longer valid.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformDestroyMenuItem(gpointer data,      // IN
                           gpointer user_data) // IN (unused)
{
   GHIMenuItem *gmi;

   ASSERT(data);

   gmi = data;
   g_key_file_free(gmi->keyfile);
   g_free(gmi->keyfilePath);
   g_free(gmi->exepath);
   g_free(gmi);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformDestroyMenuDirectory --
 *
 *      Frees the memory associated with a GHIMenuDirectory object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The specified GHIMenuDirectory object is no longer valid.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformDestroyMenuDirectory(gpointer data) // IN
{
   GHIMenuDirectory *gmd = (GHIMenuDirectory *) data;

   // gmd->dirname comes from a static const array, so it should never be freed
   g_ptr_array_foreach(gmd->items, GHIPlatformDestroyMenuItem, NULL);
   g_ptr_array_free(gmd->items, TRUE);
}
#endif // GTK2


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
#ifdef GTK2
   return TRUE;
#else
   return FALSE;
#endif
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
GHIPlatformInit(VMU_ControllerCB *vmuControllerCB,  // IN
                void *ctx)                          // IN
{
   extern const char **environ;
   GHIPlatform *ghip;

   ghip = Util_SafeCalloc(1, sizeof *ghip);
   ghip->directoriesTracked = g_array_new(FALSE, FALSE, sizeof(GHIDirectoryWatch));
   ghip->nativeEnviron = System_GetNativeEnviron(environ);
   AppUtil_Init();

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


#ifdef GTK2
/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformFreeValue --
 *
 *      Frees a hash table entry. Typically called from a g_hashtable
 *      iterator. Just the value is destroyed, not the key.
 *      Also called directly from GHIPlatformCloseStartMenu.
 *
 * Results:
 *      TRUE always.
 *
 * Side effects:
 *      The specified value will no longer be valid.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
GHIPlatformFreeValue(gpointer key,       // IN
                     gpointer value,     // IN
                     gpointer user_data) // IN
{
   g_free(value);

   return TRUE;
}
#endif // GTK2


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformCleanupMenuEntries --
 *
 *      Frees all the memory associated with the menu information, including active menu
 *      handles and the internal applications menu representation.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformCleanupMenuEntries(GHIPlatform *ghip) // IN
{
#ifdef GTK2
   if (ghip->menuHandles) {
      g_hash_table_foreach_remove(ghip->menuHandles, GHIPlatformFreeValue, NULL);
      g_hash_table_destroy(ghip->menuHandles);
      ghip->menuHandles = NULL;
   }

   if (ghip->apps) {
      g_hash_table_destroy(ghip->appsByDesktopEntry);
      g_hash_table_destroy(ghip->appsByExecutable);
      g_tree_destroy(ghip->apps);
      ghip->apps = NULL;
   }
#endif // GTK2
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

   GHIPlatformSetMenuTracking(ghip, FALSE);
   g_array_free(ghip->directoriesTracked, TRUE);
   ghip->directoriesTracked = NULL;
   if (ghip->nativeEnviron) {
      System_FreeNativeEnviron(ghip->nativeEnviron);
      ghip->nativeEnviron = NULL;
   }
   free(ghip);
}


#ifdef GTK2


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformCollectIconInfo --
 *
 *      Sucks all the icon information for a particular application from the system, and
 *      appends it into the DynBuf for returning to the host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Adds data into the DynBuf.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformCollectIconInfo(GHIPlatform *ghip,        // IN
                           GHIMenuItem *ghm,         // IN
                           unsigned long windowID,   // IN
                           DynBuf *buf)              // IN/OUT
{
   GPtrArray *pixbufs;
   char tbuf[1024];
   gsize totalIconBytes;
   char *ctmp = NULL;
   int i;

   if (ghm) {
      ctmp = g_key_file_get_string(ghm->keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                   G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
   }

   pixbufs = AppUtil_CollectIconArray(ctmp, windowID);

   /*
    * Now see if all of these icons can fit into our reply.
    */
   totalIconBytes = DynBuf_GetSize(buf);
   for (i = 0; i < pixbufs->len; i++) {
      gsize thisIconBytes;
      GdkPixbuf *pixbuf = g_ptr_array_index(pixbufs, i);

      thisIconBytes = ICON_SPACE_PADDING; // Space used by the width/height/size strings, and breathing room
      thisIconBytes += gdk_pixbuf_get_width(pixbuf)
                       * gdk_pixbuf_get_height(pixbuf)
                       * 4 /* image will be BGRA */;
      if ((thisIconBytes + totalIconBytes) < GUESTMSG_MAX_IN_SIZE) {
         totalIconBytes += thisIconBytes;
      } else if (pixbufs->len == 1) {
         GdkPixbuf *newIcon;
         volatile double newWidth;
         volatile double newHeight;
         volatile double scaleFactor;

         newWidth = gdk_pixbuf_get_width(pixbuf);
         newHeight = gdk_pixbuf_get_height(pixbuf);
         scaleFactor = (GUESTMSG_MAX_IN_SIZE - totalIconBytes - ICON_SPACE_PADDING);
         scaleFactor /= (newWidth * newHeight * 4.0);
         if (scaleFactor > 0.95) {
            /*
             * Ensures that we remove at least a little bit of data from the icon.
             * Otherwise we can get things like scalefactors of '0.999385' which result
             * in an image of exactly the same size. A scaleFactor of 0.95 will remove at
             * least one row or column from any icon large enough to go past the limit.
             */
            scaleFactor = 0.95;
         }

         newWidth *= scaleFactor;
         newHeight *= scaleFactor;

         /*
          * If this is the only icon available, try scaling it down to the largest icon
          * that will comfortably fit in the reply.
          *
          * Adding 0.5 to newWidth & newHeight is an easy way of rounding to the closest
          * integer.
          */
         newIcon = gdk_pixbuf_scale_simple(pixbuf,
                                           (int)(newWidth + 0.5),
                                           (int)(newHeight + 0.5),
                                           GDK_INTERP_HYPER);
         g_object_unref(G_OBJECT(pixbuf));
         g_ptr_array_index(pixbufs, i) = newIcon;
         i--; // Try including the newly scaled-down icon
      } else {
         g_object_unref(G_OBJECT(pixbuf));
         g_ptr_array_remove_index_fast(pixbufs, i);
         i--;
      }
   }

   /*
    * Now that we actually have all available icons loaded and checked, dump their count
    * and contents into the reply.
    */
   Str_Sprintf(tbuf, sizeof tbuf, "%u", pixbufs->len);
   DynBuf_AppendString(buf, tbuf);

   for (i = 0; i < pixbufs->len; i++) {
      int width;
      int height;
      GdkPixbuf *pixbuf;
      guchar *pixels;
      int x, y;
      int rowstride;
      int n_channels;

      pixbuf = g_ptr_array_index(pixbufs, i);

      width = gdk_pixbuf_get_width(pixbuf);
      height = gdk_pixbuf_get_height(pixbuf);
      Str_Sprintf(tbuf, sizeof tbuf, "%d", width);
      DynBuf_AppendString(buf, tbuf);
      Str_Sprintf(tbuf, sizeof tbuf, "%d", height);
      DynBuf_AppendString(buf, tbuf);

      Str_Sprintf(tbuf, sizeof tbuf, "%d", width * height * 4);
      DynBuf_AppendString(buf, tbuf);

      ASSERT (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
      ASSERT (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);
      rowstride = gdk_pixbuf_get_rowstride(pixbuf);
      n_channels = gdk_pixbuf_get_n_channels(pixbuf);
      pixels = gdk_pixbuf_get_pixels(pixbuf);
      for (y = height - 1; y >= 0; y--) { // GetBinaryInfo icons are bottom-to-top. :(
         for (x = 0; x < width; x++) {
            char bgra[4];
            guchar *p; // Pointer to RGBA data in GdkPixbuf

            p = pixels + (y * rowstride) + (x * n_channels);
            bgra[0] = p[2];
            bgra[1] = p[1];
            bgra[2] = p[0];
            if (n_channels > 3) {
               bgra[3] = p[3];
            } else {
               bgra[3] = 0xFF;
            }
            DynBuf_Append(buf, bgra, 4);
         }
      }

      DynBuf_AppendString(buf, "");

   }

   AppUtil_FreeIconArray(pixbufs);
}
#endif // GTK2


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
                         DynBuf *buf)               // OUT: binary information
{
#ifdef GTK2
   const char *realCmd = NULL;
   char *keyfilePath = NULL;
   unsigned long windowID = 0;
   gpointer freeMe = NULL;
   GHIMenuItem *ghm = NULL;
   char *ctmp;
   UriParserStateA state;
   UriUriA uri;

   ASSERT(ghip);
   ASSERT(pathURIUtf8);
   ASSERT(buf);

   memset(&state, 0, sizeof state);
   memset(&uri, 0, sizeof uri);
   state.uri = &uri;

   if (pathURIUtf8[0] == '/') {
      realCmd = pathURIUtf8;
   } else if (uriParseUriA(&state, pathURIUtf8) == URI_SUCCESS) {
      if (URI_TEXTRANGE_EQUAL(uri.scheme, "file")) {
         UriQueryListA *queryList = NULL;
         int itemCount;

         realCmd = freeMe = GHIPlatformUriPathToString(uri.pathHead);
         if (uriDissectQueryMallocA(&queryList, &itemCount,
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
      } else {
         uriFreeUriMembersA(&uri);
         Debug("Binary URI %s does not have a 'file' scheme\n", pathURIUtf8);
         return FALSE;
      }
   } else {
      uriFreeUriMembersA(&uri);
      return FALSE;
   }

   GHIPlatformSetMenuTracking(ghip, TRUE);

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
      ghm = g_hash_table_lookup(ghip->appsByDesktopEntry, keyfilePath);
      g_free(keyfilePath);
   }

   if (!ghm) {
      /*
       * Now that we have the full path, look it up in our hash table of GHIMenuItems
       */
      ghm = g_hash_table_lookup(ghip->appsByExecutable, realCmd);
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
            realCmd = freeMe = ctmp;
         } else {
            realCmd = newPath;
         }

         ghm = g_hash_table_lookup(ghip->appsByExecutable, realCmd);
      }
   }
   /*
    * Stick the app name into 'buf'.
    */
   if (ghm) {
      ctmp = g_key_file_get_locale_string(ghm->keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                   G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL);
      if (!ctmp) {
         ctmp = g_path_get_basename(realCmd);
      }
      DynBuf_AppendString(buf, ctmp);
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
      DynBuf_AppendString(buf, ctmp);
   }

   free(freeMe);
   ctmp = freeMe = NULL;

   GHIPlatformCollectIconInfo(ghip, ghm, windowID, buf);

   return TRUE;
#else // !GTK2
   return FALSE;
#endif // GTK2
}


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
 *      TRUE if everything went ok, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
GHIPlatformGetBinaryHandlers(GHIPlatform *ghip,      // IN: platform-specific state
                             const char *pathUtf8,   // IN: full path to the executable
                             XDR *xdrs)              // OUT: binary information
{
   return FALSE;
}


#ifdef GTK2


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformGetDesktopName --
 *
 *      Figures out which desktop environment we're running under.
 *
 * Results:
 *      Desktop name if successful, NULL otherwise.
 *
 * Side effects:
 *      Allocates memory to hold return value.
 *
 *-----------------------------------------------------------------------------
 */

static const char *
GHIPlatformGetDesktopName(void)
{
   int i;
   static const char *clientMappings[][2] = {
      {"gnome-panel", "GNOME"},
      {"gnome-session", "GNOME"},
      {"nautilus", "GNOME"},
      {"ksmserver", "KDE"},
      {"kicker", "KDE"},
      {"startkde", "KDE"},
      {"konqueror", "KDE"},
      {"xfce-mcs-manage", "XFCE"},
      {"xfwm4", "XFCE"},
      {"ROX-Session", "ROX"}
   };
   Display *display;
   Window rootWindow;
   Window temp1;        // throwaway
   Window temp2;        // throwaway
   Window *children = NULL;
   unsigned int nchildren;
   static const char *desktopEnvironment = NULL;

   /*
    * NB: While window managers may change during vmware-user's execution, TTBOMK
    * desktop environments cannot, so this is safe.
    */
   if (desktopEnvironment) {
      return desktopEnvironment;
   }

   display = gdk_x11_get_default_xdisplay();
   rootWindow = DefaultRootWindow(display);

   if (XQueryTree(display, rootWindow, &temp1, &temp2, &children, &nchildren) == 0) {
      return NULL;
   }

   for (i = 0; i < nchildren && !desktopEnvironment; i++) {
      XClassHint wmClass = { 0, 0 };
      int j;

      /*
       * Try WM_CLASS first, then try WM_NAME.
       */

      if (XGetClassHint(display, children[i], &wmClass) != 0) {
         for (j = 0; j < ARRAYSIZE(clientMappings) && !desktopEnvironment; j++) {
            if ((strcasecmp(clientMappings[j][0], wmClass.res_name) == 0) ||
                (strcasecmp(clientMappings[j][0], wmClass.res_class) == 0)) {
               desktopEnvironment = clientMappings[j][1];
            }
         }
         XFree(wmClass.res_name);
         XFree(wmClass.res_class);
      }

      if (!desktopEnvironment) {
         char *name = NULL;

         if ((XFetchName(display, children[i], &name) == 0) ||
             name == NULL) {
            continue;
         }

         for (j = 0; j < ARRAYSIZE(clientMappings) && !desktopEnvironment; j++) {
            if (!strcmp(clientMappings[j][0], name)) {
               desktopEnvironment = clientMappings[j][1];
            }
         }

         XFree(name);
      }
   }

   XFree(children);
   return desktopEnvironment;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformIsMenuItemAllowed --
 *
 *      This routine tells the caller, based on policies defined by the .desktop file,
 *      whether the requested application should be displayed in the Unity menus.
 *
 * Results:
 *      TRUE if the item should be displayed, FALSE if it should not be.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GHIPlatformIsMenuItemAllowed(GHIPlatform *ghip, // IN:
                             GKeyFile *keyfile) // IN:
{
   const char *dtname;

   ASSERT(ghip);
   ASSERT(keyfile);

   /*
    * Examine the "NoDisplay" and "Hidden" properties.
    */
   if (g_key_file_get_boolean(keyfile,
                              G_KEY_FILE_DESKTOP_GROUP,
                              G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY,
                              NULL) ||
       g_key_file_get_boolean(keyfile,
                              G_KEY_FILE_DESKTOP_GROUP,
                              G_KEY_FILE_DESKTOP_KEY_HIDDEN,
                              NULL)) {
      Debug("%s: contains either NoDisplay or Hidden keys.\n", __func__);
      return FALSE;
   }

   /*
    * NB: This may return NULL.
    * XXX Perhaps that should be changed to return an empty string?
    */
   dtname = GHIPlatformGetDesktopName();

   /*
    * Check our desktop environment name against the OnlyShowIn and NotShowIn
    * lists.
    *
    * NB:  If the .desktop file defines OnlyShowIn as an empty string, we
    * effectively ignore it.  (Another interpretation would be that an application
    * shouldn't appear at all, but that's what the NoDisplay and Hidden keys are
    * for.)
    *
    * XXX I didn't see anything in the Key-value file parser reference, but I'm
    * wondering if there's some other GLib string list searching goodness that
    * would obviate the boilerplate-ish code below.
    */
   {
      gchar **onlyShowList = NULL;
      gsize nstrings;

      onlyShowList = g_key_file_get_string_list(keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                                G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN,
                                                &nstrings, NULL);
      if (onlyShowList && nstrings) {
         Bool matchedOnlyShowIn = FALSE;
         int i;

         if (dtname) {
            for (i = 0; i < nstrings; i++) {
               if (strcasecmp(dtname, onlyShowList[i]) == 0) {
                  matchedOnlyShowIn = TRUE;
                  break;
               }
            }
         }

         if (!matchedOnlyShowIn) {
            Debug("%s: OnlyShowIn does not include our desktop environment, %s.\n",
                  __func__, dtname ? dtname : "(not set)");
            g_strfreev(onlyShowList);
            return FALSE;
         }
      }
      g_strfreev(onlyShowList);
   }

   if (dtname) {
      gchar **notShowList = NULL;
      gsize nstrings;
      int i;

      notShowList = g_key_file_get_string_list(keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                               G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN,
                                               &nstrings, NULL);
      if (notShowList && nstrings) {
         for (i = 0; i < nstrings; i++) {
            if (strcasecmp(dtname, notShowList[i]) == 0) {
               Debug("%s: NotShowIn includes our desktop environment, %s.\n",
                     __func__, dtname);
               g_strfreev(notShowList);
               return FALSE;
            }
         }
      }
      g_strfreev(notShowList);
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformGetExecFromKeyFile --
 *
 *      Given a GLib GKeyFile, extract path(s) from the TryExec or Exec
 *      keys, normalize them, and return them to the caller.
 *
 * Results:
 *      Returns a string pointer to an absolute executable pathname on success or
 *      NULL on failure.
 *
 * Side effects:
 *      This routine returns memory allocated by GLib.  Caller is responsible
 *      for freeing it via g_free.
 *
 *-----------------------------------------------------------------------------
 */

static gchar *
GHIPlatformGetExecFromKeyfile(GHIPlatform *ghip, // IN
                              GKeyFile *keyfile) // IN
{
   gchar *exe = NULL;

   /*
    * TryExec is supposed to be a path to an executable without arguments that,
    * if set but not found or not executable, indicates that this menu item should
    * be skipped.
    */
   {
      gchar *tryExec;

      tryExec = g_key_file_get_string(keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, NULL);
      if (tryExec) {
         gchar *ctmp;
         ctmp = g_find_program_in_path(tryExec);

         if (ctmp == NULL) {
            Debug("%s: Entry has TryExec=%s, but it was not found in our PATH.\n",
                  __func__, tryExec);
            g_free(tryExec);
            return NULL;
         }

         g_free(ctmp);
         g_free(tryExec);
      }
   }

   /*
    * Next up:  Look up Exec key and do some massaging to skip over common interpreters.
    */
   {
      char *exec;
      char **argv;
      int argc;
      int i;
      gboolean parsed;

      exec = g_key_file_get_string(keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                   G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);

      if (!exec) {
         Debug("%s: Missing Exec key.\n", __func__);
         return NULL;
      }

      parsed = g_shell_parse_argv(exec, &argc, &argv, NULL);
      g_free(exec);

      if (!parsed) {
         Debug("%s: Unable to parse shell arguments.\n", __func__);
         return NULL;
      }

      for (i = 0; i < argc; i++) {
         /*
          * The Exec= line in the .desktop file may list other boring helper apps before
          * the name of the main app. getproxy is a common one. We need to skip those
          * arguments in the cmdline.
          */
         if (!AppUtil_AppIsSkippable(argv[i])) {
            exe = g_strdup(argv[i]);
            break;
         }
      }
      g_strfreev(argv);
   }

   /*
    * Turn it into a full path.  Yes, if we can't get an absolute path, we'll return
    * NULL.
    */
   if (exe && *exe != '/') {
      gchar *ctmp;

      ctmp = g_find_program_in_path(exe);
      g_free(exe);
      exe = ctmp;
      if (!exe) {
         Debug("%s: Unable to find program in PATH.\n", __func__);
      }
   }

   return exe;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformAddMenuItem --
 *
 *      Examines an application's .desktop file and inserts it into an appropriate
 *      Unity application menu.
 *
 * Results:
 *      A new GHIMenuItem will be created.  If our desired menu directory doesn't
 *      already exist, then we'll create that, too.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformAddMenuItem(GHIPlatform *ghip,       // IN:
                       const char *keyfilePath, // IN:
                       GKeyFile *keyfile,       // IN:
                       char *exePath)           // IN:
{
   /*
    * A list of categories that a .desktop file should be in in order to be relayed to
    * the host.
    *
    * NB: "Other" is a generic category where we dump applications for which we can't
    * determine an appropriate category.  This is "safe" as long as menu-spec doesn't
    * register it, and I don't expect that to happen any time soon.  It is -extremely-
    * important that "Other" be the final entry in this list.
    *
    * XXX See desktop-entry-spec and make use of .directory files.
    */
   static const char *validCategories[][2] = {
      /*
       * Bug 372348:
       * menu-spec category     pretty string
       */
      { "AudioVideo",           "Sound & Video" },
      { "Development",          0 },
      { "Education",            0 },
      { "Game",                 "Games" },
      { "Graphics",             0 },
      { "Network",              0 },
      { "Office",               0 },
      { "Settings",             0 },
      { "System",               0 },
      { "Utility",              0 },
      { "Other",                0 }
   };

   GHIMenuDirectory *gmd;
   GHIMenuItem *gmi;
   Bool foundIt = FALSE;
   char **categories = NULL;
   gsize numcats;
   int kfIndex;                 // keyfile categories index/iterator
   int vIndex;                  // validCategories index/iterator

   /*
    * Figure out if this .desktop file is in a category we want to put on our menus,
    * and if so which one...
    */
   categories = g_key_file_get_string_list(keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                           G_KEY_FILE_DESKTOP_KEY_CATEGORIES,
                                           &numcats, NULL);
   if (categories) {
      for (kfIndex = 0; kfIndex < numcats && !foundIt; kfIndex++) {
         /*
          * NB:  See validCategories' comment re: "Other" being the final, default
          * category.  It explains why we condition on ARRAYSIZE() - 1.
          */
         for (vIndex = 0; vIndex < ARRAYSIZE(validCategories) - 1; vIndex++) {
            if (!strcasecmp(categories[kfIndex], validCategories[vIndex][0])) {
               foundIt = TRUE;
               break;
            }
         }
      }
      g_strfreev(categories);
   }

   /*
    * If not found, fall back to "Other".
    */
   if (!foundIt) {
      vIndex = ARRAYSIZE(validCategories) - 1;
   }

   /*
    * We have all the information we need to create the new GHIMenuItem.
    */
   gmi = g_new0(GHIMenuItem, 1);
   gmi->keyfilePath = g_strdup(keyfilePath);
   gmi->keyfile = keyfile;
   gmi->exepath = exePath;

   gmd = g_tree_lookup(ghip->apps, validCategories[vIndex][0]);

   if (!gmd) {
      /*
       * A GHIMenuDirectory object does not yet exist for the validCategory
       * that this .desktop is in, so create that object.
       */
      gmd = g_new0(GHIMenuDirectory, 1);
      gmd->dirname = validCategories[vIndex][0];
      gmd->prettyDirname = validCategories[vIndex][1];
      gmd->items = g_ptr_array_new();
      g_tree_insert(ghip->apps, (gpointer)validCategories[vIndex][0], gmd);
      Debug("Created new category '%s'\n", gmd->dirname);
   }

   g_ptr_array_add(gmd->items, gmi);
   g_hash_table_insert(ghip->appsByExecutable, gmi->exepath, gmi);
   g_hash_table_insert(ghip->appsByDesktopEntry, gmi->keyfilePath, gmi);
   Debug("Loaded desktop item for %s into %s\n", gmi->exepath, gmd->dirname);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformReadDesktopFile --
 *
 *      Reads a .desktop file into our internal representation of the available
 *      applications.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformReadDesktopFile(GHIPlatform *ghip, // IN
                           const char *path)  // IN
{
   GKeyFile *keyfile = NULL;
   gchar *exe = NULL;

   Debug("%s: Analyzing %s.\n", __func__, path);

   /*
    * First load our .desktop file into a GLib GKeyFile structure.  Then perform
    * some rudimentary policy checks based on keys like NoDisplay and OnlyShowIn.
    */

   keyfile = g_key_file_new();
   if (!keyfile) {
      Debug("%s: g_key_file_new failed.\n", __func__);
      return;
   }

   if (!g_key_file_load_from_file(keyfile, path, 0, NULL) ||
       !GHIPlatformIsMenuItemAllowed(ghip, keyfile)) {
      g_key_file_free(keyfile);
      Debug("%s: Unable to load .desktop file or told to skip it.\n", __func__);
      return;
   }

   /*
    * Okay, policy checks passed.  Next up, obtain a normalized executable path,
    * and if successful insert it into our menus.
    */

   exe = GHIPlatformGetExecFromKeyfile(ghip, keyfile);
   if (exe) {
      /* The following routine takes ownership of keyfile and exec. */
      GHIPlatformAddMenuItem(ghip, path, keyfile, exe);
   } else {
      Debug("%s: Could not find executable for %s\n", __func__, path);
      g_key_file_free(keyfile);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformReadApplicationsDir --
 *
 *      Reads in the .desktop files in a particular directory.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformReadApplicationsDir(GHIPlatform *ghip, // IN
                               const char *dir)   // IN
{
   DIR *dirh;
   struct dirent *dent;
   GHIDirectoryWatch dirWatch;

   ASSERT(ghip);
   ASSERT(dir);

   dirh = opendir(dir);
   if (!dirh) {
      return;
   }

   dirWatch.directoryPath = strdup(dir);
   g_array_append_val(ghip->directoriesTracked, dirWatch);

   while ((dent = readdir(dirh))) {
      char subpath[PATH_MAX];
      struct stat sbuf;
      int subpathLen;

      if (!strcmp(dent->d_name, ".") ||
          !strcmp(dent->d_name, "..") ||
          !strcmp(dent->d_name, ".hidden")) {
         continue;
      }

      subpathLen = Str_Sprintf(subpath, sizeof subpath, "%s/%s", dir, dent->d_name);
      if (subpathLen >= (sizeof subpath - 1)) {
         Warning("There may be a recursive symlink or long path,"
                 " somewhere above %s. Skipping.\n", subpath);
         closedir(dirh);
         return;
      }
      if (dent->d_type == DT_UNKNOWN && stat(subpath, &sbuf)) {
         continue;
      }

      if (dent->d_type == DT_DIR ||
          (dent->d_type == DT_UNKNOWN
           && S_ISDIR(sbuf.st_mode))) {
         GHIPlatformReadApplicationsDir(ghip, subpath);
      } else if ((dent->d_type == DT_REG ||
                  (dent->d_type == DT_UNKNOWN
                   && S_ISREG(sbuf.st_mode)))
                 && StrUtil_EndsWith(dent->d_name, ".desktop")) {
         GHIPlatformReadDesktopFile(ghip, subpath);
      }
   }

   closedir(dirh);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformReadAllApplications --
 *
 *      Reads in information on all the applications that have .desktop files on this
 *      system.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      ghip->applist is created.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformReadAllApplications(GHIPlatform *ghip) // IN
{
   ASSERT(ghip);

   if (!ghip->apps) {
      int i;

      ghip->apps = g_tree_new_full((GCompareDataFunc)strcmp, NULL, NULL,
                                   GHIPlatformDestroyMenuDirectory);
      ghip->appsByExecutable = g_hash_table_new(g_str_hash, g_str_equal);
      ghip->appsByDesktopEntry = g_hash_table_new(g_str_hash, g_str_equal);

      for (i = 0; i < ARRAYSIZE(desktopDirs); i++) {
         if (StrUtil_StartsWith(desktopDirs[i], "~/")) {
            char cbuf[PATH_MAX];

            Str_Sprintf(cbuf, sizeof cbuf, "%s/%s",
                        g_get_home_dir(), desktopDirs[i] + 2);
            GHIPlatformReadApplicationsDir(ghip, cbuf);
         } else {
            GHIPlatformReadApplicationsDir(ghip, desktopDirs[i]);
         }
      }
   }
}
#endif // GTK2


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
#ifdef GTK2
   char temp[64];
   GHIMenuHandle *gmh;
   int itemCount = 0;
   Bool retval = FALSE;

   ASSERT(ghip);
   ASSERT(rootUtf8);
   ASSERT(buf);

   GHIPlatformSetMenuTracking(ghip, TRUE);

   if (!ghip->menuHandles) {
      ghip->menuHandles = g_hash_table_new(g_direct_hash, g_direct_equal);
   }

   if (!ghip->apps) {
      return FALSE;
   }

   gmh = g_new0(GHIMenuHandle, 1);
   gmh->handleID = ++ghip->nextMenuHandle;

   if (!strcmp(rootUtf8, UNITY_START_MENU_LAUNCH_FOLDER)) {
      gmh->handleType = LAUNCH_FOLDER;
      itemCount = g_tree_nnodes(ghip->apps);
      retval = TRUE;
   } else if (!strcmp(rootUtf8, UNITY_START_MENU_FIXED_FOLDER)) {
      /*
       * XXX Not yet implemented
       */
      gmh->handleType = FIXED_FOLDER;
      retval = TRUE;
   } else if (*rootUtf8) {
      gmh->handleType = DIRECTORY_FOLDER;

      if (StrUtil_StartsWith(rootUtf8, UNITY_START_MENU_LAUNCH_FOLDER)) {
         gmh->gmd = g_tree_lookup(ghip->apps,
                                  rootUtf8 + sizeof(UNITY_START_MENU_LAUNCH_FOLDER));
         if (gmh->gmd) {
            itemCount = gmh->gmd->items->len;
            retval = TRUE;
         }
      }
   }

   if (!retval) {
      g_free(gmh);
      return retval;
   }

   Debug("Opened start menu tree for %s with %d items, handle %d\n",
         rootUtf8, itemCount, gmh->handleID);

   g_hash_table_insert(ghip->menuHandles, GINT_TO_POINTER(gmh->handleID), gmh);

   Str_Sprintf(temp, sizeof temp, "%d %d", gmh->handleID, itemCount);
   DynBuf_AppendString(buf, temp);

   return TRUE;
#else // !GTK2
   return FALSE;
#endif // GTK2
}


#ifdef GTK2
/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformFindLaunchMenuItem --
 *
 *      A GTraverseFunc used to find the right item in the list of directories.
 *
 * Results:
 *      TRUE if tree traversal should stop, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
GHIPlatformFindLaunchMenuItem(gpointer key,   // IN
                              gpointer value, // IN
                              gpointer data)  // IN
{
   GHITreeTraversal *td;

   ASSERT(data);
   ASSERT(value);
   td = data;

   td->currentItem++;
   if (td->currentItem == td->desiredItem) {
      td->gmd = value;
      return TRUE;
   }

   return FALSE;
}
#endif // GTK2


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

   queryItems = alloca((argc + 1) * sizeof *queryItems);

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
   uriString = alloca(10 + 3 * strlen(gmi->exepath));
   if (uriUnixFilenameToUriStringA(gmi->exepath, uriString)) {
      g_strfreev(argv);
      return NULL;
   }
   if (uriComposeQueryCharsRequiredA(queryItems, &nchars) != URI_SUCCESS) {
      g_strfreev(argv);
      return NULL;
   }
   queryString = alloca(nchars + 1);
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
#ifdef GTK2
   GHIMenuHandle *gmh;
   char *itemName = NULL;
   uint itemFlags = 0;
   char *itemPath = NULL;
   char *localizedItemName = NULL;
   Bool freeItemName = FALSE;
   Bool freeItemPath = FALSE;
   Bool freeLocalItemName = FALSE;
   char temp[64];

   ASSERT(ghip);
   ASSERT(ghip->menuHandles);
   ASSERT(buf);

   gmh = g_hash_table_lookup(ghip->menuHandles, GINT_TO_POINTER(handle));
   if (!gmh) {
      return FALSE;
   }

   switch (gmh->handleType) {
   case LAUNCH_FOLDER:
      {
         GHITreeTraversal traverseData = { -1, itemIndex, NULL };

         /*
          * We're iterating through the list of directories.
          */
         if (!ghip->apps) {
            return FALSE;
         }

         g_tree_foreach(ghip->apps, GHIPlatformFindLaunchMenuItem, &traverseData);
         if (!traverseData.gmd) {
            return FALSE;
         }

         itemPath = "";
         itemFlags = UNITY_START_MENU_ITEM_DIRECTORY; // It's a directory
         itemName = g_strdup_printf("%s/%s", UNITY_START_MENU_LAUNCH_FOLDER,
                                    traverseData.gmd->dirname);
         freeItemName = TRUE;
         localizedItemName = traverseData.gmd->prettyDirname ?
            (char *)traverseData.gmd->prettyDirname :
            (char *)traverseData.gmd->dirname;
      }
      break;
   case FIXED_FOLDER:
      return FALSE;

   case DIRECTORY_FOLDER:
      {
         GHIMenuItem *gmi;

         if (gmh->gmd->items->len <= itemIndex) {
            return FALSE;
         }

         gmi = g_ptr_array_index(gmh->gmd->items, itemIndex);

         localizedItemName = g_key_file_get_locale_string(gmi->keyfile,
                                                          G_KEY_FILE_DESKTOP_GROUP,
                                                          G_KEY_FILE_DESKTOP_KEY_NAME,
                                                          NULL, NULL);
         freeLocalItemName = TRUE;
         itemName = g_strdup_printf("%s/%s/%s", UNITY_START_MENU_LAUNCH_FOLDER,
                                    gmh->gmd->dirname, localizedItemName);
         freeItemName = TRUE;

         itemPath = GHIPlatformMenuItemToURI(ghip, gmi);
         freeItemPath = TRUE;
      }
      break;
   }

   DynBuf_AppendString(buf, itemName);
   Str_Sprintf(temp, sizeof temp, "%u", itemFlags);
   DynBuf_AppendString(buf, temp);
   DynBuf_AppendString(buf, itemPath ? itemPath : "");
   DynBuf_AppendString(buf, localizedItemName ? localizedItemName : itemName);

   if (freeItemName) {
      g_free(itemName);
   }
   if (freeItemPath) {
      g_free(itemPath);
   }
   if (freeLocalItemName) {
      g_free(localizedItemName);
   }

   return TRUE;
#else // !GTK2
   return FALSE;
#endif // GTK2
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
#ifdef GTK2
   GHIMenuHandle *gmh;

   ASSERT(ghip);
   if (!ghip->menuHandles) {
      return TRUE;
   }

   gmh = g_hash_table_lookup(ghip->menuHandles, GINT_TO_POINTER(handle));
   if (!gmh) {
      return TRUE;
   }

   g_hash_table_remove(ghip->menuHandles, GINT_TO_POINTER(gmh->handleID));
   GHIPlatformFreeValue(NULL, gmh, NULL);

   return TRUE;
#else // !GTK2
   return FALSE;
#endif // GTK2
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
               ghm = g_hash_table_lookup(ghip->appsByDesktopEntry,
                                         targetDotDesktop);
            }
            if (!ghm) {
               ASSERT (fullargs->len > 0);
               ghm = g_hash_table_lookup(ghip->appsByExecutable,
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

   if (GHIPlatformCombineArgs(ghip, fileUtf8, &fullArgv, &fullArgc) &&
       fullArgc > 0) {
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
                             G_SPAWN_SEARCH_PATH |
                             G_SPAWN_STDOUT_TO_DEV_NULL |
                             G_SPAWN_STDERR_TO_DEV_NULL,
                             NULL, NULL, NULL, NULL);
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
GHIPlatformShellAction(GHIPlatform *ghip,       // IN: platform-specific state
                       const XDR *xdrs)         // IN: XDR Serialized arguments
{
   /*
    * TODO: implement the shell action execution.
    * The GHIPlatformShellUrlOpen() below is left for reference, but is not
    * used right now. Its functionality should be integrated here.
    */
   ASSERT(ghip);
   ASSERT(xdrs);

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
                           const XDR *xdrs)      // IN: XDR Serialized arguments
{
   ASSERT(ghip);
   ASSERT(xdrs);

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
                                      const XDR *xdrs)    // IN: XDR Serialized arguments
{
   ASSERT(ghip);
   ASSERT(xdrs);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIPlatformSetMenuTracking --
 *
 *      Turns menu tracking on/off.
 *
 *      XXX needs additional implementation work, as per the comment above
 *      GHIDirectoryWatch.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
GHIPlatformSetMenuTracking(GHIPlatform *ghip, // IN
                           Bool isEnabled)    // IN
{
   int i;
   ASSERT(ghip);

   if (isEnabled == ghip->trackingEnabled) {
      return;
   }

   ghip->trackingEnabled = isEnabled;
   if (isEnabled) {
      GHIPlatformReadAllApplications(ghip);
   } else {
      GHIPlatformCleanupMenuEntries(ghip);

      for (i = 0; i < ghip->directoriesTracked->len; i++) {
         GHIDirectoryWatch *dirWatch;

         dirWatch = &g_array_index(ghip->directoriesTracked, GHIDirectoryWatch, i);
         g_free(dirWatch->directoryPath);
      }
      g_array_set_size(ghip->directoriesTracked, 0);
   }
}


/*
 *------------------------------------------------------------------------------
 *
 * GHIPlatformGetProtocolHandlers --
 *
 *     XXX Needs to be implemented for Linux/X11 guests.
 *     Retrieve the list of protocol handlers from the guest.
 *
 * Results:
 *     TRUE on success
 *     FALSE on error
 *
 * Side effects:
 *     None
 *
 *------------------------------------------------------------------------------
 */

Bool
GHIPlatformGetProtocolHandlers(GHIPlatform *ghip,                           // UNUSED
                               GHIProtocolHandlerList *protocolHandlerList) // IN
{
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
GHIPlatformSetOutlookTempFolder(GHIPlatform *ghip,  // IN: platform-specific state
                                const XDR *xdrs)    // IN: XDR Serialized arguments
{
   ASSERT(ghip);
   ASSERT(xdrs);

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * GHIPlatformRestoreOutlookTempFolder --
 *
 *    Set the temporary folder used by Microsoft Outlook to store attachments
 *    opened by the user.
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
GHIPlatformRestoreOutlookTempFolder(GHIPlatform *ghip)  // IN: platform-specific state
{
   ASSERT(ghip);

   return FALSE;
}


/**
 * @brief Performs an action on the Trash (aka Recycle Bin) folder.
 *
 * Performs an action on the Trash (aka Recycle Bin) folder. Currently, the
 * only supported actions are to open the folder, or empty it.
 *
 * @param[in] ghip Pointer to platform-specific GHI data.
 * @param[in] xdrs Pointer to XDR serialized arguments.
 *
 * @retval TRUE  The action was performed.
 * @retval FALSE The action couldn't be performed.
 */

Bool
GHIPlatformTrashFolderAction(GHIPlatform *ghip,
                             const XDR   *xdrs)
{
   ASSERT(ghip);
   ASSERT(xdrs);
   return FALSE;
}


/* @brief Returns the icon of the Trash (aka Recycle Bin) folder.
 *
 * Gets the icon of the Trash (aka Recycle Bin) folder, and returns it
 * to the host.
 *
 * @param[in]  ghip Pointer to platform-specific GHI data.
 * @param[out] xdrs Pointer to XDR serialized data to send to the host.
 *
 * @retval TRUE  The icon was fetched successfully.
 * @retval FALSE The icon could not be fetched.
 */

Bool
GHIPlatformTrashFolderGetIcon(GHIPlatform *ghip,
                              XDR         *xdrs)
{
   ASSERT(ghip);
   ASSERT(xdrs);
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
                             const XDR   *xdrs)
{
   ASSERT(ghip);
   ASSERT(xdrs);
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
                            const XDR   *xdrs)
{
   ASSERT(ghip);
   ASSERT(xdrs);
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
                                const GHIGetExecInfoHashRequest *request,
                                GHIGetExecInfoHashReply *reply)
{
   ASSERT(ghip);
   ASSERT(request);
   ASSERT(reply);

   return FALSE;
}
