/*
 *	 Author: mdonahue
 *  Created: Jan 17, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCONFIGPARAMSCHAIN_H_
#define CCONFIGPARAMSCHAIN_H_

#include "Common/IConfigParams.h"

#include "Common/CConfigParams.h"

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CConfigParamsChain : public IConfigParams {
public:
	CConfigParamsChain();
	virtual ~CConfigParamsChain();

	void initialize(CConfigParams::EKeyManagement keyManagement,
					CConfigParams::EValueManagement valueManagement,
					const SmartPtrIConfigParams& baseParams);

	void insert(const char* key, GVariant* value);

public: // IConfigParams
	GVariant* lookup(const char* key, const EParamDisposition disposition = PARAM_REQUIRED) const;

	std::string getSectionName() const;

private:
	static void destroyValueCallback(gpointer ptr);

private:
	SmartPtrCConfigParams _theseParams;
	SmartPtrIConfigParams _baseParams;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CConfigParamsChain);
};

CAF_DECLARE_SMART_POINTER(CConfigParamsChain);

}

#endif /* CCONFIGPARAMSCHAIN_H_ */
