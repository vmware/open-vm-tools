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
}

CPersistenceNamespaceDb::~CPersistenceNamespaceDb() {
}

void CPersistenceNamespaceDb::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	setCmd();
	_isInitialized = true;
}

SmartPtrCPersistenceDoc CPersistenceNamespaceDb::getUpdated(
		const int32 timeout) {
	CAF_CM_FUNCNAME_VALIDATE("getUpdated");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	//If nothing has been updated, skip all of the unneeded work
	string updates = getValue("updates");
	if (!updates.empty()) {
		//EP Doc
		string epPrivateKey = getValue("ep.private.key");
		string epCert = getValue("ep.cert");
		SmartPtrCLocalSecurityDoc endpoint;
		endpoint.CreateInstance();
		endpoint->initialize(std::string(), epPrivateKey, epCert);

		//App collection
		std::deque<SmartPtrCRemoteSecurityDoc> applicationCollectionInner;
		string applications = getValue("applications");
		Cdeqstr appList = CStringUtils::split(applications, ',');
		for (Cdeqstr::iterator it = appList.begin(); it != appList.end(); it++) {
			string appKey = *it;
			string appCmsCipher = getValue(appKey + ".cms.cipher");

			std::deque<std::string> cmsCertCollectionInner;
			string appCmsCertChain = getValue(appKey + ".cms.cert_chain");
			Cdeqstr appCertList = CStringUtils::split(appCmsCertChain, ',');
			for (Cdeqstr::iterator it = appCertList.begin(); it != appCertList.end(); it++) {
				cmsCertCollectionInner.push_back(*it);
			}

			SmartPtrCCertCollectionDoc cmsCertCollection;
			cmsCertCollection.CreateInstance();
			cmsCertCollection->initialize(cmsCertCollectionInner);

			string appId = appKey;
			string cmsCert = getValue(appKey + ".cms.cert");
			SmartPtrCRemoteSecurityDoc application;
			application.CreateInstance();
			application->initialize(appId, "amqpBroker_default", cmsCert, appCmsCipher, cmsCertCollection);

			applicationCollectionInner.push_back(application);
		}

		SmartPtrCRemoteSecurityCollectionDoc applicationCollection;
		applicationCollection.CreateInstance();
		applicationCollection->initialize(applicationCollectionInner);

		string protocols = getValue("protocols");
		Cdeqstr protocolList = CStringUtils::split(protocols, ',');
		std::deque<SmartPtrCAmqpBrokerDoc> amqpBrokerCollectionInner;
		for (Cdeqstr::iterator it = protocolList.begin(); it != protocolList.end(); it++) {
			string protocolKey = *it;
			//Protoccol Doc
			std::deque<std::string> tlsCertCollectionInner;
			string tlsCertChain = getValue(protocolKey + ".tls.cert.chain");
			Cdeqstr tlsCertList = CStringUtils::split(tlsCertChain, ',');
			for (Cdeqstr::iterator it = tlsCertList.begin(); it != tlsCertList.end(); it++) {
				tlsCertCollectionInner.push_back(*it);
			}

			Cdeqstr tlsCipherCollection;
			string tlsCiphers = getValue(protocolKey + ".tls.ciphers");
			Cdeqstr tlsCipherList = CStringUtils::split(tlsCiphers, ',');
			for (Cdeqstr::iterator it = tlsCipherList.begin(); it != tlsCipherList.end(); it++) {
				tlsCipherCollection.push_back(*it);
			}

			SmartPtrCCertCollectionDoc tlsCertCollection;
			tlsCertCollection.CreateInstance();
			tlsCertCollection->initialize(tlsCertCollectionInner);

			//For now, we only support one broker.
			string amqpBrokerId = getValue(protocolKey + ".amqpBrokerId");
			string uri = getValue(protocolKey + ".uri");
			string tlsCert = getValue(protocolKey + ".tls.cert");
			string tlsProtocol = getValue(protocolKey + ".tls.protocol");
			SmartPtrCAmqpBrokerDoc amqpBroker;
			amqpBroker.CreateInstance();
			amqpBroker->initialize(amqpBrokerId, uri, tlsCert, tlsProtocol, tlsCipherCollection, tlsCertCollection);

			amqpBrokerCollectionInner.push_back(amqpBroker);
		}

		SmartPtrCAmqpBrokerCollectionDoc amqpBrokerCollection;
		amqpBrokerCollection.CreateInstance();
		amqpBrokerCollection->initialize(amqpBrokerCollectionInner);

		SmartPtrCPersistenceProtocolDoc persistenceProtocol;
		persistenceProtocol.CreateInstance();
		persistenceProtocol->initialize(amqpBrokerCollection);

		// Persist doc
		SmartPtrCPersistenceDoc persistence;
		persistence.CreateInstance();
		persistence->initialize(endpoint, applicationCollection, persistenceProtocol, "1.0");

		return persistence;
	}
	else {
		return SmartPtrCPersistenceDoc();
	}
}

void CPersistenceNamespaceDb::update(
		const SmartPtrCPersistenceDoc& persistenceDoc) {
	CAF_CM_FUNCNAME("update");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(persistenceDoc);

	//Update LocalSecurity info
	if (!persistenceDoc->getLocalSecurity().IsNull()){
		setValue("ep.private.key", persistenceDoc->getLocalSecurity()->getPrivateKey());
		setValue("ep.cert", persistenceDoc->getLocalSecurity()->getCert());
	}

	//Update RemoteSecurity info
	if (!persistenceDoc->getRemoteSecurityCollection().IsNull()){
		deque<SmartPtrCRemoteSecurityDoc> applications = persistenceDoc->getRemoteSecurityCollection()->getRemoteSecurity();
		for (deque<SmartPtrCRemoteSecurityDoc>::iterator it=applications.begin(); it != applications.end(); it++) {
			string appKey = (*it)->getRemoteId();
			setValue(appKey + ".cms.cert", (*it)->getCmsCert());
			//setValue(appKey + ".cms.cipher", (*it)->getCmsCipher());
			string cmsCertChain;
			Cdeqstr cmsCertList = (*it)->getCmsCertCollection()->getCert();
			for (Cdeqstr::iterator it=cmsCertList.begin(); it != cmsCertList.end(); it++) {
				if (!cmsCertChain.empty()) {
					cmsCertChain += ",";
				}
				cmsCertChain += *it;
			}
			setValue(appKey + ".cms.cert_chain", cmsCertChain);
		}
	}

	//Update PersistenceProtocol info
	if (!persistenceDoc->getPersistenceProtocol().IsNull()) {
		//For now, we only support one broker. 
		CAF_CM_ASSERT(persistenceDoc->getPersistenceProtocol()->getAmqpBrokerCollection()->getAmqpBroker().size() <= 1);

		deque<SmartPtrCAmqpBrokerDoc> brokerList = persistenceDoc->getPersistenceProtocol()->getAmqpBrokerCollection()->getAmqpBroker();
		for (deque<SmartPtrCAmqpBrokerDoc>::iterator it=brokerList.begin(); it != brokerList.end(); it++) {
			setValue("name", (*it)->getAmqpBrokerId());
			setValue("uri", (*it)->getUri());
			setValue("tls.cert", (*it)->getTlsCert());
			setValue("tls.protocol", (*it)->getTlsProtocol());
			Cdeqstr tlsCipherList = (*it)->getTlsCipherCollection();
			string tlsCiphers;
			for (Cdeqstr::iterator it=tlsCipherList.begin(); it != tlsCipherList.end(); it++) {
				if (!tlsCiphers.empty()) {
					tlsCiphers += ",";
				}
				tlsCiphers += *it;
			}
			setValue("tls.ciphers", tlsCiphers);
		}
	}
}

void CPersistenceNamespaceDb::remove(
		const SmartPtrCPersistenceDoc& persistenceDoc) {
	CAF_CM_FUNCNAME("remove");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	//Remove LocalSecurity info
	if (!persistenceDoc->getLocalSecurity().IsNull()){
		if (!persistenceDoc->getLocalSecurity()->getPrivateKey().empty()) {
			removeKey("ep.private.key");
		}
		if (!persistenceDoc->getLocalSecurity()->getCert().empty()) {
			removeKey("ep.cert");
		}
	}

	//Remove RemoteSecurity info
	if (!persistenceDoc->getRemoteSecurityCollection().IsNull()){
		deque<SmartPtrCRemoteSecurityDoc> applications = persistenceDoc->getRemoteSecurityCollection()->getRemoteSecurity();
		for (deque<SmartPtrCRemoteSecurityDoc>::iterator it=applications.begin(); it != applications.end(); it++) {
			string appKey = (*it)->getRemoteId();
			removeKey(appKey + ".cms.cert");
			removeKey(appKey + ".cms.cert_chain");
		}
	}

	//Remove PersistenceProtocol info
	if (!persistenceDoc->getPersistenceProtocol().IsNull()) {
		//For now, we only support one broker. 
		CAF_CM_ASSERT(persistenceDoc->getPersistenceProtocol()->getAmqpBrokerCollection()->getAmqpBroker().size() <= 1);
			
		deque<SmartPtrCAmqpBrokerDoc> brokerList = persistenceDoc->getPersistenceProtocol()->getAmqpBrokerCollection()->getAmqpBroker();
		for (deque<SmartPtrCAmqpBrokerDoc>::iterator it=brokerList.begin(); it != brokerList.end(); it++) {
			if (!(*it)->getAmqpBrokerId().empty()) {
				removeKey("name");
			}
			if (!(*it)->getUri().empty()) {
				removeKey("uri");
			}
			if (!(*it)->getTlsCert().empty()) {
				removeKey("tls.cert");
			}
			if (!(*it)->getTlsProtocol().empty()) {
				removeKey("tls.protocol");
			}
			removeKey("tls.ciphers");
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
		CAF_CM_LOG_DEBUG_VA1("exception: %s", ex->getMsg().c_str());		
		CAF_CM_EXCEPTION_VA3(E_UNEXPECTED,
				"NamespaceDB command failed - %s: %s: %s",
				ex->getMsg().c_str(),
				stdoutContent.c_str(),
				stderrContent.c_str());
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
		argv.push_back(value);
				
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
