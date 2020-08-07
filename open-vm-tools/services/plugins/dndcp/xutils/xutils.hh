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


#ifndef XUTILS_XUTILS_HH
#define XUTILS_XUTILS_HH


#include <gdkmm.h>
#include <gtkmm.h>

#include "stringxx/string.hh"


namespace xutils {

typedef std::list<Glib::RefPtr<Gdk::Window> > HostWindowList;

/* General initialization */
void Init();


/* General property helpers */
bool GetCardinal(Glib::RefPtr<const Gdk::Window> window,
                 const utf::string& atomName, unsigned long& retValue);
bool GetCardinalList(Glib::RefPtr<const Gdk::Window> window,
                     const utf::string& atomName,
                     std::vector<unsigned long>& retValues);

/*
 * Utility functions for virtual desktops.
 *
 * There are two components to virtual desktops: Workspaces and Viewports.
 *
 * Workspaces can contain one or more viewports. Workspace layouts have a
 * corner origin and a direction in which the workspaces are ordered.
 *
 * Viewports exist inside a workspace and are essentially one large screen
 * containing windows. The current viewport has an X, Y offset on this
 * large screen containing the physical screen full of content to display.
 *
 * Some window managers (Metacity, for example) use workspaces exclusively to
 * represent virtual desktops, while others (Enlightenment) may use workspaces
 * and viewports combined. Compiz uses multiple viewports in a single
 * workspace.
 */
void SetDesktopForWindow(Glib::RefPtr<Gdk::Window> window, uint32 desktop);
uint32 GetDesktopForWindow(Glib::RefPtr<const Gdk::Window> window);

uint32 GetNumDesktops(Glib::RefPtr<Gdk::Screen> screen);
uint32 GetCurrentDesktop(Glib::RefPtr<Gdk::Screen> screen);
extern sigc::signal<void, Glib::RefPtr<Gdk::Screen> > currentDesktopChanged;

bool GetDesktopLayout(Glib::RefPtr<Gdk::Screen> screen,
                      uint32& rows, uint32& columns, Gtk::CornerType& corner,
                      Gtk::Orientation& orientation);
extern sigc::signal<void, Glib::RefPtr<Gdk::Screen> > desktopLayoutChanged;

bool GetDesktopGeometry(Glib::RefPtr<Gdk::Screen> screen,
                        uint32& width, uint32& height);
extern sigc::signal<void, Glib::RefPtr<Gdk::Screen> > desktopGeometryChanged;

bool GetDesktopViewport(Glib::RefPtr<Gdk::Screen> screen,
                        uint32 desktopIndex, VMPoint& viewport);
extern sigc::signal<void, Glib::RefPtr<Gdk::Screen> > desktopViewportChanged;

extern sigc::signal<void, Glib::RefPtr<Gdk::Screen> > activeWindowChanged;

/* Window stacking */
void RaiseWindow(Glib::RefPtr<Gdk::Window> window,
                 Glib::RefPtr<Gdk::Window> sibling =
                    Glib::RefPtr<Gdk::Window>(),
                 guint32 timestamp = 0);
HostWindowList GetHostWindowStack();
extern sigc::signal<void, Glib::RefPtr<Gdk::Screen> > windowStackChanged;


/* Multi-head */
void GetMonitorWorkArea(Glib::RefPtr<Gdk::Screen> screen,
                        int monitor, Gdk::Rectangle& rect);

utf::string GetWindowManagerName(Glib::RefPtr<Gdk::Screen> screen);
extern sigc::signal<void, Glib::RefPtr<Gdk::Screen> > windowManagerChanged;

void SetFullscreenMonitorsHint(Glib::RefPtr<Gdk::Window> window,
                               std::vector<long> topology);

void ChangeEWMHWindowState(bool add, Glib::RefPtr<Gdk::Window> window,
                           GdkAtom state1, GdkAtom state2);
std::list<utf::string> GetEWMHWindowState(Glib::RefPtr<Gdk::Window> window);

void GetPointerLocation(const Glib::RefPtr<Gdk::Window>& window,
                        int& x, int& y, Gdk::ModifierType& mask);


extern sigc::signal<void, Glib::RefPtr<Gdk::Screen> > workAreaChanged;

bool GetXWindowSize(const Glib::RefPtr<Gdk::Window>& window, int& width,
                    int& height);
bool GetXWindowOrigin(const Glib::RefPtr<Gdk::Window>& window, int& x, int& y);
} // namespace xutils


#endif // XUTILS_XUTILS_HH
