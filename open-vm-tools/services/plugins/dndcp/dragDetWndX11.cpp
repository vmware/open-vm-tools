/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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
 * @file dragDetWndX11.cpp
 *
 * Detection window code for Linux/X11, based on Gtkmm. Includes unit test
 * code.
 */

#define G_LOG_DOMAIN "dndcp"

#include "dragDetWndX11.h"
#include "dndUIX11.h"
#include <list>

/**
 *
 * Constructor.
 */

DragDetWnd::DragDetWnd() :
   m_isVisible(false)
{
#if defined(DETWNDDEBUG)
   DebugSetAttributes();
#endif

   bool useInvisible = true;

#ifdef GTK3
   char *xdgSessionType = getenv("XDG_SESSION_TYPE");
   if (   (xdgSessionType != NULL)
       && (strstr(xdgSessionType, "wayland") != NULL)) {
      useInvisible = false;
   }
#endif

   if (useInvisible) {
      mWnd = new DragDetWndImpl<Gtk::Invisible>();
   } else {
      DragDetWndImpl<Gtk::Window> *win = new DragDetWndImpl<Gtk::Window>();
      win->set_accept_focus(false);
      win->set_decorated(false);
      win->set_keep_above();
      // Makes this window transparent because we don't want user to see it.
      win->set_opacity(0.01);
      // Calls 'Show' to create the Gtk::Window object.
      win->show();
      win->hide();

      mWnd = win;
   }
}


/**
 *
 * Destructor.
 */

DragDetWnd::~DragDetWnd()
{
}


/**
 * Get the actual widget.
 */

Gtk::Widget *
DragDetWnd::GetWnd()
{
   return mWnd;
}


/**
 * Flush the X connection.
 */

void
DragDetWnd::Flush()
{
   Glib::RefPtr<Gdk::Display> gdkdisplay = Gdk::Display::get_default();
   if (gdkdisplay) {
      gdkdisplay->sync();
      gdkdisplay->flush();
   }
}


/**
 *
 * Show the window.
 */

void
DragDetWnd::Show(void)
{
   GetWnd()->show_now();
   Flush();
}


/**
 *
 * Hide the window.
 */

void
DragDetWnd::Hide(void)
{
   GetWnd()->hide();
   Flush();
}


/**
 *
 * Raise the window.
 */

void
DragDetWnd::Raise(void)
{
   Glib::RefPtr<Gdk::Window> gdkwin = GetWnd()->get_window();
   if (gdkwin) {
      gdkwin->raise();
   }
   Flush();
}


/**
 *
 * Lower the window.
 */

void
DragDetWnd::Lower(void)
{
   Glib::RefPtr<Gdk::Window> gdkwin = GetWnd()->get_window();
   if (gdkwin) {
      gdkwin->lower();
   }
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
   Glib::RefPtr<Gdk::Screen> gdkscreen = GetWnd()->get_screen();
   return gdkscreen->get_width();
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
   Glib::RefPtr<Gdk::Screen> gdkscreen = GetWnd()->get_screen();
   return gdkscreen->get_height();
}


#if defined(DETWNDDEBUG)
/**
 *
 * Set default window attributes appropriate for debugging detection windows.
 *
 * @note This only applies to instances of DragDetWnd that are derived from
 * GTK::Window.
 */

void
DragDetWnd::DebugSetAttributes(void)
{
   GetWnd()->set_default_size(1, 1);
   GetWnd()->set_resizable(true);
   GetWnd()->set_decorated(false);
   GetWnd()->set_type_hint(Gdk::WINDOW_TYPE_HINT_DOCK);
}
#endif


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
   Glib::RefPtr<Gdk::Window> gdkwin = GetWnd()->get_window();

   if (gdkwin) {
      gdkwin->move_resize(x, y, width, height);
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
   Glib::RefPtr<Gdk::Window> gdkwin = GetWnd()->get_window();
   if (gdkwin) {
#ifndef GTK3
      int dummy;
      gdkwin->get_geometry(x, y, width, height, dummy);
#else
      gdkwin->get_geometry(x, y, width, height);
#endif
#if defined(DETWNDTEST)
      Flush();
#endif
   }
}

/*
 * Code below here is for unit tests.
 */

#if defined(DETWNDTEST)

/**
 *
 * Add a button to launch unit tests to the drag detection window.
 */

void
DragDetWndTest::CreateTestUI()
{
   m_button.set_label("Start Unit Tests");
   add(m_button);
   m_button.signal_clicked().connect(sigc::mem_fun(*this, &DragDetWndTest::RunUnitTests));
   m_button.show();
}


/**
 *
 * Run some unit tests, then exit. Requires a main program, refer to
 * bora-vmsoft/toolbox/linux/vmwareuser/detWndTest/main.cpp for an
 * example.
 */

void
DragDetWndTest::RunUnitTests()
{
   DragDetWnd testWnd;
   int testCount = 0;
   int failCount = 0;

#if defined(DETWNDDEBUG)
   testWnd.SetAttributes();
#endif
   testWnd.Show();
   int x, y, width, height;
   testWnd.GetGeometry(x, y, width, height);
   printf("Geometry is x %d y %d width %d height %d\n", x, y, width, height);

   for (int i = 10; i < 50; Gtk::Main::iteration(), i++) {
      testCount++;
      printf("Setting geometry to x %d y %d w %d h %d\n", i * 10, i * 10, i * 10, i * 10);
      testWnd.SetGeometry(i * 10, i * 10, i * 10, i * 10);
      sleep(1);
      testWnd.GetGeometry(x, y, width, height);
      printf("Geometry is x %d y %d width %d height %d\n", x, y, width, height);
      if (x != i * 10 || y != i * 10 || width != i * 10) {
         printf("FAIL x or y not correct\n");
         failCount++;
      }
   }

   for (int i = 49; i > 0; Gtk::Main::iteration(), i--) {
      testCount++;
      printf("Setting geometry to x %d y %d w %d h %d\n", i * 10, i * 10, i * 10, i * 10);
      testWnd.SetGeometry(i * 10, i * 10, i * 10, i * 10);
      sleep(1);
      testWnd.GetGeometry(x, y, width, height);
      printf("Geometry is x %d y %d width %d height %d\n", x, y, width, height);
      if (x != i * 10 || y != i * 10 || width != i * 10) {
         printf("FAIL width or height not correct\n");
         failCount++;
      }
   }

   testWnd.SetGeometry(500, 500, 300, 300);

   for (int i = 0; i < 60; Gtk::Main::iteration(), i++) {
      if (i % 2) {
         printf("Hide\n");
         testWnd.Hide();
      } else {
         printf("Show\n");
         testWnd.Show();
         testWnd.Raise();
      }
      sleep(1);
   }

   printf("Done fail count %d (%.2f%%)\n", failCount, 100.0 * failCount/testCount);
   Gtk::Main::quit();
}
#endif
