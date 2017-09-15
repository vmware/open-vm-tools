/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"
#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "IPersistence.h"
#include "Exception/CCafException.h"
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
		_persistenceRemove = persistenceRemove;

		_persistenceDir = AppConfigUtils::getRequiredString("persistence_dir");

		_configDir = AppConfigUtils::getRequiredString("config_dir");
		_persistenceAppconfigPath = FileSystemUtils::buildPath(_configDir, "persistence-appconfig");

		_monitorDir = AppConfigUtils::getRequiredString("monitor_dir");
		_restartListenerPath = FileSystemUtils::buildPath(_monitorDir, "restartListener.txt");
		_listenerConfiguredStage1Path = FileSystemUtils::buildPath(
				_monitorDir, "listenerConfiguredStage1.txt");
		_listenerConfiguredStage2Path = FileSystemUtils::buildPath(
				_monitorDir, "listenerConfiguredStage2.txt");

		std::string guestProxyDir;
#ifdef _WIN32
		std::string programData;
		CEnvironmentUtils::readEnvironmentVar("ProgramData", programData);
		guestProxyDir = FileSystemUtils::buildPath(programData, "VMware", "VMware Tools", "GuestProxyData");
#else
		guestProxyDir = "/etc/vmware-tools/GuestProxyData";
#endif

		_vcidPath = FileSystemUtils::buildPath(guestProxyDir, "VmVcUuid", "vm.vc.uuid");
		_cacertPath = FileSystemUtils::buildPath(guestProxyDir, "server", "cert.pem");

		_isInitialized = true;
	}
}

SmartPtrCPersistenceDoc CConfigEnv::getUpdated(
		const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("getUpdated");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (FileSystemUtils::doesFileExist(_listenerConfiguredStage1Path)) {
		if (_persistence.IsNull()) {
			_persistence = CPersistenceUtils::loadPersistence(_persistenceDir);
			if (FileSystemUtils::doesFileExist(_listenerConfiguredStage2Path)) {
				_persistenceUpdated = _persistence;
			}
		}

		const SmartPtrCPersistenceDoc persistenceTmp =
				CConfigEnvMerge::mergePersistence(_persistence, _cacertPath, _vcidPath);
		if (! persistenceTmp.IsNull()) {
			CPersistenceUtils::savePersistence(persistenceTmp, _persistenceDir);
			_persistence = CPersistenceUtils::loadPersistence(_persistenceDir);
			_persistenceUpdated = _persistence;

			savePersistenceAppconfig(_persistence, _configDir);

			const std::string reason = "Info changed in env";
			listenerConfiguredStage2(reason);
			restartListener(reason);
		}
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

	const SmartPtrCPersistenceDoc persistenceTmp1 =
			CPersistenceMerge::mergePersistence(_persistence, persistence);

	const SmartPtrCPersistenceDoc persistenceIn = persistenceTmp1.IsNull() ? _persistence : persistenceTmp1;
	const SmartPtrCPersistenceDoc persistenceTmp2 =
			CConfigEnvMerge::mergePersistence(persistenceIn, _cacertPath, _vcidPath);

	const SmartPtrCPersistenceDoc persistenceTmp = persistenceTmp2.IsNull() ? persistenceTmp1 : persistenceTmp2;

	if (! persistenceTmp.IsNull()) {
		CPersistenceUtils::savePersistence(persistenceTmp, _persistenceDir);
		_persistence = CPersistenceUtils::loadPersistence(_persistenceDir);
		_persistenceUpdated = _persistence;

		savePersistenceAppconfig(_persistence, _configDir);
		removePrivateKey(_persistence, _persistenceRemove);

		const std::string reason = "Info changed at source";
		listenerConfiguredStage1(reason);
		listenerConfiguredStage2(reason);
		restartListener(reason);
	}
}

void CConfigEnv::savePersistenceAppconfig(
		const SmartPtrCPersistenceDoc& persistence,
		const std::string& configDir) const {
	CAF_CM_FUNCNAME_VALIDATE("savePersistenceAppconfig");
	CAF_CM_VALIDATE_SMARTPTR(persistence);
	CAF_CM_VALIDATE_STRING(configDir);

	const SmartPtrCPersistenceProtocolDoc persistenceProtocol =
			CPersistenceUtils::loadPersistenceProtocol(
					persistence->getPersistenceProtocolCollection());
	if (persistenceProtocol.IsNull() || persistenceProtocol->getUri().empty()) {
		CAF_CM_LOG_DEBUG_VA1(
				"Can't create persistence-appconfig until protocol is established - %s",
				configDir.c_str());
	} else {
#ifdef WIN32
	const std::string newLine = "\r\n";
#else
	const std::string newLine = "\n";
#endif

		CAF_CM_LOG_DEBUG_VA1("Saving persistence-appconfig - %s", configDir.c_str());

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

void CConfigEnv::restartListener(
		const std::string& reason) const {
	FileSystemUtils::saveTextFile(_restartListenerPath, reason);
}

void CConfigEnv::listenerConfiguredStage1(
		const std::string& reason) const {
	FileSystemUtils::saveTextFile(_listenerConfiguredStage1Path, reason);
}

void CConfigEnv::listenerConfiguredStage2(
		const std::string& reason) const {
	FileSystemUtils::saveTextFile(_listenerConfiguredStage2Path, reason);
}
