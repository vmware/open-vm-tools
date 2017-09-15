/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (C) 2002-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/IAppConfig.h"
#include "CEcmSubSystemRegistry.h"
#include "CEcmDllManager.h"
#include <algorithm>

using namespace Caf;

bool CEcmSubSystemRegistry::IsRegistered( const std::string & crstrSubSystemIdentifier )
{
	std::string modulePath;
	return getAppConfig()->getString("subsystems", crstrSubSystemIdentifier, modulePath, IConfigParams::PARAM_OPTIONAL);
}

std::string CEcmSubSystemRegistry::GetModulePath( const std::string & crstrSubSystemIdentifier )
{
	std::string modulePath;
	getAppConfig()->getString("subsystems", crstrSubSystemIdentifier, modulePath, IConfigParams::PARAM_REQUIRED);
	return modulePath;
}
