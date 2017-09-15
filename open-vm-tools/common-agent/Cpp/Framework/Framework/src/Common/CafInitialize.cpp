/*
 *	 Author: mdonahue
 *  Created: Jan 24, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#ifdef WIN32
#include "Common/CWinScm.h"
#endif

#include "CafInitialize.h"

using namespace Caf;

HRESULT CafInitialize::init() {
	return S_OK;
}

HRESULT CafInitialize::serviceConfig() {
#ifdef WIN32
//	CWinScm toolsScm("VMTools");
//	SmartPtrSServiceConfig toolsConfig = toolsScm.getServiceConfig(false);
//	if (!toolsConfig.isNull()) {
//		// Add to path
//	}

	CWinScm vgAuthScm("VGAuthService");
	Caf::CWinScm::SmartPtrSServiceConfig vgAuthConfig = vgAuthScm.getServiceConfig(false);
	if (!vgAuthConfig.IsNull() && !vgAuthConfig->_binaryPathName.empty()) {
		// Add to path
		std::string dllPath = vgAuthConfig->_binaryPathName;
		if (dllPath[0] == '\"') {
			dllPath = dllPath.substr(1, dllPath.length() - 2);
		}

		const std::wstring dllPathWide = CStringUtils::convertNarrowToWide(FileSystemUtils::getDirname(dllPath));
		// NOTE:  Can't use ::AddDllDirectory because it requires a newer base version of windows.  Even so,
		// MSDN documentation specifies search order to be non-deterministic.
		if (0 == ::SetDllDirectory(dllPathWide.c_str())) {
			const DWORD lastError = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(lastError);
			::fprintf(
					stderr,
					"CafInitialize::serviceConfig() ::AddDllDirectory() Failed - : VGAuthService, msg: \"%s\"",
					errorMsg.c_str());
		}
	}
#endif
	return S_OK;
}

HRESULT CafInitialize::term() {
	return S_OK;
}
