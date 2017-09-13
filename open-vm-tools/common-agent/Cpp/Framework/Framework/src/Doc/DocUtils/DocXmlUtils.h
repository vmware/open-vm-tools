/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef DocXmlUtils_h_
#define DocXmlUtils_h_

namespace Caf {

	namespace DocXmlUtils {
		std::string DOCUTILS_LINKAGE getSchemaNamespace(
			const std::string& relSchemaNamespace);

		std::string DOCUTILS_LINKAGE getSchemaLocation(
			const std::string& relSchemaLocation);
	}
}

#endif
