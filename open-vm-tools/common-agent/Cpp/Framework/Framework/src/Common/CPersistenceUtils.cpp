/*k
 *	Author: bwilliams
 *  Created: Nov 25, 2015
 *
 *	Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"
#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"
#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolCollectionDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityCollectionDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityDoc.h"
#include "CPersistenceUtils.h"

using namespace Caf;

SmartPtrCPersistenceDoc CPersistenceUtils::loadPersistence(
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadPersistence");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	SmartPtrCPersistenceDoc persistence;
	persistence.CreateInstance();
	persistence->initialize(
			loadLocalSecurity(persistenceDir),
			loadRemoteSecurityCollection(persistenceDir),
			loadPersistenceProtocolCollection(persistenceDir),
			loadTextFile(persistenceDir, "version.txt", "1.0"));

	return persistence;
}

SmartPtrCLocalSecurityDoc CPersistenceUtils::loadLocalSecurity(
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadLocalSecurity");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	const std::string localDir = FileSystemUtils::buildPath(persistenceDir, "local");

	SmartPtrCLocalSecurityDoc localSecurity;
	localSecurity.CreateInstance();
	localSecurity->initialize(
			loadTextFile(localDir, "localId.txt"),
			loadTextFile(localDir, "privateKey.pem"),
			loadTextFile(localDir, "cert.pem"),
			FileSystemUtils::buildPath(localDir, "privateKey.pem"),
			FileSystemUtils::buildPath(localDir, "cert.pem"));

	return localSecurity;
}

SmartPtrCRemoteSecurityCollectionDoc CPersistenceUtils::loadRemoteSecurityCollection(
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadRemoteSecurityCollection");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	const std::string remoteDir = FileSystemUtils::buildPath(persistenceDir, "remote");

	std::deque<SmartPtrCRemoteSecurityDoc> remoteSecurityCollectionInner;
	if (FileSystemUtils::doesDirectoryExist(remoteDir)) {
		FileSystemUtils::DirectoryItems remoteItems = FileSystemUtils::itemsInDirectory(
				remoteDir, FileSystemUtils::REGEX_MATCH_ALL);
		for (TConstIterator<FileSystemUtils::Directories> remoteIter(remoteItems.directories); remoteIter; remoteIter++) {
			const std::string remoteId = *remoteIter;
			const std::string remoteIdDir = FileSystemUtils::buildPath(remoteDir, remoteId);
			const std::string cmsCertCollectionDir = FileSystemUtils::buildPath(remoteIdDir, "cmsCertCollection");

			std::deque<std::string> cmsCertCollectionInner;
			std::deque<std::string> cmsCertPathCollectionInner;
			if (FileSystemUtils::doesDirectoryExist(cmsCertCollectionDir)) {
				FileSystemUtils::DirectoryItems cmsCertCollectionItems = FileSystemUtils::itemsInDirectory(
						cmsCertCollectionDir, FileSystemUtils::REGEX_MATCH_ALL);
				for (TConstIterator<FileSystemUtils::Files> cmsCertCollectionIter(cmsCertCollectionItems.files); cmsCertCollectionIter; cmsCertCollectionIter++) {
					cmsCertCollectionInner.push_back(
							loadTextFile(cmsCertCollectionDir, *cmsCertCollectionIter));
					cmsCertPathCollectionInner.push_back(
							FileSystemUtils::buildPath(cmsCertCollectionDir, *cmsCertCollectionIter));
				}
			}

			SmartPtrCCertCollectionDoc cmsCertCollection;
			cmsCertCollection.CreateInstance();
			cmsCertCollection->initialize(cmsCertCollectionInner);

			SmartPtrCCertPathCollectionDoc cmsCertPathCollection;
			cmsCertPathCollection.CreateInstance();
			cmsCertPathCollection->initialize(cmsCertPathCollectionInner);

			SmartPtrCRemoteSecurityDoc remoteSecurity;
			remoteSecurity.CreateInstance();
			remoteSecurity->initialize(
					loadTextFile(remoteIdDir, "remoteId.txt"),
					loadTextFile(remoteIdDir, "protocolName.txt"),
					loadTextFile(remoteIdDir, "cmsCert.pem"),
					loadTextFile(remoteIdDir, "cmsCipherName.txt"),
					cmsCertCollection,
					FileSystemUtils::buildPath(remoteIdDir, "cmsCert.pem"),
					cmsCertPathCollection);

			remoteSecurityCollectionInner.push_back(remoteSecurity);
		}
	}

	SmartPtrCRemoteSecurityCollectionDoc remoteSecurityCollection;
	remoteSecurityCollection.CreateInstance();
	remoteSecurityCollection->initialize(remoteSecurityCollectionInner);

	return remoteSecurityCollection;
}

SmartPtrCPersistenceProtocolCollectionDoc CPersistenceUtils::loadPersistenceProtocolCollection(
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadPersistenceProtocolCollection");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	const std::string protocolDir = FileSystemUtils::buildPath(persistenceDir, "protocol");

	std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInner;
	if (FileSystemUtils::doesDirectoryExist(protocolDir)) {
		FileSystemUtils::DirectoryItems protocolItems = FileSystemUtils::itemsInDirectory(
				protocolDir, FileSystemUtils::REGEX_MATCH_ALL);
		for (TConstIterator<FileSystemUtils::Directories> protocolIter(protocolItems.directories); protocolIter; protocolIter++) {
			const std::string protocolId = *protocolIter;
			const std::string protocolIdDir = FileSystemUtils::buildPath(protocolDir, protocolId);
			const std::string tlsCipherCollectionDir = FileSystemUtils::buildPath(protocolIdDir, "tlsCipherCollection");
			const std::string tlsCertCollectionDir = FileSystemUtils::buildPath(protocolIdDir, "tlsCertCollection");

			Cdeqstr tlsCipherCollection;
			if (FileSystemUtils::doesDirectoryExist(tlsCipherCollectionDir)) {
				FileSystemUtils::DirectoryItems tlsCipherCollectionItems = FileSystemUtils::itemsInDirectory(
						tlsCipherCollectionDir, FileSystemUtils::REGEX_MATCH_ALL);
				for (TConstIterator<FileSystemUtils::Files> tlsCipherCollectionIter(tlsCipherCollectionItems.files); tlsCipherCollectionIter; tlsCipherCollectionIter++) {
					tlsCipherCollection.push_back(
							loadTextFile(tlsCipherCollectionDir, *tlsCipherCollectionIter));
				}
			}

			std::deque<std::string> tlsCertCollectionInner;
			std::deque<std::string> tlsCertPathCollectionInner;
			if (FileSystemUtils::doesDirectoryExist(tlsCertCollectionDir)) {
				FileSystemUtils::DirectoryItems tlsCertCollectionItems = FileSystemUtils::itemsInDirectory(
						tlsCertCollectionDir, FileSystemUtils::REGEX_MATCH_ALL);
				for (TConstIterator<FileSystemUtils::Files> tlsCertCollectionIter(tlsCertCollectionItems.files); tlsCertCollectionIter; tlsCertCollectionIter++) {
					tlsCertCollectionInner.push_back(
							loadTextFile(tlsCertCollectionDir, *tlsCertCollectionIter));
					tlsCertPathCollectionInner.push_back(
							FileSystemUtils::buildPath(tlsCertCollectionDir, *tlsCertCollectionIter));
				}
			}

			SmartPtrCCertCollectionDoc tlsCertCollection;
			tlsCertCollection.CreateInstance();
			tlsCertCollection->initialize(tlsCertCollectionInner);

			SmartPtrCCertPathCollectionDoc tlsCertPathCollection;
			tlsCertPathCollection.CreateInstance();
			tlsCertPathCollection->initialize(tlsCertPathCollectionInner);

			SmartPtrCPersistenceProtocolDoc persistenceProtocol;
			persistenceProtocol.CreateInstance();
			persistenceProtocol->initialize(
					loadTextFile(protocolIdDir, "protocolName.txt"),
					loadTextFile(protocolIdDir, "uri.txt"),
					loadTextFile(protocolIdDir, "uri_amqp.txt"),
					loadTextFile(protocolIdDir, "uri_tunnel.txt"),
					loadTextFile(protocolIdDir, "tlsCert.pem"),
					loadTextFile(protocolIdDir, "tlsProtocol.txt"),
					tlsCipherCollection,
					tlsCertCollection,
					FileSystemUtils::buildPath(protocolIdDir, "uri_amqp.txt"),
					FileSystemUtils::buildPath(protocolIdDir, "uri_tunnel.txt"),
					FileSystemUtils::buildPath(protocolIdDir, "tlsCert.pem"),
					tlsCertPathCollection);

			persistenceProtocolCollectionInner.push_back(persistenceProtocol);
		}
	}

	SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollection;
	persistenceProtocolCollection.CreateInstance();
	persistenceProtocolCollection->initialize(persistenceProtocolCollectionInner);

	return persistenceProtocolCollection;
}

SmartPtrCPersistenceProtocolDoc CPersistenceUtils::loadPersistenceProtocol(
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadPersistenceProtocol");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	const SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollection =
			loadPersistenceProtocolCollection(persistenceDir);

	return loadPersistenceProtocol(persistenceProtocolCollection);
}

SmartPtrCPersistenceProtocolDoc CPersistenceUtils::loadPersistenceProtocol(
		const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollection) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadPersistenceProtocol");
	CAF_CM_VALIDATE_SMARTPTR(persistenceProtocolCollection);

	std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInner =
			persistenceProtocolCollection->getPersistenceProtocol();
	CAF_CM_VALIDATE_BOOL(persistenceProtocolCollectionInner.size() <= 1);

	SmartPtrCPersistenceProtocolDoc rc;
	if (persistenceProtocolCollectionInner.size() == 1) {
		rc = persistenceProtocolCollectionInner.front();
	}

	return rc;
}

void CPersistenceUtils::savePersistence(
		const SmartPtrCPersistenceDoc& persistence,
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPersistenceUtils", "savePersistence");
	CAF_CM_VALIDATE_SMARTPTR(persistence);
	CAF_CM_VALIDATE_STRING(persistenceDir);

	const std::string protocolDir = FileSystemUtils::buildPath(persistenceDir, "protocol", "amqpBroker_default");
	const std::string uriAmqp = loadTextFile(protocolDir, "uri_amqp.txt");
	const std::string uriTunnel = loadTextFile(protocolDir, "uri_tunnel.txt");

	if (FileSystemUtils::doesDirectoryExist(persistenceDir)) {
		CAF_CM_LOG_DEBUG_VA1("Removing directory - %s", persistenceDir.c_str());
		FileSystemUtils::recursiveRemoveDirectory(persistenceDir);
	}

	const SmartPtrCLocalSecurityDoc localSecurity = persistence->getLocalSecurity();
	const SmartPtrCRemoteSecurityCollectionDoc remoteSecurityCollection =
			persistence->getRemoteSecurityCollection();
	const SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollection =
			persistence->getPersistenceProtocolCollection();

	saveLocalSecurity(localSecurity, persistenceDir);
	saveRemoteSecurityCollection(remoteSecurityCollection, persistenceDir);
	savePersistenceProtocolCollection(persistenceProtocolCollection, persistenceDir,
			uriAmqp, uriTunnel);
	FileSystemUtils::saveTextFile(persistenceDir, "version.txt", persistence->getVersion());
}

void CPersistenceUtils::saveLocalSecurity(
		const SmartPtrCLocalSecurityDoc& localSecurity,
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "saveLocalSecurity");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	if (localSecurity) {
		const std::string locCertsDir = createDirectory(persistenceDir, "local");

		if (! localSecurity->getLocalId().empty()) {
			FileSystemUtils::saveTextFile(
					locCertsDir, "localId.txt", localSecurity->getLocalId());
		}

		if (! localSecurity->getCert().empty()) {
			FileSystemUtils::saveTextFile(
					locCertsDir, "cert.pem", localSecurity->getCert());
		}

		if (! localSecurity->getPrivateKey().empty()) {
			FileSystemUtils::saveTextFile(
					locCertsDir, "privateKey.pem", localSecurity->getPrivateKey());
		}
	}
}

void CPersistenceUtils::saveRemoteSecurityCollection(
		const SmartPtrCRemoteSecurityCollectionDoc& remoteSecurityCollection,
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "saveRemoteSecurityCollection");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	if (! remoteSecurityCollection.IsNull()) {
		const std::deque<SmartPtrCRemoteSecurityDoc> remoteSecurityCollectionInner =
				remoteSecurityCollection->getRemoteSecurity();
		if (! remoteSecurityCollectionInner.empty()) {
			const std::string rmtCertsDir = createDirectory(persistenceDir, "remote");

			for (TConstIterator<std::deque<SmartPtrCRemoteSecurityDoc> >
					remoteSecurityIter(remoteSecurityCollectionInner);
					remoteSecurityIter;
					remoteSecurityIter++) {
				const SmartPtrCRemoteSecurityDoc remoteSecurity = *remoteSecurityIter;
				CAF_CM_VALIDATE_SMARTPTR(remoteSecurity);

				const std::string remoteId = remoteSecurity->getRemoteId();
				CAF_CM_VALIDATE_STRING(remoteId);

				const std::string persistenceDir = createDirectory(rmtCertsDir, remoteId);

				FileSystemUtils::saveTextFile(
						persistenceDir, "remoteId.txt", remoteSecurity->getRemoteId());

				if (! remoteSecurity->getProtocolName().empty()) {
					FileSystemUtils::saveTextFile(
							persistenceDir, "protocolName.txt", remoteSecurity->getProtocolName());
				}

				if (! remoteSecurity->getCmsCert().empty()) {
					FileSystemUtils::saveTextFile(
							persistenceDir, "cmsCert.pem", remoteSecurity->getCmsCert());
				}

				if (! remoteSecurity->getCmsCipherName().empty()) {
					FileSystemUtils::saveTextFile(
							persistenceDir, "cmsCipherName.txt", remoteSecurity->getCmsCipherName());
				}

				if (! remoteSecurity->getCmsCertCollection().IsNull()) {
					const std::deque<std::string> cmsCertCollectionInner =
							remoteSecurity->getCmsCertCollection()->getCert();

					if (! cmsCertCollectionInner.empty()) {
						const std::string certCollectionDir = createDirectory(persistenceDir, "cmsCertCollection");
						saveCollection(cmsCertCollectionInner, certCollectionDir, "cmsCert", ".pem");
					}
				}
			}
		}
	}
}

void CPersistenceUtils::savePersistenceProtocolCollection(
		const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollection,
		const std::string& persistenceDir,
		const std::string& uriAmqp,
		const std::string& uriTunnel) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "savePersistenceProtocolCollection");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	if (! persistenceProtocolCollection.IsNull()) {
		const std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInner =
				persistenceProtocolCollection->getPersistenceProtocol();
		if (! persistenceProtocolCollectionInner.empty()) {
			const std::string protocolDir = createDirectory(persistenceDir, "protocol");

			for (TConstIterator<std::deque<SmartPtrCPersistenceProtocolDoc> >
					persistenceProtocolIter(persistenceProtocolCollectionInner);
					persistenceProtocolIter;
					persistenceProtocolIter++) {
				const SmartPtrCPersistenceProtocolDoc persistenceProtocol = *persistenceProtocolIter;

				const std::string protocolName = persistenceProtocol->getProtocolName();
				CAF_CM_VALIDATE_STRING(protocolName);

				const std::string amqpQueueDir = createDirectory(protocolDir, protocolName);

				FileSystemUtils::saveTextFile(
						amqpQueueDir, "protocolName.txt", persistenceProtocol->getProtocolName());

				if (! persistenceProtocol->getUri().empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "uri.txt", persistenceProtocol->getUri());
				}

				const std::string uriAmqpTmp = persistenceProtocol->getUriAmqp().empty() ?
						uriAmqp : persistenceProtocol->getUriAmqp();
				if (! uriAmqpTmp.empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "uri_amqp.txt", uriAmqpTmp);
				}

				const std::string uriTunnelTmp = persistenceProtocol->getUriTunnel().empty() ?
						uriTunnel : persistenceProtocol->getUriTunnel();
				if (! uriTunnelTmp.empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "uri_tunnel.txt", uriTunnelTmp);
				}

				if (! persistenceProtocol->getTlsCert().empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "tlsCert.pem", persistenceProtocol->getTlsCert());
				}

				if (! persistenceProtocol->getTlsProtocol().empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "tlsProtocol.txt", persistenceProtocol->getTlsProtocol());
				}

				const std::deque<std::string> tlsCipherCollectionInner =
						persistenceProtocol->getTlsCipherCollection();
				if (! tlsCipherCollectionInner.empty()) {
					const std::string cipherDir = createDirectory(amqpQueueDir, "tlsCipherCollection");
					saveCollection(tlsCipherCollectionInner, cipherDir, "tlsCipher", ".txt");
				}

				if (! persistenceProtocol->getTlsCertCollection().IsNull()) {
					const std::deque<std::string> tlsCertCollectionInner =
							persistenceProtocol->getTlsCertCollection()->getCert();

					if (! tlsCertCollectionInner.empty()) {
						const std::string certCollectionDir = createDirectory(amqpQueueDir, "tlsCertCollection");
						saveCollection(tlsCertCollectionInner, certCollectionDir, "tlsCert", ".pem");
					}
				}
			}
		}
	}
}

std::string CPersistenceUtils::loadTextFile(
		const std::string& dir,
		const std::string& file,
		const std::string& defaultVal,
		const bool isTrimRight) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPersistenceUtils", "loadTextFile");
	CAF_CM_VALIDATE_STRING(dir);
	CAF_CM_VALIDATE_STRING(file);

	const std::string path = FileSystemUtils::buildPath(dir, file);

	std::string rc;
	if (FileSystemUtils::doesFileExist(path)) {
		rc = FileSystemUtils::loadTextFile(path);
		if (isTrimRight) {
			rc = CStringUtils::trimRight(rc);
		}
	} else {
		CAF_CM_LOG_DEBUG_VA1("File not found - %s", path.c_str());
		rc = defaultVal;
	}

	return rc;
}

std::string CPersistenceUtils::createDirectory(
		const std::string& directory,
		const std::string& subdir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "createDirectory");
	CAF_CM_VALIDATE_STRING(directory);
	CAF_CM_VALIDATE_STRING(subdir);

	const std::string dirPath = FileSystemUtils::buildPath(directory, subdir);
	if (! FileSystemUtils::doesDirectoryExist(dirPath)) {
		FileSystemUtils::createDirectory(dirPath);
	}

	return dirPath;
}

void CPersistenceUtils::saveCollection(
		const Cdeqstr& collection,
		const std::string& directory,
		const std::string& filePrefix,
		const std::string& filePostfix) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "saveCollection");
	CAF_CM_VALIDATE_STL(collection);
	CAF_CM_VALIDATE_STRING(directory);
	CAF_CM_VALIDATE_STRING(filePrefix);
	CAF_CM_VALIDATE_STRING(filePostfix);

	int32 cnt = 0;
	for (TConstIterator<Cdeqstr> elemIter(collection); elemIter; elemIter++) {
		const std::string elem = *elemIter;
		const std::string cntStr = CStringConv::toString<int32>(cnt++);
		std::string elemFilename = filePrefix + cntStr + filePostfix;
		FileSystemUtils::saveTextFile(directory, elemFilename, elem);
	}
}
