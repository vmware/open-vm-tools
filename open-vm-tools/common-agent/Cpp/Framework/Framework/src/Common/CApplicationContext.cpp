/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IBean.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Common/CApplicationContext.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"

using namespace Caf;

CApplicationContext::CApplicationContext(void) :
	m_isInitialized(false),
	CAF_CM_INIT_LOG("CApplicationContext") {
}

CApplicationContext::~CApplicationContext(void) {
}

void CApplicationContext::initialize() {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(m_isInitialized);

	const std::string beanConfigFile = getDefaultBeanConfigFile();

	Cdeqstr filenameCollection;
	filenameCollection.push_front(beanConfigFile);

	initialize(filenameCollection);
}

void CApplicationContext::initialize(const Cdeqstr& filenameCollection) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(m_isInitialized);
	CAF_CM_VALIDATE_STL(filenameCollection);

	for (TConstIterator<Cdeqstr> filenameIter(filenameCollection);
		filenameIter; filenameIter++) {
		const std::string beanConfigFile = *filenameIter;

		parseBeanConfig(
				beanConfigFile,
				_beanCollection);
	}

	CBeanGraph beanGraph;
	createBeanGraph(
			_beanCollection,
			beanGraph,
			_beanTopologySort);

	try {
		initializeBeans(
				_beanCollection,
				_beanTopologySort);
	} CAF_CM_CATCH_ALL;

	if (CAF_CM_ISEXCEPTION) {
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CCafException *ex = CAF_CM_GETEXCEPTION;
		ex->AddRef();
		CAF_CM_CLEAREXCEPTION;
		try {
			terminateBeans(_beanTopologySort);
		} CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
		_beanTopologySort.clear();
		_beanCollection.clear();
		CAF_CM_GETEXCEPTION = ex;
		CAF_CM_THROWEXCEPTION;
	}

	m_isInitialized = true;
}

void CApplicationContext::terminate() {
	CAF_CM_FUNCNAME_VALIDATE("terminate");
	CAF_CM_PRECOND_ISINITIALIZED(m_isInitialized);

	terminateBeans(_beanTopologySort);
	_beanTopologySort.clear();
	_beanCollection.clear();
	_filenameCollection.clear();
}

IAppContext::SmartPtrCBeans CApplicationContext::getBeans() const {
	CAF_CM_FUNCNAME_VALIDATE("getBeans");
	CAF_CM_PRECOND_ISINITIALIZED(m_isInitialized);

	SmartPtrCBeans beans;
	beans.CreateInstance();
	for (TSmartConstMapIterator<CBeanCollection> beanIter(_beanCollection);
		beanIter; beanIter++) {
		beans->insert(CBeans::value_type(
			beanIter.getKey().c_str(),
			beanIter->_bean));
	}

	return beans;
}

SmartPtrIBean CApplicationContext::getBean(const std::string& beanId) const {
	CAF_CM_FUNCNAME("getBean");
	CAF_CM_PRECOND_ISINITIALIZED(m_isInitialized);
	CAF_CM_VALIDATE_STRING(beanId);

	CBeanCollection::const_iterator iter = _beanCollection.find(beanId);
	if (iter == _beanCollection.end()) {
		CAF_CM_EXCEPTIONEX_VA1(
				NoSuchElementException,
				0,
				"Bean not found - %s",
				beanId.c_str());
	}

	CAF_CM_LOG_DEBUG_VA1(
			"Bean Found - %s",
			beanId.c_str());
	return iter->second->_bean;
}

std::string CApplicationContext::getDefaultBeanConfigFile() const {
	CAF_CM_FUNCNAME("parseBeanConfig");

	// Get the bean config file
	const std::string beanConfigFile =
			AppConfigUtils::getRequiredString("bean_config_file");
	if (!FileSystemUtils::doesFileExist(beanConfigFile)) {
		CAF_CM_EXCEPTIONEX_VA1(
				FileNotFoundException,
				0,
				"The bean config file [%s] does not exist.",
				beanConfigFile.c_str());
	}

	return beanConfigFile;
}

void CApplicationContext::parseBeanConfig(
	const std::string& beanConfigFile,
	CBeanCollection& beanCollection) const {
	CAF_CM_FUNCNAME("parseBeanConfig");
	CAF_CM_VALIDATE_STRING(beanConfigFile);
	CAF_CM_LOG_DEBUG_VA1("Parsing bean config file %s", beanConfigFile.c_str());

	// We will look up class references early in the process to fail as early
	// as possible and to make logging better.
	// Parse the bean config file
	CXmlElement::SmartPtrCElementCollection rootElements =
			CXmlUtils::parseFile(beanConfigFile, "caf:beans")->getAllChildren();
	for (TSmartConstMultimapIterator<CXmlElement::CElementCollection> rootChild(*rootElements);
			rootChild;
			rootChild++) {

		// if the child is a bean...
		if (rootChild->getName() == "bean") {
			// Syntactic sugar
			const SmartPtrCXmlElement beanElement = *rootChild;

			// Bean attributes
			const std::string beanId = beanElement->findRequiredAttribute("id");
			CAF_CM_LOG_DEBUG_VA1("Parsing bean [id=%s]", beanId.c_str());
			const std::string beanClass = beanElement->findRequiredAttribute("class");
			CAF_CM_LOG_DEBUG_VA2(
					"Checking bean class [id=%s][class=%s]",
					beanId.c_str(),
					beanClass.c_str());
			if (!CEcmSubSystemRegistry::IsRegistered(beanClass)) {
				CAF_CM_EXCEPTIONEX_VA3(
						NoSuchElementException,
						0,
						"Bean class %s is not registered. Fix the AppConfig file. "
						"[bean id=%s][bean_config_file=%s]",
						beanClass.c_str(),
						beanId.c_str(),
						beanConfigFile.c_str());
			}

			// get optional constructor args and properties
			CBeanCtorArgCollection beanCtorArgs;
			Cmapstrstr beanProperties;
			CAF_CM_LOG_DEBUG_VA1("Parsing bean ctor args and properties [id=%s]", beanId.c_str());
			CXmlElement::SmartPtrCElementCollection beanElements = beanElement->getAllChildren();
			for (TSmartConstMultimapIterator<CXmlElement::CElementCollection> beanChild(*beanElements);
					beanChild;
					beanChild++) {
				if (beanChild->getName() == "property") {
					// Syntactic sugar
					const SmartPtrCXmlElement propArgElement = *beanChild;

					// property attributes
					const std::string name = propArgElement->findRequiredAttribute("name");
					const std::string value = propArgElement->findRequiredAttribute("value");
					if (!beanProperties.insert(std::make_pair(name, value)).second) {
						CAF_CM_EXCEPTIONEX_VA3(
								DuplicateElementException,
								0,
								"Bean property name is duplicated. "
								"[bean id=%s][property name=%s][bean_config_file=%s]",
								beanId.c_str(),
								name.c_str(),
								beanConfigFile.c_str());
					}
				}
				else if (beanChild->getName() == "constructor-arg") {
					// Syntactic sugar
					const SmartPtrCXmlElement ctorArgElement = *beanChild;

					// ctor attributes
					const uint32 ctorArgIndex = CStringConv::fromString<uint32>(ctorArgElement->findRequiredAttribute("index"));
					CBeanCtorArg::ARG_TYPE ctorArgType = CBeanCtorArg::NOT_SET;
					std::string ctorArgValue = ctorArgElement->findOptionalAttribute("value");
					if (ctorArgValue.length() > 0) {
						ctorArgType = CBeanCtorArg::VALUE;
					} else {
						ctorArgValue = ctorArgElement->findOptionalAttribute("ref");
						if (ctorArgValue.length() > 0) {
							ctorArgType = CBeanCtorArg::REFERENCE;
						} else {
							CAF_CM_EXCEPTIONEX_VA2(
									InvalidArgumentException,
									0,
									"Bean constructor argument must be of type value or ref and cannot be empty. "
									"[bean id=%s][bean_config_file=%s]",
									beanId.c_str(),
									beanConfigFile.c_str());
						}
					}

					if (!beanCtorArgs.insert(
							CBeanCtorArgCollection::value_type(
									ctorArgIndex,
									CBeanCtorArg(ctorArgType, ctorArgValue))).second) {
						CAF_CM_EXCEPTIONEX_VA3(
								DuplicateElementException,
								0,
								"Bean has a duplicate constructor-arg index. "
								"[bean id=%s][bean_config_file=%s][arg-index=%d]",
								beanId.c_str(),
								beanConfigFile.c_str(),
								ctorArgIndex);
					}
					CAF_CM_LOG_DEBUG_VA4(
							"Bean ctor arg parsed [id=%s][arg-index=%d][arg-type=%s][arg-value=%s]",
							beanId.c_str(),
							ctorArgIndex,
							(CBeanCtorArg::VALUE == ctorArgType ? "VALUE" : "REFERENCE"),
							ctorArgValue.c_str());
				}
			}

			// Add the bean definition to the collection
			SmartPtrCBeanNode beanNode;
			beanNode.CreateInstance();
			beanNode->_id = beanId;
			beanNode->_class = beanClass;
			beanNode->_ctorArgs = beanCtorArgs;
			beanNode->_properties = beanProperties;

			if (!beanCollection.insert(
					CBeanCollection::value_type(
							beanId,
							beanNode)).second) {
				CAF_CM_EXCEPTIONEX_VA3(
						DuplicateElementException,
						0,
						"Duplicate bean definition detected. "
						"[bean id=%s][bean class=%s][bean_config_file=%s]",
						beanId.c_str(),
						beanNode->_class.c_str(),
						beanConfigFile.c_str());
			}
		}
	}

	CAF_CM_LOG_DEBUG_VA2(
			"Bean configuration file defined %d beans. "
			"[file=%s]",
			beanCollection.size(),
			beanConfigFile.c_str());
}

void CApplicationContext::createBeanGraph(
		CBeanCollection& beanCollection,
		CBeanGraph& beanGraph,
		CBeanGraph::ClistVertexEdges& beanTopologySort) const {
	CAF_CM_FUNCNAME("createBeanGraph");

	// Iterate the bean collection and create the beans. They will not be initialized.
	// Two name sets will be built: bean names and contstructor-arg ref names.
	// These two sets will be compared to ensure that all referenced beans exist.
	Csetstr beanNames;
	Csetstr beanCtorRefNames;
	for (TSmartMapIterator<CBeanCollection> beanIter(beanCollection);
			beanIter;
			beanIter++) {

		// Create the bean and add it to the collection
		CAF_CM_LOG_DEBUG_VA2(
				"Creating bean [id=%s][class=%s]",
				beanIter.getKey().c_str(),
				beanIter->_class.c_str());
		beanIter->_bean.CreateInstance(beanIter->_class.c_str());

		// Add the bean id to the beanNames set
		if (!beanNames.insert(beanIter->_id).second) {
			CAF_CM_LOG_DEBUG_VA1(
				"Internal logic error: duplicate bean detected. "
				"[id=%s]",
				beanIter->_id.c_str());
		}

		// Add ref constructor args to the ctor ref name set
		for (TConstMapIterator<CBeanCtorArgCollection> beanCtorArg(beanIter->_ctorArgs);
				beanCtorArg;
				beanCtorArg++) {
			if (CBeanCtorArg::REFERENCE == beanCtorArg->_type) {
				beanCtorRefNames.insert(beanCtorArg->_value);
			}
		}
	}

	// Make sure that all beans referenced as ctor args exist
	Csetstr beanNameDiff;
	std::set_difference(
			beanCtorRefNames.begin(),
			beanCtorRefNames.end(),
			beanNames.begin(),
			beanNames.end(),
			std::inserter(beanNameDiff, beanNameDiff.end()));

	if (beanNameDiff.size()) {
		for (TConstIterator<Csetstr> missingName(beanNameDiff);
				missingName;
				missingName++) {
			CAF_CM_LOG_ERROR_VA1(
					"No bean definition exists for constructor-arg referenced bean '%s'",
					missingName->c_str());
		}
		CAF_CM_EXCEPTIONEX_VA0(
				NoSuchElementException,
				0,
				"One or more bean constructor-args references beans that are not defined.");
	}

	// Create a graph node for each bean
	for (TSmartConstMapIterator<CBeanCollection> beanIter(beanCollection);
			beanIter;
			beanIter++) {
		beanGraph.addVertex(*beanIter);
	}

	// Okay. Now connect the vertices according the constructor-arg references.
	// The resulting graph will give us the initialization/tear-down order.
	for (TSmartConstMapIterator<CBeanCollection> beanIter(beanCollection);
			beanIter;
			beanIter++) {
		for (TConstMapIterator<CBeanCtorArgCollection> ctorArg(beanIter->_ctorArgs);
				ctorArg;
				ctorArg++) {
			if (CBeanCtorArg::REFERENCE == ctorArg->_type) {
				CBeanCollection::const_iterator ctorBean = beanCollection.find(ctorArg->_value);
				if (beanCollection.end() == ctorBean) {
					CAF_CM_EXCEPTIONEX_VA1(
							NoSuchElementException,
							0,
							"Internal error: constructor-arg referenced bean '%s' is missing",
							ctorArg->_value.c_str());
				}
				beanGraph.addEdge(ctorBean->second, *beanIter);
			}
		}
	}

	// And finally - compute the bean topology sort order
	beanTopologySort = beanGraph.topologySort();

	// Debugging - you will thank me for this later
	CAF_CM_LOG_DEBUG_VA0("BEGIN: Bean initialization order")
	for (TSmartConstIterator<CBeanGraph::ClistVertexEdges> beanNode(beanTopologySort);
			beanNode;
			beanNode++) {
		CAF_CM_LOG_DEBUG_VA1("bean id=%s", beanNode->_id.c_str());
	}
	CAF_CM_LOG_DEBUG_VA0("END: Bean initialization order")
}

void CApplicationContext::initializeBeans(
		CBeanCollection& beanCollection,
		CBeanGraph::ClistVertexEdges& beanTopologySort) const {
	CAF_CM_FUNCNAME("initializeBeans");

	for (TSmartIterator<CBeanGraph::ClistVertexEdges> beanNode(beanTopologySort);
			beanNode;
			beanNode++) {
		CAF_CM_LOG_DEBUG_VA1("Initializing bean %s", beanNode->_id.c_str());

		// The bean should not have been initialized
		if (beanNode->_isInitialized) {
			CAF_CM_EXCEPTIONEX_VA1(
					IllegalStateException,
					0,
					"Internal error: Bean [%s] has already been initialized.",
					beanNode->_id.c_str());
		}

		// Iterate the contructor-args and build a collection to
		// pass to the bean initializer
		IBean::Cargs beanInitArgs;
		for (TConstMapIterator<CBeanCtorArgCollection> ctorArg(beanNode->_ctorArgs);
				ctorArg;
				ctorArg++) {
			switch (ctorArg->_type) {
				case CBeanCtorArg::REFERENCE: {
						CBeanCollection::const_iterator bean = beanCollection.find(ctorArg->_value);
						if (!bean->second->_isInitialized) {
							CAF_CM_EXCEPTIONEX_VA2(
									NullPointerException,
									0,
									"Internal error: Referenced bean not initialized. "
									"[bean id=%s][constructor-arg ref=%s]",
									beanNode->_id.c_str(),
									ctorArg->_value.c_str());
						}
						beanInitArgs.push_back(IBean::CArg(bean->second->_bean));
						CAF_CM_LOG_DEBUG_VA1(
								"constructor-arg ref=%s",
								ctorArg->_value.c_str());
					}
					break;

				case CBeanCtorArg::VALUE:
					beanInitArgs.push_back(IBean::CArg(ctorArg->_value));
					CAF_CM_LOG_DEBUG_VA1(
							"constructor-arg value=%s",
							ctorArg->_value.c_str());
					break;

				default:
					CAF_CM_EXCEPTIONEX_VA2(
							InvalidArgumentException,
							0,
							"Internal error: Bean constructor-arg is not a ref or value "
							"[bean id=%s][constructor-arg index=%d]",
							beanNode->_id.c_str(),
							ctorArg.getKey());
			}
		}

		// Iterate the bean properties and resolve value references
		SmartPtrIAppConfig appConfig = getAppConfig();
		Cmapstrstr properties = beanNode->_properties;

		for (TMapIterator<Cmapstrstr> property(properties);
				property;
				property++) {
			*property = appConfig->resolveValue(*property);
		}

		// Initialize the bean
		beanNode->_bean->initializeBean(beanInitArgs, properties);
		beanNode->_isInitialized = true;
	}
}

void CApplicationContext::terminateBeans(CBeanGraph::ClistVertexEdges& beanTopologySort) const {
	CAF_CM_FUNCNAME("terminateBeans");

	// Important! Iterate in reverse order of initialization
	// Some beans may not be initialized because of exceptions during init process
	for (CBeanGraph::ClistVertexEdges::reverse_iterator beanNode = beanTopologySort.rbegin();
			beanNode != beanTopologySort.rend();
			beanNode++) {
		if ((*beanNode)->_isInitialized) {
			CAF_CM_LOG_DEBUG_VA1(
					"Terminating bean %s",
					(*beanNode)->_id.c_str());
			try {
				(*beanNode)->_bean->terminateBean();
			}
			CAF_CM_CATCH_ALL;
			CAF_CM_LOG_CRIT_CAFEXCEPTION;
			CAF_CM_CLEAREXCEPTION;
		} else {
			CAF_CM_LOG_DEBUG_VA1(
					"Skipping termination of uninitialized bean %s",
					(*beanNode)->_id.c_str());
		}
	}
}
