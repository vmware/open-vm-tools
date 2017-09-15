/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef stdafx_h_
#define stdafx_h_

namespace Caf {
	extern const char* _sObjIdGuestAuthenticatorInstance;
	extern const char* _sObjIdGuestAuthenticator;
}

#include <CommonDefines.h>
#include <Integration.h>
#include <DocUtils.h>

#ifndef __APPLE__
#include <VGAuthAuthentication.h>

#include "CVgAuthError.h"
#include "CVgAuthInitializer.h"
#include "CGuestAuthenticatorInstance.h"
#include "CGuestAuthenticator.h"
#endif

#endif // #ifndef stdafx_h_
