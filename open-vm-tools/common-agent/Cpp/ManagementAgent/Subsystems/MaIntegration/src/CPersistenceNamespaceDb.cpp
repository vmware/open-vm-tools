/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"
#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolCollectionDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityCollectionDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityDoc.h"
#include "Exception/CCafException.h"
#include "CPersistenceNamespaceDb.h"
#include <string>

using namespace Caf;

CPersistenceNamespaceDb::CPersistenceNamespaceDb() :
	_isInitialized(false),
	_isReady(false),
	_dataReady2Read(false),
	_dataReady2Update(false),
	_dataReady2Remove(false),
	_polledDuringStart(false),
	_pollingIntervalSecs(86400),
	_pollingStartedTimeMs(0),
	CAF_CM_INIT_LOG("CPersistenceNamespaceDb") {
	CAF_CM_INIT_THREADSAFE;
	_nsdbNamespace = "com.vmware.caf.guest.rw";
}

CPersistenceNamespaceDb::~CPersistenceNamespaceDb() {
}

void CPersistenceNamespaceDb::initializeBean(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties) {
}

void CPersistenceNamespaceDb::terminateBean() {
}

void CPersistenceNamespaceDb::initialize() {
	CAF_CM_LOCK_UNLOCK;

	if (! _isInitialized) {
		_polledDuringStart = false;
		_nsdbPollerSignalFile = AppConfigUtils::getRequiredString("monitor", "nsdb_poller_signal_file");
		_pollingIntervalSecs = AppConfigUtils::getRequiredUint32("monitor", "nsdb_polling_interval_secs");
		_pollingStartedTimeMs = CDateTimeUtils::getTimeMs();
		setCmd();
		_isInitialized = true;
	}
}

SmartPtrCPersistenceDoc CPersistenceNamespaceDb::getUpdated(
		const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("getUpdated");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("getUpdated");
	SmartPtrCPersistenceDoc rc;
	if (isDataReady2Read() && isReady()) {
		const std::string updatesCur = getValue("updates");
		if (_updates.compare(updatesCur) != 0) {
			_updates = updatesCur;
			const std::string version = getValue("version");

			//EP Doc
			const std::string epLocalId = getValue("ep.local_id");
			const std::string epPrivateKey = getValue("ep.private_key");
			const std::string epCert = getValue("ep.cert");

			SmartPtrCLocalSecurityDoc endpoint;
			endpoint.CreateInstance();
			endpoint->initialize(epLocalId, epPrivateKey, epCert);

			//App collection
			std::deque<SmartPtrCRemoteSecurityDoc> applicationCollectionInner;
			const std::string applications = getValue("applications");
			Cdeqstr appList = CStringUtils::split(applications, ',');
			for (Cdeqstr::iterator appIt = appList.begin(); appIt != appList.end(); appIt++) {
				const std::string appKey = "app." + *appIt;
				const std::string appId = getValue(appKey + ".remote_id");
				const std::string appProtocolName = getValue(appKey + ".protocol_name");
				const std::string appCmsCipher = getValue(appKey + ".cms.cipher");

				std::deque<std::string> cmsCertCollectionInner;
				const std::string appCmsCertChain = getValue(appKey + ".cms.cert_chain");
				Cdeqstr appCertList = CStringUtils::split(appCmsCertChain, ',');
				for (Cdeqstr::iterator appCertIt = appCertList.begin(); appCertIt != appCertList.end(); appCertIt++) {
					cmsCertCollectionInner.push_back(*appCertIt);
				}

				SmartPtrCCertCollectionDoc cmsCertCollection;
				cmsCertCollection.CreateInstance();
				cmsCertCollection->initialize(cmsCertCollectionInner);

				const std::string cmsCert = getValue(appKey + ".cms.cert");
				SmartPtrCRemoteSecurityDoc application;
				application.CreateInstance();
				application->initialize(appId, appProtocolName, cmsCert, appCmsCipher,
						cmsCertCollection);

				applicationCollectionInner.push_back(application);
			}

			SmartPtrCRemoteSecurityCollectionDoc applicationCollection;
			applicationCollection.CreateInstance();
			applicationCollection->initialize(applicationCollectionInner);

			const std::string protocols = getValue("protocols");
			Cdeqstr protocolList = CStringUtils::split(protocols, ',');
			std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInner;
			for (Cdeqstr::iterator protocolIt = protocolList.begin(); protocolIt != protocolList.end(); protocolIt++) {
				const std::string protocolKey = "protocol." + *protocolIt;
				//Protoccol Doc
				std::deque<std::string> tlsCertCollectionInner;
				const std::string tlsCertChain = getValue(protocolKey + ".tls.cert.chain");
				Cdeqstr tlsCertList = CStringUtils::split(tlsCertChain, ',');
				for (Cdeqstr::iterator tlsCertIt = tlsCertList.begin(); tlsCertIt != tlsCertList.end(); tlsCertIt++) {
					tlsCertCollectionInner.push_back(*tlsCertIt);
				}

				Cdeqstr tlsCipherCollection;
				const std::string tlsCiphers = getValue(protocolKey + ".tls.ciphers");
				Cdeqstr tlsCipherList = CStringUtils::split(tlsCiphers, ',');
				for (Cdeqstr::iterator tlsCipherIt = tlsCipherList.begin(); tlsCipherIt != tlsCipherList.end(); tlsCipherIt++) {
					tlsCipherCollection.push_back(*tlsCipherIt);
				}

				SmartPtrCCertCollectionDoc tlsCertCollection;
				tlsCertCollection.CreateInstance();
				tlsCertCollection->initialize(tlsCertCollectionInner);

				//For now, we only support one broker.
				const std::string protocolName = getValue(protocolKey + ".protocol_name");
				const std::string tlsCert = getValue(protocolKey + ".tls.cert");
				const std::string tlsProtocol = getValue(protocolKey + ".tls.protocol");
				const std::string uri = getValue(protocolKey + ".uri");
				const std::string uriAmqp = getValue(protocolKey + ".uri.amqp");
				const std::string uriTunnel = getValue(protocolKey + ".uri.tunnel");

				SmartPtrCPersistenceProtocolDoc persistenceProtocol;
				persistenceProtocol.CreateInstance();
				persistenceProtocol->initialize(
						protocolName, uri, uriAmqp, uriTunnel, tlsCert, tlsProtocol,
						tlsCipherCollection, tlsCertCollection);

				persistenceProtocolCollectionInner.push_back(persistenceProtocol);
			}

			SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollection;
			persistenceProtocolCollection.CreateInstance();
			persistenceProtocolCollection->initialize(persistenceProtocolCollectionInner);

			// Persist doc
			rc.CreateInstance();
			rc->initialize(endpoint, applicationCollection, persistenceProtocolCollection, version);
		}
	}

	if (rc.IsNull()) {
		rc = _persistenceUpdate;
	}

	_dataReady2Read = false;


	return rc;
}

void CPersistenceNamespaceDb::update(
		const SmartPtrCPersistenceDoc& persistenceDoc) {
	CAF_CM_FUNCNAME("update");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("update");
	if (isDataReady2Update() && isReady()) {
		const SmartPtrCPersistenceDoc persistenceCur = persistenceDoc.IsNull() ? _persistenceUpdate : persistenceDoc;
		if (! persistenceCur.IsNull()) {
			_persistenceUpdate = SmartPtrCPersistenceDoc();
			setValue("version", persistenceCur->getVersion());

			//Update LocalSecurity info
			if (!persistenceCur->getLocalSecurity().IsNull()){
				setValue("ep.local_id", persistenceCur->getLocalSecurity()->getLocalId());
				setValue("ep.private_key", persistenceCur->getLocalSecurity()->getPrivateKey());
				setValue("ep.cert", persistenceCur->getLocalSecurity()->getCert());
			}

			//Update RemoteSecurity info
			if (!persistenceCur->getRemoteSecurityCollection().IsNull()){
				std::deque<SmartPtrCRemoteSecurityDoc> applications =
						persistenceCur->getRemoteSecurityCollection()->getRemoteSecurity();
				for (std::deque<SmartPtrCRemoteSecurityDoc>::iterator appIt = applications.begin(); appIt != applications.end(); appIt++) {
					const std::string appKey = "app." + (*appIt)->getRemoteId();

					setValue(appKey + ".remote_id", (*appIt)->getRemoteId());
					setValue(appKey + ".cms.cert", (*appIt)->getCmsCert());
					setValue(appKey + ".cms.cipher", (*appIt)->getCmsCipherName());
					setValue(appKey + ".protocol_name", (*appIt)->getProtocolName());

					std::string cmsCertChain;
					if (! (*appIt)->getCmsCertCollection().IsNull()) {
						Cdeqstr cmsCertList = (*appIt)->getCmsCertCollection()->getCert();
						for (Cdeqstr::iterator cmsCertIt=cmsCertList.begin(); cmsCertIt != cmsCertList.end(); cmsCertIt++) {
							if (!cmsCertChain.empty()) {
								cmsCertChain += ",";
							}
							cmsCertChain += *cmsCertIt;
						}

						setValue(appKey + ".cms.cert_chain", cmsCertChain);
					}
				}
			}

			//Update PersistenceProtocol info
			if (!persistenceCur->getPersistenceProtocolCollection().IsNull()) {
				//For now, we only support one broker.
				CAF_CM_ASSERT(persistenceCur->getPersistenceProtocolCollection()->getPersistenceProtocol().size() <= 1);

				std::deque<SmartPtrCPersistenceProtocolDoc> brokerList = persistenceCur->getPersistenceProtocolCollection()->getPersistenceProtocol();
				for (std::deque<SmartPtrCPersistenceProtocolDoc>::iterator protIt=brokerList.begin(); protIt != brokerList.end(); protIt++) {
					const std::string protocolKey = "protocol." + (*protIt)->getProtocolName();
					setValue(protocolKey + ".protocol_name", (*protIt)->getProtocolName());
					setValue(protocolKey + ".uri", (*protIt)->getUri());
					setValue(protocolKey + ".uri.amqp", (*protIt)->getUriAmqp());
					setValue(protocolKey + ".uri.tunnel", (*protIt)->getUriTunnel());
					setValue(protocolKey + ".tls.cert", (*protIt)->getTlsCert());
					setValue(protocolKey + ".tls.protocol", (*protIt)->getTlsProtocol());

					Cdeqstr tlsCipherList = (*protIt)->getTlsCipherCollection();
					std::string tlsCiphers;
					for (Cdeqstr::iterator tlsCipherIt=tlsCipherList.begin(); tlsCipherIt != tlsCipherList.end(); tlsCipherIt++) {
						if (!tlsCiphers.empty()) {
							tlsCiphers += ",";
						}
						tlsCiphers += *tlsCipherIt;
					}
					setValue(protocolKey + ".tls.ciphers", tlsCiphers);

					if (! (*protIt)->getTlsCertCollection().IsNull()) {
						Cdeqstr tlsCertList = (*protIt)->getTlsCertCollection()->getCert();
						std::string tlsCerts;
						for (Cdeqstr::iterator tlsCertIt=tlsCertList.begin(); tlsCertIt != tlsCertList.end(); tlsCertIt++) {
							if (!tlsCerts.empty()) {
								tlsCerts += ",";
							}
							tlsCerts += *tlsCertIt;
						}
						setValue(protocolKey + ".tls.cert_chain", tlsCerts);
					}
				}
			}
		}
	} else {
		if (! persistenceDoc.IsNull()) {
			_persistenceUpdate = persistenceDoc;
		}
	}

	_dataReady2Update = false;
}

void CPersistenceNamespaceDb::remove(
		const SmartPtrCPersistenceDoc& persistenceDoc) {
	CAF_CM_FUNCNAME("remove");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_LOG_DEBUG_VA0("remove");
	if (isDataReady2Remove() && isReady()) {
		const SmartPtrCPersistenceDoc persistenceCur = persistenceDoc.IsNull() ? _persistenceRemove : persistenceDoc;
		if (! persistenceCur.IsNull()) {
			_persistenceRemove = SmartPtrCPersistenceDoc();
			if (!persistenceCur->getLocalSecurity().IsNull()){
				if (!persistenceCur->getLocalSecurity()->getLocalId().empty()) {
					removeKey("ep.local_id");
				}
				if (!persistenceCur->getLocalSecurity()->getPrivateKey().empty()) {
					removeKey("ep.private_key");
				}
				if (!persistenceCur->getLocalSecurity()->getCert().empty()) {
					removeKey("ep.cert");
				}
			}

			//Remove RemoteSecurity info
			if (!persistenceCur->getRemoteSecurityCollection().IsNull()){
				std::deque<SmartPtrCRemoteSecurityDoc> applications = persistenceCur->getRemoteSecurityCollection()->getRemoteSecurity();
				for (std::deque<SmartPtrCRemoteSecurityDoc>::iterator it=applications.begin(); it != applications.end(); it++) {
					std::string appKey = "app." + (*it)->getRemoteId();
					if (!(*it)->getProtocolName().empty()) {
						removeKey(appKey + ".protocol_name");
					}
					if (!(*it)->getCmsCert().empty()) {
						removeKey(appKey + ".cms.cert");
					}
					if (!(*it)->getCmsCertCollection().IsNull() && !(*it)->getCmsCertCollection()->getCert().empty()) {
						removeKey(appKey + ".cms.cert_chain");
					}
					if (!(*it)->getCmsCipherName().empty()) {
						removeKey(appKey + ".cms.cipher");
					}
				}
			}

			//Remove PersistenceProtocol info
			if (!persistenceCur->getPersistenceProtocolCollection().IsNull()) {
				//For now, we only support one broker.
				CAF_CM_ASSERT(persistenceCur->getPersistenceProtocolCollection()->getPersistenceProtocol().size() <= 1);

				std::deque<SmartPtrCPersistenceProtocolDoc> brokerList = persistenceCur->getPersistenceProtocolCollection()->getPersistenceProtocol();
				for (std::deque<SmartPtrCPersistenceProtocolDoc>::iterator it=brokerList.begin(); it != brokerList.end(); it++) {
					std::string protocolKey = "protocol." + (*it)->getProtocolName();
					if (!(*it)->getUri().empty()) {
						removeKey(protocolKey + "uri");
					}
					if (!(*it)->getUriAmqp().empty()) {
						removeKey(protocolKey + "uri.amqp");
					}
					if (!(*it)->getUriTunnel().empty()) {
						removeKey(protocolKey + "uri.tunnel");
					}
					if (!(*it)->getTlsCert().empty()) {
						removeKey(protocolKey + "tls.cert");
					}
					if (!(*it)->getTlsProtocol().empty()) {
						removeKey(protocolKey + "tls.protocol");
					}
					if (!(*it)->getTlsCipherCollection().empty()) {
						removeKey(protocolKey + "tls.ciphers");
					}
					if (!(*it)->getTlsCertCollection().IsNull() && !(*it)->getTlsCertCollection()->getCert().empty()) {
						removeKey(protocolKey + "tls.cert_chain");
					}
				}
			}
		}
	} else {
		if (! persistenceDoc.IsNull()) {
			_persistenceRemove = persistenceDoc;
		}
	}

	_dataReady2Remove = false;
}

void CPersistenceNamespaceDb::setCmd() {
	CAF_CM_FUNCNAME("setCmd");

	std::string nsdbCmdDir;
	std::string nsdbCmdFile;
#ifdef WIN32
	//  "C:/Program Files/VMware/VMware Tools/VMwareNamespaceCmd.exe";
	//  bin_dir=C:/Program Files/VMware/VMware Tools/VMware CAF/pme//bin
	nsdbCmdDir = AppConfigUtils::getRequiredString("globals", "bin_dir");

	//Back up two levels
	nsdbCmdDir = FileSystemUtils::getDirname(nsdbCmdDir);
	nsdbCmdDir = FileSystemUtils::getDirname(nsdbCmdDir);

	nsdbCmdFile = "VMwareNamespaceCmd.exe";
#else
	nsdbCmdDir = "/usr/sbin";
	nsdbCmdFile = "vmware-namespace-cmd";
#endif

	_nsdbCmdPath = FileSystemUtils::buildPath(nsdbCmdDir, nsdbCmdFile);
	CAF_CM_LOG_DEBUG_VA1("_nsdbCmdPath: %s", _nsdbCmdPath.c_str());
	if (!FileSystemUtils::doesFileExist(_nsdbCmdPath)) {
		CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"Namespace DB command not found - %s", _nsdbCmdPath.c_str());
	}
}

std::string CPersistenceNamespaceDb::getValue(const std::string& key) {
	CAF_CM_FUNCNAME("getValue");
	CAF_CM_VALIDATE_STRING(key);

	CAF_CM_LOG_DEBUG_VA0("getValue");
	std::string value;
	std::string stdoutContent;
	std::string stderrContent;
	try {
		value = getValueRaw(key, stdoutContent, stderrContent);
	}
	catch(ProcessFailedException* ex){
		CAF_CM_LOG_DEBUG_VA1("ProcessFailedException - %s", ex->getMsg().c_str());
		CAF_CM_EXCEPTION_VA3(E_UNEXPECTED,
				"NamespaceDB command failed - %s: %s: %s",
				ex->getMsg().c_str(),
				stdoutContent.c_str(),
				stderrContent.c_str());
	}
	return value;
}

void CPersistenceNamespaceDb::setValue(
		const std::string& key,
		const std::string& value) {
	CAF_CM_FUNCNAME("setValue");
	CAF_CM_VALIDATE_STRING(key);

	CAF_CM_LOG_DEBUG_VA0("setValue");
	if (_removedKeys.find(key) == _removedKeys.end()) {
		if (value.empty()) {
			CAF_CM_LOG_DEBUG_VA1("Cannot set empty value: %s", key.c_str());
			return;
		}

		std::string stdoutContent;
		std::string stderrContent;
		Cdeqstr argv;
		std::string tmpFile;

		try {
			tmpFile = FileSystemUtils::saveTempTextFile("caf_nsdb_XXXXXX", value);
			CAF_CM_LOG_DEBUG_VA2("Setting %s to %s", key.c_str(), value.c_str());
			argv.push_back(_nsdbCmdPath);
			argv.push_back("set-key");
			argv.push_back(_nsdbNamespace);
			argv.push_back("-k");
			argv.push_back(key);
			argv.push_back("-f");
			argv.push_back(tmpFile);

			ProcessUtils::runSync(argv, stdoutContent, stderrContent);
		}
		catch(ProcessFailedException* ex){
			CAF_CM_LOG_DEBUG_VA1("exception: %s", ex->getMsg().c_str());
			CAF_CM_EXCEPTION_VA3(E_UNEXPECTED,
					"NamespaceDB command failed - %s: %s: %s",
					ex->getMsg().c_str(),
					stdoutContent.c_str(),
					stderrContent.c_str());
		}
		if ( !tmpFile.empty() && FileSystemUtils::doesFileExist(tmpFile)) {
			FileSystemUtils::removeFile(tmpFile);
		}
	} else {
		CAF_CM_LOG_DEBUG_VA1("Cannot set a removed key: %s", key.c_str());
	}
}
	
void CPersistenceNamespaceDb::removeKey(const std::string& key) {
	CAF_CM_FUNCNAME("removeKey");	
	CAF_CM_VALIDATE_STRING(key);

	CAF_CM_LOG_DEBUG_VA0("removeKey");
	if (_removedKeys.find(key) == _removedKeys.end()) {
		std::string stdoutContent;
		std::string stderrContent;
		Cdeqstr argv;

		try {
			argv.push_back(_nsdbCmdPath);
			argv.push_back("delete-key");
			argv.push_back(_nsdbNamespace);
			argv.push_back("-k");
			argv.push_back(key);

			ProcessUtils::runSync(argv, stdoutContent, stderrContent);
			_removedKeys.insert(key);
		}
		catch(ProcessFailedException* ex){
			CAF_CM_LOG_DEBUG_VA1("exception: %s", ex->getMsg().c_str());
			CAF_CM_EXCEPTION_VA3(E_UNEXPECTED,
					"NamespaceDB command failed - %s: %s: %s",
					ex->getMsg().c_str(),
					stdoutContent.c_str(),
					stderrContent.c_str());
		}
	} else {
		CAF_CM_LOG_DEBUG_VA1("Key already removed: %s", key.c_str());
	}
}

bool CPersistenceNamespaceDb::isDataReady() {
	CAF_CM_FUNCNAME_VALIDATE("isDataReady");

	CAF_CM_LOG_DEBUG_VA0("isDataReady method");

	// Check if data is ready to read/modify
	bool rc = false;
	if (!_polledDuringStart) {
		rc = true;
		_polledDuringStart = true;
		CAF_CM_LOG_DEBUG_VA0("Set NSDB polling during service start");
	}
	if (FileSystemUtils::doesFileExist(_nsdbPollerSignalFile)) {
		rc = true;
		CAF_CM_LOG_DEBUG_VA1("NSDB poller signal file %s exists.", _nsdbPollerSignalFile.c_str());
		FileSystemUtils::removeFile(_nsdbPollerSignalFile);
	}
	CAF_CM_LOG_DEBUG_VA4("NSDB poller signal file %s, _pollingStartedTimeMs=%ld, _pollingIntervalSecs=%ld, rc=%s.",
			_nsdbPollerSignalFile.c_str(), long(_pollingStartedTimeMs), long(_pollingIntervalSecs), rc?"true":"false");
	if (CDateTimeUtils::calcRemainingTime(_pollingStartedTimeMs, _pollingIntervalSecs * 1000) <= 0) {
		rc = true;
		CAF_CM_LOG_DEBUG_VA0("The next polling interval reached.");
	}
	if (rc) {
		_pollingStartedTimeMs = CDateTimeUtils::getTimeMs();
		_dataReady2Read = _dataReady2Update = _dataReady2Remove = true;
	}

	return rc;
}

bool CPersistenceNamespaceDb::isDataReady2Read() {
        CAF_CM_FUNCNAME_VALIDATE("isDataReady2Read");

	CAF_CM_LOG_DEBUG_VA1("_dataReady2Read = %s", _dataReady2Read?"true":"false");

	return (isDataReady() || _dataReady2Read);
}

bool CPersistenceNamespaceDb::isDataReady2Update() {
        CAF_CM_FUNCNAME_VALIDATE("isDataReady2Update");

	CAF_CM_LOG_DEBUG_VA1("_dataReady2Update = %s", _dataReady2Update?"true":"false");
	return (isDataReady() || _dataReady2Update);

}

bool CPersistenceNamespaceDb::isDataReady2Remove() {
        CAF_CM_FUNCNAME_VALIDATE("isDataReady2Remove");

	CAF_CM_LOG_DEBUG_VA1("_dataReady2Remove = %s", _dataReady2Remove?"true":"false");
	return (isDataReady() || _dataReady2Remove);

}

bool CPersistenceNamespaceDb::isReady() {
	CAF_CM_FUNCNAME("isReady");

	CAF_CM_LOG_DEBUG_VA0("isReady method");


	bool rc = true;
	if (! _isReady) {
		std::string stdoutContent;
		std::string stderrContent;
		try {
			(void) getValueRaw("updates", stdoutContent, stderrContent);
			_isReady = true;
		}
		catch(ProcessFailedException* ex) {
			if ((stderrContent.find("There is no namespace database associated with this virtual machine") != std::string::npos)
					|| (stderrContent.find("Permission denied") != std::string::npos)) {
				CAF_CM_LOG_DEBUG_VA1(
						"Received expected exception - msg: %s", ex->getMsg().c_str());
				rc = false;
			} else {
				CAF_CM_LOG_DEBUG_VA3(
						"ProcessFailedException - msg: %s, stdout: %s, stderr: %s",
						ex->getMsg().c_str(), stdoutContent.c_str(), stderrContent.c_str());
				CAF_CM_EXCEPTION_VA1(E_UNEXPECTED,
						"NamespaceDB command failed - msg: %s", ex->getMsg().c_str());
			}
		}
	}

	return rc;
}

std::string CPersistenceNamespaceDb::getValueRaw(
		const std::string& key,
		std::string& stdoutContent,
		std::string& stderrContent) {
	CAF_CM_FUNCNAME_VALIDATE("getValueRaw");
	CAF_CM_VALIDATE_STRING(key);

	CAF_CM_LOG_ERROR_VA0("getValueRaw");
	Cdeqstr argv;
	argv.push_back(_nsdbCmdPath);
	argv.push_back("get-value");
	argv.push_back(_nsdbNamespace);
	argv.push_back("-k");
	argv.push_back(key);

	ProcessUtils::runSync(argv, stdoutContent, stderrContent);
	std::string value = stdoutContent;

	//strip spaces
	value = CStringUtils::trim(value);
	//strip quotes
	//TODO: make this less of a hack and add it to StringUtils
	if (value.length() > 1) {
		if (value[0] == '"')
			value.erase(0,1);
		if (value[value.length()-1] == '"')
			value.erase(value.length()-1,1);
	}

	return value;
}
