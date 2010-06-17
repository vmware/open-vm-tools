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
 * toolboxOptions.c --
 *
 *     The options tab for the linux gtk toolbox
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "toolboxGtkInt.h"
#include "debug.h"
#include "guestApp.h"
#include "rpcout.h"
#include "wiper.h"
#include "vmware/guestrpc/tclodefs.h"

/*
 * Globals
 */
static GtkWidget *shrinkList;
static GtkWidget *shrinkWipeDlg;
static GtkWidget *shrinkWipeProgress;
static Wiper_State *wiper = NULL;

void Shrink_OnShrinkClicked(GtkButton *btn, gpointer user_data);
Bool Shrink_DoWipe(WiperPartition *part, GtkWidget* mainWnd);
void  Shrink_OnWipeDestroy(GtkWidget *widget, gpointer user_data);


/*
 *-----------------------------------------------------------------------------
 *
 * Shrink_Create  --
 *
 *      Create, layout, and init the Shrink tab UI and all its widgets.
 *
 * Results:
 *      The Shrink tab widget (it's a vbox).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget*
Shrink_Create(GtkWidget *mainWnd)
{
   GtkWidget *shrinktab;
   GtkWidget *label;
   GtkWidget *scrollwin;
   GtkWidget *hbox;
   GtkWidget *button;
   GtkWidget *viewport;
   GtkWidget *ebox;

   WiperPartition_List plist;
   gchar *items;
   int newrow;

   shrinktab = gtk_vbox_new(FALSE, 10);
   gtk_widget_show(shrinktab);
   gtk_container_set_border_width(GTK_CONTAINER(shrinktab), 10);

   /* Only root can do shrink. */
   if (geteuid() != 0) {
      Debug("User not allowed to do shrink");
      label =
         gtk_label_new("This option is enabled only if you run VMware Tools as root.");
      gtk_widget_show(label);
      gtk_box_pack_start(GTK_BOX(shrinktab), label, FALSE, FALSE, 0);
   } else {
      label = gtk_label_new("Select the partitions you wish to shrink.");
      gtk_widget_show(label);
      gtk_box_pack_start(GTK_BOX(shrinktab), label, FALSE, FALSE, 0);
      gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
      gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

      scrollwin = gtk_scrolled_window_new(NULL, NULL);
      gtk_widget_show(scrollwin);
      gtk_box_pack_start(GTK_BOX(shrinktab), scrollwin, TRUE, TRUE, 0);
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

      {
         GtkStyle *style;
         GdkColor color;
         gdk_color_parse("#FFFFFF", &color);
         style = gtk_style_new();
         style->bg[GTK_STATE_NORMAL] = color;
         gtk_widget_set_style(ebox, style);
         gtk_style_unref(style);
      }

      hbox = gtk_hbox_new(FALSE, 0);
      gtk_widget_show(hbox);
      gtk_box_pack_end(GTK_BOX(shrinktab), hbox, FALSE, FALSE, 0);

#ifdef GTK2
      button = gtk_button_new_with_mnemonic("_Shrink");
#else
      button = gtk_button_new_with_label("Shrink");
#endif
      gtk_widget_show(button);
      gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
      gtk_widget_set_sensitive(button, FALSE);
      gtk_signal_connect(GTK_OBJECT(button), "clicked",
                         GTK_SIGNAL_FUNC(Shrink_OnShrinkClicked), mainWnd);

      if (GuestApp_IsDiskShrinkCapable()) {
         if (GuestApp_IsDiskShrinkEnabled()) {
            gtk_widget_set_sensitive(button, TRUE);
            shrinkList = gtk_clist_new(1);
            gtk_widget_show(shrinkList);
            gtk_container_add(GTK_CONTAINER(ebox), shrinkList);
            gtk_container_set_border_width(GTK_CONTAINER(shrinkList), 0);
            gtk_clist_set_selection_mode(GTK_CLIST(shrinkList), GTK_SELECTION_MULTIPLE);

            Wiper_Init(NULL);
            if (WiperPartition_Open(&plist)) {
               DblLnkLst_Links *curr, *next;

               DblLnkLst_ForEachSafe(curr, next, &plist.link) {
                  WiperPartition *part = DblLnkLst_Container(curr, WiperPartition, link);

                  if (part->type != PARTITION_UNSUPPORTED) {
                     /*
                      * Detach the element we are interested in so it is not
                      * destroyed when we call WiperPartition_Close.
                      */
                     DblLnkLst_Unlink1(&part->link);
                     items  = part->mountPoint;
                     newrow = gtk_clist_append(GTK_CLIST(shrinkList), &items);
                     gtk_clist_set_row_data_full(GTK_CLIST(shrinkList), newrow,
                                 part, (GDestroyNotify)WiperSinglePartition_Close);
                  }
               }
               WiperPartition_Close(&plist);
            }
         } else {
            label = gtk_label_new(SHRINK_DISABLED_ERR);
            gtk_widget_show(label);
            gtk_container_add(GTK_CONTAINER(ebox), label);
            gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
            gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
            gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
         }
      } else {
         label = gtk_label_new(SHRINK_FEATURE_ERR);
         gtk_widget_show(label);
         gtk_container_add(GTK_CONTAINER(ebox), label);
         gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
         gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
         gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
      }
   }

   return shrinktab;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Shrink_OnShrinkClicked  --
 *
 *      Callback for the gtk signal "clicked" on the Shrink tab's shrink button
 *      Cycle thru all the selected partitions wipe them (Shrink_DoWipe).
 *      After wiping all selected partitions, tell the vmx to shrink disks.
 *      If the user clicks cancel (Shrink_DoWipe returns FALSE), cancel the
 *      entire operation and don't send a mesage to the vmx.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The selected partitons will be wiped and shrunk.
 *
 *-----------------------------------------------------------------------------
 */

void
Shrink_OnShrinkClicked(GtkButton *btn,     // IN: unused
                       gpointer user_data) // IN: unused
{
   int rnum;
   GList *slist;
   WiperPartition *part;
   int disks_to_shrink = 0;
   GtkWidget *mainWnd = GTK_WIDGET(user_data);
   slist = GTK_CLIST(shrinkList)->selection;
   if (slist) {
      if (!ToolsMain_YesNoBox("Shrink Disk",
                                "Do you want to prepare the disk(s) for shrinking?\n")) {
         return;
      }
      do {
         rnum =  GPOINTER_TO_UINT(slist->data);
         part = gtk_clist_get_row_data(GTK_CLIST(shrinkList), rnum);
         if (Shrink_DoWipe(part, mainWnd)) {
            disks_to_shrink++;
            slist = slist->next;
         } else {
            disks_to_shrink = 0;
            break;
         }
      } while (slist);

      if (disks_to_shrink > 0) {
         if (ToolsMain_YesNoBox("Shrink Disk",
                                "Do you want to shrink the disk(s)?\n")) {
            if (RpcOut_sendOne(NULL, NULL, DISK_SHRINK_CMD)) {
               ToolsMain_MsgBox("Information", "The shrink process has finished.");
            }
            gtk_clist_unselect_all(GTK_CLIST(shrinkList));
         }
      }
   } else {
      ToolsMain_MsgBox("Information", "Please select a partition\n");
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Shrink_DoWipe  --
 *
 *      Wipe a single partition, displaying a modal dialog with a progress
 *      bar. This function works similar to Win32's DoModal in that it blocks
 *      the caller and pumps its own messages, returning only when the wiper
 *      operation is done or canceled.
 *
 * Results:
 *      TRUE if the wipe operation completes successfully, FALSE on error or
 *      user cancel.
 *
 * Side effects:
 *      The wipe operation will fill the partition with dummy files.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Shrink_DoWipe(WiperPartition *part, GtkWidget* mainWnd) // IN: partition to be wiped
{
   Bool performShrink = FALSE;
   GtkWidget *btn;
   int progress = 0;
   unsigned char *err;

   /*
    * Verify that shrinking is still possible before going through with the
    * wiping. This obviously isn't atomic, but it should take care of
    * the case where the user takes a snapshot with the toolbox open.
    */
   if (GuestApp_IsDiskShrinkEnabled()) {
         performShrink = TRUE;
   }

   if (!performShrink) {
      ToolsMain_MsgBox("Error", SHRINK_CONFLICT_ERR);
      return FALSE;
   }

   shrinkWipeDlg = gtk_dialog_new();
   gtk_window_set_title(GTK_WINDOW(shrinkWipeDlg), "Please Wait...");
   gtk_window_set_transient_for(GTK_WINDOW(shrinkWipeDlg), GTK_WINDOW(mainWnd));
   gtk_window_set_position(GTK_WINDOW(shrinkWipeDlg), GTK_WIN_POS_CENTER);
   gtk_widget_show(shrinkWipeDlg);
   gtk_window_set_modal(GTK_WINDOW(shrinkWipeDlg), TRUE);
   gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(shrinkWipeDlg)->vbox),
                                  10);
   gdk_window_set_icon_list(shrinkWipeDlg->window, gIconList);
   gtk_signal_connect(GTK_OBJECT(shrinkWipeDlg), "destroy",
                      GTK_SIGNAL_FUNC(Shrink_OnWipeDestroy), shrinkWipeDlg);

   shrinkWipeProgress = gtk_progress_bar_new();
   gtk_widget_show(shrinkWipeProgress);
   gtk_progress_set_show_text(GTK_PROGRESS(shrinkWipeProgress), TRUE);
   gtk_progress_set_format_string(GTK_PROGRESS(shrinkWipeProgress),
                                  "Preparing to shrink... (%p%%)");
   gtk_progress_set_text_alignment(GTK_PROGRESS(shrinkWipeProgress), 0, 0.5);
   gtk_progress_set_activity_mode(GTK_PROGRESS(shrinkWipeProgress), FALSE);
   gtk_progress_bar_set_bar_style(GTK_PROGRESS_BAR(shrinkWipeProgress),
                                  GTK_PROGRESS_CONTINUOUS);
   gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(shrinkWipeProgress),
                                    GTK_PROGRESS_LEFT_TO_RIGHT);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(shrinkWipeDlg)->vbox),
                      shrinkWipeProgress, FALSE, FALSE, 0);

#ifdef GTK2
   btn = gtk_button_new_with_mnemonic("_Cancel");
#else
   btn = gtk_button_new_with_label("Cancel");
#endif
   gtk_widget_show(btn);
   gtk_box_pack_end(GTK_BOX(GTK_DIALOG(shrinkWipeDlg)->action_area), btn,
                    FALSE, FALSE, 0);
   gtk_widget_set_usize(btn, 70, 25);
   gtk_signal_connect_object(GTK_OBJECT(btn), "clicked",
                             GTK_SIGNAL_FUNC(gtk_widget_destroy),
                             GTK_OBJECT(shrinkWipeDlg));

   gtk_widget_show_all(shrinkWipeDlg);


   wiper = Wiper_Start(part, MAX_WIPER_FILE_SIZE);

   while (progress < 100 && wiper != NULL) {
      err = Wiper_Next(&wiper, &progress);
      if (strlen(err) > 0) {
         if (strcmp(err, "error.create") == 0) {
            ToolsMain_MsgBox("Error", "Unable to create wiper file\n");
         }
         else {
            ToolsMain_MsgBox("Error", err);
         }
         wiper = NULL;
         gtk_widget_destroy(shrinkWipeDlg);
      } else {
         gtk_progress_set_percentage(GTK_PROGRESS(shrinkWipeProgress),
                                     progress/100.0);
      }

      while (gtk_events_pending()) {
         gtk_main_iteration();
      }

   }

   if (progress >= 100) {
      wiper = NULL;
      gtk_widget_destroy(shrinkWipeDlg);
      return TRUE;
   } else {
      return FALSE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Shrink_OnWipeDestroy  --
 *
 *      Callback for the gtk signal "destroy" on the wipe progress dialog.
 *      Cancel the wipe operation, setting global variables so the loop in
 *      Shrink_DoWipe will exit.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The wipe operation will be canceled, and "zero" files removed.
 *
 *-----------------------------------------------------------------------------
 */

void
Shrink_OnWipeDestroy(GtkWidget *widget,     // IN: the cancel button
                     gpointer user_data) // IN: unused
{
   if (wiper != NULL) {
      Wiper_Cancel(&wiper);
      wiper = NULL;
   }
}
