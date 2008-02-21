/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * toolboxOptions.c --
 *
 *     The options tab for the linux gtk toolbox
 */

#include "toolboxInt.h"
#include "vm_version.h"
#include "vm_app.h"
#include "guestApp.h"
#include "vmcheck.h"
#include "wiper.h"

/*
 *-----------------------------------------------------------------------------
 *
 * Options_Create  --
 *
 *      Create, layout, and init the Options tab UI and all its widgets.
 *
 * Results:
 *      The Options tab widget (it's a vbox).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget*
Options_Create(GtkWidget* mainWnd)
{
   GtkWidget* optionstab;
   GtkWidget* label;
   uint32 version, type = VMX_TYPE_UNSET;

   optionstab = gtk_vbox_new (FALSE, 10);
   gtk_widget_show(optionstab);
   gtk_container_set_border_width(GTK_CONTAINER(optionstab), 10);

   label = gtk_label_new("Miscellaneous Options");
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(optionstab), label, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

   /* Load the correct strings for the UI. */
   VmCheck_GetVersion(&version, &type);
   if (type == VMX_TYPE_SCALABLE_SERVER) {
      optionsTimeSync = gtk_check_button_new_with_label("Time synchronization between the virtual machine\nand the ESX Server.");
   } else {
      optionsTimeSync = gtk_check_button_new_with_label("Time synchronization between the virtual machine\nand the host operating system.");
   }

   gtk_widget_show(optionsTimeSync);
   gtk_box_pack_start(GTK_BOX(optionstab), optionsTimeSync, FALSE, FALSE, 0);
   gtk_label_set_justify(GTK_LABEL(GTK_BIN(GTK_BUTTON(optionsTimeSync))->child),
                         GTK_JUSTIFY_LEFT);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(optionsTimeSync),
                                (GuestApp_OldGetOptions() & VMWARE_GUI_SYNC_TIME));

   gtk_signal_connect(GTK_OBJECT(optionsTimeSync), "toggled",
                      GTK_SIGNAL_FUNC(Options_OnTimeSyncToggled), NULL);

   return optionstab;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Options_OnTimeSyncToggled --
 *
 *      Callback for the gtk signal "toggled" on the Options tab's timesync
 *      checkbox. Sends the new and old values thru the backdoor. The VMX
 *      should turn time syncing on or off. 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If time syncing is turned on the system time may be changed
 *
 *-----------------------------------------------------------------------------
 */

void
Options_OnTimeSyncToggled(gpointer btn,  // IN: timesync button
                          gpointer data) // IN: unused
{

   Bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));

   /* Send the opposite of the new value as the old value */
   GuestApp_SetOptionInVMX(TOOLSOPTION_SYNCTIME,
                           !enabled ? "1" : "0",
                           enabled ? "1" : "0");
}
