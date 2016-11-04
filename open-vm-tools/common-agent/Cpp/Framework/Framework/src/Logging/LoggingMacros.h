/*
 *  Author: bwilliams
 *  Created: 1/14/2011
 *
 *  Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef LoggingMacros_h_
#define LoggingMacros_h_

#define CAF_CM_CREATE_LOG \
	private: \
	CLogger _logger

#define CAF_CM_INIT_LOG( _className_ ) \
	_cm_className_( _className_ ), \
	_logger(_className_)

#define CAF_CM_STATIC_FUNC_LOG_VALIDATE( _scope_, _funcName_ ) \
	CLogger _logger(_scope_); \
	CAF_CM_STATIC_FUNC_VALIDATE( _scope_, _funcName_ )

#define CAF_CM_STATIC_FUNC_LOG_ONLY( _scope_, _funcName_ ) \
	CLogger _logger(_scope_); \
	CAF_CM_FUNCNAME_VALIDATE( _funcName_ )

#define CAF_CM_STATIC_FUNC_LOG( _scope_, _funcName_ ) \
	CLogger _logger(_scope_); \
	CAF_CM_STATIC_FUNC( _scope_, _funcName_ )

#define CAF_CM_LOG_GET_PRIORITY (_logger.getPriority())
#define CAF_CM_LOG_SET_PRIORITY(_priorityLevel_) (_logger.setPriority(_priorityLevel_))
#define CAF_CM_IS_LOG_ENABLED(_priorityLevel_) (_logger.isPriorityEnabled(_priorityLevel_))
#define CAF_CM_IS_LOG_DEBUG_ENABLED (_logger.isPriorityEnabled(log4cpp::Priority::DEBUG))
#define CAF_CM_IS_LOG_INFO_ENABLED (_logger.isPriorityEnabled(log4cpp::Priority::INFO))
#define CAF_CM_IS_LOG_WARN_ENABLED (_logger.isPriorityEnabled(log4cpp::Priority::WARN))
#define CAF_CM_IS_LOG_ERROR_ENABLED (_logger.isPriorityEnabled(log4cpp::Priority::ERROR))
#define CAF_CM_IS_LOG_CRIT_ENABLED (_logger.isPriorityEnabled(log4cpp::Priority::CRIT))

/*
 * Provide priority level
 */
#define CAF_CM_LOG_CAFEXCEPTION(_priorityLevel_) { \
		_logger.log(_priorityLevel_, _cm_funcName_,  __LINE__, _cm_exception_); \
	}

#define CAF_CM_LOG_VA0(_priorityLevel_, _msg_) { \
		_logger.logMessage(_priorityLevel_, _cm_funcName_,  __LINE__, _msg_); \
	}

#define CAF_CM_LOG_VA1(_priorityLevel_, _fmt_, _arg1_) { \
		_logger.logVA(_priorityLevel_, _cm_funcName_,  __LINE__, _fmt_, _arg1_); \
	}

#define CAF_CM_LOG_VA2(_priorityLevel_, _fmt_, _arg1_, _arg2_) { \
		_logger.logVA(_priorityLevel_, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_); \
	}

#define CAF_CM_LOG_VA3(_priorityLevel_, _fmt_, _arg1_, _arg2_, _arg3_) { \
		_logger.logVA(_priorityLevel_, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_); \
	}

#define CAF_CM_LOG_VA4(_priorityLevel_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_) { \
		_logger.logVA(_priorityLevel_, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_); \
	}

#define CAF_CM_LOG_VA5(_priorityLevel_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_) { \
		_logger.logVA(_priorityLevel_, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_); \
	}

#define CAF_CM_LOG_VA6(_priorityLevel_, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_) { \
		_logger.logVA(_priorityLevel_, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_); \
	}

/*
 * Debug Macros
 */
#define CAF_CM_LOG_DEBUG_CAFEXCEPTION { \
		_logger.log(log4cpp::Priority::DEBUG, _cm_funcName_,  __LINE__, _cm_exception_); \
	}

#define CAF_CM_LOG_DEBUG_VA0(_msg_) { \
		_logger.logMessage(log4cpp::Priority::DEBUG, _cm_funcName_,  __LINE__, _msg_); \
	}

#define CAF_CM_LOG_DEBUG_VA1(_fmt_, _arg1_) { \
		_logger.logVA(log4cpp::Priority::DEBUG, _cm_funcName_,  __LINE__, _fmt_, _arg1_); \
	}

#define CAF_CM_LOG_DEBUG_VA2(_fmt_, _arg1_, _arg2_) { \
		_logger.logVA(log4cpp::Priority::DEBUG, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_); \
	}

#define CAF_CM_LOG_DEBUG_VA3(_fmt_, _arg1_, _arg2_, _arg3_) { \
		_logger.logVA(log4cpp::Priority::DEBUG, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_); \
	}

#define CAF_CM_LOG_DEBUG_VA4(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_) { \
		_logger.logVA(log4cpp::Priority::DEBUG, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_); \
	}

#define CAF_CM_LOG_DEBUG_VA5(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_) { \
		_logger.logVA(log4cpp::Priority::DEBUG, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_); \
	}

#define CAF_CM_LOG_DEBUG_VA6(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_) { \
		_logger.logVA(log4cpp::Priority::DEBUG, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_); \
	}

/*
 * Info Macros
 */
#define CAF_CM_LOG_INFO_CAFEXCEPTION { \
		_logger.log(log4cpp::Priority::INFO, _cm_funcName_,  __LINE__, _cm_exception_); \
	}

#define CAF_CM_LOG_INFO_VA0(_msg_) { \
		_logger.logMessage(log4cpp::Priority::INFO, _cm_funcName_,  __LINE__, _msg_); \
	}

#define CAF_CM_LOG_INFO_VA1(_fmt_, _arg1_) { \
		_logger.logVA(log4cpp::Priority::INFO, _cm_funcName_,  __LINE__, _fmt_, _arg1_); \
	}

#define CAF_CM_LOG_INFO_VA2(_fmt_, _arg1_, _arg2_) { \
		_logger.logVA(log4cpp::Priority::INFO, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_); \
	}

#define CAF_CM_LOG_INFO_VA3(_fmt_, _arg1_, _arg2_, _arg3_) { \
		_logger.logVA(log4cpp::Priority::INFO, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_); \
	}

#define CAF_CM_LOG_INFO_VA4(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_) { \
		_logger.logVA(log4cpp::Priority::INFO, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_); \
	}

#define CAF_CM_LOG_INFO_VA5(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_) { \
		_logger.logVA(log4cpp::Priority::INFO, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_); \
	}

#define CAF_CM_LOG_INFO_VA6(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_) { \
		_logger.logVA(log4cpp::Priority::INFO, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_); \
	}

/*
 * Warn Macros
 */
#define CAF_CM_LOG_WARN_CAFEXCEPTION { \
		_logger.log(log4cpp::Priority::WARN, _cm_funcName_,  __LINE__, _cm_exception_); \
	}

#define CAF_CM_LOG_WARN_VA0(_msg_) { \
		_logger.logMessage(log4cpp::Priority::WARN, _cm_funcName_,  __LINE__, _msg_); \
	}

#define CAF_CM_LOG_WARN_VA1(_fmt_, _arg1_) { \
		_logger.logVA(log4cpp::Priority::WARN, _cm_funcName_,  __LINE__, _fmt_, _arg1_); \
	}

#define CAF_CM_LOG_WARN_VA2(_fmt_, _arg1_, _arg2_) { \
		_logger.logVA(log4cpp::Priority::WARN, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_); \
	}

#define CAF_CM_LOG_WARN_VA3(_fmt_, _arg1_, _arg2_, _arg3_) { \
		_logger.logVA(log4cpp::Priority::WARN, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_); \
	}

#define CAF_CM_LOG_WARN_VA4(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_) { \
		_logger.logVA(log4cpp::Priority::WARN, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_); \
	}

#define CAF_CM_LOG_WARN_VA5(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_) { \
		_logger.logVA(log4cpp::Priority::WARN, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_); \
	}

#define CAF_CM_LOG_WARN_VA6(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_) { \
		_logger.logVA(log4cpp::Priority::WARN, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_); \
	}

/*
 * Error Macros
 */
#define CAF_CM_LOG_ERROR_CAFEXCEPTION { \
		_logger.log(log4cpp::Priority::ERROR, _cm_funcName_,  __LINE__, _cm_exception_); \
	}

#define CAF_CM_LOG_ERROR_VA0(_msg_) { \
		_logger.logMessage(log4cpp::Priority::ERROR, _cm_funcName_,  __LINE__, _msg_); \
	}

#define CAF_CM_LOG_ERROR_VA1(_fmt_, _arg1_) { \
		_logger.logVA(log4cpp::Priority::ERROR, _cm_funcName_,  __LINE__, _fmt_, _arg1_); \
	}

#define CAF_CM_LOG_ERROR_VA2(_fmt_, _arg1_, _arg2_) { \
		_logger.logVA(log4cpp::Priority::ERROR, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_); \
	}

#define CAF_CM_LOG_ERROR_VA3(_fmt_, _arg1_, _arg2_, _arg3_) { \
		_logger.logVA(log4cpp::Priority::ERROR, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_); \
	}

#define CAF_CM_LOG_ERROR_VA4(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_) { \
		_logger.logVA(log4cpp::Priority::ERROR, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_); \
	}

#define CAF_CM_LOG_ERROR_VA5(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_) { \
		_logger.logVA(log4cpp::Priority::ERROR, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_); \
	}

#define CAF_CM_LOG_ERROR_VA6(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_) { \
		_logger.logVA(log4cpp::Priority::ERROR, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_); \
	}

/*
 * Crit Macros
 */
#define CAF_CM_LOG_CRIT_CAFEXCEPTION { \
		_logger.log(log4cpp::Priority::CRIT, _cm_funcName_,  __LINE__, _cm_exception_); \
	}

#define CAF_CM_LOG_CRIT_VA0(_msg_) { \
		_logger.logMessage(log4cpp::Priority::CRIT, _cm_funcName_,  __LINE__, _msg_); \
	}

#define CAF_CM_LOG_CRIT_VA1(_fmt_, _arg1_) { \
		_logger.logVA(log4cpp::Priority::CRIT, _cm_funcName_,  __LINE__, _fmt_, _arg1_); \
	}

#define CAF_CM_LOG_CRIT_VA2(_fmt_, _arg1_, _arg2_) { \
		_logger.logVA(log4cpp::Priority::CRIT, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_); \
	}

#define CAF_CM_LOG_CRIT_VA3(_fmt_, _arg1_, _arg2_, _arg3_) { \
		_logger.logVA(log4cpp::Priority::CRIT, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_); \
	}

#define CAF_CM_LOG_CRIT_VA4(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_) { \
		_logger.logVA(log4cpp::Priority::CRIT, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_); \
	}

#define CAF_CM_LOG_CRIT_VA5(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_) { \
		_logger.logVA(log4cpp::Priority::CRIT, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_); \
	}

#define CAF_CM_LOG_CRIT_VA6(_fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_) { \
		_logger.logVA(log4cpp::Priority::CRIT, _cm_funcName_,  __LINE__, _fmt_, _arg1_, _arg2_, _arg3_, _arg4_, _arg5_, _arg6_); \
	}

#endif // #define LoggingMacros_h_
