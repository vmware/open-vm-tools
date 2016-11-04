/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CSchemaCacheManager_h_
#define CSchemaCacheManager_h_


#include "Doc/CafCoreTypesDoc/CFullyQualifiedClassGroupDoc.h"
#include "Doc/ProviderInfraDoc/CSchemaSummaryDoc.h"

namespace Caf {

/// Simple container class for identifying a class.
class CClassId {
public:
	SmartPtrCFullyQualifiedClassGroupDoc _fqc;
	std::string toString() const {
		return _fqc->getClassNamespace() + "::" + _fqc->getClassName() + "::" + _fqc->getClassVersion();
	}
};
bool operator< (const CClassId& lhs, const CClassId& rhs);

/// Creates a provider request.
class CSchemaCacheManager {
private:
	typedef std::map<CClassId, std::string> CClassCollection;

public:
	CSchemaCacheManager();
	virtual ~CSchemaCacheManager();

public:
	void initialize();

	std::string findProvider(
		const SmartPtrCFullyQualifiedClassGroupDoc& fqc);

private:
	void processSchemaSummaries(
		const std::string& schemaCacheDirPath,
		CClassCollection& classCollection) const;

	void addNewClasses(
		const SmartPtrCSchemaSummaryDoc& schemaSummary,
		const std::string& schemaSummaryFilePath,
		CClassCollection& classCollection) const;

	void waitForSchemaCacheCreation(
		const std::string& schemaCacheDir,
		const uint16 maxWaitSecs) const;

private:
	bool _isInitialized;
	std::string _schemaCacheDirPath;
	CClassCollection _classCollection;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CSchemaCacheManager);
};

CAF_DECLARE_SMART_POINTER(CSchemaCacheManager);

}

#endif // #ifndef CSchemaCacheManager_h_
