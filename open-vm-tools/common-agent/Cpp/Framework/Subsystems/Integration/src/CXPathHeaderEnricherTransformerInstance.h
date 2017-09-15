/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CXPathHeaderEnricherTransformerInstance_h_
#define CXPathHeaderEnricherTransformerInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "CXPathHeaderEnricherItem.h"
#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ITransformer.h"

namespace Caf {

class CXPathHeaderEnricherTransformerInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer
{
public:
	CXPathHeaderEnricherTransformerInstance();
	virtual ~CXPathHeaderEnricherTransformerInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(ITransformer)
	CAF_END_QI()

public: // IIntegrationObject
	void initialize(
		const IBean::Cargs& ctorArgs,
		const IBean::Cprops& properties,
		const SmartPtrIDocument& configSection);

	std::string getId() const;

public: // IIntegrationComponentInstance
	void wire(
		const SmartPtrIAppContext& appContext,
		const SmartPtrIChannelResolver& channelResolver);

public: // ITransformer
	SmartPtrIIntMessage transformMessage(
		const SmartPtrIIntMessage& message);

private:
	bool isInsertable(
		const std::string& name,
		const SmartPtrCXPathHeaderEnricherItem& value,
		const IIntMessage::SmartPtrCHeaders& headers);

	std::string evaluateXPathExpression(
		const std::string& name,
		const SmartPtrCXPathHeaderEnricherItem& value,
		const std::string& payloadXmlStr);

private:
	bool _isInitialized;
	std::string _id;
	SmartPtrIDocument _configSection;

	bool _defaultOverwrite;
	bool _shouldSkipNulls;
	typedef std::map<std::string, SmartPtrCXPathHeaderEnricherItem> Items;
	Items _headerItems;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CXPathHeaderEnricherTransformerInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CXPathHeaderEnricherTransformerInstance);

}

#endif // #ifndef CXPathHeaderEnricherTransformerInstance_h_
