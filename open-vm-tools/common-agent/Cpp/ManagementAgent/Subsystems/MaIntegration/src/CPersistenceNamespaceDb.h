/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef _MaIntegration_CPersistenceNamespaceDb_h_
#define _MaIntegration_CPersistenceNamespaceDb_h_

using namespace Caf;

/// TODO - describe class
class CPersistenceNamespaceDb :
	public TCafSubSystemObjectRoot<CPersistenceNamespaceDb>,
	public IBean,
	public IPersistence {
public:
	CPersistenceNamespaceDb();
	virtual ~CPersistenceNamespaceDb();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdPersistenceNamespaceDb)

	CAF_BEGIN_INTERFACE_MAP(CPersistenceNamespaceDb)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IPersistence)
	CAF_END_INTERFACE_MAP()

public: // IBean
	virtual void initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties);

	virtual void terminateBean();

public: // IPersistence
	void initialize();

	SmartPtrCPersistenceDoc getUpdated(
			const int32 timeout);

	void update(
			const SmartPtrCPersistenceDoc& persistence);

	void remove(
			const SmartPtrCPersistenceDoc& persistence);
			
private:
	static std::string getCmdPath();
	void setCmd();
	std::string getValue(const std::string& key);
	void setValue(const std::string& key, const std::string& value);
	void removeKey(const std::string& key);

private:
	bool _isInitialized;
	static const std::string _NAMESPACE_DB_CMD_FILE;
	static const std::string _NAMESPACE;
	std::string _namespaceDbCmd;
	Cmapstrstr cache;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CPersistenceNamespaceDb);
};

#endif // #ifndef _MaIntegration_CPersistenceNamespaceDb_h_
