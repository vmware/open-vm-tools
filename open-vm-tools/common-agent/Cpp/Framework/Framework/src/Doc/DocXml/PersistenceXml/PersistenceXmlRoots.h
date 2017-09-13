/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef PersistenceXmlRoots_h_
#define PersistenceXmlRoots_h_

namespace Caf {

	namespace XmlRoots {
		/// Saves the PersistenceDoc to a string.
		std::string PERSISTENCEXML_LINKAGE savePersistenceToString(
			const SmartPtrCPersistenceDoc persistenceDoc);

		/// Parses the PersistenceDoc from the string.
		SmartPtrCPersistenceDoc PERSISTENCEXML_LINKAGE parsePersistenceFromString(
			const std::string xml);

		/// Saves the PersistenceDoc to a file.
		void PERSISTENCEXML_LINKAGE savePersistenceToFile(
			const SmartPtrCPersistenceDoc persistenceDoc,
			const std::string filePath);

		/// Parses the PersistenceDoc from the file.
		SmartPtrCPersistenceDoc PERSISTENCEXML_LINKAGE parsePersistenceFromFile(
			const std::string filePath);
	}
}

#endif /* PersistenceXmlRoots_h_ */
