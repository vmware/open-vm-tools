/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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


#ifndef _DND_GUEST_H_
#define _DND_GUEST_H_

#if defined(_WIN32)

typedef struct {
   HWND detWnd;
   void (*setMode) (Bool);
} UnityDnD;

/*
 * Callers who desire the gtk implementation must include at least one gtk
 * header before this one.
 */
#elif defined(GTK_CHECK_VERSION)

#include <gtk/gtk.h>

typedef struct {
   GtkWidget *detWnd;
   void (*setMode) (Bool);
} UnityDnD;


#else

/* Probably compiling a stub. */
typedef struct UnityDnD UnityDnD;
   
#endif

#endif // _DND_GUEST_H_

