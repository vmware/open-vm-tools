/*********************************************************
 * Copyright (C) 2004-2016,2018 VMware, Inc. All rights reserved.
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
 * libExport.hh --
 *
 *      Declares proper decorations for dll-export interface
 *      for all modules in libs.
 */

#ifndef LIB_EXPORT_HH
#define LIB_EXPORT_HH

#include "vm_api.h"

#ifndef LIB_EXPORT
   #ifdef LIB_EXPORT_SOURCE
      #define LIB_EXPORT VMW_LIB_DYNAMIC
   #else
      #define LIB_EXPORT VMW_LIB_CLIENT
   #endif
#endif

#ifndef LIB_EXPORT_WUI
   #ifdef LIB_EXPORT_WUI_SOURCE
      #define LIB_EXPORT_WUI VMW_LIB_DYNAMIC
   #else
      #define LIB_EXPORT_WUI VMW_LIB_CLIENT
   #endif
#endif

#ifndef VMSTRING_EXPORT
   #ifdef VMSTRING_EXPORT_SOURCE
      #define VMSTRING_EXPORT VMW_LIB_DYNAMIC
   #else
      #define VMSTRING_EXPORT VMW_LIB_CLIENT
   #endif
#endif

#endif // LIB_EXPORT_HH
