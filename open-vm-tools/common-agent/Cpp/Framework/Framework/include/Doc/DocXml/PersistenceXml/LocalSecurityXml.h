/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef LocalSecurityXml_h_
#define LocalSecurityXml_h_


#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"

#include "Doc/DocXml/PersistenceXml/PersistenceXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

	/// Streams the LocalSecurity class to/from XML
	namespace LocalSecurityXml {

		/// Adds the LocalSecurityDoc into the XML.
		void PERSISTENCEXML_LINKAGE add(
			const SmartPtrCLocalSecurityDoc localSecurityDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the LocalSecurityDoc from the XML.
		SmartPtrCLocalSecurityDoc PERSISTENCEXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
