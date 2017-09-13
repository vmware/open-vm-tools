/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CPersistenceProtocolDoc_h_
#define CPersistenceProtocolDoc_h_

namespace Caf {

/// A simple container for objects of type CPersistenceProtocolDoc
class CPersistenceProtocolDoc {
public:
	CPersistenceProtocolDoc() :
		_isInitialized(false) {}
	virtual ~CPersistenceProtocolDoc() {}

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCAmqpBrokerCollectionDoc& amqpBrokerCollection = SmartPtrCAmqpBrokerCollectionDoc()) {
		if (! _isInitialized) {
			_amqpBrokerCollection = amqpBrokerCollection;

			_isInitialized = true;
		}
	}

public:
	/// Accessor for the AmqpBrokerCollection
	SmartPtrCAmqpBrokerCollectionDoc getAmqpBrokerCollection() const {
		return _amqpBrokerCollection;
	}

private:
	SmartPtrCAmqpBrokerCollectionDoc _amqpBrokerCollection;

	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CPersistenceProtocolDoc);
};

CAF_DECLARE_SMART_POINTER(CPersistenceProtocolDoc);

}

#endif
