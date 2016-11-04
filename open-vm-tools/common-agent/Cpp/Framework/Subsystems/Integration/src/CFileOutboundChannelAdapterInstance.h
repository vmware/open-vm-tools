/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CFileOutboundChannelAdapterInstance_h_
#define CFileOutboundChannelAdapterInstance_h_


#include "Integration/IErrorProcessor.h"

#include "Common/IAppContext.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/IMessageHandler.h"

namespace Caf {

class CFileOutboundChannelAdapterInstance :
	public IIntegrationObject,
	public IIntegrationComponentInstance,
	public IMessageHandler,
	public IErrorProcessor {
public:
	CFileOutboundChannelAdapterInstance();
	virtual ~CFileOutboundChannelAdapterInstance();

	CAF_BEGIN_QI()
		CAF_QI_ENTRY(IIntegrationObject)
		CAF_QI_ENTRY(IIntegrationComponentInstance)
		CAF_QI_ENTRY(IMessageHandler)
		CAF_QI_ENTRY(IErrorProcessor)
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

public: // IMessageHandler
	void handleMessage(
		const SmartPtrIIntMessage& message);

	SmartPtrIIntMessage getSavedMessage() const;

	void clearSavedMessage();

public: // IErrorProcessor
	SmartPtrIIntMessage processErrorMessage(
		const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;
	std::string _id;
	std::string _directory;
	std::string _temporaryFileSuffix;
	std::string _modeStr;
	bool _autoCreateDirectory;
	bool _deleteSourceFiles;
	SmartPtrIIntMessage _savedMessage;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CFileOutboundChannelAdapterInstance);

private:
	void savePayloadToFile(
		const SmartPtrIIntMessage& message) const;

	std::string generateFilename(
		const SmartPtrIIntMessage& message) const;

	FileSystemUtils::FILE_MODE_TYPE translateMode(
		const std::string modeStr) const;

	void deleteSourceFiles(
		const SmartPtrIIntMessage& message) const;
};
CAF_DECLARE_SMART_QI_POINTER(CFileOutboundChannelAdapterInstance);
}

#endif // #ifndef CFileOutboundChannelAdapterInstance_h_
