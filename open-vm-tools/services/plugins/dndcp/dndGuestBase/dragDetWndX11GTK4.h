/*********************************************************
 * Copyright (c) 2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

/**
 * @file dragDetWndGTK4.h
 *
 *    Header for the DragDetWnd class.
 */

#ifndef DRAG_DET_WND_GTK4_H
#define DRAG_DET_WND_GTK4_H

#include <gtkmm.h>
#include <gio/gio.h>
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>

#if !defined(DETWNDTEST)
#include "dnd.h"
#endif
#include "fakeMouseWayland/fakeMouseWayland.h"

using source_drag_begin_handler = void (const Glib::RefPtr< Gdk::Drag > &, void *);
using source_drag_end_handler =  void (const Glib::RefPtr< Gdk::Drag > &, bool, void *);
using source_drag_cancel_handler = bool (const Glib::RefPtr< Gdk::Drag > &, Gdk::DragCancelReason, void *);
using source_drag_prepare_handler = Glib::RefPtr< Gdk::ContentProvider > (double, double, void *);
using target_drag_accept_handler = bool (const Glib::RefPtr< Gdk::Drop > &, void *);
using target_drag_drop_handler = bool (const Glib::ValueBase &, double, double, void *);
using target_drag_enter_handler = Gdk::DragAction (double, double, void *);
using target_drag_leave_handler = void (void *);
using target_drag_motion_handler = Gdk::DragAction (double, double, void *);
using target_drag_value_handler = void (void *);

class DragDetWnd
{
public:
   DragDetWnd(int UseUInput_fd = -1);
   virtual ~DragDetWnd();
   GtkWindow * GetWnd_gobj();
   Gtk::Widget *GetWnd();

   bool register_source_drag_begin_handler(source_drag_begin_handler handler, void *ctx);
   bool register_source_drag_end_handler(source_drag_end_handler handler, void *ctx);
   bool register_source_drag_cancel_handler(source_drag_cancel_handler handler, void *ctx);
   bool register_source_drag_prepare_handler(source_drag_prepare_handler handler, void *ctx);
   bool register_target_drag_accept_handler(target_drag_accept_handler handler, void *ctx);
   bool register_target_drag_enter_handler(target_drag_enter_handler handler, void *ctx);
   bool register_target_drag_drop_handler(target_drag_drop_handler handler, void *ctx);
   bool register_target_drag_leave_handler(target_drag_leave_handler handler, void *ctx);
   bool register_target_drag_motion_handler(target_drag_motion_handler handler, void *ctx);
   bool register_target_value_handler(target_drag_value_handler handler, void *ctx);
   bool EnableDND(void);
   bool DisableDND(void);
   void UpdateDetWnd(bool Show, int32 x, int32 y);
   bool SimulateXEvents(const bool showWidget, const bool buttonEvent,
                        const bool buttonPress, const bool moveWindow,
                        const bool coordsProvided,
                        const int xCoord, const int yCoord);
   void StartDrag(Glib::RefPtr<Gdk::ContentProvider> provider,
                  int mouseX,
                  int mouseY);
   bool Is_DropValue_Ready(void);
   bool Get_Drop_Value(std::string &value, std::string &value_type);
   bool Is_Drop_Supported(const Glib::RefPtr< Gdk::Drop > &drop);
   bool Is_Current_Drop_Supported(void);
   unsigned long Get_Current_Drag_ctx(void);
   unsigned long Get_Current_Drop_ctx(void);
   unsigned long Get_Detwnd_ctx(void);

private:
   void Show();
   void Hide();
   void Raise();
   void Lower();
   void Flush();
   int GetScreenWidth();
   int GetScreenHeight();
   void SetGeometry(const int x, const int y,
                    const int width, const int height);
   void GetGeometry(int &x, int &y, int &width, int &height);
   void SetIsVisible(const bool isVisible) {m_isVisible = isVisible;};
   bool GetIsVisible() {return m_isVisible;};
   bool SimulateMouseMove(const int x, const int y);

   bool m_isVisible;
   Gtk::Window *mWnd;
   Glib::RefPtr< Gtk::DragSource > mSource;
   Glib::RefPtr< Gtk::DropTarget > mTarget;
   bool mUseUInput;
   int mScreenWidth;
   int mScreenHeight;
};

#endif // DRAG_DET_WND_H
