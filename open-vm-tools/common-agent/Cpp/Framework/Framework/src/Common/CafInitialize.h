/*
 *	 Author: mdonahue
 *  Created: Jan 24, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CAFINITIALIZE_H_
#define CAFINITIALIZE_H_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CafInitialize {
public:
	static HRESULT init();
	static HRESULT serviceConfig();
	static HRESULT term();

	CAF_CM_DECLARE_NOCREATE(CafInitialize);
};

}

#endif /* CAFINITIALIZE_H_ */
