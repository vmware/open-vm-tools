/*
 *  Created: Oct 09, 2002
 *
 *	Copyright (c) 2002-2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */
#ifndef _CEcmSubSystemRegistry_H_
#define _CEcmSubSystemRegistry_H_

#include <map>

namespace Caf {

class SUBSYSTEMBASE_LINKAGE CEcmSubSystemRegistry
{
public:
	static bool IsRegistered( const std::string & crstrSubSystemIdentifier );

	static std::string GetModulePath( const std::string & crstrSubSystemIdentifier );
};
}

#endif // _CEcmSubSystem_H_
