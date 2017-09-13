/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef PersistenceXml_h_
#define PersistenceXml_h_

namespace Caf {

	/// Streams the Persistence class to/from XML
	namespace PersistenceXml {

		/// Adds the PersistenceDoc into the XML.
		void PERSISTENCEXML_LINKAGE add(
			const SmartPtrCPersistenceDoc persistenceDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the PersistenceDoc from the XML.
		SmartPtrCPersistenceDoc PERSISTENCEXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
