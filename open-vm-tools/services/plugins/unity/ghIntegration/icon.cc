/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * icon.c --
 *
 *	GHI/X11 icon collection code.
 */


#include <math.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gdesktopappinfo.h>

extern "C" {
#include "vmware.h"
#include "guest_msg_def.h"
}

#include "ghiX11icon.h"


/*
 * GHI/X11 still pumps icons over GuestMsg, which limits us to 64K.  Until we
 * switch transports, we'll have to scale down icons to fit within limits.
 */

static const size_t MAX_ICON_SIZE = GUESTMSG_MAX_IN_SIZE - 1024;


/*
 * Local function declarations.
 */

static void  AppendFileToArray(const gchar* iconPath,
                               std::list<GHIBinaryIconInfo>& iconList);
static void  AppendPixbufToArray(const GdkPixbuf* pixbuf,
                                 std::list<GHIBinaryIconInfo>& iconList,
                                 bool scaleHint = false);
static gint* GetIconSizesDescending(GtkIconTheme *iconTheme,
                                    const gchar* iconName);
static Bool  GetIconsForGIcon(GIcon* gicon,
                              std::list<GHIBinaryIconInfo>& iconList);
static GdkPixbuf* ShrinkPixbuf(const GdkPixbuf* pixbuf, size_t maxSize);


/*
 *-----------------------------------------------------------------------------
 *
 * GHIX11IconGetIconsForDesktopFile --
 *
 *      Given an application's .desktop file, look up and return the app's icons
 *      as BGRA data.  Icons are sorted in descending order by size.
 *
 * Results:
 *      Returns FALSE if any errors were encountered and TRUE otherwise.
 *
 * Side effects:
 *      iconList.size() may grow.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GHIX11IconGetIconsForDesktopFile(const char* desktopFile,                // IN
                                 std::list<GHIBinaryIconInfo>& iconList) // OUT
{
   GDesktopAppInfo* desktopAppInfo = NULL;
   GIcon* gicon = NULL;
   Bool success = FALSE;

   desktopAppInfo = g_desktop_app_info_new_from_filename(desktopFile);
   if (desktopAppInfo) {
      GAppInfo* appInfo = (GAppInfo*)G_APP_INFO(desktopAppInfo);
      gicon = g_app_info_get_icon(appInfo);
      if (gicon) {
         success = GetIconsForGIcon(gicon, iconList);
      }
      g_object_unref(desktopAppInfo);
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GHIX11IconGetIconsByName --
 *
 *      Try to find icons identified by a string.  The string may refer to a
 *      generic name leading to searching an icon theme, or it may be an
 *      absolute path to an icon file.
 *
 * Results:
 *      Returns FALSE if any errors were encountered and TRUE otherwise.
 *
 * Side effects:
 *      iconList.size() may grow.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GHIX11IconGetIconsByName(const char* iconName,                          // IN
                         std::list<GHIBinaryIconInfo>& iconList)        // OUT
{
   GIcon *gicon;
   Bool retval = FALSE;

   gicon = g_icon_new_for_string(iconName, NULL);
   if (gicon) {
      retval = GetIconsForGIcon(gicon, iconList);
      g_object_unref(G_OBJECT(gicon));
   }

   return retval;
}


/*
 * Local functions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * GetIconsForGIcon --
 *
 *      Given a GLib GIcon, search the default icon theme or filesystem for
 *      icons.
 *
 * Results:
 *      Returns FALSE if any errors were encountered and TRUE otherwise.
 *
 * Side effects:
 *      iconList.size() may grow.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetIconsForGIcon(GIcon* gicon,                                  // IN
                 std::list<GHIBinaryIconInfo>& iconList)        // OUT
{
   gchar* iconName = NULL;
   GtkIconTheme* iconTheme;
   Bool success = FALSE;

   ASSERT(gicon);

   /*
    * We can handle two icon types, themed and file.  A themed icon is
    * provided by and varies by icon theme whereas a file icon is stored
    * in a single file.  (There's a special case where GIO thinks we have
    * a themed icon, but it turns out to be a file icon.  More on that later.)
    */

   iconName = g_icon_to_string(gicon);
   iconTheme = gtk_icon_theme_get_default();

   if (G_IS_THEMED_ICON(gicon) && gtk_icon_theme_has_icon(iconTheme, iconName)) {

      /*
       * Sweet - GTK claims that our icon is themed and can give us pixbufs for
       * it at various sizes.
       *
       * Remember what I said about GIO thinking we have a themed icon that might
       * not be?  If an icon doesn't have a size, then it's one of those such
       * icons, and we have to fall back to loading directly from a file ourselves.
       */

      gint* iconSizes = GetIconSizesDescending(iconTheme, iconName);

      if (iconSizes && *iconSizes != 0) {
         gint* sizeIter;

         for (sizeIter = iconSizes; *sizeIter; sizeIter++) {
            GdkPixbuf* pixbuf;

            pixbuf = gtk_icon_theme_load_icon(iconTheme, iconName, *sizeIter,
                                              (GtkIconLookupFlags)0, NULL);
            if (pixbuf) {
               AppendPixbufToArray(pixbuf, iconList);
               g_object_unref(pixbuf);
            }
         }
      } else if (iconSizes && *iconSizes == 0) {
         GtkIconInfo* iconInfo;

         iconInfo = gtk_icon_theme_lookup_icon(iconTheme, iconName, 0,
                                               (GtkIconLookupFlags)0);
         if (iconInfo) {
            AppendFileToArray(gtk_icon_info_get_filename(iconInfo), iconList);
            gtk_icon_info_free(iconInfo);
         }
      }

      g_free(iconSizes);
   } else if (G_IS_FILE_ICON(gicon)) {
      GFileIcon* fileIcon;
      GFile* file;
      char* path;

      fileIcon = G_FILE_ICON(gicon);
      file = g_file_icon_get_file(fileIcon);
      path = g_file_get_path(file);
      ASSERT(path);

      AppendFileToArray(path, iconList);

      g_free(path);
   } else {
      /* Give up. */
      goto out;
   }

   success = TRUE;

out:
   if (iconName) {
      g_free(iconName);
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AppendFileToArray --
 *
 *      Load an icon from a file into a pixbuf, then append it to a GPtrArray of
 *      GHIX11Icons.
 *
 * Results:
 *      Appends an icon on success, no-op on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
AppendFileToArray(const gchar* iconPath,                  // IN
                  std::list<GHIBinaryIconInfo>& iconList) // OUT
{
   GdkPixbuf* pixbuf;

   if ((pixbuf = gdk_pixbuf_new_from_file(iconPath, NULL)) != NULL) {
      AppendPixbufToArray(pixbuf, iconList, true);
      g_object_unref(pixbuf);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * AppendPixbufToArray --
 *
 *      Appends a pixbuf to a GPtrArray of GHIX11Icons.
 *
 * Results:
 *      Appends an icon on success, no-op on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
AppendPixbufToArray(
   const GdkPixbuf* pixbuf,                // IN
   std::list<GHIBinaryIconInfo>& iconList, // OUT
   bool scaleHint)                         // IN: Scale icon to fit GuestMsg
{
   GHIBinaryIconInfo ghiIcon;
   guchar* pixels;
   guint width;
   guint height;
   guint x, y;
   guint rowstride;
   guint n_channels;
   guint bgraStride;
   bool scaled = false;
   GdkPixbuf *scaledPixbuf = NULL;

rescaled:
   ASSERT(pixbuf);
   ASSERT(gdk_pixbuf_get_colorspace(pixbuf) == GDK_COLORSPACE_RGB);
   ASSERT(gdk_pixbuf_get_bits_per_sample(pixbuf) == 8);

   rowstride = gdk_pixbuf_get_rowstride(pixbuf);
   n_channels = gdk_pixbuf_get_n_channels(pixbuf);
   pixels = gdk_pixbuf_get_pixels(pixbuf);

   ghiIcon.width = width = gdk_pixbuf_get_width(pixbuf);
   ghiIcon.height = height = gdk_pixbuf_get_height(pixbuf);
   bgraStride = width * 4;
   ghiIcon.dataBGRA.resize(height * bgraStride);

   if (   !scaled
       && scaleHint
       && height * bgraStride > MAX_ICON_SIZE) {
      scaled = true;
      pixbuf = scaledPixbuf = ShrinkPixbuf(pixbuf, MAX_ICON_SIZE);
      goto rescaled;
   }

   /* GetBinaryInfo icons are bottom-to-top. */
   for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
         guchar* b; // Pointer to BGRA data in ghiIcon.
         guchar* p; // Pointer to RGBA data in GdkPixbuf
         gint bgraOffset = y * bgraStride + x * 4;
         gint pixbufOffset = (height - y - 1) * rowstride + x * n_channels;

         b = &ghiIcon.dataBGRA[bgraOffset];
         p = &pixels[pixbufOffset];

         b[0] = p[2];
         b[1] = p[1];
         b[2] = p[0];
         b[3] = (n_channels > 3) ? p[3] : 0xFF;
      }
   }

   iconList.push_back(ghiIcon);

   if (scaledPixbuf) {
      g_object_unref(G_OBJECT(scaledPixbuf));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetIconSizesDescending --
 *
 *      Query an icon theme for an icon's sizes.  Return the sizes as a gint
 *      array in descending order.
 *
 * Results:
 *      Pointer to a gint array on success or NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
DescendingIntCmp(const void* a,
                 const void* b)
{
   gint ia = *(gint* )a;
   gint ib = *(gint* )b;

   return (ia < ib) ? 1 : (ia == ib) ? 0 : -1;
}

static gint*
GetIconSizesDescending(GtkIconTheme* iconTheme, // IN
                       const gchar* iconName)   // IN
{
   gint* iconSizes = NULL;
   gint* iter;
   size_t nsizes;

   if (!gtk_icon_theme_has_icon(iconTheme, iconName)) {
      return NULL;
   }

   iconSizes = gtk_icon_theme_get_icon_sizes(iconTheme, iconName);
   ASSERT(iconSizes);

   /*
    * iconSizes is a 0-terminated array. If there are no sizes or the
    * array has only 1 element, this function has no remaining work to do.
    * (No point sorting a 1 element array.)
    */
   if (!iconSizes || *iconSizes == 0) {
      return iconSizes;
   }

   /*
    * Sort the array in descending order.  First we have to determine its
    * size, then we'll just pass it off to qsort.  Note that we don't consider
    * the final, terminating element when sorting, because the icon array may
    * contain a -1 to signify a scalable icon.
    */
   for (iter = iconSizes, nsizes = 0; *iter; iter++, nsizes++);
   qsort(iconSizes, nsizes, sizeof *iter, DescendingIntCmp);

   return iconSizes;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShrinkPixbuf --
 *
 *      Scale a pixbuf to fit within transport size constraints.
 *
 * Results:
 *      Pointer to new pixbuf.  Caller should free with g_object_unref.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GdkPixbuf*
ShrinkPixbuf(const GdkPixbuf* pixbuf, // IN
             size_t maxSize)          // IN: Must scale to less than this.
{
   GdkPixbuf *newIcon;
   volatile double newWidth;
   volatile double newHeight;
   volatile double scaleFactor;

   newWidth = gdk_pixbuf_get_width(pixbuf);
   newHeight = gdk_pixbuf_get_height(pixbuf);
   scaleFactor = maxSize / (newWidth * newHeight * 4.0);
   /*
    * Ensures that we remove at least a little bit of data from the icon.
    * Otherwise we can get things like scalefactors of '0.999385' which result
    * in an image of exactly the same size. A scaleFactor of 0.95 will remove at
    * least one row or column from any icon large enough to go past the limit.
    */
   scaleFactor = MIN(scaleFactor, 0.95);

   newWidth *= scaleFactor;
   newHeight *= scaleFactor;
   newIcon = gdk_pixbuf_scale_simple(pixbuf,
                                     (int)ceil(newWidth),
                                     (int)ceil(newHeight),
                                     GDK_INTERP_HYPER);
   return newIcon;
}
