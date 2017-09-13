/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CertCollectionXml_h_
#define CertCollectionXml_h_

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
