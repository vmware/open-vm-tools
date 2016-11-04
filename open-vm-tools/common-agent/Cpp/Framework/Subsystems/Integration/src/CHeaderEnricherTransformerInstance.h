/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CHeaderEnricherTransformerInstance_h_
#define CHeaderEnricherTransformerInstance_h_


#include "Integration/IIntegrationComponentInstance.h"

#include "Common/IAppContext.h"
#include "Integration/Core/CExpressionHandler.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/ITransformer.h"
#include "Integration/IIntegrationObject.h"

namespace Caf {

class CHeaderEnricherTransformerInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public ITransformer
{
public:
	CHeaderEnricherTransformerInstance();
	virtual ~CHeaderEnricherTransformerInstance();

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
	bool _isInitialized;
	std::string _id;
	std::string _errorChannelRef;
	SmartPtrIDocument _configSection;
	typedef std::map<std::string, SmartPtrITransformer> Transformers;
	Transformers _headerWithRef;
	typedef std::map<std::string, SmartPtrCExpressionHandler> Expressions;
	Expressions _headerWithExpression;
	Cmapstrstr _headerWithValue;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CHeaderEnricherTransformerInstance);
};
CAF_DECLARE_SMART_QI_POINTER(CHeaderEnricherTransformerInstance);

}

#endif // #ifndef CHeaderEnricherTransformerInstance_h_
