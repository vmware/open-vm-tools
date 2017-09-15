/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CEventManifestDoc_h_
#define CEventManifestDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type EventManifest
class RESPONSEDOC_LINKAGE CEventManifestDoc {
public:
	CEventManifestDoc();
	virtual ~CEventManifestDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string classNamespace,
		const std::string className,
		const std::string classVersion,
		const std::string operationName,
		const SmartPtrCAttachmentCollectionDoc attachmentCollection);

public:
	/// Accessor for the ClassNamespace
	std::string getClassNamespace() const;

	/// Accessor for the ClassName
	std::string getClassName() const;

	/// Accessor for the ClassVersion
	std::string getClassVersion() const;

	/// Accessor for the OperationName
	std::string getOperationName() const;

	/// Accessor for the AttachmentCollection
	SmartPtrCAttachmentCollectionDoc getAttachmentCollection() const;

private:
	bool _isInitialized;

	std::string _classNamespace;
	std::string _className;
	std::string _classVersion;
	std::string _operationName;
	SmartPtrCAttachmentCollectionDoc _attachmentCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CEventManifestDoc);
};

CAF_DECLARE_SMART_POINTER(CEventManifestDoc);

}

#endif
