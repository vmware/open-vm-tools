/*********************************************************
 * Copyright (C) 2006-2019 VMware, Inc. All rights reserved.
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
#include "product.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint64 ProductMask;
#define PRODUCTMASK_HOSTED (PRODUCT_WORKSTATION |\
                            PRODUCT_PLAYER      |\
                            PRODUCT_CVP         |\
                            PRODUCT_FUSION      |\
                            PRODUCT_VMRC)

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
   PRODUCTSTATE_FLAG_BUNDLEIDENTIFIER = 1 << 7,
} ProductStateSerializationFlags;

/*
 * Public functions.
 *
 * PR 567850
 * ProductState_Set should only be called once. Subsequent calls will be
 * ignored.
 */

void ProductState_Set(Product product,
                      const char *name,
                      const char *version,
                      unsigned int buildNumber,
                      ProductCaps capabilities,
                      const char *licenseName,
                      const char *licenseVersion,
                      const char *bundleIdentifier);
void ProductState_SetProduct(uint64 product);
void ProductState_SetName(const char *name);
void ProductState_SetLicenseName(const char *licenseName);
unsigned int  ProductState_GetBuildNumber(void);
const char   *ProductState_GetBuildNumberString(void);
const char   *ProductState_GetBundleIdentifier(void);
ProductCaps   ProductState_GetCapabilities(void);
const char   *ProductState_GetCompilationOption(void);
const char   *ProductState_GetConfigName(void);
const char   *ProductState_GetFullVersion(void);
void          ProductState_GetHelp(Product *helpProduct,
                                   const char **helpVersion);
const char   *ProductState_GetLicenseName(void);
const char   *ProductState_GetLicenseVersion(void);
const char   *ProductState_GetName(void);
Product       ProductState_GetProduct(void);
const char   *ProductState_GetRegistryPath(void);
char         *ProductState_GetRegistryPathForProduct(const char *productName);
const char   *ProductState_GetVersion(void);
void          ProductState_GetVersionNumber(unsigned int *major,
                                            unsigned int *minor,
                                            unsigned int *patchLevel);

Bool ProductState_IsProduct(ProductMask product);
Bool ProductState_AllowUnlicensedVMX(void);

void ProductState_SetConfigName(const char *configName);
/* etc */

void ProductState_SetHelp(Product helpProduct,
                          const char *helpVersion);

char *ProductState_Serialize(ProductStateSerializationFlags flags);
ProductStateSerializationFlags ProductState_Deserialize(const char *state);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _PRODUCT_STATE_H_ */
