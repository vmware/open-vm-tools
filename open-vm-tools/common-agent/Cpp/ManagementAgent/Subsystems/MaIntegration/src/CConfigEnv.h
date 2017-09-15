/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _MaIntegration_CConfigEnv_h_
#define _MaIntegration_CConfigEnv_h_


#include "IBean.h"

#include "Doc/PersistenceDoc/CPersistenceDoc.h"
#include "IPersistence.h"
#include "IConfigEnv.h"

using namespace Caf;

/// TODO - describe class
class CConfigEnv :
	public TCafSubSystemObjectRoot<CConfigEnv>,
	public IBean,
	public IConfigEnv {
public:
	CConfigEnv();
	virtual ~CConfigEnv();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdConfigEnv)

	CAF_BEGIN_INTERFACE_MAP(CConfigEnv)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IConfigEnv)
	CAF_END_INTERFACE_MAP()

public: // IBean
	virtual void initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties);

	virtual void terminateBean();

public: // IConfigEnv
	void initialize(
			const SmartPtrIPersistence& persistenceRemove);

	SmartPtrCPersistenceDoc getUpdated(
			const int32 timeout);

	void update(
			const SmartPtrCPersistenceDoc& persistence);

private:
	void savePersistenceAppconfig(
			const SmartPtrCPersistenceDoc& persistence,
			const std::string& configDir) const;

	void executeScript(
			const std::string& scriptPath,
			const std::string& scriptResultsDir) const;

	void removePrivateKey(
			const SmartPtrCPersistenceDoc& persistence,
			const SmartPtrIPersistence& persistenceRemove) const;

	std::string calcListenerContext(
			const std::string& uriSchema,
			const std::string& configDir) const;

	void restartListener(
			const std::string& reason) const;

	void listenerConfiguredStage1(
			const std::string& reason) const;

	void listenerConfiguredStage2(
			const std::string& reason) const;

private:
	bool _isInitialized;
	std::string _persistenceDir;
	std::string _configDir;
	std::string _persistenceAppconfigPath;
	std::string _monitorDir;
	std::string _restartListenerPath;
	std::string _listenerConfiguredStage1Path;
	std::string _listenerConfiguredStage2Path;
	std::string _vcidPath;
	std::string _cacertPath;

	SmartPtrCPersistenceDoc _persistence;
	SmartPtrCPersistenceDoc _persistenceUpdated;
	SmartPtrIPersistence _persistenceRemove;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CConfigEnv);
};

#endif // #ifndef _MaIntegration_CConfigEnv_h_
