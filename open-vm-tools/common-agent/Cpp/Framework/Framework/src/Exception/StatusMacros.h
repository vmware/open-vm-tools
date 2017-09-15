/*
 *	 Author: mdonahue
 *  Created: Jan 13, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef STATUSMACROS_H_
#define STATUSMACROS_H_

typedef std::pair<HRESULT, std::string> CCafStatus;

#define CAF_CM_VALIDATE_STATUS(_status_) \
		{ \
			CCafStatus _int_status_ = _status_; \
			if (_int_status_.first != S_OK) \
				CAF_CM_EXCEPTION_VA0(_int_status_.first, _int_status_.second.c_str()); \
		}

#define CAF_CM_STATUS _status_

#define CAF_CM_INIT_STATUS CCafStatus CAF_CM_STATUS = std::make_pair(S_OK, "");

#define CAF_CM_SET_STATUS_FROM_EXCEPTION(_arg_) \
		{ \
			if (_arg_->isPopulated()) \
				CAF_CM_STATUS = std::make_pair(_arg_->getError(), _arg_->getMsg()); \
			else \
				CAF_CM_STATUS = std::make_pair(E_FAIL, "Unknown exception"); \
		}

#endif /* STATUSMACROS_H_ */
