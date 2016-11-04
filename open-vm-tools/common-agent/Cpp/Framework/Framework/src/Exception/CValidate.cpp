/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "CValidate.h"
#include "ValidationMacros.h"

using namespace Caf;

//  Some helper macros that make is easy to validate input arguments.
#define CAF_EXCEPTION_VALIDATE(_valmsg_, _variable_text_ ) \
		CAF_CM_EXCEPTION_VA2(E_INVALIDARG, "%s %s", _valmsg_, _variable_text_)

void CValidate::constructed(const bool value, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (! value ) {
	    	CAF_CM_EXCEPTION_VA0(ERROR_INVALID_STATE, _sPRECOND_ISCONSTRUCTED);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::initialized(const bool value, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (! value ) {
			CAF_CM_EXCEPTION_VA0(OLE_E_BLANK, _sPRECOND_ISINITIALIZED);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notInitialized(const bool value, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
		if (value ) {
			CAF_CM_EXCEPTION_VA0(ERROR_ALREADY_INITIALIZED, _sPRECOND_ISNOTINITIALIZED);
		}
	}
	CAF_CM_EXIT;
}

void CValidate::notEmptyStr(const std::string& value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (value.length() == 0 ) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_STRINGEMPTY, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notEmptyStr(const std::wstring& value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (value.length() == 0 ) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_STRINGEMPTY, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notEmptyUuid(const UUID value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (::IsEqualGUID(value, CAFCOMMON_GUID_NULL) ) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_GUID, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notNullOrEmptyStr(const char* value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (NULL == value) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_STRINGPTRNULL, name);
	    }
	    if (value[ 0 ] == '\0' ) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_STRINGPTREMPTY, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notNullOrEmptyStr(const wchar_t* value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (NULL == value) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_STRINGPTRNULL, name);
	    }
	    if (value[ 0 ] == L'\0') {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_STRINGPTREMPTY, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notNullOrEmptyPtrArr(const void** value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (NULL == value) {
	    	CAF_EXCEPTION_VALIDATE( _sVALIDATE_PTRARRAYNULL, name);
	    }
	    if (NULL == value[ 0 ]) {
	    	CAF_EXCEPTION_VALIDATE( _sVALIDATE_PTRARRAYEMPTY, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notNullInterface(const ICafObject* value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (NULL == value) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_INTERFACE, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notNullPtr(const void* value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (NULL == value) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_NOTNULL, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::nullPtr(const void* value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (NULL != value) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_NULL, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::zero(const int32 value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
		if (value != 0 ) {
			CAF_EXCEPTION_VALIDATE(_sVALIDATE_ZERO, name);
		}
	}
	CAF_CM_EXIT;
}

void CValidate::notZero(const int32 value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
		if (value == 0 ) {
			CAF_EXCEPTION_VALIDATE(_sVALIDATE_ISNOTZERO, name);
		}
	}
	CAF_CM_EXIT;
}

void CValidate::positive(const int32 value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (value <= 0 ) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_POSITIVE, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::negative(const int32 value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (value >= 0 ) {
	        CAF_EXCEPTION_VALIDATE(_sVALIDATE_NEGATIVE, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::nonNegative(const int32 value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (value < 0 ) {
	    	CAF_EXCEPTION_VALIDATE(_sVALIDATE_NONNEGATIVE, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::nonNegative64(const int64 value, const char* name, const char* className, const char* funcName) {
    CAF_CM_STATIC_FUNC(className, funcName);

    CAF_CM_ENTER {
        if (value < 0 ) {
            CAF_EXCEPTION_VALIDATE(_sVALIDATE_NONNEGATIVE, name);
        }
    }
    CAF_CM_EXIT;
}

void CValidate::isTrue(const bool value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (! value ) {
	    	CAF_EXCEPTION_VALIDATE(_sVALIDATE_BOOL, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::notEmptyStl(const size_t value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (value < 1) {
	    	CAF_EXCEPTION_VALIDATE(_sVALIDATE_STL, name);
	    }
	}
	CAF_CM_EXIT;
}

void CValidate::emptyStl(const size_t value, const char* name, const char* className, const char* funcName) {
	CAF_CM_STATIC_FUNC(className, funcName);

	CAF_CM_ENTER {
	    if (value != 0) {
	    	CAF_EXCEPTION_VALIDATE(_sVALIDATE_STL_EMPTY, name);
	    }
	}
	CAF_CM_EXIT;
}
