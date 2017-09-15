/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

//{{CAF_SUBSYSTEM}}

#include <CommonDefines.h>
#include <Integration.h>
#include <DocXml.h>
#include <DocUtils.h>

namespace Caf {
	extern const char* _sObjIdErrorToResponseTransformerInstance;
	extern const char* _sObjIdErrorToResponseTransformer;
	extern const char* _sObjIdPayloadHeaderEnricherInstance;
	extern const char* _sObjIdPayloadHeaderEnricher;
	extern const char* _sObjIdEnvelopeToPayloadTransformerInstance;
	extern const char* _sObjIdEnvelopeToPayloadTransformer;
}

#include "CErrorToResponseTransformerInstance.h"
#include "CErrorToResponseTransformer.h"

#include "CPayloadHeaderEnricherInstance.h"
#include "CPayloadHeaderEnricher.h"

#include "CEnvelopeToPayloadTransformerInstance.h"
#include "CEnvelopeToPayloadTransformer.h"

#endif // #ifndef stdafx_h_
