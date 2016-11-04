/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_CPollerMetadata_h_
#define _IntegrationContracts_CPollerMetadata_h_

namespace Caf {

class CPollerMetadata {
public:
	CPollerMetadata() :
		_maxMessagesPerPoll(0),
		_fixedRate(0) {}

public:
	uint32 getMaxMessagesPerPoll() const {
		return _maxMessagesPerPoll;
	}
	void putMaxMessagesPerPoll(const uint32& maxMessagesPerPoll) {
		_maxMessagesPerPoll = maxMessagesPerPoll;
	}

	uint32 getFixedRate() const {
		return _fixedRate;
	}
	void putFixedRate(const uint32& fixedRate) {
		_fixedRate = fixedRate;
	}

private:
	uint32 _maxMessagesPerPoll;
	uint32 _fixedRate;

private:
	CPollerMetadata (const CPollerMetadata&);
	CPollerMetadata & operator=(const CPollerMetadata&);
};

CAF_DECLARE_SMART_POINTER(CPollerMetadata);

}

#endif // #ifndef _IntegrationContracts_CPollerMetadata_h_
