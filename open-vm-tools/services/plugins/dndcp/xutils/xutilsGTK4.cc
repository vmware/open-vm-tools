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

#ifndef GTK4
#error "This should only build with GTK4"
#endif

#include "xutils/xutilsGTK4.hh"

/* These must be after the gtkmm includes(in xutilsGTK4.hh), as gtkmm is quite picky. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/x11/gdkx.h>
#include <cstring>

#include "vm_assert.h"


namespace xutils {
/*
 *-----------------------------------------------------------------------------
 *
 * xutils::GetCardinal --
 *
 *      Utility function to get only one cardinal from a window property.
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
GetCardinal(Glib::RefPtr<const Gdk::Surface> surface,  // IN: Surface
            const utf::string& atomName,               // IN: Atom name
            unsigned long& retValue)                   // OUT: Return value
{
   ASSERT(surface);
   ASSERT(!atomName.empty());

   std::vector<unsigned long> retValues;
   bool result = GetCardinalList(surface, atomName, retValues);

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
 *      true if the function succeeded, along with return values for retValues.
 *      Otherwise false.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

bool
GetCardinalList(Glib::RefPtr<const Gdk::Surface> surface, // IN: Surface
                const utf::string& atomName,              // IN: Atom name
                std::vector<unsigned long>& retValues)    // OUT: Return values
{
   ASSERT(surface);
   ASSERT(surface->get_display());
   ASSERT(!atomName.empty());

   GdkDisplay* gdkDisplay = const_cast<GdkDisplay*>(surface->get_display()->gobj());

   Atom atom = gdk_x11_get_xatom_by_name_for_display(gdkDisplay, atomName.c_str());
   Atom type;
   int format;
   unsigned long nitems;
   unsigned long bytes_after;
   uint8* values;
   bool result = false;

   gdk_x11_display_error_trap_push(gdkDisplay);

   /*
    * About the parameter "format" of Xorg XGetWindowProperty function.
    * The "format" param specifies whether the data should be viewed as a list
    * of 8-bit, 16-bit, or 32-bit quantities. Possible values are 8, 16, and 32.
    * This information allows the X server to correctly perform byte-swap
    * operations as necessary. If the format is 16-bit or 32-bit, you must
    * explicitly cast your data pointer to an (unsigned char *) in the call
    * to XChangeProperty.
    *
    * For more info, please refer to the document from Xorg:
    * https://www.x.org/archive/X11R7.5/doc/man/man3/XChangeProperty.3.html
    */
   int ret = XGetWindowProperty(GDK_DISPLAY_XDISPLAY(gdkDisplay),
                                gdk_x11_display_get_xrootwindow(gdkDisplay),
                                atom, 0, G_MAXLONG, False, XA_CARDINAL, &type,
                                &format, &nitems, &bytes_after, &values);
   int err = gdk_x11_display_error_trap_pop(gdkDisplay);

   if (ret == Success && err == 0) {
      if (type == XA_CARDINAL && nitems > 0) {
         retValues.resize(nitems);
         result = true;

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
      }
   }

   if (values != NULL) {
      XFree(values);
   }
   return result;
}


} // namespace xutils
