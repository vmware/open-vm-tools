/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
 * capsProvider.h --
 *
 *      Interface implemented by dnd manager objects to obtain their
 *      capabilities. Mainly needed by Windows host and guest code.
 */

#ifndef __CAPS_PROVIDER_H__
#define __CAPS_PROVIDER_H__

#if defined VMX86_TOOLS || COMPILE_WITHOUT_CUI
#   ifdef LIB_EXPORT
#   undef LIB_EXPORT
#   endif
#define LIB_EXPORT
#else
#include "libExport.hh"
#endif

#if defined(SWIG)
class CapsProvider
#else
class LIB_EXPORT CapsProvider
#endif
{
public:
   virtual ~CapsProvider() {};
   virtual Bool CheckCapability(uint32 caps) = 0;
   virtual uint64 GetDnDSizeThreshold() { return 0; } // 0 means no size control
};

#endif

