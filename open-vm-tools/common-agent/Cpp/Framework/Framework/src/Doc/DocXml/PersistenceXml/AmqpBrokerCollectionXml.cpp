/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#include "stdafx.h"
#include "AmqpBrokerCollectionXml.h"

using namespace Caf;

void AmqpBrokerCollectionXml::add(
	const SmartPtrCAmqpBrokerCollectionDoc amqpBrokerCollectionDoc,
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpBrokerCollectionXml", "add");
	CAF_CM_VALIDATE_SMARTPTR(amqpBrokerCollectionDoc);
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const std::deque<SmartPtrCAmqpBrokerDoc> amqpBrokerVal =
		amqpBrokerCollectionDoc->getAmqpBroker();

	if (! amqpBrokerVal.empty()) {
		for (TConstIterator<std::deque<SmartPtrCAmqpBrokerDoc> > amqpBrokerIter(amqpBrokerVal);
			amqpBrokerIter; amqpBrokerIter++) {
			const SmartPtrCXmlElement amqpBrokerXml =
				thisXml->createAndAddElement("amqpBroker");
			AmqpBrokerXml::add(*amqpBrokerIter, amqpBrokerXml);
		}
	}
}

SmartPtrCAmqpBrokerCollectionDoc AmqpBrokerCollectionXml::parse(
	const SmartPtrCXmlElement thisXml) {
	CAF_CM_STATIC_FUNC_VALIDATE("AmqpBrokerCollectionXml", "parse");
	CAF_CM_VALIDATE_SMARTPTR(thisXml);

	const CXmlElement::SmartPtrCElementCollection amqpBrokerChildrenXml =
		thisXml->findOptionalChildren("amqpBroker");

	std::deque<SmartPtrCAmqpBrokerDoc> amqpBrokerVal;
	if (! amqpBrokerChildrenXml.IsNull() && ! amqpBrokerChildrenXml->empty()) {
		for (TConstIterator<CXmlElement::CElementCollection> amqpBrokerXmlIter(*amqpBrokerChildrenXml);
			amqpBrokerXmlIter; amqpBrokerXmlIter++) {
			const SmartPtrCXmlElement amqpBrokerXml = amqpBrokerXmlIter->second;
			const SmartPtrCAmqpBrokerDoc amqpBrokerDoc =
				AmqpBrokerXml::parse(amqpBrokerXml);
			amqpBrokerVal.push_back(amqpBrokerDoc);
		}
	}

	SmartPtrCAmqpBrokerCollectionDoc amqpBrokerCollectionDoc;
	amqpBrokerCollectionDoc.CreateInstance();
	amqpBrokerCollectionDoc->initialize(amqpBrokerVal);

	return amqpBrokerCollectionDoc;
}

