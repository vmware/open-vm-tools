/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * productState.h --
 *
 *      ProductState is a runtime encapsulation of the identity of a product
 *      and product dependent characteristics.
 */


#ifndef _PRODUCT_STATE_H_
#define _PRODUCT_STATE_H_

#include "vm_basic_types.h"


/*
 * Public types.
 */

typedef enum {
   PRODUCT_GENERIC = 0,
   PRODUCT_WORKSTATION = 1 << 0,
   PRODUCT_SERVER = 1 << 1,
   PRODUCT_ESX = 1 << 2,
   PRODUCT_PLAYER = 1 << 3,
   PRODUCT_TOOLS = 1 << 4,
   PRODUCT_VDM_CLIENT = 1 << 5,
   /* etc */
} Product;
typedef uint64 ProductMask;

typedef uint64 ProductCaps;
/*
 * Define as needed.
 *
 * #define PRODUCT_CAP_FOO (1 << 0)
 */

typedef enum {
   PRODUCTSTATE_FLAG_NONE = 0,
   PRODUCTSTATE_FLAG_PRODUCT = 1 << 0,
   PRODUCTSTATE_FLAG_NAME = 1 << 1,
   PRODUCTSTATE_FLAG_VERSION = 1 << 2,
   PRODUCTSTATE_FLAG_BUILDNUMBER = 1 << 3,
   PRODUCTSTATE_FLAG_CAPABILITIES = 1 << 4,
   PRODUCTSTATE_FLAG_LICENSENAME = 1 << 5,
   PRODUCTSTATE_FLAG_LICENSEVERSION = 1 << 6,
} ProductStateSerializationFlags;

/*
 * Public functions.
 *
 * It is not generally safe to cache returned const pointers; if a caller
 * wants to cache a value, they should copy it.
 */

void ProductState_Set(Product product, const char *name, const char *version,
                      unsigned int buildNumber, ProductCaps capabilities,
                      const char *licenseName, const char *licenseVersion);
void ProductState_Reset(void);

Product ProductState_GetProduct(void);
Bool ProductState_IsProduct(ProductMask product);
const char *ProductState_GetName(void);
const char *ProductState_GetVersion(void);
unsigned int ProductState_GetBuildNumber(void);
ProductCaps ProductState_GetCapabilities(void);
const char *ProductState_GetLicenseName(void);
const char *ProductState_GetLicenseVersion(void);
/* etc */

const char *ProductState_GetFullVersion(void);
const char *ProductState_GetBuildNumberString(void);
const char *ProductState_GetRegistryPath(void);
char *ProductState_GetRegistryPathForProduct(const char *productName);
void ProductState_GetVersionNumber(unsigned int *major, unsigned int *minor,
                                   unsigned int *patchLevel);

char *ProductState_Serialize(ProductStateSerializationFlags flags);
ProductStateSerializationFlags ProductState_Deserialize(const char *state);


#endif /* _PRODUCT_STATE_H_ */
