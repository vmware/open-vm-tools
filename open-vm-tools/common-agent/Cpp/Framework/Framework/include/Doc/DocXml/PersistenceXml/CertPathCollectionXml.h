/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CertPathCollectionXml_h_
#define CertPathCollectionXml_h_


#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"

#include "Doc/DocXml/CafCoreTypesXml/CafCoreTypesXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

	/// Streams the CertPathCollection class to/from XML
	namespace CertPathCollectionXml {

		/// Adds the CertPathCollectionDoc into the XML.
		void CAFCORETYPESXML_LINKAGE add(
			const SmartPtrCCertPathCollectionDoc certPathCollectionDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the CertPathCollectionDoc from the XML.
		SmartPtrCCertPathCollectionDoc CAFCORETYPESXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
