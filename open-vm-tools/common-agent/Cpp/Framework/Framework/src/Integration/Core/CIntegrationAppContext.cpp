/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IBean.h"
#include "Integration/Core/CChannelResolver.h"
#include "Integration/Core/CDocument.h"
#include "Integration/IChannelInterceptor.h"
#include "Integration/IChannelInterceptorInstance.h"
#include "Integration/IChannelInterceptorSupport.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationAppContext.h"
#include "Integration/IIntegrationAppContextAware.h"
#include "Integration/IIntegrationComponent.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IMessageProducer.h"
#include "Integration/IPhased.h"
#include "Integration/ISmartLifecycle.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Integration/Core/CIntegrationAppContext.h"
#include "Exception/CCafException.h"

using namespace Caf;

CIntegrationAppContext::CIntegrationAppContext() :
	_isInitialized(false),
	_isIntegrationObjectCollectionReady(false),
	_lifecycleBeansStarted(false),
	_timeoutMs(0),
	CAF_CM_INIT_LOG("CIntegrationAppContext") {
}

CIntegrationAppContext::~CIntegrationAppContext() {
	if (_weakSelfReference) {
		_weakSelfReference->set(NULL);
	}
}

void CIntegrationAppContext::initialize(
		const uint32 timeoutMs) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

	const std::string beanConfigPath = getDefaultBeanConfigPath();
	initializeRaw(timeoutMs, beanConfigPath, true);
}

void CIntegrationAppContext::initialize(
	const uint32 timeoutMs,
	const std::string& beanConfigPath) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(beanConfigPath);

	initializeRaw(timeoutMs, beanConfigPath, true);
}

void CIntegrationAppContext::initializeTwoPhase(
	const uint32 timeoutMs,
	const std::string& beanConfigPath) {
	CAF_CM_FUNCNAME_VALIDATE("initializeTwoPhase");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(beanConfigPath);

	initializeRaw(timeoutMs, beanConfigPath, false);
}

void CIntegrationAppContext::initializeRaw(
	const uint32 timeoutMs,
	const std::string& beanConfigPath,
	const bool startLifecycleBeans) {
	CAF_CM_FUNCNAME("initializeRaw");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(beanConfigPath);

	_timeoutMs = timeoutMs;

	_weakSelfReference.CreateInstance();
	_weakSelfReference->set(this);

	const Cdeqstr beanConfigPathCollection =
	getBeanConfigFiles(beanConfigPath);

	_applicationContext.CreateInstance();
	_applicationContext->initialize(beanConfigPathCollection);
	const IAppContext::SmartPtrCBeans contextBeans =
		_applicationContext->getBeans();

	_integrationObjectCollection = assign(contextBeans, beanConfigPathCollection);

	// Set context aware objects' appContext reference
	for (TSmartConstMapIterator<CIntegrationObjectCollection> object(*_integrationObjectCollection);
			object;
			object++) {
		SmartPtrIIntegrationAppContextAware contextAwareObj;
		contextAwareObj.QueryInterface(*object, false);
		if (contextAwareObj) {
			contextAwareObj->setIntegrationAppContext(_weakSelfReference);
		}
	}

	// Inject channel interceptors
	injectChannelInterceptors(_integrationObjectCollection);
	_isIntegrationObjectCollectionReady = true;

	// Create the channel resolver and wire up the integration objects
	SmartPtrCChannelResolver channelResolver;
	channelResolver.CreateInstance();
	channelResolver->initialize(_integrationObjectCollection);
	_channelResolver = channelResolver;
	wire(_applicationContext, _integrationObjectCollection, _channelResolver);

	// Build up the collection of ILifecycle objects and determine
	// the startup order.
	//
	// ISmartLifecycle objects will provide a phase number to state their
	// desired relative load order.
	//
	// ILifecycle objects do not provide phase and will be set to order:
	// 0 - non-message producing
	// 1 - message producing
	for (TSmartConstMapIterator<CIntegrationObjectCollection> object(*_integrationObjectCollection);
			object;
			object++) {
		SmartPtrILifecycle lifecycleObj;
		lifecycleObj.QueryInterface(*object, false);
		if (lifecycleObj) {
			SmartPtrISmartLifecycle smartLifecycleObj;
			smartLifecycleObj.QueryInterface(*object, false);
			if (smartLifecycleObj) {
				SmartPtrIPhased phasedObj;
				phasedObj.QueryInterface(*object, false);
				if (phasedObj) {
					_lifecycleBeans.insert(
							LifecycleBeans::value_type(
									phasedObj->getPhase(),
									lifecycleObj));
				} else {
					CAF_CM_EXCEPTIONEX_VA1(
							NoSuchInterfaceException,
							0,
							"ISmartLifecycle object '%s' must also "
							"support the IPhased interface.",
							object.getKey().c_str());
				}
			} else {
				SmartPtrIMessageProducer messageProducer;
				messageProducer.QueryInterface(*object, false);
				_lifecycleBeans.insert(
						LifecycleBeans::value_type(
								messageProducer && messageProducer->isMessageProducer() ? 1 : 0,
								lifecycleObj));
			}
		}
	}

	if (_lifecycleBeans.size() && CAF_CM_IS_LOG_DEBUG_ENABLED) {
		CAF_CM_LOG_DEBUG_VA0("Lifecycle startup order:");
		for (TSmartConstMultimapIterator<LifecycleBeans> lcBean(_lifecycleBeans);
				lcBean;
				lcBean++) {
			SmartPtrIIntegrationObject obj;
			obj.QueryInterface(*lcBean);
			CAF_CM_LOG_DEBUG_VA2(
					"    [phase=%d][id=%s]",
					lcBean.getKey(),
					obj->getId().c_str());
		}
	}

	// Start the lifecycle objects
	if (startLifecycleBeans) {
		_lifecycleBeansStarted = true;
		startStop(_lifecycleBeans, _timeoutMs, true);
	}

	_isInitialized = true;
}

void CIntegrationAppContext::startLifecycleBeans() {
	CAF_CM_FUNCNAME_VALIDATE("startLifecycleBeans");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_PRECOND_ISNOTINITIALIZED(_lifecycleBeansStarted);

	_lifecycleBeansStarted = true;
	startStop(_lifecycleBeans, _timeoutMs, true);
}

void CIntegrationAppContext::terminate(const uint32 timeoutMs) {
	CAF_CM_FUNCNAME("terminate");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_PRECOND_ISINITIALIZED(_lifecycleBeansStarted);

	try {
		_weakSelfReference->set(NULL);
		startStop(_lifecycleBeans, timeoutMs, false);
		_isInitialized = false;
		_isIntegrationObjectCollectionReady = false;
		_channelResolver = NULL;
		_integrationObjectCollection->clear();
		_integrationObjectCollection = NULL;
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	try {
		_applicationContext->terminate();
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	try {
		_applicationContext = NULL;
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;
}

SmartPtrIIntegrationObject CIntegrationAppContext::getIntegrationObject(
		const std::string& id) const {
	CAF_CM_FUNCNAME("getIntegrationObject");
	CAF_CM_VALIDATE_STRING(id);
	CAF_CM_PRECOND_ISINITIALIZED(_isIntegrationObjectCollectionReady);

	CIntegrationObjectCollection::const_iterator objIter =
			_integrationObjectCollection->find(id);
	SmartPtrIIntegrationObject object;
	if (objIter != _integrationObjectCollection->end()) {
		object = objIter->second;
	} else {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Integration object [id=%s] is not in the collection",
				id.c_str());
	}

	return object;
}

void CIntegrationAppContext::getIntegrationObject(const IID& iid, void **ppv) const {
	CAF_CM_FUNCNAME("getIntegrationObject");
	CAF_CM_PRECOND_ISINITIALIZED(_isIntegrationObjectCollectionReady);
	CAF_CM_VALIDATE_PTR(ppv);

	for (TSmartConstMapIterator<CIntegrationObjectCollection> objIter(*_integrationObjectCollection);
			objIter;
			objIter++) {
		void *result;
		(*objIter).QueryInterface(iid, &result);
		if (result) {
			if (*ppv) {
				CAF_CM_EXCEPTIONEX_VA1(
						DuplicateElementException,
						0,
						"More than one integration object [iid=%s] was found.",
						BasePlatform::UuidToString(iid).c_str());
			} else {
				*ppv = result;
			}
		}
	}

	if (!*ppv) {
			CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Integration object [iid=%s] is not in the collection",
				BasePlatform::UuidToString(iid).c_str());
	}
}

IIntegrationAppContext::SmartPtrCObjectCollection
CIntegrationAppContext::getIntegrationObjects(const IID& iid) const {
	CAF_CM_FUNCNAME_VALIDATE("getIntegrationObjects");

	SmartPtrCObjectCollection collection;
	collection.CreateInstance();
	CAF_CM_PRECOND_ISINITIALIZED(_isIntegrationObjectCollectionReady);
	for (TSmartMapIterator<CIntegrationObjectCollection> objIter(*_integrationObjectCollection);
			objIter;
			objIter++) {
		void *result;
		(*objIter).QueryInterface(iid, &result);
		if (result) {
			collection->push_back(*objIter);
			objIter->Release();
		}
	}

	return collection;
}

SmartPtrIBean CIntegrationAppContext::getBean(const std::string& name) const {
	CAF_CM_FUNCNAME_VALIDATE("getBean");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(name);

	return _applicationContext->getBean(name);
}

SmartPtrIMessageChannel CIntegrationAppContext::resolveChannelName(
	const std::string& channelName) const {
	CAF_CM_FUNCNAME_VALIDATE("resolveChannelName");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(channelName);

	return _channelResolver->resolveChannelName(channelName);
}

SmartPtrIIntegrationObject CIntegrationAppContext::resolveChannelNameToObject(
	const std::string& channelName) const {
	CAF_CM_FUNCNAME_VALIDATE("resolveChannelNameToObject");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(channelName);

	return _channelResolver->resolveChannelNameToObject(channelName);
}

SmartPtrCIntegrationObjectCollection CIntegrationAppContext::assign(
	const IAppContext::SmartPtrCBeans& contextBeans,
	const Cdeqstr& beanConfigPathCollection) const {
	CAF_CM_FUNCNAME("assign");
	CAF_CM_VALIDATE_SMARTPTR(contextBeans);
	CAF_CM_VALIDATE_STL(beanConfigPathCollection);

	SmartPtrCIntegrationObjectCollection integrationObjectCollection;
	integrationObjectCollection.CreateInstance();
	addBuiltIn("nullChannelBean", contextBeans, integrationObjectCollection);
	addBuiltIn("errorChannelBean", contextBeans, integrationObjectCollection);

	// Parse the bean config file
	for (TConstIterator<Cdeqstr> beanConfigPathIter(beanConfigPathCollection);
		beanConfigPathIter; beanConfigPathIter++) {
		const std::string beanConfigPath = *beanConfigPathIter;

		const SmartPtrCXmlElement configBeanRoot = CXmlUtils::parseFile(beanConfigPath, "caf:beans");
		const CXmlElement::SmartPtrCElementCollection configBeanChildren =
		configBeanRoot->getAllChildren();
		for (TSmartConstMapIterator<CXmlElement::CElementCollection> configBeanIter(*configBeanChildren);
			configBeanIter; configBeanIter++) {
			const SmartPtrCXmlElement configBean = *configBeanIter;

			// if the child is anything but a bean...
			if ((configBean->getName() != "bean") && (configBean->getName() != "import")) {
				SmartPtrCDocument document;
				document.CreateInstance();
				document->initialize(configBean);
				const SmartPtrIDocument configSection = document;

				SmartPtrIIntegrationObject integrationObject;
				for (TSmartConstMapIterator<IAppContext::CBeans> contextBeanIter(*contextBeans);
					contextBeanIter; contextBeanIter++) {
					const SmartPtrIBean contextBean = *contextBeanIter;

					SmartPtrIIntegrationComponent integrationComponent;
					integrationComponent.QueryInterface(contextBean, false);

					if (!integrationComponent.IsNull() &&
						integrationComponent->isResponsible(configSection)) {
						integrationObject = integrationComponent->createObject(configSection);
						if (!integrationObject) {
							CAF_CM_EXCEPTIONEX_VA2(
									NullPointerException,
									0,
									"Component took responsibility for section - beanId: %s,"
									" section: %s - but createObject() returned NULL",
									contextBeanIter.getKey().c_str(),
									configSection->getName().c_str());
						}

						CAF_CM_LOG_DEBUG_VA3(
							"Component took responsibility for section - beanId: %s, section: %s, id: %s",
							contextBeanIter.getKey().c_str(), configSection->getName().c_str(), integrationObject->getId().c_str());

						break;
					}
				}

				if (integrationObject.IsNull()) {
					CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
						"Nothing took responsibility for the section: %s",
						configBean->saveToStringRaw().c_str());
				}

				const std::string id = integrationObject->getId();
				if (integrationObjectCollection->find(id) != integrationObjectCollection->end()) {
					CAF_CM_EXCEPTIONEX_VA1(DuplicateElementException, ERROR_ALREADY_EXISTS,
						"Duplicate id: %s", id.c_str());
				}

				integrationObjectCollection->insert(std::make_pair(id, integrationObject));
			}
		}
	}

	return integrationObjectCollection;
}

void CIntegrationAppContext::injectChannelInterceptors(
	const SmartPtrCIntegrationObjectCollection& integrationObjectCollection) const {
	CAF_CM_FUNCNAME("injectChannelInterceptors");
	CAF_CM_VALIDATE_SMARTPTR(integrationObjectCollection);
	CAF_CM_LOG_DEBUG_VA0("Searching for Channels and ChannelInterceptors");

	// Build two collections: Channels and ChannelInterceptors
	// ChannelInterceptors are wired in order, if specified.
	// Order does not need to be unique. Multiple interceptors at the same
	// order will be wired in first-seen order.
	typedef std::multimap<uint32, SmartPtrIChannelInterceptorInstance> CChannelInterceptorMap;
	CChannelInterceptorMap channelInterceptors;
	typedef std::list<SmartPtrIMessageChannel> CChannelList;
	CChannelList channels;
	for (TSmartConstMapIterator<CIntegrationObjectCollection> object(*integrationObjectCollection);
			object;
			object++) {
		SmartPtrIMessageChannel channel;
		channel.QueryInterface(*object, false);
		if (channel) {
			channels.push_back(channel);
		} else {
			SmartPtrIChannelInterceptorInstance interceptorInstance;
			interceptorInstance.QueryInterface(*object, false);
			if (interceptorInstance) {
				channelInterceptors.insert(
						CChannelInterceptorMap::value_type(
								interceptorInstance->getOrder(),
								interceptorInstance));
			}
		}
	}

	if (channelInterceptors.size()) {
		// For each channel create a list of ChannelInterceptors that
		// apply to it and add the list to the channel.
		for (TSmartConstIterator<CChannelList> channel(channels);
				channel;
				channel++) {
			// Get the channel id
			SmartPtrIIntegrationObject integrationObj;
			integrationObj.QueryInterface(*channel);
			const std::string channelId = integrationObj->getId();

			// Channels must support the IChannelInterceptorSupport interface
			SmartPtrIChannelInterceptorSupport interceptorSupport;
			interceptorSupport.QueryInterface(*channel, false);
			if (!interceptorSupport) {
				CAF_CM_EXCEPTIONEX_VA1(
						UnsupportedOperationException,
						E_NOINTERFACE,
						"Channel [%s] does not support the required "
						"IChannelInterceptorSupport interface",
						channelId.c_str());
			}

			IChannelInterceptorSupport::InterceptorCollection interceptors;
			for (TSmartConstMultimapIterator<CChannelInterceptorMap> interceptorInstance(channelInterceptors);
					interceptorInstance;
					interceptorInstance++) {
				integrationObj.QueryInterface(*interceptorInstance);
				const std::string interceptorId = integrationObj->getId();
				if (interceptorInstance->isChannelIdMatched(channelId)) {
					CAF_CM_LOG_DEBUG_VA3(
							"Adding interceptor [id=%s order=%d] to channel [%s]",
							interceptorId.c_str(),
							interceptorInstance->getOrder(),
							channelId.c_str());
					SmartPtrIChannelInterceptor interceptor;
					interceptor.QueryInterface(*interceptorInstance);
					interceptors.push_back(interceptor);
				}
			}

			if (interceptors.size()) {
				interceptorSupport->setInterceptors(interceptors);
			}
		}
	} else {
		CAF_CM_LOG_DEBUG_VA0("No ChannelInterceptors were found");
	}
}

void CIntegrationAppContext::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrCIntegrationObjectCollection& integrationObjectCollection,
	const SmartPtrCChannelResolver& channelResolver) const {
	CAF_CM_FUNCNAME_VALIDATE("wire");
	CAF_CM_VALIDATE_INTERFACE(appContext);
	CAF_CM_VALIDATE_SMARTPTR(integrationObjectCollection);

	for (TSmartConstMapIterator<CIntegrationObjectCollection> integrationObjectIter(*integrationObjectCollection);
		integrationObjectIter; integrationObjectIter++) {
		const SmartPtrIIntegrationObject integrationObject = *integrationObjectIter;

		SmartPtrIIntegrationComponentInstance integrationComponentInstance;
		integrationComponentInstance.QueryInterface(integrationObject, false);
		if (integrationComponentInstance) {
			CAF_CM_LOG_DEBUG_VA1(
				"Wiring - id: %s", integrationObject->getId().c_str());
			integrationComponentInstance->wire(appContext, channelResolver);
		}
	}
}

void CIntegrationAppContext::startStop(
	const LifecycleBeans& lifecycleBeans,
	const uint32 timeoutMs,
	const bool isStart) const {
	CAF_CM_FUNCNAME("startStop");

	if (isStart) {
		for (LifecycleBeans::const_iterator lifecycleObjIter = lifecycleBeans.begin();
				lifecycleObjIter != lifecycleBeans.end();
				++lifecycleObjIter) {
			SmartPtrILifecycle lifecycleObj = lifecycleObjIter->second;
			SmartPtrIIntegrationObject integrationObj;
			integrationObj.QueryInterface(lifecycleObj, true);
			bool bAutoStart = true;
			SmartPtrISmartLifecycle smartLifecycleObj;
			smartLifecycleObj.QueryInterface(lifecycleObj, false);
			if (smartLifecycleObj) {
				bAutoStart = smartLifecycleObj->isAutoStartup();
			}
			if (bAutoStart) {
				CAF_CM_LOG_DEBUG_VA2(
					"Starting lifecycle object [phase:%d][id:%s]",
					lifecycleObjIter->first,
					integrationObj->getId().c_str());
				lifecycleObj->start(timeoutMs);
			} else {
				CAF_CM_LOG_DEBUG_VA2(
					"Skipping lifecycle object start [phase:%d][id:%s] "
					"because isAutoStartup=false",
					lifecycleObjIter->first,
					integrationObj->getId().c_str());
			}
		}
	} else {
		for (LifecycleBeans::const_reverse_iterator lifecycleObjIter = lifecycleBeans.rbegin();
				lifecycleObjIter != lifecycleBeans.rend();
				++lifecycleObjIter) {
			try {
				SmartPtrILifecycle lifecycleObj = lifecycleObjIter->second;
				SmartPtrIIntegrationObject integrationObj;
				integrationObj.QueryInterface(lifecycleObj, true);
				CAF_CM_LOG_DEBUG_VA2(
					"Stopping lifecycle object [phase:%d][id:%s]",
					lifecycleObjIter->first,
					integrationObj->getId().c_str());
				lifecycleObj->stop(timeoutMs);
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_ERROR_CAFEXCEPTION;
			CAF_CM_CLEAREXCEPTION;
		}
	}
}

void CIntegrationAppContext::addBuiltIn(
	const std::string& beanId,
	const IAppContext::SmartPtrCBeans& beans,
	SmartPtrCIntegrationObjectCollection& integrationObjectCollection) const {
	CAF_CM_FUNCNAME_VALIDATE("addBuiltIn");
	CAF_CM_VALIDATE_STRING(beanId);
	CAF_CM_VALIDATE_SMARTPTR(beans);
	CAF_CM_VALIDATE_SMARTPTR(integrationObjectCollection);

	const SmartPtrIDocument configSection;

	const IAppContext::CBeans::const_iterator beanIter = beans->find(beanId);
	if (beanIter != beans->end()) {
		const SmartPtrIBean bean = beanIter->second;

		SmartPtrIIntegrationComponent integrationComponent;
		integrationComponent.QueryInterface(bean, false);

		if (!integrationComponent.IsNull()) {
			const SmartPtrIIntegrationObject integrationObject =
			integrationComponent->createObject(configSection);

			const std::string id = integrationObject->getId();
			integrationObjectCollection->insert(std::make_pair(id, integrationObject));
		}
	}
}

std::string CIntegrationAppContext::getDefaultBeanConfigPath() const {
	CAF_CM_FUNCNAME("getDefaultBeanConfigPath");

	// Get the bean config file
	const std::string appConfigKey = "bean_config_file";
	std::string beanConfigPath = AppConfigUtils::getRequiredString(appConfigKey);
	beanConfigPath = CStringUtils::expandEnv(beanConfigPath);
	if (!FileSystemUtils::doesFileExist(beanConfigPath)) {
		CAF_CM_EXCEPTIONEX_VA2(FileNotFoundException, ERROR_FILE_NOT_FOUND,
			"The bean config file does not exist - appConfigKey: %s, file: %s",
			appConfigKey.c_str(), beanConfigPath.c_str());
	}

	return beanConfigPath;
}

Cdeqstr CIntegrationAppContext::getBeanConfigFiles(
	const std::string& beanConfigPath) const {
	CAF_CM_FUNCNAME("getBeanConfigFiles");
	CAF_CM_VALIDATE_STRING(beanConfigPath);

	// Get the bean config file
	const std::string beanConfigPathExp = CStringUtils::expandEnv(beanConfigPath);
	if (!FileSystemUtils::doesFileExist(beanConfigPathExp)) {
		CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
			"The bean config file does not exist - file: %s",
			beanConfigPathExp.c_str());
	}

	CAF_CM_LOG_DEBUG_VA1("Adding bean file - %s", beanConfigPathExp.c_str());
	Cdeqstr beanConfigPathCollection;
	beanConfigPathCollection.push_front(beanConfigPathExp);

	const SmartPtrCXmlElement rootXml = CXmlUtils::parseFile(beanConfigPathExp, "caf:beans");
	const CXmlElement::SmartPtrCElementCollection beanChildrenXml = rootXml->getAllChildren();
	CAF_CM_VALIDATE_SMARTPTR(beanChildrenXml);

	const std::string beanConfigDirname = FileSystemUtils::getDirname(beanConfigPathExp);
	for (TSmartConstMapIterator<CXmlElement::CElementCollection> beanXmlIter(*beanChildrenXml);
		beanXmlIter; beanXmlIter++) {
		if (beanXmlIter.getKey().compare("import") == 0) {
			const SmartPtrCXmlElement importXml = *beanXmlIter;
			const std::string import = importXml->findRequiredAttribute("resource");
			const std::string importExp = CStringUtils::expandEnv(import);

			std::string importPath = FileSystemUtils::buildPath(beanConfigDirname, importExp);
			if (! FileSystemUtils::doesFileExist(importPath)) {
				importPath = importExp;
				if (! FileSystemUtils::doesFileExist(importPath)) {
					CAF_CM_EXCEPTIONEX_VA2(FileNotFoundException, ERROR_FILE_NOT_FOUND,
						"Import does not exist - origFile: %s, importPath: %s",
						beanConfigPath.c_str(), importPath.c_str());
				}
			}

			CAF_CM_LOG_DEBUG_VA1("Adding bean file - %s", importPath.c_str());
			beanConfigPathCollection.push_front(importPath);
		}
	}

	return beanConfigPathCollection;
}

CIntegrationAppContext::
CIntegrationAppContextWeakReference::CIntegrationAppContextWeakReference() :
	_context(NULL),
	CAF_CM_INIT("CIntegrationAppContext") {
	CAF_CM_INIT_THREADSAFE;
}

void CIntegrationAppContext::
CIntegrationAppContextWeakReference::set(CIntegrationAppContext *context) {
	CAF_CM_LOCK_UNLOCK;
	_context = context;
}

SmartPtrIIntegrationObject CIntegrationAppContext::
CIntegrationAppContextWeakReference::getIntegrationObject(
		const std::string& id) const {
	CAF_CM_FUNCNAME_VALIDATE("getIntegrtationObject");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_context);
	return _context->getIntegrationObject(id);
}

void CIntegrationAppContext::
CIntegrationAppContextWeakReference::getIntegrationObject(
		const IID& iid,
		void **ppv) const {
	CAF_CM_FUNCNAME_VALIDATE("getIntegrtationObject");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_context);
	_context->getIntegrationObject(iid, ppv);
}

IIntegrationAppContext::SmartPtrCObjectCollection CIntegrationAppContext::
CIntegrationAppContextWeakReference::getIntegrationObjects(
		const IID& iid) const {
	CAF_CM_FUNCNAME_VALIDATE("getIntegrtationObjects");
	CAF_CM_LOCK_UNLOCK;
	CAF_CM_VALIDATE_PTR(_context);
	return _context->getIntegrationObjects(iid);
}
