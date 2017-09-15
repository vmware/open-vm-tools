/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CApplicationContext_h_
#define CApplicationContext_h_


#include "Common/IAppContext.h"

#include "IBean.h"

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER( CApplicationContext);

class COMMONAGGREGATOR_LINKAGE CApplicationContext : public IAppContext {
private:
	// Bean constructor arguments
	struct CBeanCtorArg {
		typedef enum {
			NOT_SET,
			REFERENCE,
			VALUE
		} ARG_TYPE;

		CBeanCtorArg(const ARG_TYPE type, const std::string& value) :
			_type(type),
			_value(value) {}

		ARG_TYPE _type;
		std::string _value;
	};

	// key=constructor-arg index
	typedef std::map<uint32, CBeanCtorArg> CBeanCtorArgCollection;

	struct CBeanNode {
		CBeanNode() :
			_isInitialized(false) {}

		std::string _id;
		std::string _class;
		SmartPtrIBean _bean;
		CBeanCtorArgCollection _ctorArgs;
		Cmapstrstr _properties;
		bool _isInitialized;
	};
	CAF_DECLARE_SMART_POINTER(CBeanNode);

	struct CBeanNodeLess {
		bool operator()(
				const SmartPtrCBeanNode& lhs,
				const SmartPtrCBeanNode& rhs) const {
			return std::less<std::string>()(lhs->_id, rhs->_id);
		}
	};

	typedef TEdgeListGraph<SmartPtrCBeanNode, CBeanNodeLess> CBeanGraph;

	// key=bean id
	typedef std::map<std::string, SmartPtrCBeanNode> CBeanCollection;

public:
	CApplicationContext();
	virtual ~CApplicationContext();

public:
	void initialize();
	void initialize(const Cdeqstr& filenameCollection);

	void terminate();

	SmartPtrCBeans getBeans() const;

public: // IApplicationContext
	SmartPtrIBean getBean(const std::string& name) const;

private:
	bool m_isInitialized;
	CBeanCollection _beanCollection;
	CBeanGraph::ClistVertexEdges _beanTopologySort;
	Cdeqstr _filenameCollection;

private:
	std::string getDefaultBeanConfigFile() const;

	void parseBeanConfig(
			const std::string& beanConfigFile,
			CBeanCollection& beanCollection) const;

	void createBeanGraph(
			CBeanCollection& beanCollection,
			CBeanGraph& beanGraph,
			CBeanGraph::ClistVertexEdges& beanTopologySort) const;

	void initializeBeans(
			CBeanCollection& beanCollection,
			CBeanGraph::ClistVertexEdges& beanTopologySort) const;

	void terminateBeans(CBeanGraph::ClistVertexEdges& beanTopologySort) const;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CApplicationContext);
};

}

#endif // #ifndef CApplicationContext_h_
