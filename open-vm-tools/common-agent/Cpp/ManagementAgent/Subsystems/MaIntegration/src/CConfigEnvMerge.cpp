/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CConfigEnvMerge.h"

using namespace Caf;

SmartPtrCPersistenceDoc CConfigEnvMerge::mergePersistence(
		const SmartPtrCPersistenceDoc& persistence,
		const std::string& cacertPath,
		const std::string& vcidPath) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CConfigEnvMerge", "mergePersistence");
	CAF_CM_VALIDATE_SMARTPTR(persistence);
	CAF_CM_VALIDATE_STRING(cacertPath);
	CAF_CM_VALIDATE_STRING(vcidPath);

	const std::string cacert = FileSystemUtils::doesFileExist(cacertPath) ?
			FileSystemUtils::loadTextFile(cacertPath) : std::string();
	const std::string vcid = FileSystemUtils::doesFileExist(vcidPath) ?
			FileSystemUtils::loadTextFile(vcidPath) : std::string();
	const std::string protocol = isTunnelEnabled() ? "tunnel" : "amqp";

	std::string vcidDiff;
	if (! vcid.empty()) {
		if (persistence->getLocalSecurity()->getLocalId().compare(vcid) != 0) {
			CAF_CM_LOG_DEBUG_VA2("vcid changed - %s != %s",
					persistence->getLocalSecurity()->getLocalId().c_str(), vcid.c_str());
			vcidDiff = vcid;
		}
	}

	const std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInnerDiff =
			mergePersistenceProtocolCollectionInner(
					persistence->getPersistenceProtocolCollection()->getPersistenceProtocol(),
					protocol, vcid, cacert);

	SmartPtrCPersistenceDoc rc;
	if (! vcidDiff.empty() || ! persistenceProtocolCollectionInnerDiff.empty()) {
		SmartPtrCLocalSecurityDoc localSecurity = persistence->getLocalSecurity();
		if (! vcidDiff.empty()) {
			CAF_CM_LOG_DEBUG_VA0("Creating local security diff");
			localSecurity.CreateInstance();
			localSecurity->initialize(
					vcidDiff,
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

std::deque<SmartPtrCPersistenceProtocolDoc> CConfigEnvMerge::mergePersistenceProtocolCollectionInner(
		const std::deque<SmartPtrCPersistenceProtocolDoc>& persistenceProtocolCollectionInner,
		const std::string& protocol,
		const std::string& vcid,
		const std::string& cacert) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CConfigEnvMerge", "mergePersistenceProtocolCollectionInner");
	CAF_CM_VALIDATE_BOOL(persistenceProtocolCollectionInner.size() <= 1);

	std::deque<SmartPtrCPersistenceProtocolDoc> rc;
	std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInnerDiff;
	std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInnerAll;
	for (TConstIterator<std::deque<SmartPtrCPersistenceProtocolDoc> > persistenceProtocolIter(persistenceProtocolCollectionInner);
		persistenceProtocolIter; persistenceProtocolIter++) {
		const SmartPtrCPersistenceProtocolDoc persistenceProtocol = *persistenceProtocolIter;

		const std::string uriDiff = mergeUri(persistenceProtocol->getUri(), protocol, vcid);
		const SmartPtrCCertCollectionDoc tlsCertCollectionDiff =
				mergeTlsCertCollection(persistenceProtocol->getTlsCertCollection(), cacert);

		SmartPtrCPersistenceProtocolDoc persistenceProtocolDiff;
		persistenceProtocolDiff.CreateInstance();
		persistenceProtocolDiff->initialize(
				persistenceProtocol->getProtocolName(),
				uriDiff.empty() ? persistenceProtocol->getUri() : uriDiff,
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
		const std::string& srcUri,
		const std::string& protocol,
		const std::string& vcid) {
	CAF_CM_STATIC_FUNC_LOG_ONLY("CConfigEnvMerge", "mergeUri");
	std::string rc;
	if (! srcUri.empty()) {
		UriUtils::SUriRecord uriData;
		UriUtils::parseUriString(srcUri, uriData);

		bool isUriChanged = false;
		//TODO: Comment back in once isTunnelEnabled() has been implemented
//		if (uriData.protocol.compare(protocol) != 0) {
//			uriData.protocol = protocol;
//			isUriChanged = true;
//		}
		std::string tunnelVcid = vcid;
		if (uriData.protocol.compare("tunnel") == 0) {
			tunnelVcid += "-agentId1";
		}
		if (! tunnelVcid.empty() && (uriData.path.compare(tunnelVcid) != 0)) {
			uriData.path = tunnelVcid;
			isUriChanged = true;
		}

		if (isUriChanged) {
			rc = UriUtils::buildUriString(uriData);
			CAF_CM_LOG_DEBUG_VA2("uri changed - %s != %s", srcUri.c_str(), rc.c_str());
		}
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

//TODO: Implement isTunnelEnabled
bool CConfigEnvMerge::isTunnelEnabled() {
	return false;
}
