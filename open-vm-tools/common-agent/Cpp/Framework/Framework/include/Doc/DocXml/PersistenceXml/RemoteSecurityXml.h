/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef RemoteSecurityXml_h_
#define RemoteSecurityXml_h_


#include "Doc/PersistenceDoc/CRemoteSecurityDoc.h"

#include "Doc/DocXml/PersistenceXml/PersistenceXmlLink.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

	/// Streams the RemoteSecurity class to/from XML
	namespace RemoteSecurityXml {

		/// Adds the RemoteSecurityDoc into the XML.
		void PERSISTENCEXML_LINKAGE add(
			const SmartPtrCRemoteSecurityDoc remoteSecurityDoc,
			const SmartPtrCXmlElement thisXml);

		/// Parses the RemoteSecurityDoc from the XML.
		SmartPtrCRemoteSecurityDoc PERSISTENCEXML_LINKAGE parse(
			const SmartPtrCXmlElement thisXml);
	}
}

#endif
