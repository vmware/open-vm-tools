/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CProviderRegDoc_h_
#define CProviderRegDoc_h_

namespace Caf {

/// A simple container for objects of type ProviderReg
class PROVIDERINFRADOC_LINKAGE CProviderRegDoc {
public:
	CProviderRegDoc();
	virtual ~CProviderRegDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string providerNamespace,
		const std::string providerName,
		const std::string providerVersion,
		const int32 staleSec,
		const bool isSchemaVisible,
		const std::string invokerRelPath);

public:
	/// Accessor for the ProviderNamespace
	std::string getProviderNamespace() const;

	/// Accessor for the ProviderName
	std::string getProviderName() const;

	/// Accessor for the ProviderVersion
	std::string getProviderVersion() const;

	/// Accessor for the StaleSec
	int32 getStaleSec() const;

	/// Accessor for the IsSchemaVisible
	bool getIsSchemaVisible() const;

	/// Accessor for the InvokerRelPath
	std::string getInvokerRelPath() const;

private:
	std::string _providerNamespace;
	std::string _providerName;
	std::string _providerVersion;
	int32 _staleSec;
	bool _isSchemaVisible;
	std::string _invokerRelPath;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CProviderRegDoc);
};

CAF_DECLARE_SMART_POINTER(CProviderRegDoc);

}

#endif
