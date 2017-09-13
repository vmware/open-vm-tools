/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef AmqpBrokerCollectionXml_h_
#define AmqpBrokerCollectionXml_h_

namespace Caf {

	/// Streams the AmqpBrokerCollection class to/from XML
	namespace AmqpBrokerCollectionXml {

		/// Adds the AmqpBrokerCollectionDoc into the XML.
		void CAFCORETYPESXML_LINKAGE add(
			const SmartPtrCAmqpBrokerCollectionDoc amqpBrokerCollectionDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the AmqpBrokerCollectionDoc from the XML.
		SmartPtrCAmqpBrokerCollectionDoc CAFCORETYPESXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
