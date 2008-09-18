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
 * toolbox-gtk.c --
 *
 *     The linux toolbox app with a GTK interface.
 */


#include <stdlib.h>
#include <string.h>

#include "toolboxGtkInt.h"
#include "vm_assert.h"
#include "vm_app.h"
#include "eventManager.h"
#include "guestApp.h"
#include "vmcheck.h"
#include "debug.h"
#include "strutil.h"
#include "rpcout.h"
#include "rpcin.h"
#include "vmsignal.h"
#include "str.h"
#include "file.h"
#include "smallIcon.xpm"
#include "conf.h"
#include "toolboxgtk_version.h"
#include "util.h"
#include "system.h"

#include "embed_version.h"
VM_EMBED_VERSION(TOOLBOXGTK_VERSION_STRING);

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DEBUG_PREFIX "vmtbox"

#define INVALID_VALUE "Invalid option"
#define INVALID_OPTION "Invalid value"
#define INVALID_COMMAND "Invalid command format"


/*
 * Globals
 */
static char *hlpDir = NULL;
static Display *gXDisplay;
static Window gXRoot;
RpcIn *gRpcInCtlPanel;
static GtkWidget *toolsMain;
static Bool optionAutoHide;
static guint gTimeoutId;
static const char **gNativeEnviron;

/* Help pages. These need to be in the same order as the tabs in the UI. */
static const char *gHelpPages[] = {
   "index.html",
   "tools_options.htm",
   "tools_devices.htm",
   "tools_scripts.htm",
   "tools_shrink.htm",
   "tools_about.htm",
};

/*
 * All signals that:
 * . Can terminate the process
 * . May occur even if the program has no bugs
 */
static int const gSignals[] = {
   SIGHUP,
   SIGINT,
   SIGQUIT,
   SIGTERM,
   SIGUSR1,
   SIGUSR2,
};


/*
 * From toolboxInt.h
 */
GdkPixmap* pixmap;
GdkBitmap* bitmask;
GdkColormap* colormap;
GtkWidget *optionsTimeSync;
GtkWidget *scriptsApply;
DblLnkLst_Links *gEventQueue;

void ToolsMainCleanupRpc(void);
void ToolsMainSignalHandler(int sig);
void ToolsMain_OnDestroy(GtkWidget *widget, gpointer data);
void ToolsMain_YesNoBoxOnClicked(GtkButton *btn, gpointer user_data);
void ToolsMain_OnHelp(gpointer btn, gpointer data);
void ToolsMain_OpenHelp(const char *help);

GtkWidget* ToolsMain_Create(void);


Bool RpcInResetCB(RpcInData *data);
Bool RpcInSetOptionCB(char const **result, size_t *resultLen, const char *name,
                      const char *args, size_t argsSize, void *clientData);
Bool RpcInCapRegCB(char const **result, size_t *resultLen, const char *name,
                   const char *args, size_t argsSize, void *clientData);
void RpcInErrorCB(void *clientdata, char const *status);
gint EventQueuePump(gpointer data);

void ShowUsage(char const *prog);


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMainCleanupRpc  --
 *
 *      Cleanup the backdoor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The application will close.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsMainCleanupRpc(void)
{
   if (gRpcInCtlPanel) {
      if (!RpcIn_stop(gRpcInCtlPanel)) {
         Debug("Failed to stop RpcIn loop\n");
      }

      RpcIn_Destruct(gRpcInCtlPanel);
      gRpcInCtlPanel = NULL;
   }

   /* Remove timeout so event queue isn't pumped after being destroyed. */
   gtk_timeout_remove(gTimeoutId);
   ASSERT(gEventQueue);
   EventManager_Destroy(gEventQueue);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMainSignalHandler  --
 *
 *      Handler for Posix signals. We do this to ensure that we exit
 *      gracefully.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The application will close.
 *
 *-----------------------------------------------------------------------------
 */

void ToolsMainSignalHandler(int sig) // IN
{
   /* We want to kill the event manager before gtk_main_quit. */
   ToolsMainCleanupRpc();
   gtk_main_quit();
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMain_OpenHelp --
 *
 *      Open the browser on the specified help page.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsMain_OpenHelp(const char *help) // IN
{
   char helpPage[1000];

   if (hlpDir == NULL) {
      ToolsMain_MsgBox("Error", "Unable to determine where help pages are stored.");
      return;
   }

   if (help == NULL) {
      ToolsMain_MsgBox("Error", "No help was found for the page.");
      return;
   }

   Str_Snprintf(helpPage, sizeof helpPage, "file:%s/%s", hlpDir, help);
   if (!GuestApp_OpenUrl(helpPage, FALSE)) {
      ToolsMain_MsgBox("Help Unavailable",
                       "Sorry, but help requires a web browser.  You may need "
                       "to modify your PATH environment variable accordingly.");
      return;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMain_OnHelp --
 *
 *      Callback for the gtk signal "clicked" on the Main window's help
 *      button.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsMain_OnHelp(gpointer btn,  // IN: Unused
                 gpointer data) // IN: notebook containing all tabs
{
   gint page;
   GtkNotebook *nb;

   nb = GTK_NOTEBOOK(data);
   if (!nb) {
      return;
   }

   page = gtk_notebook_get_current_page(nb) + 1;
   ASSERT(page > 0);

   if (page >= ARRAYSIZE(gHelpPages)) {
      char *text;
      GtkWidget *curPage;
      curPage = gtk_notebook_get_nth_page(nb, page - 1);
      gtk_label_get(GTK_LABEL(gtk_notebook_get_tab_label(nb,curPage)), &text);
      Warning("No help page for tab %s, defaulting to index.\n", text);
      page = 0;
   }

   ToolsMain_OpenHelp(gHelpPages[page]);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMain_MsgBox --
 *
 *      Display a modal dialog with a title, message, and an OK button.
 *      Used for informational messages. The dialog box is destroyed when
 *      the OK button is clicked not when this function returns.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Since its modal, the UI won't recieve any events until this dialog
 *      is destroyed.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsMain_MsgBox(gchar *title, // IN: dialog's title
                 gchar *msg)   // IN: dialog's message
{
   GtkWidget *dialog;
   GtkWidget *label;
   GtkWidget *okbtn;

   dialog = gtk_dialog_new();
   gtk_window_set_title(GTK_WINDOW(dialog), title);
   gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(toolsMain));
   gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
   gtk_widget_show(dialog);
   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
   gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 10);
   gdk_window_set_icon(dialog->window, NULL, pixmap, bitmask);

   label = gtk_label_new (msg);
   gtk_widget_show(label);
   gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

#ifdef GTK2
   okbtn = gtk_button_new_with_mnemonic("_OK");
#else
   okbtn = gtk_button_new_with_label("OK");
#endif
   gtk_widget_show(okbtn);
   gtk_box_pack_end(GTK_BOX(GTK_DIALOG(dialog)->action_area), okbtn, FALSE, FALSE, 0);
   gtk_widget_set_usize(okbtn, 70, 25);
   gtk_signal_connect_object(GTK_OBJECT(okbtn), "clicked",
                             GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(dialog));
   GTK_WIDGET_SET_FLAGS(okbtn, GTK_CAN_DEFAULT);
   gtk_widget_grab_default(okbtn);
   gtk_widget_show_all(dialog);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMain_YesNoBox --
 *
 *      Display a modal dialog with a title, message, and two buttons, yes/no.
 *      Used for "are you sure" questions. This functions like a Win32
 *      doModal() function. It pump message itself and doesn't return until
 *      the user clicks "Yes" or "No".
 *
 * Results:
 *      TRUE is the user clicked "Yes", FALSE otherwise
 *
 * Side effects:
 *      Since its modal, the UI won't recieve any events until this dialog
 *      is destroyed.
 *
 *-----------------------------------------------------------------------------
 */

Bool
ToolsMain_YesNoBox(gchar *title, gchar *msg)
{
   GtkWidget *dialog;
   GtkWidget *label;
   GtkWidget *btn;
   int ret = 0;

   dialog = gtk_dialog_new();
   gtk_window_set_title(GTK_WINDOW(dialog), title);
   gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(toolsMain));
   gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
   gtk_widget_show(dialog);
   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
   gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 10);
   gdk_window_set_icon(dialog->window, NULL, pixmap, bitmask);

   label = gtk_label_new (msg);
   gtk_widget_show(label);
   gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

#ifdef GTK2
   btn = gtk_button_new_with_mnemonic("_Yes");
#else
   btn = gtk_button_new_with_label("Yes");
#endif
   gtk_widget_show(btn);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), btn, FALSE, FALSE, 0);
   gtk_widget_set_usize(btn, 70, 25);
   gtk_signal_connect(GTK_OBJECT(btn), "clicked",
                      GTK_SIGNAL_FUNC(ToolsMain_YesNoBoxOnClicked), &ret);

#ifdef GTK2
   btn = gtk_button_new_with_mnemonic("_No");
#else
   btn = gtk_button_new_with_label("No");
#endif
   gtk_widget_show(btn);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), btn, FALSE, FALSE, 0);
   gtk_widget_set_usize(btn, 70, 25);
   gtk_signal_connect(GTK_OBJECT(btn), "clicked",
                      GTK_SIGNAL_FUNC(ToolsMain_YesNoBoxOnClicked), &ret);

   gtk_widget_show_all(dialog);

   while (gtk_events_pending() || ret == 0) {
      gtk_main_iteration();
   }

   return (ret == 1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMain_YesNoBoxOnClicked --
 *
 *      Helper function for ToolsMain_YesNoBox. This is the signal handler for
 *      YesNoBox's button clicks. It sets a return value then destroys the
 *      dialog.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      user_data will contain 1 is "Yes" was clicked, 2 is "No".
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsMain_YesNoBoxOnClicked(GtkButton *btn,     // IN: clicked button
                           gpointer user_data) // OUT: pointer to result value
{
   char *text;
   Bool *ret = (Bool*)user_data;
   gtk_label_get(GTK_LABEL(GTK_BIN(btn)->child), &text);
   if (strcmp(text, "Yes") == 0) {
      *ret = 1;
   } else if (strcmp(text, "No") == 0) {
      *ret = 2;
   }
   gtk_widget_destroy(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(btn))));
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMain_OnDestroy  --
 *
 *      Callback for the gtk signal "destroy" on the main window.
 *      Exit the gtk loop, causing main() to exit. But first, ask
 *      the user to save a dirty scripts tab.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The application will close.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsMain_OnDestroy(GtkWidget *widget, // IN: Unused
                    gpointer data)     // IN: Unused
{
   if (scriptsApply != NULL && GTK_WIDGET_IS_SENSITIVE(scriptsApply)) {
      if (ToolsMain_YesNoBox("Save changes?",
                             "Do you want to save your changes to scripts tab?")) {
         Scripts_OnApply(scriptsApply, NULL);
      }
   }

   /* We want to kill the event manager before gtk_main_quit. */
   ToolsMainCleanupRpc();
   gtk_main_quit();
}


/*
 *-----------------------------------------------------------------------------
 *
 * EventQueuePump  --
 *
 *      Handle events in the event queue. This function is re-registered as a
 *      gtk_timeout every time, since we only want to be called when it is time
 *      for the next event in the queue.
 *
 * Results:
 *      1 if there were no problems, 0 otherwise
 *
 * Side effects:
 *      The events in the queue will be called, they could do anything.
 *
 *-----------------------------------------------------------------------------
 */

gint
EventQueuePump(gpointer data) // IN: Unused
{
   int ret;
   uint64 sleepUsecs;

   gtk_timeout_remove(gTimeoutId);

   ret = EventManager_ProcessNext(gEventQueue, &sleepUsecs);
   if (ret != 1) {
      Warning("Unexpected end of EventManager loop: returned value is %d.\n\n",
              ret);
      return 0;
   }
   gTimeoutId = gtk_timeout_add(sleepUsecs/1000, &EventQueuePump, NULL);
   return 1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsMain_Create  --
 *
 *      Create, layout, and init the main UI  and all its components.
 *      This calls the tab-specific *_Create functions, and then hooks
 *      up some callbacks for "destroy" and copy/paste.
 *
 * Results:
 *      The main window widget.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget*
ToolsMain_Create(void)
{
   GtkWidget *ToolsMain;
   GtkWidget *notebookMain;
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *btn;
   char *result;
   size_t resultLen;

   ToolsMain = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(ToolsMain), ("VMware Tools Properties"));
   gtk_window_set_default_size(GTK_WINDOW(ToolsMain), 300, 400);

   gtk_signal_connect(GTK_OBJECT(ToolsMain), "destroy",
                      GTK_SIGNAL_FUNC(ToolsMain_OnDestroy), NULL);

   vbox = gtk_vbox_new(FALSE, 10);
   gtk_widget_show(vbox);
   gtk_container_add(GTK_CONTAINER(ToolsMain), vbox);
   gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

   notebookMain = gtk_notebook_new();
   gtk_widget_show(notebookMain);
   gtk_box_pack_start(GTK_BOX(vbox), notebookMain, TRUE, TRUE, 0);
   gtk_container_set_border_width(GTK_CONTAINER(notebookMain), 0);

#ifdef GTK2
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), Options_Create(ToolsMain),
                            gtk_label_new_with_mnemonic(TAB_LABEL_OPTIONS));
#else
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), Options_Create(ToolsMain),
                            gtk_label_new(TAB_LABEL_OPTIONS));
#endif

   /*
    * Beginning with ACE1, a VM could be configured to prevent editing of
    * device state from the guest. So we enable the devices page only if the
    * command fails (meaning we're pre-ACE1), or if the command succeeds and
    * we're allowed to edit the devices.
    */
   if (!RpcOut_sendOne(&result, &resultLen, "vmx.capability.edit_devices") ||
       strcmp(result, "0") != 0) {
#ifdef GTK2
      gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain),
                               Devices_Create(ToolsMain),
                               gtk_label_new_with_mnemonic(TAB_LABEL_DEVICES));
#else
      gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain),
                               Devices_Create(ToolsMain),
                               gtk_label_new(TAB_LABEL_DEVICES));
#endif
   } else {
      Debug("User not allowed to edit devices");
   }
   free(result);

#ifdef GTK2
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), Scripts_Create(ToolsMain),
                            gtk_label_new_with_mnemonic(TAB_LABEL_SCRIPTS));
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), Shrink_Create(ToolsMain),
                            gtk_label_new_with_mnemonic(TAB_LABEL_SHRINK));
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), Record_Create(ToolsMain),
                            gtk_label_new_with_mnemonic(TAB_LABEL_RECORD));
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), About_Create(ToolsMain),
                            gtk_label_new_with_mnemonic(TAB_LABEL_ABOUT));
#else
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), Scripts_Create(ToolsMain),
                            gtk_label_new(TAB_LABEL_SCRIPTS));
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), Shrink_Create(ToolsMain),
                            gtk_label_new(TAB_LABEL_SHRINK));
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), Record_Create(ToolsMain),
                            gtk_lable_new(TAB_LABEL_RECORD));
   gtk_notebook_append_page(GTK_NOTEBOOK(notebookMain), About_Create(ToolsMain),
                            gtk_label_new(TAB_LABEL_ABOUT));
#endif

   hbox = gtk_hbutton_box_new();
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 10);
   gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_EDGE);
   gtk_widget_show(hbox);
   gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   /*
    * The HIG says that the Help button should be in the lower left, and all
    * other buttons in the lower right.
    *
    * See http://developer.gnome.org/projects/gup/hig/2.0/windows-alert.html#alert-button-order
    */
#ifdef GTK2
   btn = gtk_button_new_with_mnemonic("_Help");
#else
   btn = gtk_button_new_with_label("Help");
#endif
   gtk_widget_show(btn);
   gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
   gtk_signal_connect(GTK_OBJECT(btn), "clicked", GTK_SIGNAL_FUNC(ToolsMain_OnHelp),
                      notebookMain);

#ifdef GTK2
   btn = gtk_button_new_with_mnemonic("_Close");
#else
   btn = gtk_button_new_with_label("Close");
#endif
   gtk_widget_show(btn);
   gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
   gtk_signal_connect_object(GTK_OBJECT(btn), "clicked",
                             GTK_SIGNAL_FUNC(gtk_widget_destroy),
                             GTK_OBJECT(ToolsMain));
   GTK_WIDGET_SET_FLAGS(btn, GTK_CAN_DEFAULT);
   gtk_widget_grab_default(btn);

   return ToolsMain;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInResetCB  --
 *
 *      Callback called when the vmx has done a reset on the backdoor channel
 *
 * Results:
 *      TRUE if we reply successfully, FALSE otherwise
 *
 * Side effects:
 *      Send an "ATR" to thru the backdoor.
 *
 *-----------------------------------------------------------------------------
 */

Bool
RpcInResetCB(RpcInData *data) // IN/OUT
{
   Debug("----------toolbox: Received 'reset' from vmware\n");

   return RPCIN_SETRETVALS(data, "ATR " TOOLS_CTLPANEL_NAME, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInErrorCB  --
 *
 *      Callback called when their is some error on the backdoor channel.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
RpcInErrorCB(void *clientdata, char const *status)
{
   Warning("Error in the RPC recieve loop: %s\n", status);
   Warning("Another instance of VMware Tools Properties may be running.\n\n");
   ToolsMain_OnDestroy(NULL, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInSetOptionCB
 *
 *      Parse a "Set_Option" TCLO cmd from the vmx and update the local
 *      copy of the option.
 *
 * Results:
 *      TRUE if the set option command was executed.
 *      FALSE if something failed.
 *
 * Side effects:
 *      Start or stop processes (like time syncing) that could be affected
 *      by option's new value.
 *
 *-----------------------------------------------------------------------------
 */

Bool
RpcInSetOptionCB(char const **result,     // OUT
                 size_t *resultLen,       // OUT
                 const char *name,        // IN
                 const char *args,        // IN
                 size_t argsSize,         // Unused
                 void *clientData)        // Unused
{
   char *option;
   char *value;
   int index = 0;
   Bool ret = FALSE;
   char *retStr = NULL;

   /* parse the option & value string */
   option = StrUtil_GetNextToken(&index, args, " ");
   if (!option) {
      retStr = INVALID_COMMAND;
      goto exit;
   }
   index++; // ignore leading space before value
   value = StrUtil_GetNextToken(&index, args, "");
   if (!value) {
      retStr = INVALID_COMMAND;
      goto free_option;
   } else if (strlen(value) == 0) {
      retStr = INVALID_COMMAND;
      goto free_value;
   }

   /* Validate the option name & value */
   if (strcmp(option, TOOLSOPTION_SYNCTIME) == 0) {
      if (strcmp(value, "1") == 0) {
         gtk_signal_handler_block_by_func(GTK_OBJECT(optionsTimeSync),
                                          GTK_SIGNAL_FUNC(Options_OnTimeSyncToggled),
                                          NULL);
         gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(optionsTimeSync),
                                      TRUE);
         gtk_signal_handler_unblock_by_func(GTK_OBJECT(optionsTimeSync),
                                            GTK_SIGNAL_FUNC(Options_OnTimeSyncToggled),
                                            NULL);
      } else if (strcmp(value, "0") == 0) {
         gtk_signal_handler_block_by_func(GTK_OBJECT(optionsTimeSync),
                                          GTK_SIGNAL_FUNC(Options_OnTimeSyncToggled),
                                          NULL);
         gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(optionsTimeSync),
                                      FALSE);
         gtk_signal_handler_unblock_by_func(GTK_OBJECT(optionsTimeSync),
                                            GTK_SIGNAL_FUNC(Options_OnTimeSyncToggled),
                                            NULL);
      } else {
         retStr = INVALID_VALUE;
         goto free_value;
      }
   } else if (strcmp(option, TOOLSOPTION_AUTOHIDE) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         optionAutoHide = TRUE;
         retStr = INVALID_VALUE;
         goto free_value;
      }
   } else {
         retStr = INVALID_OPTION;
         goto free_value;
   }

   ret = TRUE;
   retStr = "";
 free_value:
   free(value);
 free_option:
   free(option);
 exit:
   return RpcIn_SetRetVals(result, resultLen, retStr, ret);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInCapRegCB --
 *
 *      Handler for TCLO 'Capabilities_Register'.
 *
 * Results:
 *      TRUE if we can reply, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
RpcInCapRegCB(char const **result,     // OUT
              size_t *resultLen,       // OUT
              const char *name,        // IN
              const char *args,        // IN
              size_t argsSize,         // Unused
              void *clientData)        // Unused
{
   return RpcIn_SetRetVals(result, resultLen, "Not implemented", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OnViewportSizeRequest --
 *
 *      "size_request" signal handler for the viewport widget.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
OnViewportSizeRequest(GtkWidget *widget,           // IN
                      GtkRequisition *requisition, // OUT
                      gpointer user_data)          // Unused
{
   ASSERT(widget);
   ASSERT(requisition);

   /*
    * In GTK+ 1.2, gtk_viewport_size_request() requests 5 more pixels than what
    * its child really needs. It is hard-coded, I kid you not. We workaround it
    * here. --hpreg
    */
   requisition->width -= 5;
   requisition->height -= 5;

   /*
    * In all GTK+ versions between 1.2 and 2.4, gtk_viewport_size_request():
    * o Requests room for the shadow even if there is no shadow.
    * o Accounts for the border_width 4 times instead of 2 in the height.
    * We workaround these issues here. --hpreg
    */
#ifdef GTK2
#  define WIDGET_TO_XTHICKNESS(widget) (widget)->style->xthickness
#  define WIDGET_TO_YTHICKNESS(widget) (widget)->style->ythickness
#else
#  define WIDGET_TO_XTHICKNESS(widget) (widget)->style->klass->xthickness
#  define WIDGET_TO_YTHICKNESS(widget) (widget)->style->klass->ythickness
#endif
   if (GTK_VIEWPORT(widget)->shadow_type == GTK_SHADOW_NONE) {
      requisition->width -= 2 * WIDGET_TO_XTHICKNESS(widget);
      requisition->height -= 2 * WIDGET_TO_YTHICKNESS(widget);
   }
   requisition->height -= 2 * GTK_CONTAINER(widget)->border_width;
}


/*
 *-----------------------------------------------------------------------------
 *
 * InitHelpDir --
 *
 *      Queries the Tools config dictionary for the location of the Toolbox
 *      Help docs.  If not found, will try to fall back to semi-safe defaults.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If a suitable directory was found, hlpDir will point to a buffer
 *      containing it.  Otherwise it will remain NULL.
 *
 *-----------------------------------------------------------------------------
 */

void
InitHelpDir(GuestApp_Dict *pConfDict)   // IN
{
   const char *tmpDir = NULL;

   ASSERT(hlpDir == NULL);

   tmpDir = GuestApp_GetDictEntry(pConfDict, CONFNAME_HELPDIR);
   if (!tmpDir || !File_Exists(tmpDir)) {
      unsigned int i;

      static const char *candidates[] = {
         "/usr/lib/vmware-tools/hlp",        // Linux, Solaris
         "/usr/local/lib/vmware-tools/hlp",  // FreeBSD
      };

      for (i = 0; i < ARRAYSIZE(candidates); i++) {
         if (File_Exists(candidates[i])) {
            tmpDir = candidates[i];
            break;
         }
      }
   }

   if (tmpDir) {
      hlpDir = Util_SafeStrdup(tmpDir);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShowUsage --
 *
 *      Print out usage information to stdout.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
ShowUsage(char const *prog)
{
   fprintf(stderr,
           "Usage:\n"
           "   %s --help\n"
           "      Display this help message.\n"
           "\n"
           "   %s --minimize|--iconify\n"
           "      Start the toolbox window minimized.\n"
           "\n"
           "   %s --version\n"
           "      Show the VMware(R) Tools version.\n"
           "\n",
           prog, prog, prog);
}


/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *      This is main
 *
 * Results:
 *      0 on success.
 *
 * Side effects:
 *      The linux toolbox ui will run and do a variety of tricks for your
 *      amusement.
 *
 *-----------------------------------------------------------------------------
 */
int
main(int argc,                  // IN: ARRAY_SIZEOF(argv)
     char *argv[],              // IN: argument vector
     const char *envp[])        // IN: environment vector
{
   Bool optIconify, optHelp, optVersion;
   struct sigaction olds[ARRAYSIZE(gSignals)];
   GuestApp_Dict *pConfDict;

   if (!VmCheck_IsVirtualWorld()) {
#ifndef ALLOW_TOOLS_IN_FOREIGN_VM
      Warning("The VMware Toolbox must be run inside a virtual machine.\n");
      return 1;
#else
      runningInForeignVM = TRUE;
#endif
   }

   if (Signal_SetGroupHandler(gSignals, olds, ARRAYSIZE(gSignals),
                              ToolsMainSignalHandler) == 0 ) {
      Panic("vmware-toolbox can't set signal handler\n");
   }

   pConfDict = Conf_Load();
   Debug_Set(GuestApp_GetDictEntryBool(pConfDict, CONFNAME_LOG), DEBUG_PREFIX);
   Debug_EnableToFile(GuestApp_GetDictEntry(pConfDict, CONFNAME_LOGFILE), FALSE);
   InitHelpDir(pConfDict);
   GuestApp_FreeDict(pConfDict);

   optionAutoHide = FALSE;

   /*
    * Parse the command line. This is based on the code guestd/main.c, but it's
    * much simpler because we have only three options, all are long style, and
    * none take arguments. We only allow one option at a time.
    *
    * We do it by hand because getopt() doesn't handle long options, and
    * getopt_long is a GNU extension.
    *
    * argv[0] is the program name, as usual
    */

   /*
    * Optional arguments
    */

   /* Default values */
   optIconify = FALSE;
   optHelp = FALSE;
   optVersion = FALSE;

   if (argc == 2) {
      if (strcmp(argv[1], "--iconify") == 0) {
         optIconify = TRUE;
      } else if (strcmp(argv[1], "--minimize") == 0) {
         optIconify = TRUE;
      } else if (strcmp(argv[1], "--version") == 0) {
         optVersion = TRUE;
      } else {
         optHelp = TRUE;
      }
   } else if (argc > 2) {
      optHelp = TRUE;
   }

   if (optHelp) {
      ShowUsage(argv[0]);
      exit(0);
   }
   if (optVersion) {
      printf("VMware(R) Tools version %s\n", TOOLS_VERSION);
      exit(0);
   }

   /*
    * Determine our pre-VMware wrapper native environment for use with spawned
    * applications.
    */
   gNativeEnviron = System_GetNativeEnviron(envp);
   GuestApp_SetSpawnEnviron(gNativeEnviron);

   /*
    * See bug 73119. Some distros (SUSE 9.2 64-bit) set LC_CTYPE to the UTF-8 version of
    * the locale. This makes the toolbox use a bad font. If we don't cal gtk_set_locale(),
    * we're unaffected by this. The potential downside is that our app won't be localized.
    * But, we're not anyway, so it doesn't matter.
    *
    * gtk_set_locale();
    */
   gtk_init(&argc, &argv);

   gEventQueue = EventManager_Init();
   if (gEventQueue == NULL) {
      Warning("Unable to create the event queue.\n\n");
      return -1;
   }

   /* Setup RPC callbacks */
   gRpcInCtlPanel = RpcIn_Construct(gEventQueue);
   if (gRpcInCtlPanel == NULL) {
      Warning("Unable to create the gRpcInCtlPanel object.\n\n");
      return -1;
   }

   if (!RpcIn_start(gRpcInCtlPanel, RPCIN_POLL_TIME, RpcInResetCB, NULL, RpcInErrorCB,
                    NULL)) {
      Warning("Unable to start the gRpcInCtlPanel receive loop.\n\n");
      return -1;
   }

   RpcIn_RegisterCallback(gRpcInCtlPanel, "Capabilities_Register",
			  RpcInCapRegCB, NULL);
   RpcIn_RegisterCallback(gRpcInCtlPanel, "Set_Option",
	                  RpcInSetOptionCB, NULL);

   toolsMain = ToolsMain_Create();
   gtk_widget_show(toolsMain);

   if (optIconify) {
      XIconifyWindow(GDK_WINDOW_XDISPLAY(GTK_WIDGET(toolsMain)->window),
                     GDK_WINDOW_XWINDOW(GTK_WIDGET(toolsMain)->window),
                     DefaultScreen (GDK_DISPLAY ()));
   }

   /* Create the icon from a pixmap */
   colormap = gtk_widget_get_colormap(toolsMain);
   pixmap = gdk_pixmap_colormap_create_from_xpm_d(NULL,colormap,&bitmask,NULL,
                                                  smallIcon_xpm);
   gdk_window_set_icon(toolsMain->window, NULL, pixmap, bitmask);

   gXDisplay = GDK_WINDOW_XDISPLAY(toolsMain->window);
   gXRoot = RootWindow(gXDisplay, DefaultScreen(gXDisplay));

   /*
    * Setup the some events and a pump for the EventManager.
    * We use gtk_timeouts for this.
    */
   gTimeoutId = gtk_timeout_add(0, &EventQueuePump, NULL);

   /*
    * We'll block here until the window is destroyed or a
    * signal is received.
    */
   gtk_main();

   Signal_ResetGroupHandler(gSignals, olds, ARRAYSIZE(gSignals));
   System_FreeNativeEnviron(gNativeEnviron);

   gdk_pixmap_unref(pixmap);
   gdk_bitmap_unref(bitmask);
   free(hlpDir);

   return 0;
}
