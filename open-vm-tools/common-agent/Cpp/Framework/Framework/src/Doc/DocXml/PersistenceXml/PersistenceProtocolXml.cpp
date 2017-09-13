/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#include "stdafx.h"
#include "PersistenceProtocolXml.h"

using namespace Caf;

void PersistenceProtocolXml::add(
	const SmartPtrCPersistenceProtocolDoc persistenceProtocolDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PersistenceProtocolXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(persistenceProtocolDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const SmartPtrCAmqpBrokerCollectionDoc amqpBrokerCollectionVal =
			persistenceProtocolDoc->getAmqpBrokerCollection();
	if (! amqpBrokerCollectionVal.IsNull()) {
		const SmartPtrCXmlElement amqpBrokerCollectionXml =
			thisXml->createAndAddElement("amqpBrokerCollection");
		AmqpBrokerCollectionXml::add(amqpBrokerCollectionVal, amqpBrokerCollectionXml);
	}
}

SmartPtrCPersistenceProtocolDoc PersistenceProtocolXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("PersistenceProtocolXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const SmartPtrCXmlElement amqpBrokerCollectionXml =
		thisXml->findOptionalChild("amqpBrokerCollection");
	SmartPtrCAmqpBrokerCollectionDoc amqpBrokerCollectionVal;
	if (! amqpBrokerCollectionXml.IsNull()) {
		amqpBrokerCollectionVal = AmqpBrokerCollectionXml::parse(amqpBrokerCollectionXml);
	}

	SmartPtrCPersistenceProtocolDoc persistenceProtocolDoc;
	persistenceProtocolDoc.CreateInstance();
	persistenceProtocolDoc->initialize(
		amqpBrokerCollectionVal);

	return persistenceProtocolDoc;
}

