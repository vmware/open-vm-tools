/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef PersistenceProtocolXml_h_
#define PersistenceProtocolXml_h_


#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"

#include "Doc/DocXml/PersistenceXml/PersistenceXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

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
