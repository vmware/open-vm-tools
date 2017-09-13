/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef AmqpBrokerXml_h_
#define AmqpBrokerXml_h_

namespace Caf {

	/// Streams the AmqpBroker class to/from XML
	namespace AmqpBrokerXml {

		/// Adds the AmqpBrokerDoc into the XML.
		void PERSISTENCEXML_LINKAGE add(
			const SmartPtrCAmqpBrokerDoc amqpBrokerDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the AmqpBrokerDoc from the XML.
		SmartPtrCAmqpBrokerDoc PERSISTENCEXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
