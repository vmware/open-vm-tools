/*
 *	 Author: bwilliams
 *  Created: April 6, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CAFCORETYPESDOCINC_H_
#define CAFCORETYPESDOCINC_H_

namespace Caf {
	typedef enum {
		PROPERTY_NONE,
		PROPERTY_STRING,
		PROPERTY_SINT32,
		PROPERTY_UINT32,
		PROPERTY_SINT64,
		PROPERTY_UINT64,
		PROPERTY_DECIMAL,
		PROPERTY_DOUBLE,
		PROPERTY_BOOLEAN,
		PROPERTY_DATETIME
	} PROPERTY_TYPE;

	typedef enum {
		PARAMETER_NONE,
		PARAMETER_STRING,
		PARAMETER_SINT32,
		PARAMETER_UINT32,
		PARAMETER_SINT64,
		PARAMETER_UINT64,
		PARAMETER_DECIMAL,
		PARAMETER_DOUBLE,
		PARAMETER_BOOLEAN,
		PARAMETER_DATETIME
	} PARAMETER_TYPE;

	typedef enum {
		LOGGINGLEVEL_NONE,
		LOGGINGLEVEL_DEBUG,
		LOGGINGLEVEL_INFO,
		LOGGINGLEVEL_WARN,
		LOGGINGLEVEL_ERROR,
		LOGGINGLEVEL_CRITICAL
	} LOGGINGLEVEL_TYPE;

	typedef enum {
		LOGGINGCOMPONENT_NONE,
		LOGGINGCOMPONENT_COMMUNICATIONS,
		LOGGINGCOMPONENT_MANAGEMENTAGENT,
		LOGGINGCOMPONENT_UINT32,
		LOGGINGCOMPONENT_PROVIDERFRAMEWORK,
		LOGGINGCOMPONENT_PROVIDER
	} LOGGINGCOMPONENT_TYPE;

	typedef enum {
		CMS_POLICY_NONE,
		CMS_POLICY_CAF_ENCRYPTED,
		CMS_POLICY_CAF_SIGNED,
		CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED,
		CMS_POLICY_APP_SIGNED,
		CMS_POLICY_APP_ENCRYPTED,
		CMS_POLICY_APP_ENCRYPTED_AND_SIGNED
	} CMS_POLICY;
}

#include "CFullyQualifiedClassGroupDoc.h"
#include "CAttachmentNameCollectionDoc.h"

#include "CLoggingLevelElemDoc.h"
#include "CLoggingLevelCollectionDoc.h"

#include "CRequestInstanceParameterDoc.h"
#include "CRequestParameterDoc.h"

#include "CInlineAttachmentDoc.h"
#include "CInlineAttachmentCollectionDoc.h"

#include "CAttachmentDoc.h"
#include "CAttachmentCollectionDoc.h"

#include "CParameterCollectionDoc.h"
#include "COperationDoc.h"

#include "CPropertyDoc.h"
#include "CPropertyCollectionDoc.h"

#include "CAddInCollectionDoc.h"
#include "CAddInsDoc.h"

#include "CAuthnAuthzDoc.h"
#include "CAuthnAuthzCollectionDoc.h"

#include "CProtocolDoc.h"
#include "CProtocolCollectionDoc.h"

#include "CRequestConfigDoc.h"
#include "CRequestHeaderDoc.h"

#include "CClassFiltersDoc.h"
#include "CClassSpecifierDoc.h"
#include "CStatisticsDoc.h"

#endif /* CAFCORETYPESDOCINC_H_ */
