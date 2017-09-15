/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 * @file dragDetWnd.h
 *
 *    Header for the DragDetWnd class.
 */

#ifndef DRAG_DET_WND_H
#define DRAG_DET_WND_H

#include <gtkmm.h>

#if !defined(DETWNDTEST)
extern "C" {
   #include "dnd.h"
}
#endif

class DnDUI;

class DragDetWnd : public Gtk::Invisible
{
public:
   DragDetWnd();
   virtual ~DragDetWnd();

   void Show();
   void Hide();
   void Raise();
   void Lower();
   int GetScreenWidth();
   int GetScreenHeight();
   void SetGeometry(const int x, const int y,
                    const int width, const int height);
   void GetGeometry(int &x, int &y, int &width, int &height);
   void SetIsVisible(const bool isVisible) {m_isVisible = isVisible;};
   bool GetIsVisible() {return m_isVisible;};
#if defined(DETWNDEBUG)
   void DebugSetAttributes();
#endif
private:
   void Flush();
   bool m_isVisible;
};

#if defined(DETWNDTEST)
class DragDetWndTest : public Gtk::Window
{
public:
   void CreateTestUI();
private:
   virtual void RunUnitTests();
   Gtk::Button m_button;
};

#endif

#endif // DRAG_DET_WND_H
