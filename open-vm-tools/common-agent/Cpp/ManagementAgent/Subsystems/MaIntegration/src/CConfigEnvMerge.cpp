/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CConfigEnvMerge.h"

#ifdef WIN32
	#include <winsock.h>
	#pragma comment (lib, "wsock32.lib")
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

using namespace Caf;

SmartPtrCPersistenceDoc CConfigEnvMerge::mergePersistence(
		const SmartPtrCPersistenceDoc& persistence,
		const std::string& cacertPath,
		const std::string& vcidPath) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CConfigEnvMerge", "mergePersistence");
	CAF_CM_VALIDATE_SMARTPTR(persistence);
	CAF_CM_VALIDATE_STRING(cacertPath);
	CAF_CM_VALIDATE_STRING(vcidPath);

	const std::string localId = mergeLocalId(persistence, vcidPath);

	std::string localIdDiff;
	if (persistence->getLocalSecurity()->getLocalId().compare(localId) != 0) {
		CAF_CM_LOG_DEBUG_VA2("LocalId changed - %s != %s",
				persistence->getLocalSecurity()->getLocalId().c_str(), localId.c_str());
		localIdDiff = localId;
	}

	const std::string cacert = loadTextFile(cacertPath);

	const std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInnerDiff =
			mergePersistenceProtocolCollectionInner(
					persistence->getPersistenceProtocolCollection()->getPersistenceProtocol(),
					localId, cacert);

	SmartPtrCPersistenceDoc rc;
	if (! localIdDiff.empty() || ! persistenceProtocolCollectionInnerDiff.empty()) {
		SmartPtrCLocalSecurityDoc localSecurity = persistence->getLocalSecurity();
		if (! localIdDiff.empty()) {
			CAF_CM_LOG_DEBUG_VA0("Creating local security diff");
			localSecurity.CreateInstance();
			localSecurity->initialize(
					localIdDiff,
					persistence->getLocalSecurity()->getPrivateKey(),
					persistence->getLocalSecurity()->getCert(),
					persistence->getLocalSecurity()->getPrivateKeyPath(),
					persistence->getLocalSecurity()->getCertPath());
		}

		SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollection =
				persistence->getPersistenceProtocolCollection();
		if (! persistenceProtocolCollectionInnerDiff.empty()) {
			CAF_CM_LOG_DEBUG_VA0("Creating persistence protocol diff");
			persistenceProtocolCollection.CreateInstance();
			persistenceProtocolCollection->initialize(persistenceProtocolCollectionInnerDiff);
		}

		rc.CreateInstance();
		rc->initialize(
				localSecurity,
				persistence->getRemoteSecurityCollection(),
				persistenceProtocolCollection,
				persistence->getVersion());
	}

	return rc;
}

std::string CConfigEnvMerge::mergeLocalId(
		const SmartPtrCPersistenceDoc& persistence,
		const std::string& vcidPath) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CConfigEnvMerge", "mergeLocalId");
	CAF_CM_VALIDATE_SMARTPTR(persistence);
	CAF_CM_VALIDATE_STRING(vcidPath);

	std::string rc = loadTextFile(vcidPath);
	if (rc.empty()) {
		if (persistence->getLocalSecurity()->getLocalId().empty()) {
			rc = CStringUtils::createRandomUuid();
		} else {
			rc = persistence->getLocalSecurity()->getLocalId();
		}
	}

	return rc;
}

std::deque<SmartPtrCPersistenceProtocolDoc> CConfigEnvMerge::mergePersistenceProtocolCollectionInner(
		const std::deque<SmartPtrCPersistenceProtocolDoc>& persistenceProtocolCollectionInner,
		const std::string& localId,
		const std::string& cacert) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CConfigEnvMerge", "mergePersistenceProtocolCollectionInner");
	CAF_CM_VALIDATE_BOOL(persistenceProtocolCollectionInner.size() == 1);
	CAF_CM_VALIDATE_STRING(localId);

	std::deque<SmartPtrCPersistenceProtocolDoc> rc;
	std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInnerDiff;
	std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInnerAll;
	for (TConstIterator<std::deque<SmartPtrCPersistenceProtocolDoc> > persistenceProtocolIter(persistenceProtocolCollectionInner);
		persistenceProtocolIter; persistenceProtocolIter++) {
		const SmartPtrCPersistenceProtocolDoc persistenceProtocol = *persistenceProtocolIter;

		const std::string uriDiff = mergeUri(
				persistenceProtocol->getUri(),
				persistenceProtocol->getUriAmqp(),
				persistenceProtocol->getUriTunnel(),
				localId);
		const SmartPtrCCertCollectionDoc tlsCertCollectionDiff =
				mergeTlsCertCollection(persistenceProtocol->getTlsCertCollection(), cacert);

		SmartPtrCPersistenceProtocolDoc persistenceProtocolDiff;
		persistenceProtocolDiff.CreateInstance();
		persistenceProtocolDiff->initialize(
				persistenceProtocol->getProtocolName(),
				uriDiff.empty() ? persistenceProtocol->getUri() : uriDiff,
				persistenceProtocol->getUriAmqp(),
				persistenceProtocol->getUriTunnel(),
				persistenceProtocol->getTlsCert(),
				persistenceProtocol->getTlsProtocol(),
				persistenceProtocol->getTlsCipherCollection(),
				tlsCertCollectionDiff.IsNull() ? persistenceProtocol->getTlsCertCollection() : tlsCertCollectionDiff,
				persistenceProtocol->getTlsCertPath(),
				persistenceProtocol->getTlsCertPathCollection());
		persistenceProtocolCollectionInnerAll.push_back(persistenceProtocolDiff);

		if (! uriDiff.empty() || ! tlsCertCollectionDiff.IsNull()) {
			persistenceProtocolCollectionInnerDiff.push_back(persistenceProtocolDiff);
		}
	}

	if (! persistenceProtocolCollectionInnerDiff.empty()) {
		rc = persistenceProtocolCollectionInnerAll;
	}

	return rc;
}

std::string CConfigEnvMerge::mergeUri(
		const std::string& uri,
		const std::string& uriAmqp,
		const std::string& uriTunnel,
		const std::string& localId) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CConfigEnvMerge", "mergeUri");
	CAF_CM_VALIDATE_STRING(uriAmqp);
	CAF_CM_VALIDATE_STRING(uriTunnel);
	CAF_CM_VALIDATE_STRING(localId);

	UriUtils::SUriRecord uriData;
	if (! uri.empty()) {
		UriUtils::parseUriString(uri, uriData);
	}

	const bool isTunnelEnabled = isTunnelEnabledFunc();
	const std::string uriProtocol = isTunnelEnabled ? "tunnel" : "amqp";

	bool isUriChanged = false;
	if (uriData.protocol.compare(uriProtocol) != 0) {
		const std::string uriTmp = isTunnelEnabled ? uriTunnel : uriAmqp;
		UriUtils::parseUriString(uriTmp, uriData);
		isUriChanged = true;
	}

	std::string amqpQueueId = localId;
	if (isTunnelEnabled) {
		amqpQueueId += "-agentId1";
	}

	if (uriData.path.compare(amqpQueueId) != 0) {
		uriData.path = amqpQueueId;
		isUriChanged = true;
	}

	std::string rc;
	if (isUriChanged) {
		rc = UriUtils::buildUriString(uriData);
		CAF_CM_LOG_DEBUG_VA2("uri changed - %s != %s", uri.c_str(), rc.c_str());
	}

	return rc;
}

SmartPtrCCertCollectionDoc CConfigEnvMerge::mergeTlsCertCollection(
		const SmartPtrCCertCollectionDoc& tlsCertCollection,
		const std::string& cacert) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CConfigEnvMerge", "mergeTlsCertCollection");
	CAF_CM_VALIDATE_SMARTPTR(tlsCertCollection);

	SmartPtrCCertCollectionDoc rc;
	if (! cacert.empty()) {
		const Cdeqstr tlsCertCollectionInner = tlsCertCollection->getCert();
		if (tlsCertCollectionInner.size() == 1) {
			const std::string tlsCert = tlsCertCollectionInner.front();
			if (tlsCert.compare(cacert) != 0) {
				CAF_CM_LOG_DEBUG_VA2("cacert changed - %s != %s", cacert.c_str(), tlsCert.c_str());

				Cdeqstr tlsCertCollectionInnerTmp;
				tlsCertCollectionInnerTmp.push_back(cacert);

				rc.CreateInstance();
				rc->initialize(tlsCertCollectionInnerTmp);
			}
		}
	}

	return rc;
}

bool CConfigEnvMerge::isTunnelEnabledFunc() {
	CAF_CM_STATIC_FUNC_LOG("CConfigEnvMerge", "isTunnelEnabledFunc");

	bool rc = false;

#ifdef WIN32
	try {
		WSADATA wsaData;
		int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != NO_ERROR) {
			CAF_CM_EXCEPTION_VA0(E_UNEXPECTED, "WSAStartup() Failed");
		}

		SOCKADDR_IN socketClient;
		memset(&socketClient, 0, sizeof(SOCKADDR_IN));
		socketClient.sin_family = AF_INET;
		socketClient.sin_addr.s_addr = ::inet_addr("127.0.0.1");
		socketClient.sin_port = ::htons(6672);

		SOCKET socketFd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socketFd == INVALID_SOCKET) {
			CAF_CM_EXCEPTION_VA1(E_UNEXPECTED, "Failed to open socket - %s", WSAGetLastError());
		}

		rc = (0 == ::connect(socketFd, (SOCKADDR*) &socketClient, sizeof(socketClient)));
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	WSACleanup();
#else
	int socketFd = -1;
	try {
		socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (socketFd < 0) {
			CAF_CM_EXCEPTION_VA0(E_UNEXPECTED, "Failed to open socket");
		}

		struct sockaddr_in socketClient;
		memset(&socketClient, 0, sizeof(sockaddr_in));
		socketClient.sin_family = AF_INET;
		socketClient.sin_port = htons(6672);

		int result = ::inet_aton("127.0.0.1", &socketClient.sin_addr);
		if (0 == result) {
			CAF_CM_EXCEPTION_VA0(ERROR_PATH_NOT_FOUND,
					"Failed to get address of 127.0.0.1");
		}

		rc = (0 == ::connect(socketFd, (struct sockaddr *) &socketClient,
				sizeof(socketClient)));
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	if (socketFd >= 0) {
		::close(socketFd);
	}
#endif

	return rc;
}

std::string CConfigEnvMerge::loadTextFile(
		const std::string& path) {

	std::string rc;
	if (FileSystemUtils::doesFileExist(path)) {
		rc = FileSystemUtils::loadTextFile(path);
		rc = CStringUtils::trimRight(rc);
	}

	return rc;
}
