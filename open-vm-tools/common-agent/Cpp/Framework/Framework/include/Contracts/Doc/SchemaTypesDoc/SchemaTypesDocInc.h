/*
 *	 Author: bwilliams
 *  Created: April 6, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef SCHEMATYPESDOCINC_H_
#define SCHEMATYPESDOCINC_H_

namespace Caf {

	typedef enum {
		OPERATOR_NONE,
		OPERATOR_EQUAL
	} OPERATOR_TYPE;

	typedef enum {
		ARITY_NONE,
		ARITY_UNSIGNED_BYTE = 2
	} ARITY_TYPE;

	typedef enum {
		VALIDATOR_NONE,
		VALIDATOR_ENUM,
		VALIDATOR_RANGE,
		VALIDATOR_REGEX,
		VALIDATOR_CUSTOM
	} VALIDATOR_TYPE;

	// Forward declare
	CAF_DECLARE_CLASS_AND_SMART_POINTER(CDataClassSubInstanceDoc);
}

#include "CInstanceOperationDoc.h"
#include "CInstanceOperationCollectionDoc.h"
#include "CActionClassInstanceDoc.h"
#include "CActionClassInstanceCollectionDoc.h"

#include "CCmdlMetadataDoc.h"
#include "CCmdlUnionDoc.h"
#include "CDataClassPropertyDoc.h"
#include "CDataClassSubInstanceDoc.h"
#include "CDataClassInstanceDoc.h"
#include "CDataClassInstanceCollectionDoc.h"

#include "CMethodParameterDoc.h"
#include "CInstanceParameterDoc.h"

#include "CClassIdentifierDoc.h"

#include "CMethodDoc.h"
#include "CCollectMethodDoc.h"
#include "CActionClassDoc.h"

#include "CClassPropertyDoc.h"

#include "CClassFieldDoc.h"
#include "CJoinTypeDoc.h"

#include "CClassCardinalityDoc.h"
#include "CClassInstancePropertyDoc.h"
#include "CDataClassDoc.h"
#include "CLogicalRelationshipDoc.h"
#include "CPhysicalRelationshipDoc.h"
#include "CRelationshipDoc.h"

#endif /* SCHEMATYPESDOCINC_H_ */
