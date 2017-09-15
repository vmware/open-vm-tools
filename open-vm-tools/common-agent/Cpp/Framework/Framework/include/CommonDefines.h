/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef COMMON_SYS_INC_COMMONDEFINES_H_
#define COMMON_SYS_INC_COMMONDEFINES_H_

#include <BaseDefines.h>

namespace Caf {
}

// Aggregator
#include "../src/Common/CommonAggregatorLink.h"

////////////////////////////////////////////////////////////////////////
// endian ordering repair macros
////////////////////////////////////////////////////////////////////////
#define CAF_FIX_16BIT_ENDIAN(x) x = (((x) >> 8) & 0xff) | ((x) << 8)
#define CAF_FIX_32BIT_ENDIAN(x) x = (((x) >> 24) & 0xff) | (((x) >> 8) & 0xff00) | (((x) & 0xff00) << 8) | ((x) << 24)
#define CAF_FIX_64BIT_ENDIAN(x) x = (((x) >> 56) & 0xff) | (((x) >> 40) & 0xff00) | (((x) >> 24) & 0xff0000) | \
									(((x) >> 8) & 0xff000000) | (((x) & 0xff000000) << 8) | \
									(((x) & 0xff0000) << 24) | (((x) & 0xff00) << 40) | \
									((x) << 56)

#define CAF_FIX_GUID_ENDIAN(guid)	CAF_FIX_32BIT_ENDIAN(guid.Data1); \
									CAF_FIX_16BIT_ENDIAN(guid.Data2); \
									CAF_FIX_16BIT_ENDIAN(guid.Data3)

#endif /* COMMON_SYS_INC_COMMONDEFINES_H_ */
