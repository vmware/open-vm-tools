/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef PersistenceProtocolCollectionXml_h_
#define PersistenceProtocolCollectionXml_h_


#include "Doc/PersistenceDoc/CPersistenceProtocolCollectionDoc.h"

#include "Doc/DocXml/CafCoreTypesXml/CafCoreTypesXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

	/// Streams the PersistenceProtocolCollection class to/from XML
	namespace PersistenceProtocolCollectionXml {

		/// Adds the PersistenceProtocolCollectionDoc into the XML.
		void CAFCORETYPESXML_LINKAGE add(
			const SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollectionDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the PersistenceProtocolCollectionDoc from the XML.
		SmartPtrCPersistenceProtocolCollectionDoc CAFCORETYPESXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
