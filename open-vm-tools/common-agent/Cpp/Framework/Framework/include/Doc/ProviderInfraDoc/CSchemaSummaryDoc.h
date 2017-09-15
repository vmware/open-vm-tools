/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CSchemaSummaryDoc_h_
#define CSchemaSummaryDoc_h_


#include "Doc/ProviderInfraDoc/CClassCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type SchemaSummary
class PROVIDERINFRADOC_LINKAGE CSchemaSummaryDoc {
public:
	CSchemaSummaryDoc();
	virtual ~CSchemaSummaryDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string providerNamespace,
		const std::string providerName,
		const std::string providerVersion,
		const SmartPtrCClassCollectionDoc classCollection,
		const std::string invokerPath);

public:
	/// Accessor for the ProviderNamespace
	std::string getProviderNamespace() const;

	/// Accessor for the ProviderName
	std::string getProviderName() const;

	/// Accessor for the ProviderVersion
	std::string getProviderVersion() const;

	/// Accessor for the ClassCollection
	SmartPtrCClassCollectionDoc getClassCollection() const;

	/// Accessor for the InvokerPath
	std::string getInvokerPath() const;

private:
	bool _isInitialized;

	std::string _providerNamespace;
	std::string _providerName;
	std::string _providerVersion;
	SmartPtrCClassCollectionDoc _classCollection;
	std::string _invokerPath;

private:
	CAF_CM_DECLARE_NOCOPY(CSchemaSummaryDoc);
};

CAF_DECLARE_SMART_POINTER(CSchemaSummaryDoc);

}

#endif
