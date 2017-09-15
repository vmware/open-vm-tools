/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef PersistenceXml_h_
#define PersistenceXml_h_


#include "Doc/PersistenceDoc/CPersistenceDoc.h"

#include "Doc/DocXml/PersistenceXml/PersistenceXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

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
