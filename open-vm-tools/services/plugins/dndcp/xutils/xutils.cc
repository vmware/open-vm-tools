/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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


#include <cairomm/cairomm.h>
#include <gdkmm.h>

#include "xutils/xutils.hh"

#if GTK_MAJOR_VERSION == 3
#include <gdkmm/devicemanager.h>
#endif

/* These must be after the gtkmm includes, as gtkmm is quite picky. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <cstring>

#include "vm_assert.h"


namespace xutils {


/* Actual definitions of the signals in this class. */
sigc::signal<void, Glib::RefPtr<Gdk::Screen> > currentDesktopChanged;
sigc::signal<void, Glib::RefPtr<Gdk::Screen> > desktopLayoutChanged;
sigc::signal<void, Glib::RefPtr<Gdk::Screen> > desktopGeometryChanged;
sigc::signal<void, Glib::RefPtr<Gdk::Screen> > desktopViewportChanged;
sigc::signal<void, Glib::RefPtr<Gdk::Screen> > windowStackChanged;
sigc::signal<void, Glib::RefPtr<Gdk::Screen> > windowManagerChanged;
sigc::signal<void, Glib::RefPtr<Gdk::Screen> > activeWindowChanged;
sigc::signal<void, Glib::RefPtr<Gdk::Screen> > workAreaChanged;

/* Necessary for calculating per-monitor _NET_WORKAREA in GetMonitorWorkArea() */
struct NETWMStrutPartial {
   NETWMStrutPartial()
      : left_width(0),
        left_start(0),
        left_end(0),
        right_width(0),
        right_start(0),
        right_end(0),
        top_height(0),
        top_start(0),
        top_end(0),
        bottom_height(0),
        bottom_start(0),
        bottom_end(0) {}

   int left_width, left_start, left_end;
   int right_width, right_start, right_end;
   int top_height, top_start, top_end;
   int bottom_height, bottom_start, bottom_end;
};


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::OnWindowFilter --
 *
 *      Window filter handler that listens for changes to the properties we
 *      care about and emits the appropriate signals.
 *
 * Results:
 *      GDK_FILTER_CONTINUE, always.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GdkFilterReturn
OnWindowFilter(XEvent* xevent,     // IN: Incoming event
               GdkEvent* event,    // OUT/UNUSED
               GdkScreen* _screen) // IN: The screen
{
   Glib::RefPtr<Gdk::Screen> screen = Glib::wrap(_screen, true);
   ::Display* xdisplay = xevent->xany.display;
   GdkDisplay* display = gdk_x11_lookup_xdisplay(xdisplay);
   Window rootWin = GDK_WINDOW_XID(screen->get_root_window()->gobj());

#define ATOM(name) gdk_x11_get_xatom_by_name_for_display(display, (name))

   if (xevent->type == PropertyNotify &&
       xevent->xproperty.window == rootWin) {
      if (xevent->xproperty.atom == ATOM("_NET_CLIENT_LIST_STACKING")) {
         windowStackChanged.emit(screen);
      } else if (xevent->xproperty.atom == ATOM("_NET_DESKTOP_LAYOUT")) {
         desktopLayoutChanged.emit(screen);
      } else if (xevent->xproperty.atom == ATOM("_NET_NUMBER_OF_DESKTOPS")) {
         desktopLayoutChanged.emit(screen);
      } else if (xevent->xproperty.atom == ATOM("_NET_CURRENT_DESKTOP")) {
         currentDesktopChanged.emit(screen);
      } else if (xevent->xproperty.atom == ATOM("_NET_DESKTOP_GEOMETRY")) {
         desktopGeometryChanged.emit(screen);
      } else if (xevent->xproperty.atom == ATOM("_NET_DESKTOP_VIEWPORT")) {
         desktopViewportChanged.emit(screen);
      } else if (xevent->xproperty.atom == ATOM("_NET_SUPPORTING_WM_CHECK")) {
         windowManagerChanged.emit(screen);
      } else if (xevent->xproperty.atom == ATOM("_NET_ACTIVE_WINDOW")) {
         activeWindowChanged.emit(screen);
      } else if (xevent->xproperty.atom == ATOM("_NET_WORKAREA")) {
         workAreaChanged.emit(screen);
      }
   }

#undef ATOM

   return GDK_FILTER_CONTINUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::Init --
 *
 *      Base initialization function that sets up the window filter. This
 *      is required if any signals are to be used.
 *
 *      This can be called more than once.
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
Init()
{
   static bool initialized = false;

   if (!initialized) {
      initialized = true;

      /*
       * Select PropertyChange events on the root window so that we can
       * listen for when the host window stack changes and update our
       * copy.
       */
      Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
      ::Display* xdisplay = GDK_DISPLAY_XDISPLAY(display->gobj());

      for (int i = 0; i < display->get_n_screens(); i++) {
         Glib::RefPtr<Gdk::Screen> screen = display->get_screen(i);
         Glib::RefPtr<Gdk::Window> rootWin = screen->get_root_window();
         Window xRootWin = GDK_WINDOW_XID(rootWin->gobj());

         int mask = PropertyChangeMask;

#if GTK_MAJOR_VERSION == 3
         if (gdk_x11_window_lookup_for_display(
                display->gobj(), xRootWin) != NULL) {
#else
         if (gdk_xid_table_lookup(xRootWin) != NULL) {
#endif
            /* Make sure we don't interfere with GDK. */
            XWindowAttributes attrs;
            XGetWindowAttributes(xdisplay, xRootWin, &attrs);
            mask |= attrs.your_event_mask;
         }

         XSelectInput(xdisplay, xRootWin, mask);

         gdk_window_add_filter(rootWin->gobj(), (GdkFilterFunc)OnWindowFilter,
                               screen->gobj());
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetCardinal --
 *
 *      Utility function to get a cardinal from a window property.
 *
 * Results:
 *      true if the function succeeded, along with a value for retValue.
 *      Otherwise false.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
GetCardinal(Glib::RefPtr<const Gdk::Window> window, // IN: Window
            const utf::string& atomName,            // IN: Atom name
            unsigned long& retValue)                        // OUT: Return value
{
   ASSERT(window);
   ASSERT(!atomName.empty());

   std::vector<unsigned long> retValues;
   bool result = GetCardinalList(window, atomName, retValues);

   if (result && retValues.size() == 1) {
      retValue = retValues[0];
      return true;
   }

   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetCardinalList --
 *
 *      Utility function to get a cardinal list from a window property.
 *
 * Results:
 *      true if the function succeeded, along with return values for retValue.
 *      Otherwise false.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
GetCardinalList(Glib::RefPtr<const Gdk::Window> window, // IN: Window
                const utf::string& atomName,            // IN: Atom name
                std::vector<unsigned long>& retValues)         // IN: Return values
{
   ASSERT(window);
   ASSERT(window->get_display());
   ASSERT(!atomName.empty());

   GdkDisplay* display = const_cast<GdkDisplay*>(window->get_display()->gobj());
   GdkWindow* gdkwin = const_cast<GdkWindow*>(window->gobj());

   Atom atom = gdk_x11_get_xatom_by_name_for_display(display, atomName.c_str());

   Atom type;
   int format;
   unsigned long nitems;
   unsigned long bytes_after;
   uint8* values;

   gdk_error_trap_push();
   int ret = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display),
                                GDK_WINDOW_XID(gdkwin),
                                atom, 0, G_MAXLONG, False, XA_CARDINAL, &type,
                                &format, &nitems, &bytes_after, &values);
   int err = gdk_error_trap_pop();

   if (ret == Success && !err) {
      if (type == XA_CARDINAL && nitems > 0) {
         retValues.resize(nitems);

         if (format == 8) {
            for (unsigned long i = 0; i < nitems; i++) {
               retValues[i] = values[i];
            }
         } else if (format == 16) {
            uint16* shortValues = (uint16*)values;
            for (unsigned long i = 0; i < nitems; i++) {
               retValues[i] = shortValues[i];
            }
         } else if (format == 32) {
            unsigned long* longValues = (unsigned long*)values;
            for (unsigned long i = 0; i < nitems; i++) {
               retValues[i] = longValues[i];
            }
         } else {
            NOT_IMPLEMENTED();
         }

         XFree(values);
         return true;
      }

      XFree(values);
   }

   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::SetDesktopForWindow --
 *
 *      Sets the virtual desktop that a window is on. This takes care of
 *      the workspace part of the desktop. Viewports must be handled
 *      separately by moving the window.
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
SetDesktopForWindow(Glib::RefPtr<Gdk::Window> window, // IN:
                    uint32 desktop)                   // IN:
{
   GdkScreen* screen = window->get_screen()->gobj();
   Atom tempDesktop = desktop; // Cast for 64-bit correctness.
   Window win = GDK_WINDOW_XID(window->gobj());
   Display* display = GDK_WINDOW_XDISPLAY(window->gobj());

   Atom atom = gdk_x11_get_xatom_by_name_for_display(
      window->get_display()->gobj(), "_NET_WM_DESKTOP");

   gdk_error_trap_push();
   XChangeProperty(display, win, atom,
                   XA_CARDINAL, 32, PropModeReplace,
                   (unsigned char*)&tempDesktop, 1);
   gdk_flush();
   int err = gdk_error_trap_pop();

   if (err) {
      Warning("Unable to move host window (XID %d) to desktop %d\n",
          (int)GDK_WINDOW_XID(window->gobj()), desktop);
   }

   XEvent ev;
   ev.xclient.type = ClientMessage;
   ev.xclient.serial = 0;
   ev.xclient.send_event = True;
   ev.xclient.window = win;
   ev.xclient.message_type = atom;
   ev.xclient.format = 32;
   ev.xclient.data.l[0] = desktop;
   ev.xclient.data.l[1] = 2; // source (2 gives full control)
   ev.xclient.data.l[2] = 0; // unused
   ev.xclient.data.l[3] = 0; // unused
   ev.xclient.data.l[4] = 0; // unused

   gdk_error_trap_push();
   XSendEvent(display,
              GDK_WINDOW_XID(gdk_screen_get_root_window(screen)),
              False, SubstructureRedirectMask | SubstructureNotifyMask,
              &ev);
   gdk_flush();
   err = gdk_error_trap_pop();

   if (err) {
      Warning("Unable to move host window (XID %d) to desktop %d\n",
          (int)GDK_WINDOW_XID(window->gobj()), desktop);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::SetFullscreenMonitorsHint --
 *
 *      Sets the _NET_WM_FULLSCREEN_MONITORS hint for the passed in window and
 *      monitor indices.
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
SetFullscreenMonitorsHint(Glib::RefPtr<Gdk::Window> window, // IN:
                          std::vector<long> monitors)       // IN:
{
   // monitors contains 4 monitor indices, per EWMH spec
   ASSERT(monitors.size() == 4);

   Display* display = GDK_WINDOW_XDISPLAY(window->gobj());

   XClientMessageEvent xclient;
   memset(&xclient, 0, sizeof xclient);
   xclient.type = ClientMessage;
   xclient.window = GDK_WINDOW_XID(window->gobj());
   xclient.message_type = XInternAtom(display,
                                      "_NET_WM_FULLSCREEN_MONITORS",
                                      False);
   xclient.format = 32;

   for (int i = 0; i < 4; i++) {
      xclient.data.l[i] = monitors[i];
   }

   xclient.data.l[4] = 1;

   XSendEvent(display,
              GDK_WINDOW_XID(gdk_get_default_root_window()),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *) &xclient);

   XSync(display, False);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetDesktopForWindow --
 *
 *      Retrieve the virtual desktop that a given window is shown on.
 *
 * Results:
 *      The index of the virtual desktop.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

uint32
GetDesktopForWindow(Glib::RefPtr<const Gdk::Window> window) // IN:
{
   unsigned long result = 0;
   GetCardinal(window, "_NET_WM_DESKTOP", result);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetNumDesktops --
 *
 *      Returns the number of virtual desktops.
 *
 * Results:
 *      The number of virtual desktops.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

uint32
GetNumDesktops(Glib::RefPtr<Gdk::Screen> screen) // IN:
{
   unsigned long result = 0;
   GetCardinal(screen->get_root_window(), "_NET_NUMBER_OF_DESKTOPS", result);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetCurrentDesktop --
 *
 *      Retrieve the current virtual desktop for the screen.
 *
 * Results:
 *      The index of the virtual desktop.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

uint32
GetCurrentDesktop(Glib::RefPtr<Gdk::Screen> screen) // IN
{
   unsigned long result = 0;
   GetCardinal(screen->get_root_window(), "_NET_CURRENT_DESKTOP", result);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetDesktopLayout --
 *
 *      Retrieves the current virtual desktop layout for the screen.
 *
 * Results:
 *      The virtual desktop layout information is set and the function
 *      returns true if successful. Otherwise false is returned.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
GetDesktopLayout(Glib::RefPtr<Gdk::Screen> screen, // IN: Screen
                 uint32& rows,                     // OUT: Rows
                 uint32& columns,                  // OUT: Columns
                 Gtk::CornerType& corner,          // OUT: Corner of the first
                                                   //      desktop.
                 Gtk::Orientation& orientation)    // OUT: Desktop orientation
{
   std::vector<unsigned long> values;

   if (GetCardinalList(screen->get_root_window(),
                       "_NET_DESKTOP_LAYOUT", values)) {
      switch (values[0]) {
      case 0:
         orientation = Gtk::ORIENTATION_HORIZONTAL;
         break;

      case 1:
         orientation = Gtk::ORIENTATION_VERTICAL;
         break;

      default:
         Warning("Unsupported orientation in _NET_DESKTOP_LAYOUT\n");
         return false;
      }

      columns = static_cast<uint32>(values[1]);
      rows = static_cast<uint32>(values[2]);

      if (columns == 0 && rows == 0) {
         Warning("Invalid desktop configuration in _NET_DESKTOP_LAYOUT. "
                 "Rows and columns are both 0!\n");
         return false;
      } else if (columns == 0 || rows == 0) {
         uint32 numDesktops = GetNumDesktops(screen);

         if (columns == 0) {
            columns = numDesktops / rows +
                      ((numDesktops % rows) > 0 ? 1 : 0);
         } else if (rows == 0) {
            rows = numDesktops / columns +
                   ((numDesktops % columns) > 0 ? 1 : 0);
         }
      }

      corner = Gtk::CORNER_TOP_LEFT;

      if (values.size() == 4) {
         switch (values[3]) {
         case 0:
            corner = Gtk::CORNER_TOP_LEFT;
            break;

         case 1:
            corner = Gtk::CORNER_TOP_RIGHT;
            break;

         case 2:
            corner = Gtk::CORNER_BOTTOM_RIGHT;
            break;

         case 3:
            corner = Gtk::CORNER_BOTTOM_LEFT;
            break;

         default:
            Warning("Unsupported corner in _NET_DESKTOP_LAYOUT\n");
            return false;
         }
      }

      return true;
   }

   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetDesktopGeometry --
 *
 *      Retrieves the desktop geometry for this screen.
 *
 * Results:
 *      The desktop geometry is set and the function returns true if
 *      successful. Otherwise false is returned.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
GetDesktopGeometry(Glib::RefPtr<Gdk::Screen> screen, // IN: The screen
                   uint32& width,                    // OUT: Width
                   uint32& height)                   // OUT: Height
{
   std::vector<unsigned long> values;

   if (GetCardinalList(screen->get_root_window(),
                       "_NET_DESKTOP_GEOMETRY", values)) {
      if (values.size() == 2) {
         width = values[0];
         height = values[1];
         return true;
      }
   }

   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetDesktopViewport --
 *
 *      Retrieves the viewport of the specified virtual desktop.
 *
 * Results:
 *      The viewport is set and the function returns true if successful.
 *      Otherwise false is returned.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
GetDesktopViewport(Glib::RefPtr<Gdk::Screen> screen, // IN: The screen
                   uint32 desktopIndex,              // IN: Desktop index
                   VMPoint& viewport)                // OUT: Viewport
{
   std::vector<unsigned long> values;

   if (GetCardinalList(screen->get_root_window(),
                       "_NET_DESKTOP_VIEWPORT", values)) {
      uint32 numDesktops = GetNumDesktops(screen);

      if (values.size() == numDesktops * 2) {
         viewport.x = values[desktopIndex * 2];
         viewport.y = values[desktopIndex * 2 + 1];
         return true;
      }
   }

   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::RaiseWindowInternal --
 *
 *      Internal function to handle the restack operation.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
RaiseWindowInternal(Glib::RefPtr<Gdk::Window> window,  // IN: Window to raise
                    Glib::RefPtr<Gdk::Window> sibling, // IN: The sibling
                    guint32 timestamp)                 // IN: Event timestamp
{
   GdkScreen* screen = window->get_screen()->gobj();

   if (gdk_x11_screen_supports_net_wm_hint(screen,
         gdk_atom_intern_static_string("_NET_RESTACK_WINDOW"))) {
      XEvent ev;
      ev.xclient.type = ClientMessage;
      ev.xclient.serial = 0;
      ev.xclient.send_event = True;
      ev.xclient.window = GDK_WINDOW_XID(window->gobj());
      ev.xclient.message_type =
         gdk_x11_get_xatom_by_name_for_display(window->get_display()->gobj(),
                                               "_NET_RESTACK_WINDOW");
      ev.xclient.format = 32;
      ev.xclient.data.l[0] = 2;        // source (2 gives full control)
      ev.xclient.data.l[1] = (sibling
                              ? GDK_WINDOW_XID(sibling->gobj())
                              : None); // sibling
      ev.xclient.data.l[2] = Above;    // direction
      ev.xclient.data.l[3] = 0;        // unused
      ev.xclient.data.l[4] = 0;        // unused

      XSendEvent(GDK_WINDOW_XDISPLAY(window->gobj()),
                 GDK_WINDOW_XID(gdk_screen_get_root_window(screen)),
                 False, SubstructureRedirectMask | SubstructureNotifyMask,
                 &ev);
   } else {
      /*
       * As of writing (2011-03-08), Metacity doesn't support
       * _NET_RESTACK_WINDOW and will block our attempt to raise a window unless
       * it's active, so we activate the window first.
       */
      if (gdk_x11_screen_supports_net_wm_hint(
             screen,
             gdk_atom_intern_static_string("_NET_ACTIVE_WINDOW"))) {

         XClientMessageEvent xclient;
         memset (&xclient, 0, sizeof (xclient));
         xclient.type = ClientMessage;
         xclient.window = GDK_WINDOW_XID(window->gobj());
         xclient.message_type =
            gdk_x11_get_xatom_by_name_for_display(window->get_display()->gobj(),
                                                  "_NET_ACTIVE_WINDOW");
         xclient.format = 32;
         xclient.data.l[0] = 2; // source (2 gives full control)
         xclient.data.l[1] = timestamp;
         xclient.data.l[2] = None; // currently active window
         xclient.data.l[3] = 0;
         xclient.data.l[4] = 0;

         XSendEvent(GDK_WINDOW_XDISPLAY(window->gobj()),
                    GDK_WINDOW_XID(gdk_screen_get_root_window(screen)),
                    False, SubstructureRedirectMask | SubstructureNotifyMask,
                    (XEvent*)&xclient);
      }

      int flags = CWStackMode;
      XWindowChanges changes;
      changes.stack_mode = Above;

      if (sibling) {
         changes.sibling = GDK_WINDOW_XID(sibling->gobj());
         flags |= CWSibling;
      }

      XReconfigureWMWindow(GDK_WINDOW_XDISPLAY(window->gobj()),
                           GDK_WINDOW_XID(window->gobj()),
                           DefaultScreen(GDK_WINDOW_XDISPLAY(window->gobj())),
                           flags, &changes);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::RaiseWindow --
 *
 *      Raises a window to the top of the window stack. This version
 *      accepts a timestamp instead of fetching it, useful when being
 *      called from an event handler or when using a common timestamp.
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
RaiseWindow(Glib::RefPtr<Gdk::Window> window,  // IN: Window to raise
            Glib::RefPtr<Gdk::Window> sibling, // IN/OPT: The sibling
            guint32 timestamp)                 // IN/OPT: Event timestamp
{
   /*
    * Fake an input event timestamp so that the window manager
    * will allow a restacking of this window.
    */
   gdk_x11_window_set_user_time(window->gobj(),
      timestamp == 0
      ?  gdk_x11_display_get_user_time(gdk_display_get_default())
      : timestamp);

   gdk_error_trap_push();
   RaiseWindowInternal(window, sibling, timestamp);
   gdk_flush();
   int err = gdk_error_trap_pop();

   if (err && sibling) {
      /*
       * This could be due to sibling not being a sibling window.
       * Apparently, this is possible in our case. Ignore the "sibling."
       */
      gdk_error_trap_push();
      RaiseWindowInternal(window, Glib::RefPtr<Gdk::Window>(), timestamp);
      err = gdk_error_trap_pop();
   }

   if (err) {
      /* We still have an error. Log it and continue on. */
      Glib::ustring method;

      if (gdk_x11_screen_supports_net_wm_hint(
            window->get_screen()->gobj(),
            gdk_atom_intern_static_string("_NET_RESTACK_WINDOW"))) {
         method = "_NET_RESTACK_WINDOW";
      } else {
         method = "XReconfigureWMWindow";
      }

      if (sibling) {
         Log("Unable to raise window (XID %d) over sibling (XID %d) using %s. "
             "Error code = %d\n",
             (int)GDK_WINDOW_XID(window->gobj()),
             (int)GDK_WINDOW_XID(sibling->gobj()), method.c_str(), err);
      } else {
         Log("Unable to raise window (XID %d) using %s. Error code = %d\n",
             (int)GDK_WINDOW_XID(window->gobj()), method.c_str(), err);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetHostWindowStack --
 *
 *      Returns the window stack as recorded by the window manager.
 *      This is the equivalent of gdk_screen_get_window_stack, except
 *      that function is broken on 64-bit platforms, so for the time
 *      being we need to provide our own.
 *
 * Results:
 *      The host window stack.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

HostWindowList
GetHostWindowStack()
{
   HostWindowList windows;
   GdkScreen* screen = gdk_screen_get_default();

   if (!gdk_x11_screen_supports_net_wm_hint(screen,
          gdk_atom_intern_static_string("_NET_CLIENT_LIST_STACKING"))) {
      /*
       * This is bad. We don't really have an alternative. We might want to
       * just disable Unity.
       */
      return windows;
   }

   GdkDisplay* display = gdk_display_get_default();
   unsigned long numItems = 0;
   unsigned long bytesAfter = 0;
   gint format = 0;
   Atom type = 0;
   guchar* data = NULL;

   GdkWindow* rootWin = gdk_screen_get_root_window(screen);

   gdk_error_trap_push();
   int ret = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display),
                                GDK_WINDOW_XID(rootWin),
                                gdk_x11_get_xatom_by_name_for_display(display,
                                   "_NET_CLIENT_LIST_STACKING"),
                                0, G_MAXLONG, False, XA_WINDOW,
                                &type, &format, &numItems, &bytesAfter,
                                &data);
   int err = gdk_error_trap_pop();

   if (ret == Success && !err &&
       type == XA_WINDOW && format == 32 && data != NULL && numItems > 0) {
      long* stack = (long*)data;

      for (unsigned long i = 0; i < numItems; i++) {
#if GTK_MAJOR_VERSION == 3
         GdkWindow* win =
            gdk_x11_window_foreign_new_for_display(display, stack[i]);
#else
         GdkWindow* win =
            gdk_window_foreign_new_for_display(display, stack[i]);
#endif

         if (win != NULL) {
#if GTK_MAJOR_VERSION == 3
            windows.push_back(Glib::wrap(win));
#else
            windows.push_back(Glib::wrap((GdkWindowObject*)win));
#endif
         }
      }
   }

   return windows;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetMonitorWorkArea --
 *
 *      Gets the work area on a monitor. This is the area excluding docks,
 *      which a window would size to when maximized.
 *
 *      While the window manager typically provides a work area spanning all
 *      monitors (_NET_WORKAREA), it does not provide per-monitor work areas, so
 *      we must compute our own.
 *
 * Results:
 *      The work area of the specified monitor.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
GetMonitorWorkArea(Glib::RefPtr<Gdk::Screen> screen,    // IN:
                   int monitor,                         // IN:
                   Gdk::Rectangle& workArea)            // OUT:
{
#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1,10,0)
   /*
    * Relies on Cairo::Region support available in cairo 1.10+, which is
    * not available in our FreeBSD and Solaris toolchains.  Not called by
    * Tools code, so it's fine to just ifdef this out.
    */
   NOT_IMPLEMENTED();
#else


   /*
    * Start off by getting the size of the monitor. We're going to subtract
    * from this.
    */
   Gdk::Rectangle screenGeom;
   screen->get_monitor_geometry(monitor, screenGeom);
   Cairo::RectangleInt rect;
   Cairo::RefPtr<Cairo::Region> workAreaRegion = Cairo::Region::create();

   rect.x = screenGeom.get_x();
   rect.y = screenGeom.get_y();
   rect.width = screenGeom.get_width();
   rect.height = screenGeom.get_height();
   workAreaRegion->do_union(rect);

   /*
    * If we're dealing with a reparenting window manager, then using XQueryTree
    * will _not_ give us client windows, so to get client windows reliably, we
    * need to either use XQueryTree (get all top level windows) and iterate
    * hierarchy (root -> frame -> client) to be able to test for application
    * window properties, since these are only set on the client windows and not
    * on reparenting frames, or just use _NET_CLIENT_LIST. In practice, WMs put
    * docks and panels into _NET_CLIENT_LIST, so this should give us what we
    * need.
    */
   HostWindowList windows = GetHostWindowStack();
   bool haveStrut = false;
   for (HostWindowList::const_iterator iter = windows.begin();
        iter != windows.end();
        iter++) {

      Glib::RefPtr<Gdk::Window> gdkWindow = *iter;
      std::vector<unsigned long> values;
      NETWMStrutPartial strut = NETWMStrutPartial();

      if (monitor != screen->get_monitor_at_window(gdkWindow)) {
         continue;
      }

      /*
       * The EWMH spec says that the new _NET_WM_STRUT_PARTIAL takes precedence
       * over the older _NET_WM_STRUT API.
       */
      if (   GetCardinalList(gdkWindow, "_NET_WM_STRUT_PARTIAL", values)
          && values.size() == 12) {
         haveStrut = true;
         strut.left_width = values[0];
         strut.right_width = values[1];
         strut.top_height = values[2];
         strut.bottom_height = values[3];
         strut.left_start = values[4];
         strut.left_end = values[5];
         strut.right_start = values[6];
         strut.right_end = values[7];
         strut.top_start = values[8];
         strut.top_end = values[9];
         strut.bottom_start = values[10];
         strut.bottom_end = values[11];
      } else if (   GetCardinalList(gdkWindow, "_NET_WM_STRUT", values)
                 && values.size() == 4) {
         haveStrut = true;
         strut.left_width = values[0];
         strut.right_width = values[1];
         strut.top_height = values[2];
         strut.bottom_height = values[3];

         /*
          * Per EWMH spec: "This property (_NET_WM_STRUT) is equivalent to a
          * _NET_WM_STRUT_PARTIAL property where all start values are 0 and all
          * end values are the height or width of the logical screen."
          */
         strut.left_start = 0;
         strut.left_end = screen->get_height();
         strut.right_start = 0;
         strut.right_end = screen->get_height();
         strut.top_start = 0;
         strut.top_end = screen->get_width();
         strut.bottom_start = 0;
         strut.bottom_end = screen->get_width();
      } else {
         continue;
      }

      ASSERT(haveStrut);

      /*
       * Struts can be defined on one or more of the screen edges, so we create
       * 4 rectangles and subtract each from the work area.
       *
       * Per the EWMH spec: "Struts MUST be specified in root window
       * coordinates, that is, they are not relative to the edges of any view
       * port or Xinerama monitor.... Note that the strut is relative to the
       * screen edge, and not the edge of the xinerama monitor."
       */
      Gdk::Rectangle top(strut.top_start,
                         0,
                         strut.top_end - strut.top_start,
                         strut.top_height);
      Gdk::Rectangle bottom(strut.bottom_start,
                            screen->get_height() - strut.bottom_height,
                            strut.bottom_end - strut.bottom_start,
                            strut.bottom_height);
      Gdk::Rectangle left(0,
                          strut.left_start,
                          strut.left_width,
                          strut.left_end - strut.left_start);
      Gdk::Rectangle right(screen->get_width() - strut.right_width,
                           strut.right_start,
                           strut.right_width,
                           strut.right_end - strut.right_start);

      /*
       * We want each strut's rectangle to be used as if it was taking up the
       * entire edge of the monitor, so artificially inflate height or width as
       * need be. This means that instead of using the start and end strut
       * values, we take the whole edge.
       */
      Gdk::Rectangle edge;
      bool intersects = false;

      edge = top.intersect(screenGeom, intersects);

      if (top.get_height() > 0 && intersects && !edge.has_zero_area()) {
         rect.x = screenGeom.get_x();
         rect.y = screenGeom.get_y();
         rect.width = screenGeom.get_width();
         rect.height = edge.get_height();
         workAreaRegion->subtract(rect);
      }

      edge = bottom.intersect(screenGeom, intersects);

      if (bottom.get_height() > 0 && intersects && !edge.has_zero_area()) {
         rect.x = screenGeom.get_x();
         rect.y = edge.get_y();
         rect.width = screenGeom.get_width();
         rect.height = edge.get_height();
         workAreaRegion->subtract(rect);
      }

      edge = left.intersect(screenGeom, intersects);

      if (left.get_width() > 0 && intersects && !edge.has_zero_area()) {
         rect.x = screenGeom.get_x();
         rect.y = screenGeom.get_y();
         rect.width = edge.get_width();
         rect.height = screenGeom.get_height();
         workAreaRegion->subtract(rect);
      }

      edge = right.intersect(screenGeom, intersects);

      if (right.get_width() > 0 && intersects && !edge.has_zero_area()) {
         rect.x = edge.get_x();
         rect.y = screenGeom.get_y();
         rect.width = edge.get_width();
         rect.height = screenGeom.get_height();
         workAreaRegion->subtract(rect);
      }
   }
   /* bug:2163225: _NET_WM_STRUT_PARTIAL and _NET_WM_STRUT could not be retrived in redhat 7.4,7.5,
    * root cause unknown, have to use _NET_WORKAREA to get Work area directly in redhat 7.4,7.5, note
    * this fix only works in single monitor.
    */
   int monitorNum = screen->get_n_monitors();
   if ((!haveStrut) && (1 == monitorNum)) {
      std::vector<unsigned long> values;
      if (GetCardinalList(screen->get_root_window(), "_NET_WORKAREA", values) && values.size() >= 4 ) {
         rect.x=values[0];
         rect.y=values[1];
         rect.width=values[2];
         rect.height=values[3];
      } else {
         //Property: _NET_WORKAREA not found, workArea set keeps screen size
         Log("Property:_NET_WORKAREA unable to get or in multi monitor.");
         rect = workAreaRegion->get_extents();
      }
   } else {
      rect = workAreaRegion->get_extents();
   }
   workArea.set_x(rect.x);
   workArea.set_y(rect.y);
   workArea.set_width(rect.width);
   workArea.set_height(rect.height);
#endif // if CAIRO_VERSION
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetWindowManagerName --
 *
 *      Retrieves the current Window Manager name, if we can find it. This
 *      mimics the behavior of gdk_x11_screen_get_window_manager_name(), but
 *      there seem to be issues with that method returning its cached window
 *      manager name when it shouldn't
 *      (http://bugzilla.redhat.com/show_bug.cgi?id=471927).
 *
 * Results:
 *      If we can find the current window manager name, we'll return it. If we
 *      can't, then we'll return "unknown" -- same behavior as
 *      gdk_x11_screen_get_window_manager_name().
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

utf::string
GetWindowManagerName(Glib::RefPtr<Gdk::Screen> screen) // IN: Screen
{
   utf::string wmName = "unknown";
   GdkDisplay* display = gdk_display_get_default();
   unsigned long numItems = 0;
   unsigned long bytesAfter = 0;
   gint format = 0;
   Atom type = 0;
   guchar* data = NULL;
   ::Window* window;

   GdkWindow* rootWin = gdk_screen_get_root_window(screen->gobj());

   /*
    * First, we need to get the window that our EWMH-compliant WM is using to
    * communicate its properties with.
    */
   gdk_error_trap_push();
   int ret = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display),
                                GDK_WINDOW_XID(rootWin),
                                gdk_x11_get_xatom_by_name_for_display(display,
                                   "_NET_SUPPORTING_WM_CHECK"),
                                0, G_MAXLONG, False, XA_WINDOW,
                                &type, &format, &numItems, &bytesAfter,
                                &data);
   int err = gdk_error_trap_pop();

   if (ret != Success || err || type != XA_WINDOW || data == NULL) {
      if (data) {
         XFree(data);
      }
      return wmName;
   }

   window = (::Window*)data;
   gchar* name = NULL;

   /*
    * Now, using the window provided in _NET_SUPPORTING_WM_CHECK, look for the
    * _NET_WM_NAME on it.
    */
   gdk_error_trap_push();
   ret = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display),
                            *window,
                            gdk_x11_get_xatom_by_name_for_display(display,
                                "_NET_WM_NAME"),
                            0, G_MAXLONG, False,
                            gdk_x11_get_xatom_by_name_for_display(display,
                                "UTF8_STRING"),
                            &type, &format, &numItems, &bytesAfter,
                            (guchar**)&name);
   err = gdk_error_trap_pop();

   XFree(window);

   if (ret != Success || err || name == NULL) {
      if (name != NULL) {
         XFree(name);
      }
      return wmName;
   }

   wmName = name;
   XFree(name);

   return wmName;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::ChangeEWMHWindowState --
 *
 *      Sends the requested _NET_WM_STATE change through to the root window for
 *      the Window Manager to act on.
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
ChangeEWMHWindowState(bool add,                         // IN
                      Glib::RefPtr<Gdk::Window> window, // IN
                      GdkAtom state1,                   // IN
                      GdkAtom state2)                   // IN
{
   GdkScreen* screen = window->get_screen()->gobj();
   GdkDisplay* display = const_cast<GdkDisplay*>(window->get_display()->gobj());
   Window win = GDK_WINDOW_XID(window->gobj());

   XClientMessageEvent xclient;

/* Straight from http://standards.freedesktop.org/wm-spec/wm-spec-latest.html */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

   memset(&xclient, 0, sizeof xclient);
   xclient.type = ClientMessage;
   xclient.window = win;
   xclient.message_type = gdk_x11_get_xatom_by_name_for_display(display,
                                                                "_NET_WM_STATE");
   xclient.format = 32;
   xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
   xclient.data.l[1] = gdk_x11_atom_to_xatom_for_display(display, state1);
   xclient.data.l[2] = gdk_x11_atom_to_xatom_for_display(display, state2);
   xclient.data.l[3] = 0;
   xclient.data.l[4] = 0;

   XSendEvent(GDK_DISPLAY_XDISPLAY(display),
              GDK_WINDOW_XID(gdk_screen_get_root_window(screen)),
              False, SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent*)&xclient);
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetEWMHWindowState --
 *
 *      Queries _NET_WM_STATE on the provided window and returns a std::list of
 *      utf::strings which contain the X Atom names that are set as
 *      _NET_WM_STATE for the given window.
 *
 * Results:
 *      An std::list containing the utf::string names of all _NET_WM_STATE atoms
 *      that are set on the given window.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

std::list<utf::string>
GetEWMHWindowState(Glib::RefPtr<Gdk::Window> window) // IN
{
   std::list<utf::string> atomStrings;

   GdkDisplay* display = const_cast<GdkDisplay*>(window->get_display()->gobj());
   GdkWindow* gdkwin = const_cast<GdkWindow*>(window->gobj());

   Atom type = None;
   gint format;
   gulong nitems;
   gulong bytes_after;
   guchar* data;
   Atom* atoms = NULL;

   gdk_error_trap_push();
   int ret = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display),
                                GDK_WINDOW_XID(gdkwin),
                                gdk_x11_get_xatom_by_name_for_display(
                                   display, "_NET_WM_STATE"),
                                0, G_MAXLONG, False, XA_ATOM, &type, &format,
                                &nitems, &bytes_after, &data);
   int err = gdk_error_trap_pop();

   if (ret != Success || err) {
      atomStrings.push_back("Error calling XGetWindowProperty");
      return atomStrings;
   }

   if (type != XA_ATOM) {
      XFree(data);
      atomStrings.push_back("Error: type != XA_ATOM");
      return atomStrings;
   }

   atoms = reinterpret_cast<Atom*>(data);

   for (gulong i = 0; i < nitems; ++i) {
      atomStrings.push_back(gdk_x11_get_xatom_name(atoms[i]));
   }

   XFree(atoms);

   return atomStrings;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetPointerLocation --
 *
 *      Get the location of the pointer relative to the root window.
 *
 * Results:
 *      OUT parameters are filled in.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
GetPointerLocation(const Glib::RefPtr<Gdk::Window>& window, // IN
                   int& x,                                  // OUT
                   int& y,                                  // OUT
                   Gdk::ModifierType& mask)                 // OUT
{
#if GTK_MAJOR_VERSION == 3
   Glib::RefPtr<Gdk::DeviceManager> deviceManager =
      window->get_display()->get_device_manager();
   Glib::RefPtr<Gdk::Device> device = deviceManager->get_client_pointer();

   window->get_device_position(device, x, y, mask);
   window->get_root_coords(x, y, x, y);
#else
   window->get_display()->get_pointer(x, y, mask);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetXWindowSize --
 *
 *      Get the width and height of the given window.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool GetXWindowSize(const Glib::RefPtr<Gdk::Window>& window, // IN
                    int& width,                              // OUT
                    int& height)                             // OUT
{
   Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
   ::Display *xdisplay = GDK_DISPLAY_XDISPLAY(display->gobj());
   XWindowAttributes attr;
   GdkWindow *gdkwin = const_cast<GdkWindow *>(window->gobj());

   if (XGetWindowAttributes(xdisplay, GDK_WINDOW_XID(gdkwin), &attr)) {
      width = attr.width;
      height = attr.height;
      return true;
   }
   return false;
}


/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetXWindowOrigin --
 *
 *      Get the x and y of the given window.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool GetXWindowOrigin(const Glib::RefPtr<Gdk::Window>& window, // IN
                      int& x,                                  // OUT
                      int& y)                                  // OUT
{
   Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
   ::Display *xdisplay = GDK_DISPLAY_XDISPLAY(display->gobj());
   GdkWindow *gdkwin = const_cast<GdkWindow *>(window->gobj());
   Window child;
   if (XTranslateCoordinates(xdisplay, GDK_WINDOW_XID(gdkwin),
                             XDefaultRootWindow(xdisplay), 0, 0, &x, &y,
                             &child)) {
      return true;
   }
   return false;
}


} // namespace xutils
