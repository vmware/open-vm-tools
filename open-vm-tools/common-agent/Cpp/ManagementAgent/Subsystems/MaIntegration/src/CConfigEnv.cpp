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
	CAF_CM_INIT_THREADSAFE;
}

CConfigEnv::~CConfigEnv() {
}

void CConfigEnv::initializeBean(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties) {
}

void CConfigEnv::terminateBean() {
}

void CConfigEnv::initialize(
		const SmartPtrIPersistence& persistenceRemove) {
	CAF_CM_LOCK_UNLOCK;

	if (_isInitialized) {
		if (! persistenceRemove.IsNull() && _persistenceRemove.IsNull()) {
			_persistenceRemove = persistenceRemove;
		}
	} else {
		_persistenceDir = AppConfigUtils::getRequiredString("persistence_dir");
		_scriptsDir = AppConfigUtils::getRequiredString("scripts_dir");
		_outputDir = AppConfigUtils::getRequiredString(_sAppConfigGlobalParamOutputDir);
		_configDir = AppConfigUtils::getRequiredString("config_dir");
		_persistenceAppconfigPath = FileSystemUtils::buildPath(_configDir, "persistence-appconfig");

		std::string guestProxyDir;
#ifdef _WIN32
		_startListenerScript = FileSystemUtils::buildPath(_scriptsDir, "start-listener.bat");
		_stopListenerScript = FileSystemUtils::buildPath(_scriptsDir, "stop-listener.bat");
		_startMaScript = FileSystemUtils::buildPath(_scriptsDir, "start-ma.bat");
		_stopMaScript = FileSystemUtils::buildPath(_scriptsDir, "stop-ma.bat");

		std::string programData;
		CEnvironmentUtils::readEnvironmentVar("ProgramData", programData);
		guestProxyDir = FileSystemUtils::buildPath(programData, "VMware", "VMware Tools", "GuestProxyData");
#else
		_startListenerScript = FileSystemUtils::buildPath(_scriptsDir, "start-listener");
		_stopListenerScript = FileSystemUtils::buildPath(_scriptsDir, "stop-listener");
		_startMaScript = FileSystemUtils::buildPath(_scriptsDir, "start-ma");
		_stopMaScript = FileSystemUtils::buildPath(_scriptsDir, "stop-ma");

		guestProxyDir = "/etc/vmware-tools/GuestProxyData";
#endif

		_vcidPath = FileSystemUtils::buildPath(guestProxyDir, "VmVcUuid", "vm.vc.uuid");
		_cacertPath = FileSystemUtils::buildPath(guestProxyDir, "server", "cert.pem");

		_persistence = CPersistenceUtils::loadPersistence(_persistenceDir);
		_persistenceUpdated = _persistence;
		_persistenceRemove = persistenceRemove;

		_isInitialized = true;
	}
}

SmartPtrCPersistenceDoc CConfigEnv::getUpdated(
		const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("getUpdated");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCPersistenceDoc persistenceTmp =
			CConfigEnvMerge::mergePersistence(_persistence, _cacertPath, _vcidPath);
	if (! persistenceTmp.IsNull() || ! FileSystemUtils::doesFileExist(_persistenceAppconfigPath)) {
		if (! persistenceTmp.IsNull()) {
			_persistence = persistenceTmp;
		}

		savePersistenceAppconfig(_persistence, _configDir);
		restartRunningListener();
	}

	SmartPtrCPersistenceDoc rc;
	if (! _persistenceUpdated.IsNull()) {
		CAF_CM_LOG_DEBUG_VA1("Returning persistence info - %s", _persistenceDir.c_str());
		rc = _persistenceUpdated;
		_persistenceUpdated = SmartPtrCPersistenceDoc();
	}

	return rc;
}

void CConfigEnv::update(
		const SmartPtrCPersistenceDoc& persistence) {
	CAF_CM_FUNCNAME_VALIDATE("update");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCPersistenceDoc persistenceTmp =
			CPersistenceMerge::mergePersistence(_persistence, persistence);
	if (! persistenceTmp.IsNull()) {
		CAF_CM_LOG_DEBUG_VA1("Updating persistence info - %s", _persistenceDir.c_str());

		_persistence = persistenceTmp;
		_persistenceUpdated = createPersistenceUpdated(_persistence);
		CPersistenceUtils::savePersistence(_persistence, _persistenceDir);

		restartRunningListener();

		removePrivateKey(_persistence, _persistenceRemove);
	} else {
		CAF_CM_LOG_DEBUG_VA0("Persistence info did not change");
	}
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
	CAF_CM_LOG_DEBUG_VA1("Saving persistence-appconfig - %s", configDir.c_str());

	const SmartPtrCPersistenceProtocolDoc persistenceProtocol =
			CPersistenceUtils::loadPersistenceProtocol(
					persistence->getPersistenceProtocolCollection());

	UriUtils::SUriRecord uriRecord;
	UriUtils::parseUriString(persistenceProtocol->getUri(), uriRecord);
	CAF_CM_VALIDATE_STRING(uriRecord.path);

	const std::string listenerContext = calcListenerContext(uriRecord.protocol, configDir);

	CAF_CM_LOG_DEBUG_VA2("Calculated listener context - uri: %s, protocol: %s",
			persistenceProtocol->getUri().c_str(), uriRecord.protocol.c_str());

	std::string appconfigContents;
	appconfigContents = "[globals]" + newLine;
	appconfigContents += "reactive_request_amqp_queue_id=" + uriRecord.path + newLine;
	appconfigContents += "comm_amqp_listener_context=" + listenerContext + newLine;

	FileSystemUtils::saveTextFile(_persistenceAppconfigPath, appconfigContents);
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

void CConfigEnv::removePrivateKey(
		const SmartPtrCPersistenceDoc& persistence,
		const SmartPtrIPersistence& persistenceRemove) const {
	CAF_CM_FUNCNAME_VALIDATE("removePrivateKey");
	CAF_CM_VALIDATE_SMARTPTR(persistence);

	if (! persistenceRemove.IsNull()
			&& ! persistence->getLocalSecurity()->getPrivateKey().empty()) {
		CAF_CM_LOG_DEBUG_VA0("Removing private key");

		SmartPtrCLocalSecurityDoc localSecurity;
		localSecurity.CreateInstance();
		localSecurity->initialize(std::string(), "removePrivateKey");

		SmartPtrCPersistenceDoc persistenceRemoveTmp;
		persistenceRemoveTmp.CreateInstance();
		persistenceRemoveTmp->initialize(localSecurity);

		persistenceRemove->remove(persistenceRemoveTmp);
	}
}

SmartPtrCPersistenceDoc CConfigEnv::createPersistenceUpdated(
		const SmartPtrCPersistenceDoc& persistence) const {
	CAF_CM_FUNCNAME_VALIDATE("createPersistenceUpdated");
	CAF_CM_VALIDATE_SMARTPTR(persistence);

	SmartPtrCLocalSecurityDoc localSecurity;
	if (! persistence->getLocalSecurity().IsNull()
			&& ! persistence->getLocalSecurity()->getLocalId().empty()) {
		localSecurity.CreateInstance();
		localSecurity->initialize(persistence->getLocalSecurity()->getLocalId());
	}

	SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollection;
	if (! persistence->getPersistenceProtocolCollection().IsNull()
			&& ! persistence->getPersistenceProtocolCollection()->getPersistenceProtocol().empty()) {
		const SmartPtrCPersistenceProtocolDoc persistenceProtocolTmp =
				CPersistenceUtils::loadPersistenceProtocol(persistence->getPersistenceProtocolCollection());
		if (! persistenceProtocolTmp->getUri().empty()) {
			SmartPtrCPersistenceProtocolDoc persistenceProtocol;
			persistenceProtocol.CreateInstance();
			persistenceProtocol->initialize(
					persistenceProtocolTmp->getProtocolName(),
					persistenceProtocolTmp->getUri());

			std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolInner;
			persistenceProtocolInner.push_back(persistenceProtocol);

			persistenceProtocolCollection.CreateInstance();
			persistenceProtocolCollection->initialize(persistenceProtocolInner);
		}
	}

	SmartPtrCPersistenceDoc rc;
	if (! localSecurity.IsNull() || ! persistenceProtocolCollection.IsNull()) {
		rc.CreateInstance();
		rc->initialize(localSecurity, SmartPtrCRemoteSecurityCollectionDoc(),
				persistenceProtocolCollection, persistence->getVersion());
	}

	return rc;
}

std::string CConfigEnv::calcListenerContext(
		const std::string& uriSchema,
		const std::string& configDir) const {
	CAF_CM_FUNCNAME("calcListenerContext");
	CAF_CM_VALIDATE_STRING(uriSchema);
	CAF_CM_VALIDATE_STRING(configDir);

	std::string rc;
	if (uriSchema.compare("amqp") == 0) {
		rc = FileSystemUtils::buildPath(configDir, "CommAmqpListener-context-amqp.xml");
	} else if (uriSchema.compare("tunnel") == 0) {
		rc = FileSystemUtils::buildPath(configDir, "CommAmqpListener-context-tunnel.xml");
	} else {
		CAF_CM_EXCEPTION_VA1(E_INVALIDARG, "Unknown URI schema: %s", uriSchema.c_str());
	}

	return FileSystemUtils::normalizePathWithForward(rc);
}

void CConfigEnv::restartRunningListener() const {
	// TODO: It's difficult to know when we should start the listener, because
	// it depends upon whether its been fully configured or not, and we don't
	// know when that happens within this code. All we know is that we've
	// read whatever info is on the file system and in the NSDDB, but that's it.
	// Perhaps the best we can do is to re-start the listener only if it's already
	// running because at least then we know that it's been configured.
	//executeScript(_stopListenerScript, _outputDir);
	//executeScript(_startListenerScript, _outputDir);
}
