/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CPathBuilder.h"

using namespace Caf;

std::string CPathBuilder::calcInstallProviderDir() {
	std::string installProviderDir;

	CAF_CM_ENTER
	{
		const std::string installDir = getProviderHostConfigDir(
			_sConfigInstallDir);
		installProviderDir = FileSystemUtils::buildPath(installDir, "providers");

		if (!FileSystemUtils::doesDirectoryExist(installProviderDir)) {
			FileSystemUtils::createDirectory(installProviderDir);
		}
	}
	CAF_CM_EXIT;

	return installProviderDir;
}

std::string CPathBuilder::calcInstallPackageDir() {
	std::string installPackageDir;

	CAF_CM_ENTER
	{
		const std::string installDir = getProviderHostConfigDir(
			_sConfigInstallDir);
		installPackageDir = FileSystemUtils::buildPath(installDir, "packages");

		if (!FileSystemUtils::doesDirectoryExist(installPackageDir)) {
			FileSystemUtils::createDirectory(installPackageDir);
		}
	}
	CAF_CM_EXIT;

	return installPackageDir;
}

std::string CPathBuilder::calcInstallProviderDir(
	const std::string& providerNamespace,
	const std::string& providerName,
	const std::string& providerVersion) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPathBuilder", "calcInstallProviderDir");

	std::string installProviderDir;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(providerNamespace);
		CAF_CM_VALIDATE_STRING(providerName);
		CAF_CM_VALIDATE_STRING(providerVersion);

		const std::string installDir = getProviderHostConfigDir(
			_sConfigInstallDir);
		const std::string providersDir = FileSystemUtils::buildPath(installDir, "providers");
		installProviderDir = calcDir(providerNamespace, providerName, providerVersion,
			providersDir);

		if (!FileSystemUtils::doesDirectoryExist(installProviderDir)) {
			FileSystemUtils::createDirectory(installProviderDir);
		}
	}
	CAF_CM_EXIT;

	return installProviderDir;
}

std::string CPathBuilder::calcInstallPackageDir(
	const std::string& packageNamespace,
	const std::string& packageName,
	const std::string& packageVersion) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPathBuilder", "calcInstallPackageDir");

	std::string installPackageDir;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(packageNamespace);
		CAF_CM_VALIDATE_STRING(packageName);
		CAF_CM_VALIDATE_STRING(packageVersion);

		const std::string installDir = getProviderHostConfigDir(
			_sConfigInstallDir);
		const std::string packagesDir = FileSystemUtils::buildPath(installDir, "packages");
		installPackageDir = calcDir(packageNamespace, packageName, packageVersion, packagesDir);

		if (!FileSystemUtils::doesDirectoryExist(installPackageDir)) {
			FileSystemUtils::createDirectory(installPackageDir);
		}
	}
	CAF_CM_EXIT;

	return installPackageDir;
}

std::string CPathBuilder::calcProviderSchemaCacheDir(
	const std::string& providerNamespace,
	const std::string& providerName,
	const std::string& providerVersion) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPathBuilder", "calcProviderSchemaCacheDir");

	std::string providerSchemaCacheDir;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(providerNamespace);
		CAF_CM_VALIDATE_STRING(providerName);
		CAF_CM_VALIDATE_STRING(providerVersion);

		const std::string schemaCacheDir = getProviderHostConfigDir(
			_sConfigSchemaCacheDir);
		providerSchemaCacheDir = calcDir(providerNamespace, providerName, providerVersion,
			schemaCacheDir);
	}
	CAF_CM_EXIT;

	return providerSchemaCacheDir;
}

std::string CPathBuilder::calcDir(
	const std::string& thisNamespace,
	const std::string& thisName,
	const std::string& thisVersion,
	const std::string& outputDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPathBuilder", "calcDir");

	std::string rc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(thisNamespace);
		CAF_CM_VALIDATE_STRING(thisName);
		CAF_CM_VALIDATE_STRING(thisVersion);
		CAF_CM_VALIDATE_STRING(outputDir);

		const std::string dirName = thisNamespace + "_" + thisName + "_" + thisVersion;
		rc = FileSystemUtils::buildPath(outputDir, dirName);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CPathBuilder::getRootConfigDir(const std::string& configName) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPathBuilder", "getRootConfigDir");

	std::string rc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(configName);

		const std::string configDir = AppConfigUtils::getRequiredString(configName);
		const std::string configDirExp = CStringUtils::expandEnv(configDir);
		if (!FileSystemUtils::doesDirectoryExist(configDirExp)) {
			CAF_CM_LOG_DEBUG_VA2(
				"AppConfig directory does not exist... Creating - name: %s, dir: %s",
				configName.c_str(), configDirExp.c_str());
			FileSystemUtils::createDirectory(configDirExp);
		}

		rc = FileSystemUtils::normalizePathForPlatform(configDirExp);
	}
	CAF_CM_EXIT;

	return rc;
}

std::string CPathBuilder::getProviderHostConfigDir(const std::string& configName) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPathBuilder", "getProviderHostConfigDir");

	std::string rc;

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STRING(configName);

		const std::string configDir = AppConfigUtils::getRequiredString(_sProviderHostArea,
			configName);
		const std::string configDirExp = CStringUtils::expandEnv(configDir);
		if (!FileSystemUtils::doesDirectoryExist(configDirExp)) {
			CAF_CM_LOG_DEBUG_VA2(
				"AppConfig directory does not exist... Creating - name: %s, dir: %s",
				configName.c_str(), configDirExp.c_str());
			FileSystemUtils::createDirectory(configDirExp);
		}

		rc = FileSystemUtils::normalizePathForPlatform(configDirExp);
	}
	CAF_CM_EXIT;

	return rc;
}
