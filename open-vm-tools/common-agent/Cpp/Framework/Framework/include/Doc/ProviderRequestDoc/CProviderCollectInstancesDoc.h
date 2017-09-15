/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderCollectInstancesDoc_h_
#define CProviderCollectInstancesDoc_h_


#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type ProviderCollectInstances
class PROVIDERREQUESTDOC_LINKAGE CProviderCollectInstancesDoc {
public:
	CProviderCollectInstancesDoc();
	virtual ~CProviderCollectInstancesDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string classNamespace,
		const std::string className,
		const std::string classVersion,
		const UUID jobId,
		const std::string outputDir,
		const SmartPtrCParameterCollectionDoc parameterCollection);

public:
	/// Accessor for the ClassNamespace
	std::string getClassNamespace() const;

	/// Accessor for the ClassName
	std::string getClassName() const;

	/// Accessor for the ClassVersion
	std::string getClassVersion() const;

	/// Accessor for the JobId
	UUID getJobId() const;

	/// Accessor for the OutputDir
	std::string getOutputDir() const;

	/// Accessor for the ParameterCollection
	SmartPtrCParameterCollectionDoc getParameterCollection() const;

private:
	std::string _classNamespace;
	std::string _className;
	std::string _classVersion;
	UUID _jobId;
	std::string _outputDir;
	SmartPtrCParameterCollectionDoc _parameterCollection;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderCollectInstancesDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderCollectInstancesDoc);

}

#endif
