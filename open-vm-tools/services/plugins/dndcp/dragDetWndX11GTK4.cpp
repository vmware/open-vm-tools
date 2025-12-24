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
 * @file dragDetWndX11GTK4.cpp
 *
 * Detection window code for Linux/X11, based on Gtkmm-4.0.
 */

#define G_LOG_DOMAIN "dndcp"

#include "tracer.hh"
#include "dragDetWndX11GTK4.h"
#include "dndUIX11GTK4.h"
#include <list>

extern "C" {
#include <X11/extensions/XTest.h>       /* for XTest*() */
#include <gtk/gtk.h>
#include <gdk/x11/gdkx.h>
#include <X11/Xatom.h>
}


/**
 *
 * Constructor.
 */

DragDetWnd::DragDetWnd(int useUInput_fd)
   : m_isVisible(false),
     mWnd(NULL),
     mSource(NULL),
     mTarget(NULL),
     mUseUInput(false),
     mScreenWidth(0),
     mScreenHeight(0)
{
   std::vector<GType> types;
   mWnd = new Gtk::Window();
   if (!mWnd) {
      g_error("%s: unable to allocate DragDetWnd object\n", __FUNCTION__);
      return;
   }

   // Makes this window can grab focus.
   mWnd->set_decorated(false);
   mWnd->set_resizable(false);
   mWnd->set_modal(false);
   mWnd->set_can_focus(true);
   mWnd->set_focusable(true);
   mWnd->set_sensitive(false);
   // Makes this window transparent because we don't want user to see it.
   mWnd->set_opacity(0.01);
   mWnd->set_visible(true);
   mWnd->set_visible(false);

   //dnd Source controller
   mSource = Gtk::DragSource::create();
   if (!mSource) {
      g_error("%s: unable to allocate source for DragDetWnd object\n",
              __FUNCTION__);
      goto fail;
   }
   mSource->set_actions(Gdk::DragAction::COPY | Gdk::DragAction::MOVE);

   //dnd target controller
   mTarget = Gtk::DropTarget::create(G_TYPE_INVALID, Gdk::DragAction::COPY | Gdk::DragAction::MOVE);
   if (!mTarget) {
      g_error("%s: unable to allocate target for DragDetWnd object\n",
              __FUNCTION__);
      goto fail;
   }

   // Put GDK_TYPE_FILE_LIST at first, and that will make GTK report file list when possible.
   types.push_back(GDK_TYPE_FILE_LIST);
   types.push_back(G_TYPE_FILE);
   types.push_back(G_TYPE_STRING);
   mTarget->set_gtypes(types);
   // Let GTK preload data early.
   mTarget->set_preload(true);

/* Need to check USE_UINPUT, as FreeBSD does not support this */
#ifdef USE_UINPUT
   // Uinput simulation for wayland backend.
   if (useUInput_fd != -1) {
       g_debug("%s: Using uinput simulation", __FUNCTION__);
       Display *display = XOpenDisplay(NULL);
       Screen * scrn = DefaultScreenOfDisplay(display);
       if (FakeMouse_Init(useUInput_fd, scrn->width, scrn->height)) {
           mUseUInput = true;
           mScreenWidth = scrn->width;
           mScreenHeight = scrn->height;
           g_debug("%s: mScreenWidth is %d, mScreenHeight is %d", __FUNCTION__, mScreenWidth, mScreenHeight);
       }
       // Disconnect the connection to X Server.
       XCloseDisplay(display);
   }
#endif
   return;
fail:
   if (mWnd) {
       delete mWnd;
       mWnd = NULL;
   }
}


/**
 *
 * Destructor.
 */

DragDetWnd::~DragDetWnd()
{
#ifdef USE_UINPUT
   if (mUseUInput) {
      FakeMouse_Destory();
   }
#endif
   if (mWnd) {
      delete mWnd;
      mWnd = NULL;
   }
}


/**
 * Get the actual widget.
 */

Gtk::Widget *
DragDetWnd::GetWnd()
{
   return static_cast<Gtk::Widget*>(mWnd);
}

/**
 * Register GTK callback which was triggered when a drag begin happens.
 */

bool
DragDetWnd::register_source_drag_begin_handler(source_drag_begin_handler handler, void *ctx)
{
   mSource->signal_drag_begin().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Register GTK callback which was triggered when a drag end happens.
 */

bool
DragDetWnd::register_source_drag_end_handler(source_drag_end_handler handler, void *ctx)
{
   mSource->signal_drag_end().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Register GTK callback when a drag has failed
 */

bool
DragDetWnd::register_source_drag_cancel_handler(source_drag_cancel_handler handler, void *ctx)
{
   mSource->signal_drag_cancel().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Register GTK callback when a drag is about to be initiated.
 */

bool
DragDetWnd::register_source_drag_prepare_handler(source_drag_prepare_handler handler, void *ctx)
{
   mSource->signal_prepare().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Register GTK callback when a drop operation is about to begin.
 */

bool
DragDetWnd::register_target_drag_accept_handler(target_drag_accept_handler handler, void *ctx)
{
   mTarget->signal_accept().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Register GTK callback when the pointer enters the widget.
 */

bool
DragDetWnd::register_target_drag_enter_handler(target_drag_enter_handler handler, void *ctx)
{
   mTarget->signal_enter().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Register GTK callback when the user drops the data onto the widget.
 */

bool
DragDetWnd::register_target_drag_drop_handler(target_drag_drop_handler handler, void *ctx)
{
   mTarget->signal_drop().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Register GTK callback when the pointer leaves the widget.
 */

bool
DragDetWnd::register_target_drag_leave_handler(target_drag_leave_handler handler, void *ctx)
{
   mTarget->signal_leave().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Register GTK callback when the drop value changed. when set droptarget controller preload,
 * the value may be ready befor drop action happen. When drop leave happens, the value will be
 * set to NULL.
 */

bool
DragDetWnd::register_target_value_handler(target_drag_value_handler handler, void *ctx)
{
    mTarget->property_value().signal_changed().connect(sigc::bind(sigc::ptr_fun(handler), ctx));
    return true;
}


/**
 * Register GTK callback while the pointer is moving over the drop target.
 */

bool
DragDetWnd::register_target_drag_motion_handler(target_drag_motion_handler handler, void *ctx)
{
   mTarget->signal_motion().connect(sigc::bind(handler, ctx), false);
   return true;
}


/**
 * Enable drag source and target.
 */

bool
DragDetWnd::EnableDND(void)
{
   g_debug("%s\n", __FUNCTION__);
   mWnd->add_controller(mSource);
   mWnd->add_controller(mTarget);
   return true;
}


/**
 * Get the current drag contax from source.
 */

unsigned long
DragDetWnd::Get_Current_Drag_ctx(void)
{
   Glib::RefPtr< Gdk::Drag > current_drag = mSource->get_drag();
   if (current_drag != NULL)
       return (unsigned long) current_drag->gobj();
   else
       return 0;
}


/**
 * Disable drag source and target.
 */

bool
DragDetWnd::DisableDND(void)
{
   g_debug("%s\n", __FUNCTION__);
   mWnd->remove_controller(mSource);
   mWnd->remove_controller(mTarget);
   return true;
}


/**
 * Query if drop value is ready.
 */

bool
DragDetWnd::Is_DropValue_Ready(void)
{
    Glib::ValueBase value= mTarget->get_value();
    return G_IS_VALUE(value.gobj());
}


/**
 * Query if current drop type is supported.
 */

bool
DragDetWnd::Is_Current_Drop_Supported(void)
{
   Glib::RefPtr< Gdk::Drop > current_drop = mTarget->get_drop();
   return this->Is_Drop_Supported(current_drop);
}


/**
 * Query if a drop type is supported.
 */

bool
DragDetWnd::Is_Drop_Supported(const Glib::RefPtr< Gdk::Drop > &drop)
{
   Glib::RefPtr<Gdk::ContentFormats> supported_format = drop->get_formats();
   g_debug("%s content type is %s", __FUNCTION__, supported_format->to_string().c_str());

   return (supported_format->contain_gtype(GDK_TYPE_FILE_LIST) ||
           supported_format->contain_gtype(G_TYPE_STRING));
}


/**
 * Query the current drop context from target.
 */

unsigned long
DragDetWnd::Get_Current_Drop_ctx(void)
{
   Glib::RefPtr< Gdk::Drop > current_drop = mTarget->get_drop();
   if (current_drop != NULL)
       return (unsigned long) mTarget->get_drop()->gobj();
   else
       return 0;
}


/**
 * Get drop value from detect window.
 */

bool
DragDetWnd::Get_Drop_Value(std::string &value, std::string &value_type)
{
    Glib::ValueBase cur_value= mTarget->get_value();
    bool ret = false;

    if (G_IS_VALUE(cur_value.gobj())) {
      if (G_VALUE_HOLDS(cur_value.gobj(), GDK_TYPE_FILE_LIST)) {
         GSList *file_list = gdk_file_list_get_files(static_cast<GdkFileList *>(g_value_get_boxed(cur_value.gobj())));
         g_debug("%s Get file list ...", __FUNCTION__);
         for (GSList *l = file_list; l != nullptr; l = l->next) {
            GFile *file = static_cast<GFile *>(l->data);
            gchar *uri = g_file_get_uri(file);
            if (uri) {
               g_debug("%s Get file uri: %s", __FUNCTION__, uri);
               value += uri;
               value += '\n';
            }
         }
         if (value.length()) {
            ret = true;
            value_type = DRAG_TARGET_NAME_URI_LIST;
         }
      } else if (G_VALUE_HOLDS(cur_value.gobj(), G_TYPE_FILE)) {
         GFile* gfile = G_FILE(g_value_get_object(cur_value.gobj()));
         g_debug("%s Get file ...", __FUNCTION__);
         if (gfile) {
            gchar *uri = g_file_get_uri(gfile);
             if (uri) {
               g_debug("%s Get file uri: %s", __FUNCTION__, uri);
               value += uri;
            }
        }
      } else if (G_VALUE_HOLDS(cur_value.gobj(), G_TYPE_STRING)) {
         const gchar* text= g_value_get_string(cur_value.gobj());
         Glib::RefPtr<Gdk::ContentFormats> supported_format = mTarget->get_drop()->get_formats();

         value += text;
         g_debug("%s Get str: %s", __FUNCTION__, text);
         if (supported_format->to_string().find(TARGET_NAME_TEXT_RTF) != Glib::ustring::npos) {
             value_type = TARGET_NAME_TEXT_RTF;
         } else {
             value_type = TARGET_NAME_STRING;
         }
         g_debug("%s Set type as: %s", __FUNCTION__, value_type.c_str());
         ret = true;

      } else {
         g_warning("%s Unsupported DND Value type %s",
	           __FUNCTION__, g_type_name(G_TYPE_FROM_INSTANCE(cur_value.gobj())));
      }
    } else
       g_debug("%s No value available %ld", __FUNCTION__, (unsigned long) cur_value.gobj());

   return ret;
}


/**
 * Flush the X connection.
 */

void
DragDetWnd::Flush()
{
   Glib::RefPtr<Gdk::Display> gdkdisplay = Gdk::Display::get_default();
   g_debug("%s\n", __FUNCTION__);
   if (gdkdisplay) {
      gdkdisplay->sync();
      gdkdisplay->flush();
   }
}


/**
 * Show the window.
 */

void
DragDetWnd::Show(void)
{
   Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
   int width = 0;
   int height = 0;
   g_debug("%s: enter\n", __FUNCTION__);
   if (display) {
      Glib::RefPtr<Gio::ListModel> monitors = display->get_monitors();
      if (!monitors || monitors->get_n_items() == 0)
         return;

      GListModel* c_model = monitors->gobj();
      for (guint i = 0; i < monitors->get_n_items(); ++i) {
         GObject* c_item = G_OBJECT(g_list_model_get_item(c_model, i));
         Glib::RefPtr<Gdk::Monitor> monitor = Glib::wrap(GDK_MONITOR(c_item), true);
         Gdk::Rectangle geometry = monitor->property_geometry();
         mWnd->set_default_size(geometry.get_width(), geometry.get_height());
         break;
      }
   } else {
          g_error("%s: Cannot get default display \n", __FUNCTION__);
          return;
   }

   mWnd->set_sensitive(true);
   mWnd->set_visible(true);

   if (mWnd->grab_focus()) {
      g_debug("%s: grab_focus successfully\n", __FUNCTION__);
   } else {
      Warning("%s: grab_focus failed! May unable to detect drag actions\n", __FUNCTION__);
   }

   m_isVisible = true;
   // Add a delay to let dnd warming up
   usleep(300);

   if (mWnd->get_focus())
   {
      g_debug("%s: detwnd have focus on %s\n", __FUNCTION__, G_OBJECT_TYPE_NAME(mWnd->get_focus()->gobj()));
   }
   else
      g_debug("%s: detwnd doesn't have focus\n", __FUNCTION__);

   g_debug("%s: exiting \n", __FUNCTION__);
}


/**
 * Hide the window.
 */

void
DragDetWnd::Hide(void)
{
   g_debug("%s: enter\n", __FUNCTION__);
   g_debug("%s: detwnd current visible is  %d\n", __FUNCTION__, mWnd->is_visible());
   g_debug("%s: detwnd current focus is %d", __FUNCTION__, gtk_widget_has_focus(GTK_WIDGET(mWnd->gobj())));

   mWnd->set_sensitive(false);
   mWnd->set_visible(false);
   m_isVisible = false;
   g_debug("%s: exiting \n", __FUNCTION__);
}


/**
 * Lower the the GTK window by set lower its surface
 */

static bool lower_wnd(Gtk::Window *window)
{
   GtkWidget* widget = GTK_WIDGET(window->gobj());
   GtkNative* native = gtk_widget_get_native(widget);

   if (native) {
      GdkSurface* surface = gtk_native_get_surface(native);
      if (surface && GDK_IS_TOPLEVEL(surface)) {
         gdk_toplevel_lower(GDK_TOPLEVEL(surface));
         return true;
      }
   }

   return false;
}


/**
 * Get the monitor of the GTK window
 */

static Glib::RefPtr<Gdk::Monitor> get_monitor(Gtk::Window *window)
{
   GtkWidget* widget = GTK_WIDGET(window->gobj());
   GtkNative* native = gtk_widget_get_native(widget);

   if (native) {
      GdkSurface* surface = gtk_native_get_surface(native);
      g_object_ref(surface);
      Glib::RefPtr<Gdk::Surface> share_surface(Glib::wrap(surface));
      if (surface)
         return Gdk::Display::get_default()->get_monitor_at_surface(share_surface);
   }
   return NULL;
}


/**
 * Raise the window.
 */

void
DragDetWnd::Raise(void)
{
   mWnd->present();
   g_debug("%s: detwnd Presenting\n", __FUNCTION__);
   Flush();
}


/**
 * Lower the window.
 */

void
DragDetWnd::Lower(void)
{
   lower_wnd(mWnd);
   Flush();
}


/**
 *
 * Get the width of the screen associated with this window.
 *
 * @return width of screen, in pixels.
 */

int
DragDetWnd::GetScreenWidth(void)
{
   Gdk::Rectangle geometry;
   Glib::RefPtr<Gdk::Monitor> monitor = get_monitor(mWnd);
   monitor->get_geometry(geometry);
   return geometry.get_width();
}


/**
 *
 * Get the height of the screen associated with this window.
 *
 * @return height of screen, in pixels.
 */

int
DragDetWnd::GetScreenHeight(void)
{
   Gdk::Rectangle geometry;
   Glib::RefPtr<Gdk::Monitor> monitor = get_monitor(mWnd);
   monitor->get_geometry(geometry);
   return geometry.get_height();
}


/**
 *
 * Set the geometry of the window.
 *
 * @param[in] x desired x-coordinate of the window.
 * @param[in] y desired y-coordinate of the window.
 * @param[in] width desired width of the window.
 * @param[in] height desired height of the window.
 */

void
DragDetWnd::SetGeometry(const int x,
                        const int y,
                        const int width,
                        const int height)
{
   GtkWidget* widget = GetWnd()->gobj();
   GtkNative* native = gtk_widget_get_native(widget);
   Gtk::Window* gdkwin = dynamic_cast<Gtk::Window*>(GetWnd());
   GdkSurface* surface = gtk_native_get_surface(native);
   g_debug("%s: set Geo at (%d, %d, %d, %d)\n", __FUNCTION__, x, y, width, height);

   if (gdkwin && surface) {
      gdkwin->set_default_size(width, height);
      if (GDK_IS_X11_SURFACE(surface)) {
         Display* display = gdk_x11_display_get_xdisplay(gdk_surface_get_display(surface));
         Window xwindow = gdk_x11_surface_get_xid(surface);
         XMoveWindow(display, xwindow, x, y);
         XFlush(display);
      }
      Flush();
   }
}


/**
 *
 * Get the current geometry of the window.
 *
 * @param[out] x current x-coordinate of the window.
 * @param[out] y current y-coordinate of the window.
 * @param[out] width current width of the window.
 * @param[out] height current height of the window.
 *
 * @note The current geometry may be inaccurate if retrieved too quickly
 * after a change made by SetGeometry(). This is due to the realities of
 * X and window managers. Some of this is mitigated by the use of flush()
 * and sync() calls in SetGeometry(), but these are no guarantee.
 */

void
DragDetWnd::GetGeometry(int &x, int &y, int &width, int &height)
{
   GtkWidget* widget = GetWnd()->gobj();
   GtkNative* native = gtk_widget_get_native(widget);
   GdkSurface* surface = gtk_native_get_surface(native);

   width = gdk_surface_get_width(surface);
   height = gdk_surface_get_height(surface);

   if (GDK_IS_X11_SURFACE(surface)) {
      Display* display = gdk_x11_display_get_xdisplay(gdk_surface_get_display(surface));
      Window xwindow = gdk_x11_surface_get_xid(GDK_X11_SURFACE(surface));

      Window root, child;
      int dx, dy;
      unsigned int w, h, bw, depth;

      if (XGetGeometry(display, xwindow, &root, &dx, &dy, &w, &h, &bw, &depth)) {
         XTranslateCoordinates(display, xwindow, root, 0, 0, &x, &y, &child);
      }
   }
}


/**
 *
 *  Show or hide detect window and move it to the positon.
 *
 */

void
DragDetWnd::UpdateDetWnd(bool show, int32 x, int32 y)
{
  g_debug("%s: enter 0x%lx show %d x %d y %d\n",
         __FUNCTION__,
         (unsigned long) Get_Current_Drop_ctx(), show, x, y);

   /* If the window is being shown, move it to the right place. */
   if (show) {
      Show();
      Raise();
      SetGeometry(x, y, DRAG_DET_WINDOW_WIDTH * 2, DRAG_DET_WINDOW_WIDTH * 2);
      g_debug("%s: show at (%d, %d, %d, %d)\n", __FUNCTION__, x, y, DRAG_DET_WINDOW_WIDTH * 2, DRAG_DET_WINDOW_WIDTH * 2);
      /*
       * Wiggle the mouse here. Especially for G->H DnD, this improves
       * reliability of making the drag escape the guest window immensly.
       * Stolen from the legacy V2 DnD code.
       */

      SimulateMouseMove(x + 2, y + 2);
      SetIsVisible(true);
   } else {
      g_debug("%s: hide\n", __FUNCTION__);
      Hide();
      SetIsVisible(false);
   }
}


/**
 *  Start drag from drag source with current coordinate.
 */

void
DragDetWnd::StartDrag(Glib::RefPtr<Gdk::ContentProvider> provider,
                      int mouseX,
                      int mouseY)
{
   g_debug("%s: start drag from offset (%d, %d)\n", __FUNCTION__,
           mouseX, mouseY);
   mSource->set_content(provider);
}


/**
 *  adjust the current x,y coordinate slightly.
 */

static
bool adjust_coord(int *x, int *y, int width, int height)
{
   bool change = false;
   /*
    * first do left and top edges.
    */
   if (*x <= 5){
      *x = 6;
      change = true;
   }

   if (*y <= 5){
      *y = 6;
      change = true;
   }

   /*
    * next, move result away from right and bottom edges.
    */
   if (*x > width - 5){
      *x = width - 6;
      change = true;
   }

   if (*y > height - 5){
      *y = height - 6;
      change = true;
   }
   return change;
}


/*
 *-----------------------------------------------------------------------------
 *
 * TryXTestFakeDeviceButtonEvent --
 *
 *      Fake X mouse events in device level.
 *
 *      XXX The function will only be called if XTestFakeButtonEvent does
 *      not work for mouse button release. Later on we may only call this
 *      one for mouse button simulation if this is more reliable.
 *
 * Results:
 *      Returns true on success, false on failure.
 *
 * Side effects:
 *      Generates mouse events.
 *
 *-----------------------------------------------------------------------------
 */

static
bool TryXTestSimulateDeviceButtonEvent(GdkSurface* surface)
{
   XDeviceInfo *list = NULL;
   XDeviceInfo *list2 = NULL;
   XDevice *tdev = NULL;
   XDevice *buttonDevice = NULL;
   int numDevices = 0;
   int i = 0;
   int j = 0;
   XInputClassInfo *ip = NULL;
   Display *dndXDisplay;

   if (!surface) {
      g_debug("%s: unable to get widget\n", __FUNCTION__);
      return false;
   }

   dndXDisplay = GDK_SURFACE_XDISPLAY(surface);

   /* First get list of all input device. */
   if (!(list = XListInputDevices (dndXDisplay, &numDevices))) {
      g_debug("%s: XListInputDevices failed\n", __FUNCTION__);
      return false;
   } else {
      g_debug("%s: XListInputDevices got %d devices\n", __FUNCTION__, numDevices);
   }

   list2 = list;

   for (i = 0; i < numDevices; i++, list++) {
      /* We only care about mouse device. */
      if (list->use != IsXExtensionPointer) {
         continue;
      }

      tdev = XOpenDevice(dndXDisplay, list->id);
      if (!tdev) {
         g_debug("%s: XOpenDevice failed\n", __FUNCTION__);
         continue;
      }

      for (ip = tdev->classes, j = 0; j < tdev->num_classes; j++, ip++) {
         if (ip->input_class == ButtonClass) {
            buttonDevice = tdev;
            break;
         }
      }

      if (buttonDevice) {
         g_debug("%s: calling XTestFakeDeviceButtonEvent for %s\n",
               __FUNCTION__, list->name);
         XTestFakeDeviceButtonEvent(dndXDisplay, buttonDevice, 1, False,
                                    NULL, 0, CurrentTime);
         buttonDevice = NULL;
      }
      XCloseDevice(dndXDisplay, tdev);
   }
   XFreeDeviceList(list2);
   return true;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DragDetWnd::SimulateXEvents --
 *
 *      Fake X mouse events and window movement for the detection window.
 *
 *      This function shows the detection window and generates button
 *      press/release and pointer motion events.
 *
 *      XXX This code should be implemented using GDK APIs.
 *          (gdk_display_warp_pointer?)
 *
 *
 * Results:
 *      Returns true if generated X events, false on failure.
 *
 * Side effects:
 *      A ton of things.
 *
 *-----------------------------------------------------------------------------
 */

bool
DragDetWnd::SimulateXEvents(const bool showWidget, const bool buttonEvent,
                            const bool buttonPress, const bool moveWindow,
                            const bool coordsProvided,
                            const int xCoord, const int yCoord)
{
   Display *dndXDisplay;
   Window rootWnd;
   Window dndXWindow;
   Window rootReturn;
   Window childReturn;
   GdkSurface* surface;

   bool ret = false;
   int x, y;
   int rootXReturn, rootYReturn;
   int winXReturn, winYReturn;
   unsigned int maskReturn;

   TRACE_CALL();

   x = xCoord;
   y = yCoord;

   surface = gtk_native_get_surface(mWnd->get_native()->gobj());
   dndXDisplay = GDK_SURFACE_XDISPLAY(surface);
   dndXWindow = GDK_SURFACE_XID(surface);
   rootWnd = RootWindow(dndXDisplay, DefaultScreen(dndXDisplay));

   /*
    * Turn on X synchronization in order to ensure that our X events occur in
    * the order called.  In particular, we want the window movement to occur
    * before the mouse movement so that the events we are coercing do in fact
    * happen.
    */
   XSynchronize(dndXDisplay, True);

   if (showWidget) {
      g_debug("%s: showing Gtk widget\n", __FUNCTION__);
      gtk_widget_set_visible(GTK_WIDGET(mWnd->gobj()), true);
      gtk_window_present(mWnd->gobj());

   }

   /* Get the current location of the mouse if coordinates weren't provided. */
   if (!coordsProvided) {
      if (!XQueryPointer(dndXDisplay, rootWnd, &rootReturn, &childReturn,
                          &rootXReturn, &rootYReturn, &winXReturn, &winYReturn,
                          &maskReturn)) {
         Warning("%s: XQueryPointer() returned False.\n", __FUNCTION__);
         goto exit;
      }

      g_debug("%s: current mouse is at (%d, %d)\n", __FUNCTION__,
            rootXReturn, rootYReturn);

      /*
       * Position away from the edge of the window.
       */
      int width = GetScreenWidth();
      int height = GetScreenHeight();

      x = rootXReturn;
      y = rootYReturn;

      if (adjust_coord(&x, &y, width, height)) {
         g_debug("%s: adjusting mouse position. root %d, %d, adjusted %d, %d\n",
               __FUNCTION__, rootXReturn, rootYReturn, x, y);
      }
   }

   if (moveWindow) {
      /*
       * Make sure the window is at this point and at the top (raised).  The
       * window is resized to be a bit larger than we would like to increase
       * the likelihood that mouse events are attributed to our window -- this
       * is okay since the window is invisible and hidden on cancels and DnD
       * finish.
       */
      XMoveResizeWindow(dndXDisplay,
                        dndXWindow,
                        x - DRAG_DET_WINDOW_WIDTH / 2 ,
                        y - DRAG_DET_WINDOW_WIDTH / 2,
                        DRAG_DET_WINDOW_WIDTH,
                        DRAG_DET_WINDOW_WIDTH);
      XRaiseWindow(dndXDisplay, dndXWindow);
      g_debug("%s: move wnd to (%d, %d, %d, %d)\n",
              __FUNCTION__,
              x - DRAG_DET_WINDOW_WIDTH / 2 ,
              y - DRAG_DET_WINDOW_WIDTH / 2,
              DRAG_DET_WINDOW_WIDTH,
              DRAG_DET_WINDOW_WIDTH);
   }

   /*
    * Generate mouse movements over the window.  The second one makes ungrabs
    * happen more reliably on KDE, but isn't necessary on GNOME.
    */
   if (mUseUInput) {
#ifdef USE_UINPUT
       FakeMouse_Move(x, y);
       FakeMouse_Move(x + 1, y + 1);
       g_debug("%s: Uinput simulating moving mouse\n", __FUNCTION__);
#endif
   } else {
       XTestFakeMotionEvent(dndXDisplay, -1, x, y, CurrentTime);
       XTestFakeMotionEvent(dndXDisplay, -1, x + 1, y + 1, CurrentTime);
   }

   g_debug("%s: move mouse to (%d, %d) and (%d, %d)\n", __FUNCTION__, x, y, x + 1, y + 1);

   if (buttonEvent)
   {
      g_debug("%s: faking left mouse button %s\n", __FUNCTION__,
              buttonPress ? "press" : "release");
      if (mUseUInput) {
#ifdef USE_UINPUT
         ret = FakeMouse_Click(buttonPress);
         g_debug("%s: Uinput simulating click mouse with ret %d\n", __FUNCTION__, ret);
#endif
      }
      else {
         XTestFakeButtonEvent(dndXDisplay, 1, buttonPress, CurrentTime);
         XSync(dndXDisplay, False);

         if (!buttonPress) {
            /*
             * The button release simulation may be failed with some distributions
             * like Ubuntu 10.4 and RHEL 6 for guest->host DnD. So first query
             * mouse button status. If some button is still down, we will try
             * mouse device level event simulation. For details please refer
             * to bug 552807.
             */
            if (!XQueryPointer(dndXDisplay, rootWnd, &rootReturn, &childReturn,
                               &rootXReturn, &rootYReturn, &winXReturn,
                               &winYReturn, &maskReturn)) {
               Warning("%s: XQueryPointer returned False.\n", __FUNCTION__);
               goto exit;
            }

            if ((maskReturn & Button1Mask)
                 || (maskReturn & Button2Mask)
                 || (maskReturn & Button3Mask)
                 || (maskReturn & Button4Mask)
                 || (maskReturn & Button5Mask)) {
               Debug("%s: XTestFakeButtonEvent was not working for button "
                     "release, trying XTestFakeDeviceButtonEvent now.\n",
                     __FUNCTION__);
               ret = TryXTestSimulateDeviceButtonEvent(surface);
            }
            else {
               g_debug("%s: XTestFakeButtonEvent was working for button release.\n",
                       __FUNCTION__);
               ret = true;
            }
         } else {
            ret = true;
         }
      }
   }

exit:
   XSynchronize(dndXDisplay, False);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DragDetWnd::SimulateMouseMove --
 *
 *      Issue a fake mouse move event to the detection window.
 *
 * Results:
 *      Returns true on success, false on failure.
 *
 * Side effects:
 *      Generates mouse events.
 *
 *-----------------------------------------------------------------------------
 */

bool
DragDetWnd::SimulateMouseMove(const int x,        // IN: x-coord
                              const int y)        // IN: y-coord
{
   return SimulateXEvents(false, false, false, false, true, x, y);
}
