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
#include "CPersistenceMerge.h"

using namespace Caf;

SmartPtrCPersistenceDoc CPersistenceMerge::mergePersistence(
		const SmartPtrCPersistenceDoc& persistenceLoaded,
		const SmartPtrCPersistenceDoc& persistenceIn) {
	SmartPtrCPersistenceDoc rc;
	if (persistenceLoaded.IsNull()) {
		if (! persistenceIn.IsNull()) {
			rc = persistenceIn;
		}
	} else {
		if (persistenceIn.IsNull()) {
			rc = persistenceLoaded;
		} else {
			const SmartPtrCLocalSecurityDoc localSecurity = mergeLocalSecurity(
					persistenceLoaded->getLocalSecurity(),
					persistenceIn->getLocalSecurity());
			const SmartPtrCPersistenceProtocolCollectionDoc persistenceProtocolCollection = mergePersistenceProtocolCollection(
					persistenceLoaded->getPersistenceProtocolCollection(),
					persistenceIn->getPersistenceProtocolCollection());
			const SmartPtrCRemoteSecurityCollectionDoc remoteSecurityCollection = mergeRemoteSecurityCollection(
					persistenceLoaded->getRemoteSecurityCollection(),
					persistenceIn->getRemoteSecurityCollection());
			if (! localSecurity.IsNull() || ! persistenceProtocolCollection.IsNull() || ! remoteSecurityCollection.IsNull()) {
				rc.CreateInstance();
				rc->initialize(
						localSecurity.IsNull() ? persistenceIn->getLocalSecurity() : localSecurity,
						remoteSecurityCollection.IsNull() ? persistenceIn->getRemoteSecurityCollection() : remoteSecurityCollection,
						persistenceProtocolCollection.IsNull() ? persistenceIn->getPersistenceProtocolCollection() : persistenceProtocolCollection,
						persistenceIn->getVersion());
			}
		}
	}

	return rc;
}

SmartPtrCLocalSecurityDoc CPersistenceMerge::mergeLocalSecurity(
		const SmartPtrCLocalSecurityDoc& localSecurityLoaded,
		const SmartPtrCLocalSecurityDoc& localSecurityIn) {
	SmartPtrCLocalSecurityDoc rc;
	if (localSecurityLoaded.IsNull()) {
		if (! localSecurityIn.IsNull()) {
			rc = localSecurityIn;
		}
	} else {
		if (localSecurityIn.IsNull()) {
			rc = localSecurityLoaded;
		} else {
			const std::string localId = mergeStrings(
					localSecurityLoaded->getLocalId(), localSecurityIn->getLocalId());
			const std::string privateKey = mergeStrings(
					localSecurityIn->getPrivateKey(), localSecurityLoaded->getPrivateKey());
			const std::string certPath = mergeStrings(
					localSecurityIn->getCert(), localSecurityLoaded->getCertPath());
			if (! localId.empty() || ! privateKey.empty() || ! certPath.empty()) {
				rc.CreateInstance();
				rc->initialize(
						localId.empty() ? localSecurityIn->getLocalId() : localId,
						privateKey.empty() ? localSecurityIn->getPrivateKey() : privateKey,
						certPath.empty() ? localSecurityIn->getCertPath() : certPath,
						localSecurityLoaded->getPrivateKeyPath(),
						localSecurityLoaded->getCertPath());
			}
		}
	}

	return rc;
}

SmartPtrCPersistenceProtocolCollectionDoc CPersistenceMerge::mergePersistenceProtocolCollection(
		const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollectionLoaded,
		const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollectionIn) {
	SmartPtrCPersistenceProtocolCollectionDoc rc;
	if (persistenceProtocolCollectionLoaded.IsNull()) {
		if (! persistenceProtocolCollectionIn.IsNull()) {
			rc = persistenceProtocolCollectionIn;
		}
	} else {
		if (persistenceProtocolCollectionIn.IsNull()) {
			rc = persistenceProtocolCollectionLoaded;
		} else {
			const std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocolCollectionInner = mergePersistenceProtocolCollectionInner(
					persistenceProtocolCollectionLoaded->getPersistenceProtocol(),
					persistenceProtocolCollectionIn->getPersistenceProtocol());
			if (! persistenceProtocolCollectionInner.empty()) {
				rc.CreateInstance();
				rc->initialize(persistenceProtocolCollectionInner);
			}
		}
	}

	return rc;
}

std::deque<SmartPtrCPersistenceProtocolDoc> CPersistenceMerge::mergePersistenceProtocolCollectionInner(
		const std::deque<SmartPtrCPersistenceProtocolDoc>& persistenceProtocolCollectionInnerLoaded,
		const std::deque<SmartPtrCPersistenceProtocolDoc>& persistenceProtocolCollectionInnerIn) {
	CAF_CM_STATIC_FUNC_VALIDATE("CConfigEnvMerge", "mergePersistenceProtocolCollectionInner");

	std::deque<SmartPtrCPersistenceProtocolDoc> rc;
	if (persistenceProtocolCollectionInnerLoaded.empty()) {
		if (! persistenceProtocolCollectionInnerIn.empty()) {
			rc = persistenceProtocolCollectionInnerIn;
		}
	} else {
		if (persistenceProtocolCollectionInnerIn.empty()) {
			rc = persistenceProtocolCollectionInnerLoaded;
		} else {
			CAF_CM_VALIDATE_BOOL(persistenceProtocolCollectionInnerLoaded.size() == 1);
			CAF_CM_VALIDATE_BOOL(persistenceProtocolCollectionInnerIn.size() == 1);

			typedef std::map<std::string, std::pair<SmartPtrCPersistenceProtocolDoc, SmartPtrCPersistenceProtocolDoc> > CPersistenceProtocolMap;
			CPersistenceProtocolMap persistenceProtocolMap;
			for (TConstIterator<std::deque<SmartPtrCPersistenceProtocolDoc> > persistenceProtocolLoadedIter(persistenceProtocolCollectionInnerLoaded);
				persistenceProtocolLoadedIter; persistenceProtocolLoadedIter++) {
				const SmartPtrCPersistenceProtocolDoc persistenceProtocolLoaded = *persistenceProtocolLoadedIter;
				persistenceProtocolMap.insert(std::make_pair(
						persistenceProtocolLoaded->getProtocolName(),
						std::make_pair(persistenceProtocolLoaded, SmartPtrCPersistenceProtocolDoc())));
			}
			for (TConstIterator<std::deque<SmartPtrCPersistenceProtocolDoc> > persistenceProtocolInIter(persistenceProtocolCollectionInnerIn);
				persistenceProtocolInIter; persistenceProtocolInIter++) {
				const SmartPtrCPersistenceProtocolDoc persistenceProtocolIn = *persistenceProtocolInIter;
				if (persistenceProtocolMap.find(persistenceProtocolIn->getProtocolName()) == persistenceProtocolMap.end()) {
					CAF_CM_VALIDATE_BOOL(persistenceProtocolMap.empty());
					persistenceProtocolMap.insert(std::make_pair(
							persistenceProtocolIn->getProtocolName(),
							std::make_pair(SmartPtrCPersistenceProtocolDoc(), persistenceProtocolIn)));
				} else {
					persistenceProtocolMap.find(persistenceProtocolIn->getProtocolName())->second.second = persistenceProtocolIn;
				}
			}

			CAF_CM_VALIDATE_BOOL(persistenceProtocolMap.size() == 1);
			for (TConstMapIterator<CPersistenceProtocolMap> persistenceProtocolMapIter(persistenceProtocolMap);
					persistenceProtocolMapIter; persistenceProtocolMapIter++) {
				const SmartPtrCPersistenceProtocolDoc persistenceProtocolLoaded = persistenceProtocolMapIter->first;
				const SmartPtrCPersistenceProtocolDoc persistenceProtocolIn = persistenceProtocolMapIter->second;
				const SmartPtrCPersistenceProtocolDoc persistenceProtocolTmp =
						mergePersistenceProtocol(persistenceProtocolLoaded, persistenceProtocolIn);
				if (! persistenceProtocolTmp.IsNull()) {
					rc.push_back(persistenceProtocolTmp);
				}
			}
		}
	}

	return rc;
}

SmartPtrCPersistenceProtocolDoc CPersistenceMerge::mergePersistenceProtocol(
		const SmartPtrCPersistenceProtocolDoc& persistenceProtocolLoaded,
		const SmartPtrCPersistenceProtocolDoc& persistenceProtocolIn) {
	SmartPtrCPersistenceProtocolDoc rc;
	if (persistenceProtocolLoaded.IsNull()) {
		if (! persistenceProtocolIn.IsNull()) {
			rc = persistenceProtocolIn;
		}
	} else {
		if (persistenceProtocolIn.IsNull()) {
			rc = persistenceProtocolLoaded;
		} else {
			const std::string protocolName = mergeStrings(
					persistenceProtocolLoaded->getProtocolName(), persistenceProtocolIn->getProtocolName());
			const std::string uri = mergeUri(
					persistenceProtocolLoaded->getUri(), persistenceProtocolIn->getUri());
			const std::string tlsCert = mergeStrings(
					persistenceProtocolIn->getTlsCert(), persistenceProtocolLoaded->getTlsCert());
			const std::string tlsProtocol = mergeStrings(
					persistenceProtocolIn->getTlsProtocol(), persistenceProtocolLoaded->getTlsProtocol());
			const Cdeqstr tlsCipherCollection = mergeDeqstr(
					persistenceProtocolIn->getTlsCipherCollection(), persistenceProtocolLoaded->getTlsCipherCollection());
			const SmartPtrCCertCollectionDoc tlsCertCollection = mergeCertCollection(
					persistenceProtocolIn->getTlsCertCollection(), persistenceProtocolLoaded->getTlsCertCollection());

			if (! protocolName.empty() || ! uri.empty() || ! tlsCert.empty() ||
					! tlsProtocol.empty() || ! tlsCipherCollection.empty() || ! tlsCertCollection.IsNull()) {
				rc.CreateInstance();
				rc->initialize(
						protocolName.empty() ? persistenceProtocolIn->getProtocolName() : protocolName,
						uri.empty() ? persistenceProtocolIn->getUri() : uri,
						persistenceProtocolLoaded->getUriAmqp(),
						persistenceProtocolLoaded->getUriTunnel(),
						tlsCert.empty() ? persistenceProtocolIn->getTlsCert() : tlsCert,
						tlsProtocol.empty() ? persistenceProtocolIn->getTlsProtocol() : tlsProtocol,
						tlsCipherCollection.empty() ? persistenceProtocolIn->getTlsCipherCollection() : tlsCipherCollection,
						tlsCertCollection.IsNull() ? persistenceProtocolIn->getTlsCertCollection() : tlsCertCollection,
						persistenceProtocolLoaded->getUriAmqpPath(),
						persistenceProtocolLoaded->getUriTunnelPath(),
						persistenceProtocolLoaded->getTlsCertPath(),
						persistenceProtocolLoaded->getTlsCertPathCollection());
			}
		}
	}

	return rc;
}

SmartPtrCRemoteSecurityCollectionDoc CPersistenceMerge::mergeRemoteSecurityCollection(
		const SmartPtrCRemoteSecurityCollectionDoc& remoteSecurityCollectionLoaded,
		const SmartPtrCRemoteSecurityCollectionDoc& remoteSecurityCollectionIn) {
	SmartPtrCRemoteSecurityCollectionDoc rc;
	if (remoteSecurityCollectionLoaded.IsNull()) {
		if (! remoteSecurityCollectionIn.IsNull()) {
			rc = remoteSecurityCollectionIn;
		}
	} else {
		if (remoteSecurityCollectionIn.IsNull()) {
			rc = remoteSecurityCollectionLoaded;
		} else {
			const std::deque<SmartPtrCRemoteSecurityDoc> remoteSecurityCollectionInner = mergeRemoteSecurityCollectionInner(
					remoteSecurityCollectionLoaded->getRemoteSecurity(),
					remoteSecurityCollectionIn->getRemoteSecurity());
			if (! remoteSecurityCollectionInner.empty()) {
				rc.CreateInstance();
				rc->initialize(remoteSecurityCollectionInner);
			}
		}
	}

	return rc;
}

std::deque<SmartPtrCRemoteSecurityDoc> CPersistenceMerge::mergeRemoteSecurityCollectionInner(
		const std::deque<SmartPtrCRemoteSecurityDoc>& remoteSecurityCollectionInnerLoaded,
		const std::deque<SmartPtrCRemoteSecurityDoc>& remoteSecurityCollectionInnerIn) {
	std::deque<SmartPtrCRemoteSecurityDoc> rc;
	if (remoteSecurityCollectionInnerLoaded.empty()) {
		if (! remoteSecurityCollectionInnerIn.empty()) {
			rc = remoteSecurityCollectionInnerIn;
		}
	} else {
		if (remoteSecurityCollectionInnerIn.empty()) {
			rc = remoteSecurityCollectionInnerLoaded;
		} else {
			typedef std::map<std::string, std::pair<SmartPtrCRemoteSecurityDoc, SmartPtrCRemoteSecurityDoc> > CRemoteSecurityMap;
			CRemoteSecurityMap remoteSecurityMap;
			for (TConstIterator<std::deque<SmartPtrCRemoteSecurityDoc> > remoteSecurityLoadedIter(remoteSecurityCollectionInnerLoaded);
				remoteSecurityLoadedIter; remoteSecurityLoadedIter++) {
				const SmartPtrCRemoteSecurityDoc remoteSecurityLoaded = *remoteSecurityLoadedIter;
				remoteSecurityMap.insert(std::make_pair(
						remoteSecurityLoaded->getRemoteId(),
						std::make_pair(remoteSecurityLoaded, SmartPtrCRemoteSecurityDoc())));
			}
			for (TConstIterator<std::deque<SmartPtrCRemoteSecurityDoc> > remoteSecurityInIter(remoteSecurityCollectionInnerIn);
				remoteSecurityInIter; remoteSecurityInIter++) {
				const SmartPtrCRemoteSecurityDoc remoteSecurityIn = *remoteSecurityInIter;
				if (remoteSecurityMap.find(remoteSecurityIn->getRemoteId()) == remoteSecurityMap.end()) {
					remoteSecurityMap.insert(std::make_pair(
							remoteSecurityIn->getRemoteId(),
							std::make_pair(SmartPtrCRemoteSecurityDoc(), remoteSecurityIn)));
				} else {
					remoteSecurityMap.find(remoteSecurityIn->getRemoteId())->second.second = remoteSecurityIn;
				}
			}
			for (TConstMapIterator<CRemoteSecurityMap> remoteSecurityMapIter(remoteSecurityMap);
					remoteSecurityMapIter; remoteSecurityMapIter++) {
				const SmartPtrCRemoteSecurityDoc remoteSecurityLoaded = (*remoteSecurityMapIter).first;
				const SmartPtrCRemoteSecurityDoc remoteSecurityIn = (*remoteSecurityMapIter).second;
				const SmartPtrCRemoteSecurityDoc remoteSecurityTmp =
						mergeRemoteSecurity(remoteSecurityLoaded, remoteSecurityIn);
				if (! remoteSecurityTmp.IsNull()) {
					rc.push_back(remoteSecurityTmp);
				}
			}
		}
	}

	return rc;
}

SmartPtrCRemoteSecurityDoc CPersistenceMerge::mergeRemoteSecurity(
		const SmartPtrCRemoteSecurityDoc& remoteSecurityLoaded,
		const SmartPtrCRemoteSecurityDoc& remoteSecurityIn) {
	SmartPtrCRemoteSecurityDoc rc;
	if (remoteSecurityLoaded.IsNull()) {
		if (! remoteSecurityIn.IsNull()) {
			rc = remoteSecurityIn;
		}
	} else {
		if (remoteSecurityIn.IsNull()) {
			rc = remoteSecurityLoaded;
		} else {
			const std::string remoteId = mergeStrings(
					remoteSecurityLoaded->getRemoteId(), remoteSecurityIn->getRemoteId());
			const std::string protocolName = mergeStrings(
					remoteSecurityLoaded->getProtocolName(), remoteSecurityIn->getProtocolName());
			const std::string cmsCert = mergeStrings(
					remoteSecurityIn->getCmsCert(), remoteSecurityLoaded->getCmsCert());
			const std::string cmsCipherName = mergeStrings(
					remoteSecurityIn->getCmsCipherName(), remoteSecurityLoaded->getCmsCipherName());
			const SmartPtrCCertCollectionDoc cmsCertCollection = mergeCertCollection(
					remoteSecurityIn->getCmsCertCollection(), remoteSecurityLoaded->getCmsCertCollection());

			if (! remoteId.empty() || ! protocolName.empty() || ! cmsCert.empty() ||
					! cmsCipherName.empty() || ! cmsCertCollection.IsNull()) {
				rc.CreateInstance();
				rc->initialize(
						remoteId.empty() ? remoteSecurityIn->getRemoteId() : remoteId,
						protocolName.empty() ? remoteSecurityIn->getProtocolName() : protocolName,
						cmsCert.empty() ? remoteSecurityIn->getCmsCert() : cmsCert,
						cmsCipherName.empty() ? remoteSecurityIn->getCmsCipherName() : cmsCipherName,
						cmsCertCollection.IsNull() ? remoteSecurityIn->getCmsCertCollection() : cmsCertCollection,
						remoteSecurityLoaded->getCmsCertPath(),
						remoteSecurityLoaded->getCmsCertPathCollection());
			}
		}
	}

	return rc;
}

SmartPtrCCertCollectionDoc CPersistenceMerge::mergeCertCollection(
		const SmartPtrCCertCollectionDoc& certCollectionLoaded,
		const SmartPtrCCertCollectionDoc& certCollectionIn) {
	SmartPtrCCertCollectionDoc rc;
	if (certCollectionLoaded.IsNull()) {
		if (! certCollectionIn.IsNull()) {
			rc = certCollectionIn;
		}
	} else {
		if (certCollectionIn.IsNull()) {
			rc = certCollectionLoaded;
		} else {
			const std::deque<std::string> certCollectionInner = mergeDeqstr(
					certCollectionLoaded->getCert(),
					certCollectionIn->getCert());
			if (! certCollectionInner.empty()) {
				rc.CreateInstance();
				rc->initialize(certCollectionInner);
			}
		}
	}

	return rc;
}

std::string CPersistenceMerge::mergeUri(
		const std::string& uriPreferred,
		const std::string& uriOther) {
	CAF_CM_STATIC_FUNC_LOG_ONLY("CPersistenceMerge", "mergeUri");
	std::string rc;
	if (! uriPreferred.empty() && ! uriOther.empty()) {
		UriUtils::SUriRecord uriRecordPreferred;
		UriUtils::parseUriString(uriPreferred, uriRecordPreferred);

		UriUtils::SUriRecord uriRecordOther;
		UriUtils::parseUriString(uriOther, uriRecordOther);

		if (! uriRecordOther.host.empty() && (uriRecordPreferred.host.length() > 1)
				&& ('#' == uriRecordPreferred.host[0])
				&& ('#' == uriRecordPreferred.host[uriRecordPreferred.host.length() - 1])) {
			CAF_CM_LOG_DEBUG_VA2("URI host changed - %s != %s",
					uriRecordPreferred.host.c_str(), uriRecordOther.host.c_str());
			uriRecordPreferred.host = uriRecordOther.host;
		}
		if (! uriRecordOther.username.empty() && (uriRecordPreferred.username.length() > 1)
				&& ('#' == uriRecordPreferred.username[0])
				&& ('#' == uriRecordPreferred.username[uriRecordPreferred.username.length() - 1])) {
			CAF_CM_LOG_DEBUG_VA2("URI username changed - %s != %s",
					uriRecordPreferred.username.c_str(), uriRecordOther.username.c_str());
			uriRecordPreferred.username = uriRecordOther.username;
		}
		if (! uriRecordOther.username.empty() && (uriRecordPreferred.password.length() > 1)
				&& ('#' == uriRecordPreferred.password[0])
				&& ('#' == uriRecordPreferred.password[uriRecordPreferred.password.length() - 1])) {
			CAF_CM_LOG_DEBUG_VA0("URI password changed");
			uriRecordPreferred.password = uriRecordOther.password;
		}

		rc = UriUtils::buildUriString(uriRecordPreferred);
	} else {
		rc = mergeStrings(uriPreferred, uriOther);
	}

	return rc;
}

std::string CPersistenceMerge::mergeStrings(
		const std::string& strPreferred,
		const std::string& strOther) {
	return (strPreferred.compare(strOther) == 0) ? std::string() : strPreferred;
}

Cdeqstr CPersistenceMerge::mergeDeqstr(
		const Cdeqstr& deqstrPreferred,
		const Cdeqstr& deqstrOther) {
	Cdeqstr rc;
	if (deqstrPreferred.empty()) {
		if (! deqstrOther.empty()) {
			rc = deqstrOther;
		}
	} else {
		if (deqstrOther.empty()) {
			rc = deqstrPreferred;
		}
	}

	return rc;
}
