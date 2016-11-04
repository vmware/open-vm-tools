/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CPathBuilder_h_
#define CPathBuilder_h_

namespace Caf {

class CPathBuilder {
public:
	static std::string calcInstallPackageDir();

	static std::string calcInstallProviderDir();

	static std::string calcInstallProviderDir(
		const std::string& providerNamespace,
		const std::string& providerName,
		const std::string& providerVersion);

	static std::string calcInstallPackageDir(
		const std::string& packageNamespace,
		const std::string& packageName,
		const std::string& packageVersion);

	static std::string calcProviderSchemaCacheDir(
		const std::string& providerNamespace,
		const std::string& providerName,
		const std::string& providerVersion);

	static std::string calcDir(
		const std::string& thisNamespace,
		const std::string& thisName,
		const std::string& thisVersion,
		const std::string& outputDir);

	static std::string getRootConfigDir(const std::string& configName);

	static std::string getProviderHostConfigDir(const std::string& configName);

private:
	CAF_CM_DECLARE_NOCREATE( CPathBuilder);
};

}

#endif // #ifndef CPathBuilder_h_
