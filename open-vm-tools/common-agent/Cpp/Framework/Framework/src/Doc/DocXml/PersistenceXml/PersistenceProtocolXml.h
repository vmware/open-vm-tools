/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef PersistenceProtocolXml_h_
#define PersistenceProtocolXml_h_

namespace Caf {

	/// Streams the PersistenceProtocol class to/from XML
	namespace PersistenceProtocolXml {

		/// Adds the PersistenceProtocolDoc into the XML.
		void PERSISTENCEXML_LINKAGE add(
			const SmartPtrCPersistenceProtocolDoc persistenceProtocolDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the PersistenceProtocolDoc from the XML.
		SmartPtrCPersistenceProtocolDoc PERSISTENCEXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
