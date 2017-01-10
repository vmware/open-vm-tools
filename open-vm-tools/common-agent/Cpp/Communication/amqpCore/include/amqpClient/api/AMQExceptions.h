/*
 *  Created on: May 3, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPCLIENTAPI_AMQEXCEPTIONS_H_
#define AMQPCLIENTAPI_AMQEXCEPTIONS_H_

#include "Exception/CCafException.h"
#include "amqpClient/AmqpClientLink.h"

#define AMQP_CM_DECLARE_EXCEPTION_CLASS(_exclass_) \
class AMQPCLIENT_LINKAGE _exclass_ : public Caf::CCafException { \
public: \
    _exclass_(); \
    virtual ~_exclass_(); \
    void throwSelf(); \
    void throwAddRefedSelf(); \
private: \
    _exclass_(const _exclass_ &); \
    _exclass_ & operator=(const _exclass_ &); \
}; \
typedef TCafSmartPtr<_exclass_, TCafObject<_exclass_> > SmartPtr##_exclass_

#define AMQP_CM_DEFINE_EXCEPTION_CLASS(_exclass_) \
    _exclass_::_exclass_() : CCafException( #_exclass_ ) {} \
    _exclass_::~_exclass_() {} \
    void _exclass_::throwSelf() { throw this; } \
    void _exclass_::throwAddRefedSelf() { this->AddRef(); throw this; }

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @brief Exceptions defined by this library
 */
namespace AmqpExceptions {

/** @brief Unmapped AMQP exception */
//AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpException);
class AMQPCLIENT_LINKAGE AmqpException : public Caf::CCafException {
public:
    AmqpException();
    virtual ~AmqpException();
    void throwSelf();
    void throwAddRefedSelf();
private:
    AmqpException(const AmqpException &);
    AmqpException & operator=(const AmqpException &);
};
typedef TCafSmartPtr<AmqpException, TCafObject<AmqpException> > SmartPtrAmqpException;

/** @brief AMQP_ERROR_TIMEOUT exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpTimeoutException);

/** @brief AMQP_ERROR_NO_MEMORY exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpNoMemoryException);

/** @brief AMQP_ERROR_INVALID_HANDLE exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpInvalidHandleException);

/** @brief AMQP_ERROR_INVALID_ARGUMENT exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpInvalidArgumentException);

/** @brief AMQP_ERROR_WRONG_STATE exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpWrongStateException);

/** @brief AMQP_ERROR_TOO_MANY_CHANNELS exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpTooManyChannelsException);

/** @brief AMQP_ERROR_QUEUE_FULL exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpQueueFullException);

/** @brief AMQP_ERROR_FRAME_TOO_LARGE exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpFrameTooLargeException);

/** @brief AMQP_ERROR_IO_ERROR exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpIoErrorException);

/** @brief AMQP_ERROR_PROTOCOL_ERROR exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpProtocolErrorException);

/** @brief AMQP_ERROR_UNIMPLEMENTED exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpUnimplementedException);

/** @brief AMQP_ERROR_IO_INTERRUPTED exception */
AMQP_CM_DECLARE_EXCEPTION_CLASS(AmqpIoInterruptedException);

// Processing exceptions

/** @brief Unexpected frame */
AMQP_CM_DECLARE_EXCEPTION_CLASS(UnexpectedFrameException);

/** @brief Unknown class or method */
AMQP_CM_DECLARE_EXCEPTION_CLASS(UnknownClassOrMethodException);

/** @brief Connection is closed */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ConnectionClosedException);

/** @brief Channel is closed */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ChannelClosedException);

/** @brief Connection closed because of an error */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ConnectionUnexpectedCloseException);

/** @brief Connection closed because of an I/O error */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ConnectionClosedByIOException);

/** @brief Channel closed by server because of an error */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ChannelClosedByServerException);

/** @brief Channel closed by the application because it is shutting down */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ChannelClosedByShutdownException);

/** @brief Channel closed by the application user under normal circumstances */
AMQP_CM_DECLARE_EXCEPTION_CLASS(ChannelClosedByUserException);

}}}

#endif
