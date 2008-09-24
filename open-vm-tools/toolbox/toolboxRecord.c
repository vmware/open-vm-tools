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
 * toolboxRecord.c --
 *
 *     The record tab for the linux gtk toolbox.
 */

#include <unistd.h>

#include "toolboxGtkInt.h"
#include "debug.h"
#include "statelogger_backdoor_def.h"

static void RecordOnStart(gpointer, gpointer);
static void RecordOnStop(gpointer, gpointer);

/*
 *-----------------------------------------------------------------------------
 *
 * Record_Create  --
 *
 *      Create, layout, and init the Record tab UI and all its widgets.
 *
 * Results:
 *      The Record tab widget (it's a vbox).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget *
Record_Create(GtkWidget* mainWnd)
{
   GtkWidget *recordtab;
   GtkWidget *hbox[2];
   GtkWidget *startbtn;
   GtkWidget *stopbtn;
   GtkWidget *label;
   recordtab = gtk_vbox_new(FALSE, 50);
   gtk_widget_show(recordtab);
   gtk_container_set_border_width(GTK_CONTAINER(recordtab), 10);
   hbox[0] = gtk_hbox_new(FALSE, 10);
   hbox[1] = gtk_hbox_new(FALSE, 10);
   label = 
      gtk_label_new("Press start or stop button to control recording.");
   gtk_widget_show(hbox[0]);
   gtk_widget_show(hbox[1]);
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(recordtab), hbox[0], FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox[0]), label, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(recordtab), hbox[1], FALSE, FALSE, 0);
#ifdef GTK2
   startbtn = gtk_button_new_with_mnemonic("_Start");
   stopbtn = gtk_button_new_with_mnemonic("S_top");
#else
   startbtn = gtk_button_new_with_label("Start");
   stopbtn = gtk_button_new_with_label("Stop");
#endif
   gtk_widget_show(startbtn);
   gtk_box_pack_start(GTK_BOX(hbox[1]), startbtn, FALSE, FALSE, 10);
   gtk_widget_set_usize(startbtn, 70, 25);
   gtk_signal_connect(GTK_OBJECT(startbtn), "clicked",
                      GTK_SIGNAL_FUNC(RecordOnStart), NULL);
   gtk_widget_show(stopbtn);
   gtk_box_pack_end(GTK_BOX(hbox[1]), stopbtn, FALSE, FALSE, 10);
   gtk_widget_set_usize(stopbtn, 70, 6);
   gtk_signal_connect(GTK_OBJECT(stopbtn), "clicked",
                      GTK_SIGNAL_FUNC(RecordOnStop), NULL);
   
   return recordtab;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RecordOnStart --
 *
 *      Callback for the gtk signal "clicked" on the Record tab's start
 *      button. 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Host VMware product starts recording this vm.
 *
 *-----------------------------------------------------------------------------
 */

static void
RecordOnStart(gpointer btn,    // IN: unused
              gpointer data)   // IN: unused
{
   if (GuestApp_ControlRecord(STATELOGGER_BKDR_START_LOGGING) == FALSE) {
      ToolsMain_MsgBox(NULL, RECORD_VMX_ERR);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RecordOnStop --
 *
 *      Callback for the gtk signal "clicked" on the Record tab's stop
 *      button. 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Host VMware product stops recording this vm.
 *
 *-----------------------------------------------------------------------------
 */

static void
RecordOnStop(gpointer btn,    // IN: unused
             gpointer data)   // IN: unused
{
   if (GuestApp_ControlRecord(STATELOGGER_BKDR_STOP_LOGGING) == FALSE) {
      ToolsMain_MsgBox(NULL, RECORD_VMX_ERR);
   }
}

