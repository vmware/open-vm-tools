/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * @file util.cc
 *
 * This file just adds some random X utility functions.
 */

#include "x11Platform.h"


/*
 ******************************************************************************
 * UnityX11Util_IsWindowDecorationWidget --                              */ /**
 *
 * Determine if this window's role is "decoration widget".
 *
 * @param[in]   up      UnityPlatform context.
 * @param[in]   operand Window ID to search for.
 *
 * @todo Use reference counting and toplevel vs. client window distinctions
 * to determine whether this window is relevant to the host-guest window
 * tracker.  That's to say that when this window's parent/frame is eventually
 * associated with a client window, we know that this window is no longer
 * a candidate for becoming the client, and that we may stop paying attention
 * to it.
 *
 * @retval TRUE  Window is a decoration widget.
 * @retval FALSE Window is not a decoration widget.
 *
 ******************************************************************************
 */

Bool
UnityX11Util_IsWindowDecorationWidget(UnityPlatform *up,
                                      Window w)
{
   Atom propertyType;
   int propertyFormat;
   unsigned long bytesReturned = 0;
   unsigned long bytesRemaining;
   char *valueReturned = NULL;
   Bool retval = FALSE;

   if (XGetWindowProperty(up->display, w, up->atoms.WM_WINDOW_ROLE, 0, 1024,
                          False, AnyPropertyType, &propertyType,
                          &propertyFormat, &bytesReturned, &bytesRemaining,
                          (unsigned char **) &valueReturned)
       == Success
       && bytesReturned > 0
       && propertyType == XA_STRING
       && propertyFormat == 8
       && strcmp(valueReturned, "decoration widget") == 0) {
      retval = TRUE;
   }

   XFree(valueReturned);

   UnityPlatformResetErrorCount(up);
   return retval;
}
