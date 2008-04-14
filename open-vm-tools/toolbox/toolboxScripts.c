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
 * toolboxScripts.c --
 *
 *     The scripts tab for the linux gtk toolbox
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__FreeBSD__) && BSD_VERSION >= 53
# include <syslimits.h>
#endif

#include "toolboxInt.h"
#include "guestApp.h"
#include "conf.h"
#include "str.h"
#include "procMgr.h"
#include "file.h"

/*
 * From toolboxInt.h
 */
GtkWidget *scriptsApply;


/*
 * Globals
 */
/* X terminal application and option (to launch vi to edit scripts). */
static char *termApp;
static char *termAppOption;
static GuestApp_Dict *confDict;
static GtkWidget *scriptsUseScript;
static GtkWidget *scriptsDefaultScript;
static GtkWidget *scriptsCustomScript;
static GtkWidget *scriptsEdit;
static GtkWidget *scriptsRun;
static GtkWidget *scriptsPath;
static GtkWidget *scriptsBrowse;
static GtkWidget *scriptsFileDlg;
static GtkWidget *scriptsCombo;

void Scripts_UpdateEnabled(void);
void Scripts_OnComboChanged(gpointer entry, gpointer data);
void Scripts_OnUseScriptToggled(gpointer entry, gpointer data);
void Scripts_OnDefaultScriptToggled(gpointer btn, gpointer data);
void Scripts_OnBrowse(gpointer btn, gpointer data);
void Scripts_OnEdit(gpointer btn, gpointer data);
void Scripts_OnRun(gpointer btn, gpointer data);
void Scripts_BrowseOnOk(gpointer btn, gpointer data);
void Scripts_BrowseOnChanged(gpointer entry, gpointer data);
void Scripts_PathOnChanged(GtkEditable *editable, gpointer data);


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_Create  --
 *
 *      Create, layout, and init the Scripts tab UI and all its widgets.
 *
 * Results:
 *      The Scripts tab widget (it's a vbox).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget*
Scripts_Create(GtkWidget* mainWnd)
{
   GtkWidget* scriptstab;
   GtkWidget* hbox;
   GtkWidget* label;
   GList *items = NULL;
   GSList *radiobtn_group = NULL;

   confDict = Conf_Load();

   scriptstab = gtk_vbox_new(FALSE, 10);
   gtk_widget_show(scriptstab);
   gtk_container_set_border_width(GTK_CONTAINER(scriptstab), 10);

   /* Only root can edit scripts. */
   if (geteuid() != 0) {
      label = 
         gtk_label_new("This option is enabled only if you run VMware Tools as root.");
      gtk_widget_show(label);
      gtk_box_pack_start(GTK_BOX(scriptstab), label, FALSE, FALSE, 0);
   } else {
      hbox = gtk_hbox_new(FALSE, 10);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(scriptstab), hbox, FALSE, FALSE, 0);

      label = gtk_label_new("Script Event");
      gtk_widget_show(label);
      gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

      scriptsCombo = gtk_combo_new();
      gtk_widget_show(scriptsCombo);
      gtk_box_pack_start(GTK_BOX(hbox), scriptsCombo, TRUE, TRUE, 0);
      items = g_list_append(items, SCRIPT_SUSPEND);
      items = g_list_append(items, SCRIPT_RESUME);
      items = g_list_append(items, SCRIPT_OFF); 
      items = g_list_append(items, SCRIPT_ON);
      gtk_combo_set_popdown_strings(GTK_COMBO(scriptsCombo), items);
      gtk_combo_set_use_arrows(GTK_COMBO(scriptsCombo), TRUE);
      gtk_combo_set_use_arrows_always(GTK_COMBO(scriptsCombo), TRUE);
      gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(scriptsCombo)->entry), FALSE);
      gtk_signal_connect(GTK_OBJECT(GTK_COMBO(scriptsCombo)->entry), "changed",
                         GTK_SIGNAL_FUNC(Scripts_OnComboChanged), NULL);

      scriptsUseScript = gtk_check_button_new_with_label("Use Script");
      gtk_widget_show(scriptsUseScript);
      gtk_box_pack_start(GTK_BOX(scriptstab), scriptsUseScript, FALSE, FALSE, 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scriptsUseScript), TRUE);
      gtk_label_set_justify(GTK_LABEL(GTK_BIN(GTK_BUTTON(scriptsUseScript))->child),
                            GTK_JUSTIFY_LEFT);
      gtk_signal_connect(GTK_OBJECT(scriptsUseScript), "toggled",
                         GTK_SIGNAL_FUNC(Scripts_OnUseScriptToggled), NULL);

      /*
       * Try to find an available x terminal app to launch vi.
       *
       * Options with different X terminal apps to launch vi:
       *    xterm -e vi foo.txt
       *    rxvt -e vi foo.txt
       *    konsole -e vi foo.txt
       *    gnome-terminal -x vi foo.txt
       */
      termApp = NULL;
      termAppOption = "-e";
      if (getenv("GNOME_DESKTOP_SESSION_ID") != NULL &&
          GuestApp_FindProgram("gnome-terminal")) {
         termApp = "gnome-terminal";
         termAppOption = "-x";
      } else if (getenv("KDE_FULL_SESSION") != NULL && 
                 !strcmp(getenv("KDE_FULL_SESSION"), "true") &&
                 GuestApp_FindProgram("konsole")) {
         termApp = "konsole";
      } else if (GuestApp_FindProgram("xterm")) {
         termApp = "xterm";
      } else if (GuestApp_FindProgram("rxvt")) {
         termApp = "rxvt";
      } else if (GuestApp_FindProgram("konsole")) {
         termApp = "konsole";
      } else if (GuestApp_FindProgram("gnome-terminal")) {
         termApp = "gnome-terminal";
         termAppOption = "-x";
      }

      scriptsDefaultScript = 
         gtk_radio_button_new_with_label(NULL,("Default Script"));
      gtk_widget_show(scriptsDefaultScript);
      gtk_box_pack_start(GTK_BOX(scriptstab), scriptsDefaultScript, FALSE, FALSE, 0);
      radiobtn_group  = 
         gtk_radio_button_group(GTK_RADIO_BUTTON(scriptsDefaultScript));
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scriptsDefaultScript), TRUE);
      gtk_signal_connect(GTK_OBJECT(scriptsDefaultScript), "toggled",
                         GTK_SIGNAL_FUNC(Scripts_OnDefaultScriptToggled), NULL);
      
      scriptsCustomScript = gtk_radio_button_new_with_label(NULL,("Custom Script"));
      gtk_widget_show(scriptsCustomScript);
      gtk_box_pack_start(GTK_BOX(scriptstab), scriptsCustomScript, FALSE, FALSE, 0);
      gtk_radio_button_set_group(GTK_RADIO_BUTTON(scriptsCustomScript),
                                 radiobtn_group);
      
      hbox = gtk_hbox_new(FALSE, 10);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(scriptstab), hbox, FALSE, FALSE, 0);
      gtk_widget_set_usize(hbox, -1, 25);
      
      scriptsPath = gtk_entry_new();
      gtk_widget_show(scriptsPath);
      gtk_box_pack_start(GTK_BOX(hbox), scriptsPath, TRUE, TRUE, 0);
      gtk_widget_set_sensitive(scriptsPath, FALSE);
      gtk_signal_connect(GTK_OBJECT(scriptsPath), "changed",
                         GTK_SIGNAL_FUNC(Scripts_PathOnChanged), NULL);
      
      scriptsBrowse = gtk_button_new_with_label("Browse...");
      gtk_widget_show(scriptsBrowse);
      if (termApp) {
         gtk_box_pack_start(GTK_BOX(hbox), scriptsBrowse, FALSE, FALSE, 0);
      } else {
         gtk_box_pack_end(GTK_BOX(hbox), scriptsBrowse, FALSE, FALSE, 0);
      }
      gtk_widget_set_usize(scriptsBrowse, 70, 6);
      gtk_widget_set_sensitive(scriptsBrowse, FALSE);
      gtk_signal_connect(GTK_OBJECT(scriptsBrowse), "clicked", 
                         GTK_SIGNAL_FUNC(Scripts_OnBrowse), NULL);

      /* Only create edit button if there is an available X terminal app. */
      if (termApp) {
         scriptsEdit = gtk_button_new_with_label("Edit...");
         gtk_widget_show(scriptsEdit);
         gtk_box_pack_end(GTK_BOX(hbox), scriptsEdit, FALSE, FALSE, 0);
         gtk_widget_set_usize(scriptsEdit, 70, 25);
         gtk_signal_connect(GTK_OBJECT(scriptsEdit), "clicked", 
                            GTK_SIGNAL_FUNC(Scripts_OnEdit), NULL);
      }

      hbox = gtk_hbox_new(FALSE, 10);
      gtk_widget_show(hbox);
      gtk_box_pack_end(GTK_BOX(scriptstab), hbox, FALSE, FALSE, 0);
      
      scriptsRun = gtk_button_new_with_label("Run Now");
      gtk_widget_show(scriptsRun);
      gtk_box_pack_end(GTK_BOX(hbox), scriptsRun, FALSE, FALSE, 0);
      gtk_widget_set_usize(scriptsRun, 70, 25);
      gtk_signal_connect(GTK_OBJECT(scriptsRun), "clicked", 
                         GTK_SIGNAL_FUNC(Scripts_OnRun), NULL);

      scriptsApply = gtk_button_new_with_label("Apply");
      gtk_widget_show(scriptsApply);
      gtk_box_pack_end(GTK_BOX(hbox), scriptsApply, FALSE, FALSE, 0);
      gtk_widget_set_usize(scriptsApply, 70, 6);
      gtk_widget_set_sensitive(scriptsApply, FALSE);
      gtk_signal_connect(GTK_OBJECT(scriptsApply), "clicked", 
                         GTK_SIGNAL_FUNC(Scripts_OnApply), NULL);
      
      gtk_signal_emit_by_name(GTK_OBJECT(GTK_COMBO(scriptsCombo)->entry), "changed");

   }
   return scriptstab;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_UpdateEnabled --
 *
 *      Update the enabled/disabled state of the widgets on the Scripts tab. 
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
Scripts_UpdateEnabled(void)
{
   Bool enabledUse, enabledCustom;
   
   enabledUse = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scriptsUseScript));
   enabledCustom = 
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scriptsCustomScript));
   gtk_widget_set_sensitive(scriptsDefaultScript, enabledUse);
   gtk_widget_set_sensitive(scriptsCustomScript, enabledUse);
   if (termApp) {
      gtk_widget_set_sensitive(scriptsEdit, enabledUse && enabledCustom);
   }
   gtk_widget_set_sensitive(scriptsRun, enabledUse);
   gtk_widget_set_sensitive(scriptsPath, enabledUse && enabledCustom);
   gtk_widget_set_sensitive(scriptsBrowse, enabledUse && enabledCustom);
}

   
/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_OnComboChanged --
 *
 *      Callback for the gtk signal "changed" on the Scripts tab's combo box.
 *      Lookup the script paths based on the entry selected and update the UI
 *      to match the contents of confDict. It temporarily blocks the  "toggled" 
 *      signals because the callbacks for those signal should only be called 
 *      if a user makes a change, not when the state is changed internally
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The internal state of a bunch of widget is changed.
 *
 *-----------------------------------------------------------------------------
 */

void
Scripts_OnComboChanged(gpointer entry, // IN: the entry selected
                       gpointer data)  // IN: unused
{   
   const char *path, *defaultPath;
   const char *currentState;
   currentState = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(scriptsCombo)->entry));
   if (strcmp(currentState, SCRIPT_SUSPEND) == 0) {
      path = GuestApp_GetDictEntry(confDict, CONFNAME_SUSPENDSCRIPT);
      defaultPath = GuestApp_GetDictEntryDefault(confDict, CONFNAME_SUSPENDSCRIPT);
   } else if (strcmp(currentState, SCRIPT_RESUME) == 0) {
      path = GuestApp_GetDictEntry(confDict, CONFNAME_RESUMESCRIPT);
      defaultPath = GuestApp_GetDictEntryDefault(confDict, CONFNAME_RESUMESCRIPT);
   } else if (strcmp(currentState, SCRIPT_OFF) == 0) {
      path = GuestApp_GetDictEntry(confDict, CONFNAME_POWEROFFSCRIPT);      
      defaultPath = GuestApp_GetDictEntryDefault(confDict, CONFNAME_POWEROFFSCRIPT);
   } else if (strcmp(currentState, SCRIPT_ON) == 0) {
      path = GuestApp_GetDictEntry(confDict, CONFNAME_POWERONSCRIPT);      
      defaultPath = GuestApp_GetDictEntryDefault(confDict, CONFNAME_POWERONSCRIPT);
   } else {
      path = "";
      defaultPath = "";
   }

   gtk_signal_handler_block_by_func(GTK_OBJECT(scriptsUseScript),
                                    GTK_SIGNAL_FUNC(Scripts_OnUseScriptToggled),
                                    NULL);
   gtk_signal_handler_block_by_func(GTK_OBJECT(scriptsDefaultScript),
                                    GTK_SIGNAL_FUNC(Scripts_OnDefaultScriptToggled), 
                                    NULL);
   if (strcmp(path, "") == 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scriptsUseScript), FALSE);
      path = defaultPath;
   } else {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scriptsUseScript), TRUE);
   }

   if (strcmp(path, defaultPath) == 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scriptsDefaultScript),
                                   TRUE);
      gtk_entry_set_text(GTK_ENTRY(scriptsPath), (char*)defaultPath);
   }
   else {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(scriptsCustomScript),
                                   TRUE);
      gtk_entry_set_text(GTK_ENTRY(scriptsPath), (char*)path);
   }

   gtk_signal_handler_unblock_by_func(GTK_OBJECT(scriptsDefaultScript),
                                      GTK_SIGNAL_FUNC(Scripts_OnDefaultScriptToggled),
                                      NULL);
   gtk_widget_set_sensitive(scriptsApply, FALSE);
 
   gtk_signal_handler_unblock_by_func(GTK_OBJECT(scriptsUseScript),
                                      GTK_SIGNAL_FUNC(Scripts_OnUseScriptToggled),
                                      NULL);
   Scripts_UpdateEnabled();
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_OnDefaultScriptToggled --
 *
 *      Callback for the gtk signal "toggled" on the Scripts tab's default
 *      checkbox. This is a passthru to Scripts_UpdateEnabled.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Scripts_UpdateEnabled is called, it will affect the state of various widgets.
 *
 *-----------------------------------------------------------------------------
 */

void
Scripts_OnDefaultScriptToggled(gpointer btn,  // IN: unused
                               gpointer data) // IN: unused
{
   Scripts_UpdateEnabled();
   gtk_widget_set_sensitive(scriptsApply, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_OnUseScriptToggled --
 *
 *      Callback for the gtk signal "toggled" on the Scripts tab's use
 *      checkbox. This is a passthru to Scripts_UpdateEnabled.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Scripts_UpdateEnabled is called, it will affect the state of various widgets.
 *
 *-----------------------------------------------------------------------------
 */

void
Scripts_OnUseScriptToggled(gpointer btn,  // IN: unused
                           gpointer data) // IN: unused
{
   Scripts_UpdateEnabled();
   gtk_widget_set_sensitive(scriptsApply, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_OnApply --
 *
 *      Callback for the gtk signal "clicked" on the Scripts tab's apply
 *      button. This updates the confdict both in memory and on disk with
 *      the user's changes.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      tools.conf is changed on disk
 *
 *-----------------------------------------------------------------------------
 */

void
Scripts_OnApply(gpointer btn,  // IN: unused
                gpointer data) // IN: unused
{
   const char *path;
   const char *currentState;
   char *confName;
   Bool enabledUse, enabledDef;

   currentState = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(scriptsCombo)->entry));
   if (strcmp(currentState, SCRIPT_SUSPEND) == 0) {
      confName = CONFNAME_SUSPENDSCRIPT;
   } else if (strcmp(currentState, SCRIPT_RESUME) == 0) {
      confName = CONFNAME_RESUMESCRIPT;
   } else if (strcmp(currentState, SCRIPT_OFF) == 0) {
      confName = CONFNAME_POWEROFFSCRIPT;
   } else if (strcmp(currentState, SCRIPT_ON) == 0) {
      confName = CONFNAME_POWERONSCRIPT;
   } else {
      confName = "";
   }

   enabledUse = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scriptsUseScript));
   enabledDef = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scriptsDefaultScript));

   if (!enabledUse) {
      path = "";
   } else {
      if (enabledDef) {
         path = GuestApp_GetDictEntryDefault(confDict, confName);
      } else {
         path = gtk_editable_get_chars(GTK_EDITABLE(scriptsPath), 0, -1); 
      }
   }

   GuestApp_SetDictEntry(confDict, confName, path);

   GuestApp_WriteDict(confDict);
   Scripts_UpdateEnabled();
   gtk_widget_set_sensitive(scriptsApply, FALSE);

   gtk_signal_handler_block_by_func(GTK_OBJECT(scriptsPath),
                                    GTK_SIGNAL_FUNC(Scripts_PathOnChanged),
                                    NULL);
   gtk_entry_set_text(GTK_ENTRY(scriptsPath), (char *)path);
   gtk_signal_handler_unblock_by_func(GTK_OBJECT(scriptsPath),
                                      GTK_SIGNAL_FUNC(Scripts_PathOnChanged),
                                      NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_OnEdit --
 *
 *      Callback for the gtk signal "clicked" on the Scripts tab's edit
 *      button. This will fork and exec an  editor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      An editor is started.
 *
 *-----------------------------------------------------------------------------
 */

void
Scripts_OnEdit(gpointer btn,  // IN: unused
               gpointer data) // IN: unused
{
   const char *scriptName;
   char *cmd;

   if (!termApp) {
      ToolsMain_MsgBox("Error", "Unable to locate terminal application in "
                       "which to edit script.");
      return;
   }

   scriptName = gtk_entry_get_text(GTK_ENTRY(scriptsPath));
   cmd = Str_Asprintf(NULL, "%s %s vi %s >/dev/null 2>&1",
                      termApp, termAppOption, scriptName);
   if (!cmd) {
      ToolsMain_MsgBox("Error", "Failure creating command to edit script.");
      return;
   }

   if (!ProcMgr_ExecSync(cmd, NULL)) {
      ToolsMain_MsgBox("Error", "Cannot edit script because the vi editor "
                       "was not found.");
   }

   free(cmd);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_OnRun --
 *
 *      Callback for the gtk signal "clicked" on the Scripts tab's run
 *      button. This forks and execs a script
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      A script is run as root. It may have many side effects.
 *
 *-----------------------------------------------------------------------------
 */

void
Scripts_OnRun(gpointer btn,  // IN: unused
              gpointer data) // IN: unused
{
   const char *scriptName;

   scriptName = gtk_entry_get_text(GTK_ENTRY(scriptsPath));
   if (!ProcMgr_ExecSync(scriptName, NULL)) {
      ToolsMain_MsgBox("Error", "Failure executing script, please ensure the "
                       "file exists and is executable.");
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_OnBrowse --
 *
 *      Callback for the gtk signal "clicked" on the Scripts tab's browse
 *      button. Creates a file selection dialog and puts the resutls in
 *      the Scripts edit box when done. This function blocks the caller
 *      until the user closes the file dialog.
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
Scripts_OnBrowse(gpointer btn,  // IN: unused
                 gpointer data) // IN: unused
{
   char path[PATH_MAX];
   const char *defaultPath;
   struct stat statBuf;
   
   scriptsFileDlg = gtk_file_selection_new("Select a file");
   gtk_widget_show(scriptsFileDlg);

   defaultPath = gtk_entry_get_text(GTK_ENTRY(scriptsPath));
   Str_Strcpy(path, defaultPath, sizeof path);

   /* 
    * If the filename represents a directory but does not end with a path 
    * separator, append a path separator to it so that the file chooser will
    * start off in that directory instead of its parent.
    */
   if (path[strlen(path) - 1] != '/' &&
       stat(path, &statBuf) == 0 &&
       S_ISDIR(statBuf.st_mode)) {
      Str_Strcat(path, "/", sizeof path);
   }
   
   gtk_file_selection_set_filename(GTK_FILE_SELECTION(scriptsFileDlg), 
                                   path);
   gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(scriptsFileDlg));
   gtk_widget_set_sensitive(GTK_FILE_SELECTION(scriptsFileDlg)->ok_button, FALSE);
   gtk_clist_set_selection_mode(GTK_CLIST(GTK_FILE_SELECTION(scriptsFileDlg)->
                                          file_list),
                                GTK_SELECTION_BROWSE);
   gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(scriptsFileDlg)->ok_button),
                       "clicked", (GtkSignalFunc)Scripts_BrowseOnOk, &path);
   gtk_signal_connect(GTK_OBJECT(scriptsFileDlg), "destroy",
                      (GtkSignalFunc)gtk_widget_destroyed, &scriptsFileDlg);
   gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(scriptsFileDlg)->
                                        cancel_button),
                             "clicked", (GtkSignalFunc)gtk_widget_destroy,
                             GTK_OBJECT(scriptsFileDlg));
   gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(scriptsFileDlg)->selection_entry), 
                      "changed",
                      (GtkSignalFunc)Scripts_BrowseOnChanged, 
                      GTK_FILE_SELECTION(scriptsFileDlg)->ok_button);


   /* Block here and pump messages */
   while (gtk_events_pending() || scriptsFileDlg != NULL) {
      gtk_main_iteration();
   }
   
   if (*path != 0) {
      gtk_entry_set_text(GTK_ENTRY(scriptsPath), path); 
      gtk_widget_set_sensitive(scriptsApply, TRUE);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_BrowseOnChanged --
 *
 *      Callback for the gtk signal "changes" on the Scripts file browser text
 *      entry. Enable/disable the OK button based on whether the entry is NULL.
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
Scripts_BrowseOnChanged(gpointer entry, // IN: selection_entry
                        gpointer data)  // IN: ok_button
{
   gchar *text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
   gtk_widget_set_sensitive(GTK_WIDGET(data), (strlen(text) > 0));
   g_free(text);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Scripts_BrowseOnOk --
 *
 *      Callback for the gtk signal "clicked" on the Scripts file browser OK
 *      button.  Set "okPressed" to 1 so the Scripts_OnBrowse code knows to 
 *      quit looping and that OK was the reason we closed.
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
Scripts_BrowseOnOk(gpointer btn,  // IN: OK button
                   gpointer data) // IN: path
{
   const char *ret;

   ret = gtk_file_selection_get_filename(GTK_FILE_SELECTION(scriptsFileDlg));
   strncpy((char*)data, ret, PATH_MAX);
   gtk_widget_destroy(scriptsFileDlg);
}


/*
 *----------------------------------------------------------------------------
 *
 * Scripts_PathOnChanged --
 *
 *   Callback for gtk signal "changed" on the Scripts path GtkEntry.  This
 *   resets the Apply button back to sensitive mode since the text in the entry
 *   has changed.
 *
 * Results:
 *   None.
 *
 * Side effects:
 *   None.
 *
 *----------------------------------------------------------------------------
 */

void
Scripts_PathOnChanged(GtkEditable *editable, // IN: editable that has changed
                      gpointer data)         // IN: unused
{
   if (editable != (GtkEditable *)scriptsPath) {
      return;
   }

   gtk_widget_set_sensitive(scriptsApply, TRUE);
}
