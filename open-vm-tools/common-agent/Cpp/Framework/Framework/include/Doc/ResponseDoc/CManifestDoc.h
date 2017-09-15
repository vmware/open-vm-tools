/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CManifestDoc_h_
#define CManifestDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type Manifest
class RESPONSEDOC_LINKAGE CManifestDoc {
public:
	CManifestDoc();
	virtual ~CManifestDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string classNamespace,
		const std::string className,
		const std::string classVersion,
		const UUID jobId,
		const std::string operationName,
		const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection);

public:
	/// Accessor for the ClassNamespace
	std::string getClassNamespace() const;

	/// Accessor for the ClassName
	std::string getClassName() const;

	/// Accessor for the ClassVersion
	std::string getClassVersion() const;

	/// Accessor for the JobId
	UUID getJobId() const;

	/// Accessor for the OperationName
	std::string getOperationName() const;

	/// Accessor for the AttachmentNameCollection
	SmartPtrCAttachmentNameCollectionDoc getAttachmentNameCollection() const;

private:
	std::string _classNamespace;
	std::string _className;
	std::string _classVersion;
	UUID _jobId;
	std::string _operationName;
	SmartPtrCAttachmentNameCollectionDoc _attachmentNameCollection;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CManifestDoc);
};

CAF_DECLARE_SMART_POINTER(CManifestDoc);

}

#endif
