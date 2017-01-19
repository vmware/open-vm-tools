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

/**
 * @dndCPLibExport.hh --
 *
 * LibExport definition.
 */

#ifndef DND_CP_LIB_EXPORT_HH
#define DND_CP_LIB_EXPORT_HH

#if defined VMX86_TOOLS || COMPILE_WITHOUT_CUI
#   ifdef LIB_EXPORT
#   undef LIB_EXPORT
#   endif
#define LIB_EXPORT
#else
#include "libExport.hh"
#endif

#endif // DND_CP_LIB_EXPORT_HH
