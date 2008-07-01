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
 * toolboxInt.h --
 *
 *     Common defines used by the gtk toolbox
 */
#ifndef _TOOLBOX_INT_H_
# define _TOOLBOX_INT_H_

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#undef Bool
#include "vm_basic_types.h"
#include "vm_version.h"
#include "dbllnklst.h"


#define RPCIN_POLL_TIME        10  /* in 1/1000ths of a second */
#define DEVICES_POLL_TIME     100  /* in 1/1000ths of a second */
#define WIPER_POLL_TIME        10  /* in 1/1000ths of a second */
#define POST_RESET_TIME       100  /* in 1/1000ths of a second */

#define SCRIPT_SUSPEND "Suspend Guest Operating System"
#define SCRIPT_RESUME  "Resume Guest Operating System"
#define SCRIPT_OFF     "Shut Down Guest Operating System"
#define SCRIPT_ON      "Power On Guest Operating System"

#define MAX_DEVICES 50  /* maximum number of devices we'll show */

#if GTK2
#define TAB_LABEL_OPTIONS "_Options"
#define TAB_LABEL_DEVICES "De_vices"
#define TAB_LABEL_SCRIPTS "Scrip_ts"
#define TAB_LABEL_SHRINK "Shrin_k"
#define TAB_LABEL_ABOUT "A_bout"
#else
#define TAB_LABEL_OPTIONS "Options"
#define TAB_LABEL_DEVICES "Devices"
#define TAB_LABEL_SCRIPTS "Scripts"
#define TAB_LABEL_SHRINK "Shrink"
#define TAB_LABEL_ABOUT "About"
#endif

void OnViewportSizeRequest(GtkWidget *widget, GtkRequisition *requisition,
                           gpointer user_data);

Bool ToolsMain_YesNoBox(gchar* title, gchar *msg);
void ToolsMain_MsgBox(gchar* title, gchar *msg);
void ToolsMain_OnDestroy(GtkWidget *widget, gpointer data);

GtkWidget* About_Create(GtkWidget* mainWnd);
GtkWidget* Devices_Create(GtkWidget* mainWnd);
GtkWidget* Options_Create(GtkWidget* mainWnd);
GtkWidget* Scripts_Create(GtkWidget* mainWnd);
GtkWidget* Shrink_Create(GtkWidget* mainWnd);

void Options_OnTimeSyncToggled(gpointer btn, gpointer data);
void Devices_OnDeviceToggled(gpointer btn, gpointer data);
void Pointer_SetXCursorPos(int x, int y);
void Scripts_OnApply(gpointer btn, gpointer data);

extern GdkPixmap* pixmap;
extern GdkBitmap* bitmask;
extern GdkColormap* colormap;
extern GtkWidget *optionsTimeSync;
extern DblLnkLst_Links *gEventQueue;
extern GtkWidget *scriptsApply;

#endif // _TOOLBOX_INT_H_
