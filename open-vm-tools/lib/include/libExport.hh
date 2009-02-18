/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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

#ifdef _WIN32
   #ifdef LIB_EXPORT_SOURCE
      #define LIB_EXPORT __declspec(dllexport)
   #else
      #define LIB_EXPORT __declspec(dllimport)
   #endif
   
   #ifdef LIB_EXPORT_WUI_SOURCE
      #define LIB_EXPORT_WUI __declspec(dllexport)
   #else
      #define LIB_EXPORT_WUI __declspec(dllimport)
   #endif

   #ifdef VMSTRING_EXPORT_SOURCE
      #define VMSTRING_EXPORT __declspec(dllexport)
   #else
      #define VMSTRING_EXPORT __declspec(dllimport)
   #endif
#else 
   #define LIB_EXPORT
   #define LIB_EXPORT_WUI
   #define VMSTRING_EXPORT
#endif // WIN32

#endif // LIB_EXPORT_HH
