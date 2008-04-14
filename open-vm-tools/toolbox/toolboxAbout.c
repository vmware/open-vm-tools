/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * toolboxAbout.c --
 *
 *     The about tab for the linux gtk toolbox.
 */



#include <string.h>

#include "vm_version.h"
#include "vm_legal.h"
#include "str.h"

#include "bigIcon.xpm"
#include "toolboxInt.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * About_Create  --
 *
 *      Create, layout, and init the About tab UI and all its widgets.
 *
 * Results:
 *      The About tab widget (it's a vbox).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget*
About_Create(GtkWidget* mainWnd)
{
   GtkWidget *abouttab;
   GtkWidget *icon;
   GtkWidget *vbox;
   GtkWidget *label;
   GtkWidget *scrollwin;
   GtkWidget *viewport;
   GtkWidget *ebox;
   gchar buf1[sizeof (UTF8_COPYRIGHT_STRING) + sizeof (RIGHT_RESERVED) + 2];
   GdkPixmap* pix       = NULL;
   GdkBitmap* bit       = NULL;
   GdkColormap* colormap = NULL;    
   GtkStyle *style;
   GdkColor color;

   abouttab = gtk_hbox_new(FALSE, 10);
   gtk_widget_show(abouttab);
   gtk_container_set_border_width(GTK_CONTAINER(abouttab), 10);

   /* Create the icon from a pixmap */
   colormap = gtk_widget_get_colormap(abouttab);
   pix = gdk_pixmap_colormap_create_from_xpm_d(NULL, colormap, &bit, NULL, bigIcon_xpm);
   icon = gtk_pixmap_new(pix,bit);
   gdk_pixmap_unref(pix);
   gdk_bitmap_unref(bit);
   gtk_widget_show(icon);
   gtk_box_pack_start(GTK_BOX(abouttab), icon, FALSE, FALSE, 0);
   gtk_misc_set_alignment(GTK_MISC(icon), 0, 0);

   scrollwin = gtk_scrolled_window_new(NULL, NULL);
   gtk_widget_show(scrollwin);
   gtk_box_pack_start(GTK_BOX(abouttab), scrollwin, TRUE, TRUE, 0);
   gtk_container_set_border_width(GTK_CONTAINER(scrollwin), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), 
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
   viewport = 
      gtk_viewport_new(
         gtk_scrolled_window_get_hadjustment(
            GTK_SCROLLED_WINDOW(scrollwin)),
         gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(scrollwin)));
   gtk_widget_show(viewport);
   gtk_container_add(GTK_CONTAINER(scrollwin), viewport);
   gtk_signal_connect(GTK_OBJECT(viewport), "size_request",
                      GTK_SIGNAL_FUNC(OnViewportSizeRequest), 0);
   gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_IN);
   gtk_container_set_border_width(GTK_CONTAINER(viewport), 0);

   ebox = gtk_event_box_new();
   gtk_widget_show(ebox);
   gtk_container_add(GTK_CONTAINER(viewport), ebox);
   gtk_container_set_border_width(GTK_CONTAINER(ebox), 0);

   gdk_color_parse("#FFFFFF", &color);
   style = gtk_style_new();
   style->bg[GTK_STATE_NORMAL] = color;
   gtk_widget_set_style(ebox, style);
   gtk_style_unref(style);

   vbox = gtk_vbox_new(FALSE, 10);
   gtk_widget_show(vbox);
   gtk_container_add(GTK_CONTAINER(ebox), vbox);
   gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

   label = gtk_label_new(PRODUCT_NAME_PLATFORM);
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

   label = gtk_label_new("Version " TOOLS_VERSION ", " BUILD_NUMBER);
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

#ifdef _GTK2_
   Str_Snprintf(buf1, sizeof buf1, "%s %s",
                UTF8_COPYRIGHT_STRING,
                RIGHT_RESERVED);
#else
   /*
    * Redhat 8.0/Mandrake 9's locales seem to be unable to transcode
    * the copyright symbol.  So we just replace it with "(c)" if we
    * detect the failure.
    *
    * See bug# 25055.
    */
   {
      GdkWChar spambuffer[2];
      gchar *copyright = NULL;

      if(gdk_mbstowcs(spambuffer, "\251\0", 2) <= 0) {
         gchar** split;
         split = g_strsplit(COPYRIGHT_STRING, "\251", 2);
         copyright = g_strconcat(split[0], "(c)", split[1], NULL);
         g_strfreev(split);
      }
      Str_Snprintf(buf1, sizeof buf1, "%s %s", copyright ? copyright : COPYRIGHT_STRING,
                   RIGHT_RESERVED);
      g_free(copyright);
   }
#endif
   
   label = gtk_label_new(buf1);
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

   label = gtk_label_new(PATENTS_STRING);
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

   label = gtk_label_new(TRADEMARK_STRING);
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

   label = gtk_label_new(GENERIC_TRADEMARK_STRING);
   gtk_widget_show(label); 
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

   return abouttab;
}
