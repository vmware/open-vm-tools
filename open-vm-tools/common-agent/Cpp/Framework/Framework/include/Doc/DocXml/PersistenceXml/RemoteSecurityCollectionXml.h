/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef RemoteSecurityCollectionXml_h_
#define RemoteSecurityCollectionXml_h_


#include "Doc/PersistenceDoc/CRemoteSecurityCollectionDoc.h"

#include "Doc/DocXml/CafCoreTypesXml/CafCoreTypesXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

	/// Streams the RemoteSecurityCollection class to/from XML
	namespace RemoteSecurityCollectionXml {

		/// Adds the RemoteSecurityCollectionDoc into the XML.
		void CAFCORETYPESXML_LINKAGE add(
			const SmartPtrCRemoteSecurityCollectionDoc remoteSecurityCollectionDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the RemoteSecurityCollectionDoc from the XML.
		SmartPtrCRemoteSecurityCollectionDoc CAFCORETYPESXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
