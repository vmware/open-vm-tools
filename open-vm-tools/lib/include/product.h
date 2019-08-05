/*********************************************************
 * Copyright (C) 2017-2019 VMware, Inc. All rights reserved.
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
 * product.h --
 *
 *      This file contains the Product enum.
 *
 *      Products that don't want to include productState and vm_basic_types.h
 *      and want to know about the magic values for various products should
 *      include this file.
 */

#ifndef _PRODUCT_H_
#define _PRODUCT_H_

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Public types.
 */

typedef enum {
   PRODUCT_GENERIC      = 0,
   PRODUCT_WORKSTATION  = 1 << 0,
   PRODUCT_ESX          = 1 << 1,
   PRODUCT_PLAYER       = 1 << 2,
   PRODUCT_TOOLS        = 1 << 3,
   PRODUCT_VDM_CLIENT   = 1 << 4,
   PRODUCT_CVP          = 1 << 5,
   PRODUCT_FUSION       = 1 << 6,
   PRODUCT_VIEW         = 1 << 7,
   PRODUCT_VMRC         = 1 << 8,
   PRODUCT_VMACORETESTS = 1 << 9,
   PRODUCT_SRM          = 1 << 10,
   /* etc */
} Product;


#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
