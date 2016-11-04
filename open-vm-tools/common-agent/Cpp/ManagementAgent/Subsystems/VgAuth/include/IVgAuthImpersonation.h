/*
 *  Created on: Jan 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IVgAuthImpersonation_H_
#define _IVgAuthImpersonation_H_

namespace Caf {

struct __declspec(novtable)
	IVgAuthImpersonation : public ICafObject
{
	CAF_DECL_UUID("63cfac22-e1b8-4977-8907-9582657b7420")

	virtual void endImpersonation() = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IVgAuthImpersonation);

}

#endif /* _IVgAuthImpersonation_H_ */
