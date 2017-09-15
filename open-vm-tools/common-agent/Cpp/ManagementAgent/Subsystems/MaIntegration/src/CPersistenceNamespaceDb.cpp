/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CPersistenceNamespaceDb.h"
#include <string>

using namespace Caf;
using namespace std;

#ifdef WIN32
	const string CPersistenceNamespaceDb::_NAMESPACE_DB_CMD_FILE = "VMwareNamespaceCmd.exe";
#else
	const string CPersistenceNamespaceDb::_NAMESPACE_DB_CMD_FILE = "vmware-namespace-cmd";
#endif

const string CPersistenceNamespaceDb::_NAMESPACE = "com.vmware.caf.guest.rw";

CPersistenceNamespaceDb::CPersistenceNamespaceDb() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CPersistenceNamespaceDb") {
	CAF_CM_INIT_THREADSAFE;
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
		setCmd();
		_isInitialized = true;
	}
}

SmartPtrCPersistenceDoc CPersistenceNamespaceDb::getUpdated(
		const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("getUpdated");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	//If nothing has been updated, skip all of the unneeded work
	string updates = getValue("updates");
	if (!updates.empty()) {
		string version = getValue("version");

		//EP Doc
		string epLocalId = getValue("ep.local_id");
		string epPrivateKey = getValue("ep.private_key");
		string epCert = getValue("ep.cert");
		SmartPtrCLocalSecurityDoc endpoint;
		endpoint.CreateInstance();
		endpoint->initialize(epLocalId, epPrivateKey, epCert);

		//App collection
		std::deque<SmartPtrCRemoteSecurityDoc> applicationCollectionInner;
		string applications = getValue("applications");
		Cdeqstr appList = CStringUtils::split(applications, ',');
		for (Cdeqstr::iterator appIt = appList.begin(); appIt != appList.end(); appIt++) {
			string appKey = "app." + *appIt;
			string appId = getValue(appKey + ".remote_id");
			string appProtocolName = getValue(appKey + ".protocol_name");
			string appCmsCipher = getValue(appKey + ".cms.cipher");

			std::deque<std::string> cmsCertCollectionInner;
			string appCmsCertChain = getValue(appKey + ".cms.cert_chain");
			Cdeqstr appCertList = CStringUtils::split(appCmsCertChain, ',');
			for (Cdeqstr::iterator appCertIt = appCertList.begin(); appCertIt != appCertList.end(); appCertIt++) {
				cmsCertCollectionInner.push_back(*appCertIt);
			}

			SmartPtrCCertCollectionDoc cmsCertCollection;
			cmsCertCollection.CreateInstance();
			cmsCertCollection->initialize(cmsCertCollectionInner);

			string cmsCert = getValue(appKey + ".cms.cert");
			SmartPtrCRemoteSecurityDoc application;
			application.CreateInstance();
			application->initialize(appId, appProtocolName, cmsCert, appCmsCipher, cmsCertCollection);

			applicationCollectionInner.push_back(application);
		}

		SmartPtrCRemoteSecurityCollectionDoc applicationCollection;
		applicationCollection.CreateInstance();
		applicationCollection->initialize(applicationCollectionInner);

		string protocols = getValue("protocols");
		Cdeqstr protocolList = CStringUtils::split(protocols, ',');
		std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInner;
		for (Cdeqstr::iterator protocolIt = protocolList.begin(); protocolIt != protocolList.end(); protocolIt++) {
			string protocolKey = "protocol." + *protocolIt;
			//Protoccol Doc
			std::deque<std::string> tlsCertCollectionInner;
			string tlsCertChain = getValue(protocolKey + ".tls.cert.chain");
			Cdeqstr tlsCertList = CStringUtils::split(tlsCertChain, ',');
			for (Cdeqstr::iterator tlsCertIt = tlsCertList.begin(); tlsCertIt != tlsCertList.end(); tlsCertIt++) {
				tlsCertCollectionInner.push_back(*tlsCertIt);
			}

			Cdeqstr tlsCipherCollection;
			string tlsCiphers = getValue(protocolKey + ".tls.ciphers");
			Cdeqstr tlsCipherList = CStringUtils::split(tlsCiphers, ',');
			for (Cdeqstr::iterator tlsCipherIt = tlsCipherList.begin(); tlsCipherIt != tlsCipherList.end(); tlsCipherIt++) {
				tlsCipherCollection.push_back(*tlsCipherIt);
			}

			SmartPtrCCertCollectionDoc tlsCertCollection;
			tlsCertCollection.CreateInstance();
			tlsCertCollection->initialize(tlsCertCollectionInner);

			//For now, we only support one broker.
			string protocolName = getValue(protocolKey + ".protocol_name");
			string uri = getValue(protocolKey + ".uri");
			string tlsCert = getValue(protocolKey + ".tls.cert");
			string tlsProtocol = getValue(protocolKey + ".tls.protocol");
			SmartPtrCPersistenceProtocolDoc persistenceProtocol;
			persistenceProtocol.CreateInstance();
			persistenceProtocol->initialize(protocolName, uri, tlsCert, tlsProtocol, tlsCipherCollection, tlsCertCollection);

			persistenceProtocolCollectionInner.push_back(persistenceProtocol);
		}

		SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollection;
		persistenceProtocolCollection.CreateInstance();
		persistenceProtocolCollection->initialize(persistenceProtocolCollectionInner);

		// Persist doc
		SmartPtrCPersistenceDoc persistence;
		persistence.CreateInstance();
		persistence->initialize(endpoint, applicationCollection, persistenceProtocolCollection, version);

		return persistence;
	}
	else {
		return SmartPtrCPersistenceDoc();
	}
}

void CPersistenceNamespaceDb::update(
		const SmartPtrCPersistenceDoc& persistenceDoc) {
	CAF_CM_FUNCNAME("update");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (! persistenceDoc.IsNull()) {
		setValue("version", persistenceDoc->getVersion());

		//Update LocalSecurity info
		if (!persistenceDoc->getLocalSecurity().IsNull()){
			setValue("ep.local_id", persistenceDoc->getLocalSecurity()->getLocalId());
			setValue("ep.private_key", persistenceDoc->getLocalSecurity()->getPrivateKey());
			setValue("ep.cert", persistenceDoc->getLocalSecurity()->getCert());
		}

		//Update RemoteSecurity info
		if (!persistenceDoc->getRemoteSecurityCollection().IsNull()){
			deque<SmartPtrCRemoteSecurityDoc> applications = persistenceDoc->getRemoteSecurityCollection()->getRemoteSecurity();
			for (deque<SmartPtrCRemoteSecurityDoc>::iterator appIt=applications.begin(); appIt != applications.end(); appIt++) {
				string appKey = "app." + (*appIt)->getRemoteId();
				setValue(appKey + ".remote_id", (*appIt)->getRemoteId());
				setValue(appKey + ".cms.cert", (*appIt)->getCmsCert());
				setValue(appKey + ".cms.cipher", (*appIt)->getCmsCipherName());
				setValue(appKey + ".protocol_name", (*appIt)->getProtocolName());
				string cmsCertChain;
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
		if (!persistenceDoc->getPersistenceProtocolCollection().IsNull()) {
			//For now, we only support one broker.
			CAF_CM_ASSERT(persistenceDoc->getPersistenceProtocolCollection()->getPersistenceProtocol().size() <= 1);

			deque<SmartPtrCPersistenceProtocolDoc> brokerList = persistenceDoc->getPersistenceProtocolCollection()->getPersistenceProtocol();
			for (deque<SmartPtrCPersistenceProtocolDoc>::iterator protIt=brokerList.begin(); protIt != brokerList.end(); protIt++) {
				string protocolKey = "protocol." + (*protIt)->getProtocolName();
				setValue(protocolKey + ".protocol_name", (*protIt)->getProtocolName());
				setValue(protocolKey + ".uri", (*protIt)->getUri());
				setValue(protocolKey + ".tls.cert", (*protIt)->getTlsCert());
				setValue(protocolKey + ".tls.protocol", (*protIt)->getTlsProtocol());

				Cdeqstr tlsCipherList = (*protIt)->getTlsCipherCollection();
				string tlsCiphers;
				for (Cdeqstr::iterator tlsCipherIt=tlsCipherList.begin(); tlsCipherIt != tlsCipherList.end(); tlsCipherIt++) {
					if (!tlsCiphers.empty()) {
						tlsCiphers += ",";
					}
					tlsCiphers += *tlsCipherIt;
				}
				setValue(protocolKey + ".tls.ciphers", tlsCiphers);

				if (! (*protIt)->getTlsCertCollection().IsNull()) {
					Cdeqstr tlsCertList = (*protIt)->getTlsCertCollection()->getCert();
					string tlsCerts;
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
}

void CPersistenceNamespaceDb::remove(
		const SmartPtrCPersistenceDoc& persistenceDoc) {
	CAF_CM_FUNCNAME("remove");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	//Remove LocalSecurity info
	if (!persistenceDoc->getLocalSecurity().IsNull()){
		if (!persistenceDoc->getLocalSecurity()->getLocalId().empty()) {
			removeKey("ep.local_id");
		}
		if (!persistenceDoc->getLocalSecurity()->getPrivateKey().empty()) {
			removeKey("ep.private_key");
		}
		if (!persistenceDoc->getLocalSecurity()->getCert().empty()) {
			removeKey("ep.cert");
		}
	}

	//Remove RemoteSecurity info
	if (!persistenceDoc->getRemoteSecurityCollection().IsNull()){
		deque<SmartPtrCRemoteSecurityDoc> applications = persistenceDoc->getRemoteSecurityCollection()->getRemoteSecurity();
		for (deque<SmartPtrCRemoteSecurityDoc>::iterator it=applications.begin(); it != applications.end(); it++) {
			string appKey = "app." + (*it)->getRemoteId();
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
	if (!persistenceDoc->getPersistenceProtocolCollection().IsNull()) {
		//For now, we only support one broker.
		CAF_CM_ASSERT(persistenceDoc->getPersistenceProtocolCollection()->getPersistenceProtocol().size() <= 1);

		deque<SmartPtrCPersistenceProtocolDoc> brokerList = persistenceDoc->getPersistenceProtocolCollection()->getPersistenceProtocol();
		for (deque<SmartPtrCPersistenceProtocolDoc>::iterator it=brokerList.begin(); it != brokerList.end(); it++) {
			string protocolKey = "protocol." + (*it)->getProtocolName();
			if (!(*it)->getUri().empty()) {
				removeKey(protocolKey + "uri");
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

string CPersistenceNamespaceDb::getCmdPath() {
//	CAF_CM_STATIC_FUNC("CPersistenceNamespaceDb", "getCmdPath");
//	"/usr/sbin/vmware-namespace-cmd";
	string cmdPath = "/usr/sbin";
#ifdef WIN32
//	       "C:/Program Files/VMware/VMware Tools/VMwareNamespaceCmd.exe";
//  bin_dir=C:/Program Files/VMware/VMware Tools/VMware CAF/pme//bin
	cmdPath = AppConfigUtils::getRequiredString("globals", "bin_dir");
	//Back up two levels
	cmdPath = FileSystemUtils::getDirname(cmdPath);
	cmdPath = FileSystemUtils::getDirname(cmdPath);	
#endif
	return cmdPath;
}

void CPersistenceNamespaceDb::setCmd() {
	CAF_CM_FUNCNAME("setCmd");
	_namespaceDbCmd = FileSystemUtils::buildPath(getCmdPath(), _NAMESPACE_DB_CMD_FILE);
	CAF_CM_LOG_DEBUG_VA1("_namespaceDbCmd: %s", _namespaceDbCmd.c_str());
	if (!FileSystemUtils::doesFileExist(_namespaceDbCmd)) {
		CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"Namespace DB command not found - %s", _namespaceDbCmd.c_str());
	}
}

string CPersistenceNamespaceDb::getValue(const std::string& key) {
	CAF_CM_FUNCNAME("getValue");
	CAF_CM_VALIDATE_STRING(key);
	
	string value;
	string stdoutContent;
	string stderrContent;
	Cdeqstr argv;

	try {
		argv.push_back(_namespaceDbCmd);
		argv.push_back("get-value");
		argv.push_back(_NAMESPACE);
		argv.push_back("-k");
		argv.push_back(key);
		
		ProcessUtils::runSync(argv, stdoutContent, stderrContent);
		value = stdoutContent;

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
		//TODO: parse hash from nsdb value
		string hash = value; //As a temporary hack, use the entire value as the "hash"
		//if hash has not changed, return empty
		if (cache[key] == hash) {
			CAF_CM_LOG_DEBUG_VA1("Value for %s has not changed", key.c_str());
			value = "";
		}
		else {
		//if hash has changed, update key+hash cache and return value
			CAF_CM_LOG_DEBUG_VA1("Value for %s has changed", key.c_str());
			cache[key] = hash;
		}
	}
	catch(ProcessFailedException* ex){
		if (ex->getMsg().compare("There is no namespace database associated with this virtual machine") == 0) {
			CAF_CM_LOG_INFO_VA1("Acceptable exception... continuing - %s", ex->getMsg().c_str());
		} else {
			CAF_CM_LOG_DEBUG_VA1("ProcessFailedException - %s", ex->getMsg().c_str());
			CAF_CM_EXCEPTION_VA3(E_UNEXPECTED,
					"NamespaceDB command failed - %s: %s: %s",
					ex->getMsg().c_str(),
					stdoutContent.c_str(),
					stderrContent.c_str());
		}
	}
	return value;
}
	
void CPersistenceNamespaceDb::setValue(const std::string& key, const std::string& value) {
	CAF_CM_FUNCNAME("setValue");
	CAF_CM_VALIDATE_STRING(key);

	if (value.empty()) {
		return;
	}

	string stdoutContent;
	string stderrContent;
	Cdeqstr argv;
	string tmpFile;

	try {
		//TODO: generate hash of value
		//TODO: prepend delimitted hash to value

		tmpFile = FileSystemUtils::saveTempTextFile("caf_nsdb_XXXXXX", value);
		CAF_CM_LOG_DEBUG_VA2("Setting %s to %s", key.c_str(), value.c_str());
		argv.push_back(_namespaceDbCmd);
		argv.push_back("set-key");
		argv.push_back(_NAMESPACE);
		argv.push_back("-k");
		argv.push_back(key);
		argv.push_back("-f");
		argv.push_back(tmpFile);
				
		ProcessUtils::runSync(argv, stdoutContent, stderrContent);
		//Add to key+hash cache
		//TODO: generate a hash of the value string
		cache[key] = value; //As a temporary hack use the entire value as the "hash"
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
}
	
void CPersistenceNamespaceDb::removeKey(const std::string& key) {
	CAF_CM_FUNCNAME("removeKey");	
	CAF_CM_VALIDATE_STRING(key);

	string stdoutContent;
	string stderrContent;
	Cdeqstr argv;

	try {
		argv.push_back(_namespaceDbCmd);
		argv.push_back("delete-key");
		argv.push_back(_NAMESPACE);
		argv.push_back("-k");
		argv.push_back(key);

		ProcessUtils::runSync(argv, stdoutContent, stderrContent);

		//Remove from cache
		Cmapstrstr::iterator it = cache.find(key);
		if (it != cache.end()) {
			cache.erase(it);
		}
	}
	catch(ProcessFailedException* ex){
		CAF_CM_LOG_DEBUG_VA1("exception: %s", ex->getMsg().c_str());		
		CAF_CM_EXCEPTION_VA3(E_UNEXPECTED,
				"NamespaceDB command failed - %s: %s: %s",
				ex->getMsg().c_str(),
				stdoutContent.c_str(),
				stderrContent.c_str());
	}
}
