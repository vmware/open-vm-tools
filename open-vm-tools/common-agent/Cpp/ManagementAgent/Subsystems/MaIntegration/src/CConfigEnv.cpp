/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CConfigEnv.h"

using namespace Caf;

CConfigEnv::CConfigEnv() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CConfigEnv") {
}

CConfigEnv::~CConfigEnv() {
}

void CConfigEnv::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	_persistenceDir = AppConfigUtils::getRequiredString("persistence_dir");
	_scriptsDir = AppConfigUtils::getRequiredString("scripts_dir");
	_outputDir = AppConfigUtils::getRequiredString(_sAppConfigGlobalParamOutputDir);
	_configDir = AppConfigUtils::getRequiredString("config_dir");
	_vcidPath = "/etc/vmware-tools/GuestProxyData/VmVcUuid/vm.vc.uuid";

#ifdef _WIN32
	_startListenerScript = FileSystemUtils::buildPath(_scriptsDir, "start-listener.bat");
	_stopListenerScript = FileSystemUtils::buildPath(_scriptsDir, "stop-listener.bat");
	_startMaScript = FileSystemUtils::buildPath(_scriptsDir, "start-ma.bat");
	_stopMaScript = FileSystemUtils::buildPath(_scriptsDir, "stop-ma.bat");
#else
	_startListenerScript = FileSystemUtils::buildPath(_scriptsDir, "start-listener");
	_stopListenerScript = FileSystemUtils::buildPath(_scriptsDir, "stop-listener");
	_startMaScript = FileSystemUtils::buildPath(_scriptsDir, "start-ma");
	_stopMaScript = FileSystemUtils::buildPath(_scriptsDir, "stop-ma");
#endif

	_isInitialized = true;
}

SmartPtrCPersistenceDoc CConfigEnv::getUpdated(
		const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("getUpdated");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (_persistence.IsNull()) {
		_persistence = CPersistenceUtils::loadPersistence(_persistenceDir);
	}

	return _persistence;
}

void CConfigEnv::update(
		const SmartPtrCPersistenceDoc& persistence) {
	CAF_CM_FUNCNAME_VALIDATE("update");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (_persistence.IsNull()) {
		_persistence = CPersistenceUtils::loadPersistence(_persistenceDir);
	}

	const SmartPtrCPersistenceDoc persistenceTmp = merge(_persistence, persistence);
	if (_persistence != persistenceTmp) {
		_persistence = persistenceTmp;
		CPersistenceUtils::savePersistence(_persistence, _persistenceDir);
		savePersistenceAppconfig(_persistence, _configDir);

		//executeScript(_stopListenerScript, _outputDir);
		//executeScript(_stopMaScript, _outputDir);
		//executeScript(_startListenerScript, _outputDir);
		//executeScript(_startMaScript, _outputDir);
	}
}

void CConfigEnv::remove(
		const SmartPtrCPersistenceDoc& persistence) {
	CAF_CM_FUNCNAME("remove");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(persistence);

	CAF_CM_EXCEPTIONEX_VA0(UnsupportedOperationException, E_NOTIMPL, "Not implemented");
}

SmartPtrCPersistenceDoc CConfigEnv::merge(
		const SmartPtrCPersistenceDoc& persistenceLoaded,
		const SmartPtrCPersistenceDoc& persistenceIn) const {
	// persistenceIn is non-null only when it has a change.
	bool isChanged = false;
	SmartPtrCPersistenceDoc persistenceTmp = persistenceLoaded;
	if (! persistenceIn.IsNull()) {
		persistenceTmp = persistenceIn;
		isChanged = true;
	}

	const SmartPtrCLocalSecurityDoc localSecurity =
			calcLocalSecurity(persistenceTmp->getLocalSecurity());

	//TODO: Do something similar to the above for tunnel vs. non-tunnel:
	// * Tunnel - Use "/etc/vmware-tools/GuestProxyData/server/cert.pem" for the cacert
	// * Tunnel - Calculate the URL with a protocol of "tunnel"
	// * Non-tunnel - Calculate the URL with a protocol of "amqp"

	SmartPtrCPersistenceDoc rc = persistenceLoaded;
	if (isChanged || ! localSecurity.IsNull()) {
		const SmartPtrCLocalSecurityDoc localSecurityTmp =
				localSecurity.IsNull() ? persistenceTmp->getLocalSecurity() : localSecurity;

		rc.CreateInstance();
		rc->initialize(localSecurityTmp, persistenceTmp->getRemoteSecurityCollection(),
				persistenceTmp->getPersistenceProtocol());
	}

	return rc;
}

SmartPtrCLocalSecurityDoc CConfigEnv::calcLocalSecurity(
		const SmartPtrCLocalSecurityDoc& localSecurity) const {
	CAF_CM_FUNCNAME_VALIDATE("calcLocalSecurity");
	CAF_CM_VALIDATE_SMARTPTR(localSecurity);

	const std::string localIdTmp =
			localSecurity.IsNull() ? std::string() : localSecurity->getLocalId();
	const std::string localId = calcLocalId(localIdTmp);

	SmartPtrCLocalSecurityDoc rc;
	if (! localId.empty()) {
		const std::string privateKey =
				localSecurity.IsNull() ? std::string() : localSecurity->getPrivateKey();
		const std::string cert =
				localSecurity.IsNull() ? std::string() : localSecurity->getCert();
		rc.CreateInstance();
		rc->initialize(localId, privateKey, cert);
	}

	return rc;
}

std::string CConfigEnv::calcLocalId(
		const std::string& localIdCurrent) const {
	std::string rc;
	if (FileSystemUtils::doesFileExist(_vcidPath)) {
		const std::string vcid = FileSystemUtils::loadTextFile(_vcidPath);
		if (vcid.compare(localIdCurrent) != 0) {
			rc = vcid;
		}
	} else {
		if (localIdCurrent.empty()) {
			rc = CStringUtils::createRandomUuid();
		}
	}

	return rc;
}

void CConfigEnv::savePersistenceAppconfig(
		const SmartPtrCPersistenceDoc& persistence,
		const std::string& configDir) const {
	CAF_CM_FUNCNAME_VALIDATE("savePersistenceAppconfig");
	CAF_CM_VALIDATE_SMARTPTR(persistence);
	CAF_CM_VALIDATE_STRING(configDir);

	#ifdef WIN32
	const std::string newLine = "\r\n";
#else
	const std::string newLine = "\n";
#endif

	const std::string appconfigPath =
			FileSystemUtils::buildPath(configDir, "persistence-appconfig");
	const Cdeqstr appconfigColl =
			FileSystemUtils::loadTextFileIntoColl(appconfigPath);

	std::string appconfigContents;
	for (TConstIterator<Cdeqstr> iter(appconfigColl); iter; iter++) {
		const std::string appconfigLine = *iter;
		if (appconfigLine.find("reactive_request_amqp_queue_id=") != std::string::npos) {
			const SmartPtrCAmqpBrokerDoc amqpBroker =
					CPersistenceUtils::loadAmqpBroker(persistence->getPersistenceProtocol());
			appconfigContents +=
					"reactive_request_amqp_queue_id=" + amqpBroker->getAmqpBrokerId() + newLine;
		} else {
			appconfigContents += appconfigLine + newLine;
		}
	}

	FileSystemUtils::saveTextFile(appconfigPath, appconfigContents);
}

void CConfigEnv::executeScript(
	const std::string& scriptPath,
	const std::string& scriptResultsDir) const {
	CAF_CM_FUNCNAME_VALIDATE("executeScript");
	CAF_CM_VALIDATE_STRING(scriptPath);
	CAF_CM_VALIDATE_STRING(scriptResultsDir);

	Cdeqstr argv;
	argv.push_back(scriptPath);

	const std::string stdoutPath = FileSystemUtils::buildPath(
			scriptResultsDir, "stdout");
	const std::string stderrPath = FileSystemUtils::buildPath(
			scriptResultsDir, "stderr");

	ProcessUtils::runSyncToFiles(argv, stdoutPath, stderrPath);
}
