/*k
 *	Author: bwilliams
 *  Created: Nov 25, 2015
 *
 *	Copyright (c) 2015 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
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
			loadPersistenceProtocol(persistenceDir));

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

SmartPtrCPersistenceProtocolDoc CPersistenceUtils::loadPersistenceProtocol(
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadPersistenceProtocol");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	const std::string protocolDir = FileSystemUtils::buildPath(persistenceDir, "protocol");

	std::deque<SmartPtrCAmqpBrokerDoc> amqpBrokerCollectionInner;
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

			SmartPtrCAmqpBrokerDoc amqpBroker;
			amqpBroker.CreateInstance();
			amqpBroker->initialize(
					loadTextFile(protocolIdDir, "amqpBrokerId.txt"),
					loadTextFile(protocolIdDir, "uri.txt"),
					loadTextFile(protocolIdDir, "tlsCert.pem"),
					loadTextFile(protocolIdDir, "tlsProtocol.txt"),
					tlsCipherCollection,
					tlsCertCollection,
					FileSystemUtils::buildPath(protocolIdDir, "tlsCert.pem"),
					tlsCertPathCollection);

			amqpBrokerCollectionInner.push_back(amqpBroker);
		}
	}

	SmartPtrCAmqpBrokerCollectionDoc amqpBrokerCollection;
	amqpBrokerCollection.CreateInstance();
	amqpBrokerCollection->initialize(amqpBrokerCollectionInner);

	SmartPtrCPersistenceProtocolDoc persistenceProtocol;
	persistenceProtocol.CreateInstance();
	persistenceProtocol->initialize(amqpBrokerCollection);

	return persistenceProtocol;
}

SmartPtrCAmqpBrokerDoc CPersistenceUtils::loadAmqpBroker(
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadAmqpBroker");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	const SmartPtrCPersistenceProtocolDoc persistenceProtocol =
			loadPersistenceProtocol(persistenceDir);

	return loadAmqpBroker(persistenceProtocol);
}

SmartPtrCAmqpBrokerDoc CPersistenceUtils::loadAmqpBroker(
		const SmartPtrCPersistenceProtocolDoc& persistenceProtocol) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadAmqpBroker");
	CAF_CM_VALIDATE_SMARTPTR(persistenceProtocol);

	const SmartPtrCAmqpBrokerCollectionDoc amqpBrokerCollection =
			persistenceProtocol->getAmqpBrokerCollection();
	CAF_CM_VALIDATE_SMARTPTR(amqpBrokerCollection);

	std::deque<SmartPtrCAmqpBrokerDoc> amqpBrokerCollectionInner =
			amqpBrokerCollection->getAmqpBroker();
	CAF_CM_VALIDATE_BOOL(! amqpBrokerCollectionInner.empty());
	CAF_CM_VALIDATE_BOOL(amqpBrokerCollectionInner.size() <= 2);

	SmartPtrCAmqpBrokerDoc amqpBroker = amqpBrokerCollectionInner.front();
	if (amqpBrokerCollectionInner.size() == 2) {
		if (amqpBroker->getAmqpBrokerId().compare("amqpBroker_default") == 0) {
			amqpBroker = amqpBrokerCollectionInner.at(1);
		}
		CAF_CM_VALIDATE_BOOL(amqpBroker->getAmqpBrokerId().compare("amqpBroker_default") != 0);
	}

	return amqpBroker;
}

void CPersistenceUtils::savePersistence(
		const SmartPtrCPersistenceDoc& persistence,
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CPersistenceUtils", "savePersistence");
	CAF_CM_VALIDATE_SMARTPTR(persistence);
	CAF_CM_VALIDATE_STRING(persistenceDir);

	const SmartPtrCLocalSecurityDoc localSecurity = persistence->getLocalSecurity();
	const SmartPtrCRemoteSecurityCollectionDoc remoteSecurityCollection =
			persistence->getRemoteSecurityCollection();
	const SmartPtrCPersistenceProtocolDoc persistenceProtocol =
			persistence->getPersistenceProtocol();

	if (FileSystemUtils::doesDirectoryExist(persistenceDir)) {
		CAF_CM_LOG_DEBUG_VA1("Removing directory - %s", persistenceDir.c_str());
		FileSystemUtils::recursiveRemoveDirectory(persistenceDir);
	}

	saveLocalSecurity(localSecurity, persistenceDir);
	saveRemoteSecurityCollection(remoteSecurityCollection, persistenceDir);
	savePersistenceProtocol(persistenceProtocol, persistenceDir);
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

void CPersistenceUtils::savePersistenceProtocol(
		const SmartPtrCPersistenceProtocolDoc& persistenceProtocol,
		const std::string& persistenceDir) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "savePersistenceProtocol");
	CAF_CM_VALIDATE_STRING(persistenceDir);

	if (! persistenceProtocol.IsNull() && ! persistenceProtocol->getAmqpBrokerCollection().IsNull()) {
		const std::deque<SmartPtrCAmqpBrokerDoc> amqpBrokerCollectionInner =
				persistenceProtocol->getAmqpBrokerCollection()->getAmqpBroker();
		if (! amqpBrokerCollectionInner.empty()) {
			const std::string protocolDir = createDirectory(persistenceDir, "protocol");

			for (TConstIterator<std::deque<SmartPtrCAmqpBrokerDoc> >
					amqpBrokerIter(amqpBrokerCollectionInner);
					amqpBrokerIter;
					amqpBrokerIter++) {
				const SmartPtrCAmqpBrokerDoc amqpBroker = *amqpBrokerIter;

				const std::string amqpBrokerId = amqpBroker->getAmqpBrokerId();
				CAF_CM_VALIDATE_STRING(amqpBrokerId);

				const std::string amqpQueueDir = createDirectory(protocolDir, amqpBrokerId);

				if (! amqpBroker->getAmqpBrokerId().empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "amqpBrokerId.txt", amqpBroker->getAmqpBrokerId());
				}

				if (! amqpBroker->getUri().empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "uri.txt", amqpBroker->getUri());
				}

				if (! amqpBroker->getTlsCert().empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "tlsCert.pem", amqpBroker->getTlsCert());
				}

				if (! amqpBroker->getTlsProtocol().empty()) {
					FileSystemUtils::saveTextFile(
							amqpQueueDir, "tlsProtocol.txt", amqpBroker->getTlsProtocol());
				}

				const std::deque<std::string> tlsCipherCollectionInner =
						amqpBroker->getTlsCipherCollection();
				if (! tlsCipherCollectionInner.empty()) {
					const std::string cipherDir = createDirectory(amqpQueueDir, "tlsCipherCollection");
					saveCollection(tlsCipherCollectionInner, cipherDir, "tlsCipher", ".txt");
				}

				if (! amqpBroker->getTlsCertCollection().IsNull()) {
					const std::deque<std::string> tlsCertCollectionInner =
							amqpBroker->getTlsCertCollection()->getCert();

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
		const std::string& file) {
	CAF_CM_STATIC_FUNC_VALIDATE("CPersistenceUtils", "loadTextFile");
	CAF_CM_VALIDATE_STRING(dir);
	CAF_CM_VALIDATE_STRING(file);

	const std::string path = FileSystemUtils::buildPath(dir, file);

	std::string rc;
	if (FileSystemUtils::doesFileExist(path)) {
		rc = FileSystemUtils::loadTextFile(path);
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
