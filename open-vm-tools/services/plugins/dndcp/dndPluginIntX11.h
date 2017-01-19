/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * dndPluginX11Int.h --
 *
 *     Common defines used by Linux DnD plugin implementation.
 */
#ifndef __DNDPLUGIN_INTX11_H__
#define __DNDPLUGIN_INTX11_H__

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#undef Bool
#include "vm_basic_types.h"
#include "dnd.h"

#define UNGRABBED_POS (-100)

extern Display *gXDisplay;
extern Window gXRoot;
extern GtkWidget *gUserMainWidget;

#endif
