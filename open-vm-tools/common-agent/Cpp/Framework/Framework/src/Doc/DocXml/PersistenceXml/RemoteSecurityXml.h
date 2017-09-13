/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef RemoteSecurityXml_h_
#define RemoteSecurityXml_h_

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
