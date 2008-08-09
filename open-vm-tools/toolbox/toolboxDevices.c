/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * toolboxDevices.c --
 *
 *     The devices tab for the linux gtk toolbox
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "toolboxGtkInt.h"
#include "removable_device.h"
#include "guestApp.h"
#include "eventManager.h"

/*
 * Globals
 */
static GtkWidget *deviceLabel;
static GtkWidget *deviceScrollwin;

/*
 *-----------------------------------------------------------------------------
 *
 * Devices_DevicesLoop  --
 *
 *      Event Manager function for tracking the list of removable device
 *      and their connected/dissconnected state from the vmx. This function
 *      polls the backdoor for the current state of removable devices and 
 *      updates the widgets in the Devices tab accordingly. It temporarily
 *      blocks signal handlers because those handlers are meant for user
 *      interaction not for reacting to progammatic toggling of the buttons.
 *
 * Results:
 *      TRUE.
 *
 * Side effects:
 *      The Devices tab UI will change.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
Devices_UpdateLoop(void* clientData)
{
   int i;
   Bool atLeastOne = FALSE;
   GtkWidget **btnArray = (GtkWidget **)clientData;

   for (i=0; i<MAX_DEVICES; i++) {
      RD_Info info;
      if (GuestApp_GetDeviceInfo(i, &info) && strlen(info.name) > 0) {
         gtk_widget_show(btnArray[i]);
         gtk_label_set_text(GTK_LABEL(GTK_BIN(btnArray[i])->child), 
                            info.name);
         gtk_signal_handler_block_by_func(GTK_OBJECT(btnArray[i]),
                                          GTK_SIGNAL_FUNC(Devices_OnDeviceToggled),
                                          GINT_TO_POINTER(i));
         gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btnArray[i]), info.enabled);
         gtk_signal_handler_unblock_by_func(GTK_OBJECT(btnArray[i]),
                                            GTK_SIGNAL_FUNC(Devices_OnDeviceToggled),
                                            GINT_TO_POINTER(i));
         atLeastOne = TRUE;
      }
      else {
         gtk_widget_hide(btnArray[i]);
      }
   }

   if (atLeastOne) {
      gtk_widget_show(deviceScrollwin);
   } else {
      gtk_label_set_text(GTK_LABEL(deviceLabel), 
                         "No removable devices are available. Either this\nvirtual machine has no removable devices or its\nconfiguration does not allow you to connect and\ndisconnect them.");
      gtk_widget_hide(deviceScrollwin);
   }

   EventManager_Add(gEventQueue, DEVICES_POLL_TIME, Devices_UpdateLoop,
                    clientData);   
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Devices_Create  --
 *
 *      Create, layout, and init the Devices tab UI and all its widgets.
 *
 * Results:
 *      The Devices tab widget (it's a vbox).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget*
Devices_Create(GtkWidget* mainWnd)
{
   GtkWidget* devicestab;
   GtkWidget* vbox;
   GtkWidget **deviceBtns;

   int i;

   /*
    * We don't free this, but it's not really a leak because Devices_Create should
    * only be called once per app invocation
    */
   deviceBtns = calloc(MAX_DEVICES, sizeof (GtkWidget *));

   devicestab = gtk_vbox_new (FALSE, 10);
   gtk_widget_show(devicestab);
   gtk_container_set_border_width(GTK_CONTAINER(devicestab), 10);

   deviceLabel = gtk_label_new("Check a device to connect it to the virtual machine");
   gtk_widget_show(deviceLabel);
   gtk_box_pack_start(GTK_BOX(devicestab), deviceLabel, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(deviceLabel), GTK_JUSTIFY_LEFT);
   gtk_misc_set_alignment(GTK_MISC(deviceLabel), 0, 0);

   deviceScrollwin = gtk_scrolled_window_new(NULL, NULL);
   gtk_widget_show(deviceScrollwin);
   gtk_box_pack_start(GTK_BOX(devicestab), deviceScrollwin, TRUE, TRUE, 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(deviceScrollwin), 
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

   vbox = gtk_vbox_new(FALSE,0);
   gtk_widget_show(vbox);
   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(deviceScrollwin), 
                                         vbox);

   for (i=0; i<MAX_DEVICES; i++) {
      deviceBtns[i] = gtk_check_button_new_with_label("none");         
      gtk_box_pack_start(GTK_BOX(vbox), deviceBtns[i], FALSE, FALSE, 0);
      gtk_label_set_justify(GTK_LABEL(GTK_BIN(GTK_BUTTON(deviceBtns[i]))->child),
                            GTK_JUSTIFY_LEFT);
      gtk_signal_connect(GTK_OBJECT(deviceBtns[i]), "toggled",
                         GTK_SIGNAL_FUNC(Devices_OnDeviceToggled), GINT_TO_POINTER(i));
   }

   if (deviceBtns[0] != NULL) {
      Devices_UpdateLoop((void *)deviceBtns);
   }

   return devicestab;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Devices_OnDeviceToggled --
 *
 *      Callback for the gtk signal "toggled" on Devices tab checkboxes.
 *      Sends the new state thru the backdoor, causing the vmx to connect
 *      or disconnect the device
 * Results:
 *      None.
 *
 * Side effects:
 *      The vmx should connect/disconnect the removable device. There will
 *      be a lot of side effects, depending on the device.
 *
 *-----------------------------------------------------------------------------
 */

void
Devices_OnDeviceToggled(gpointer btn,  // IN: button which was toggled
                        gpointer data) // IN: device id
{
   int dev_id;
   Bool enabled;
   RD_Info info;

   dev_id = GPOINTER_TO_INT(data);
   enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));

   if (GuestApp_GetDeviceInfo(dev_id, &info)) {
      if (info.enabled != enabled) {
         char msg[64];
         snprintf(msg, sizeof(msg), "Unable to %s device. Do you want to retry?\n",
                  enabled ? "connect" : "disconnect");
         while (!GuestApp_SetDeviceState(dev_id, enabled) &&
                ToolsMain_YesNoBox("Error", msg))
            ;
      }
   } else {
      ToolsMain_MsgBox("Error", "Unable to get device info");
   }

   return;
}
