/*
 *	 Author: bwilliams
 *  Created: 10/19/2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Integration/Core/MessageHeaders.h"

namespace Caf {
	namespace MessageHeaders {
		// UUID stored as a string
		const char* _sID = "id";

		// int64
		const char* _sTIMESTAMP = "timestamp";

		// string
		const char* _sCORRELATION_ID = "correlationId";

		// string
		const char* _sREPLY_CHANNEL = "replyChannel";

		// string
		const char* _sERROR_CHANNEL = "errorChannel";

		// int64
		const char* _sEXPIRATION_DATE = "expirationDate";

		// int32
		const char* _sPRIORITY = "priority";

		// int32
		const char* _sSEQUENCE_NUMBER = "sequenceNumber";

		// int32
		const char* _sSEQUENCE_SIZE = "sequenceSize";

		const char* _sIS_THROWABLE = "_isThrowable_";

		// string
		const char* _sREQUEST_ID = "caf.msg.requestid";

		// boolean
		const char* _sMULTIPART = "caf.msg.multipart";

		// string
		const char* _sMULTIPART_WORKING_DIR = "cafcomm.internal.multipart-working-dir";

		// string
		const char* _sPROTOCOL_TYPE = "caf.protocolType";

		// string
		const char* _sPROTOCOL_CONNSTR = "caf.connStr";
	}
}
