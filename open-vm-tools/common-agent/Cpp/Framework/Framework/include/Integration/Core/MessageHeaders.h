/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef MessageHeadersInc_h_
#define MessageHeadersInc_h_

namespace Caf {
	namespace MessageHeaders {
		// UUID stored as a string
		extern INTEGRATIONCORE_LINKAGE const char* _sID;

		// int64
		extern INTEGRATIONCORE_LINKAGE const char* _sTIMESTAMP;

		// string
		extern INTEGRATIONCORE_LINKAGE const char* _sCORRELATION_ID;

		// string
		extern INTEGRATIONCORE_LINKAGE const char* _sREPLY_CHANNEL;

		// string
		extern INTEGRATIONCORE_LINKAGE const char* _sERROR_CHANNEL;

		// int64
		extern INTEGRATIONCORE_LINKAGE const char* _sEXPIRATION_DATE;

		// int32
		extern INTEGRATIONCORE_LINKAGE const char* _sPRIORITY;

		// int32
		extern INTEGRATIONCORE_LINKAGE const char* _sSEQUENCE_NUMBER;

		// int32
		extern INTEGRATIONCORE_LINKAGE const char* _sSEQUENCE_SIZE;

		extern INTEGRATIONCORE_LINKAGE const char* _sIS_THROWABLE;

		// string
		extern INTEGRATIONCORE_LINKAGE const char* _sREQUEST_ID;

		// boolean
		extern INTEGRATIONCORE_LINKAGE const char* _sMULTIPART;

		// string
		extern INTEGRATIONCORE_LINKAGE const char* _sMULTIPART_WORKING_DIR;

		// string
		extern INTEGRATIONCORE_LINKAGE const char* _sMESSAGE_TYPE;

		// string
		extern INTEGRATIONCORE_LINKAGE const char* _sPROTOCOL_TYPE;

		// string
		extern INTEGRATIONCORE_LINKAGE const char* _sPROTOCOL_CONNSTR;
	}
};

#endif // #ifndef MessageHeadersInc_h_

