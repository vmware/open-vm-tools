/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CertCollectionXml_h_
#define CertCollectionXml_h_


#include "Doc/PersistenceDoc/CCertCollectionDoc.h"

#include "Doc/DocXml/CafCoreTypesXml/CafCoreTypesXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

	/// Streams the CertCollection class to/from XML
	namespace CertCollectionXml {

		/// Adds the CertCollectionDoc into the XML.
		void CAFCORETYPESXML_LINKAGE add(
			const SmartPtrCCertCollectionDoc certCollectionDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the CertCollectionDoc from the XML.
		SmartPtrCCertCollectionDoc CAFCORETYPESXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
