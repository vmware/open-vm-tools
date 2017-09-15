/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDataClassPropertyDoc_h_
#define CDataClassPropertyDoc_h_


#include "Doc/SchemaTypesDoc/CCmdlMetadataDoc.h"

namespace Caf {

/// A simple container for objects of type DataClassProperty
class SCHEMATYPESDOC_LINKAGE CDataClassPropertyDoc {
public:
	CDataClassPropertyDoc();
	virtual ~CDataClassPropertyDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadata,
		const std::string value);

public:
	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the CmdlMetadata
	std::deque<SmartPtrCCmdlMetadataDoc> getCmdlMetadata() const;

	/// Accessor for the Value
	std::string getValue() const;

private:
	bool _isInitialized;

	std::string _name;
	std::deque<SmartPtrCCmdlMetadataDoc> _cmdlMetadata;
	std::string _value;

private:
	CAF_CM_DECLARE_NOCOPY(CDataClassPropertyDoc);
};

CAF_DECLARE_SMART_POINTER(CDataClassPropertyDoc);

}

#endif
