/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageChannel.h"
#include "Integration/Core/CChannelResolver.h"
#include "Exception/CCafException.h"

using namespace Caf;

CChannelResolver::CChannelResolver() :
	_isInitialized(false),
	CAF_CM_INIT("CChannelResolver") {
}

CChannelResolver::~CChannelResolver() {
}

void CChannelResolver::initialize(
	const SmartPtrCIntegrationObjectCollection& integrationObjectCollection) {
	CAF_CM_FUNCNAME("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTSTL_NOTEMPTY(integrationObjectCollection);

		_integrationObjectCollection = integrationObjectCollection;

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

SmartPtrIMessageChannel CChannelResolver::resolveChannelName(
	const std::string& channelName) const {
	CAF_CM_FUNCNAME("resolveChannelName");

	SmartPtrIMessageChannel messageChannel;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(channelName);

		const SmartPtrIIntegrationObject integrationObject =
			resolveChannelNameToObject(channelName);

		messageChannel.QueryInterface(integrationObject, false);
		if (messageChannel.IsNull()) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Integration object is not a message channel - %s", channelName.c_str());
		}
	}
	CAF_CM_EXIT;

	return messageChannel;
}

SmartPtrIIntegrationObject CChannelResolver::resolveChannelNameToObject(
	const std::string& channelName) const {
	CAF_CM_FUNCNAME("resolveChannelNameToObject");

	SmartPtrIIntegrationObject integrationObject;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(channelName);

		const CIntegrationObjectCollection::const_iterator integrationObjectCollectionIter =
			_integrationObjectCollection->find(channelName);
		if (integrationObjectCollectionIter == _integrationObjectCollection->end()) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Bean with given ID not found - %s", channelName.c_str());
		}

		integrationObject = integrationObjectCollectionIter->second;
	}
	CAF_CM_EXIT;

	return integrationObject;
}
