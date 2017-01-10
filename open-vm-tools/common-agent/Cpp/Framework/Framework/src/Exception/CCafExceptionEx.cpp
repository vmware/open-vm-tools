/*
 *  Created on: Oct 6, 2011
 *      Author: mdonahue
 *
 *  Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CCafExceptionEx.h"

using namespace Caf;

// General Runtime Exceptions
CAF_CM_DEFINE_EXCEPTION_CLASS(AccessDeniedException);
CAF_CM_DEFINE_EXCEPTION_CLASS(NullPointerException);
CAF_CM_DEFINE_EXCEPTION_CLASS(BufferOverflowException);
CAF_CM_DEFINE_EXCEPTION_CLASS(BufferUnderflowException);
CAF_CM_DEFINE_EXCEPTION_CLASS(InvalidArgumentException);
CAF_CM_DEFINE_EXCEPTION_CLASS(IllegalStateException);
CAF_CM_DEFINE_EXCEPTION_CLASS(IndexOutOfBoundsException);
CAF_CM_DEFINE_EXCEPTION_CLASS(NoSuchElementException);
CAF_CM_DEFINE_EXCEPTION_CLASS(DuplicateElementException);
CAF_CM_DEFINE_EXCEPTION_CLASS(UnsupportedOperationException);
CAF_CM_DEFINE_EXCEPTION_CLASS(UnsupportedVersionException);
CAF_CM_DEFINE_EXCEPTION_CLASS(InvalidHandleException);
CAF_CM_DEFINE_EXCEPTION_CLASS(TimeoutException);
CAF_CM_DEFINE_EXCEPTION_CLASS(NoSuchInterfaceException);
CAF_CM_DEFINE_EXCEPTION_CLASS(ProcessFailedException);

// AppConfig Exceptions
CAF_CM_DEFINE_EXCEPTION_CLASS(NoSuchConfigSectionException);
CAF_CM_DEFINE_EXCEPTION_CLASS(NoSuchConfigValueException);

// Dynamic Library Exception
CAF_CM_DEFINE_EXCEPTION_CLASS(LibraryFailedToLoadException);
CAF_CM_DEFINE_EXCEPTION_CLASS(NoSuchMethodException);

// I/O Exceptions
CAF_CM_DEFINE_EXCEPTION_CLASS(IOException);
CAF_CM_DEFINE_EXCEPTION_CLASS(EOFException);
CAF_CM_DEFINE_EXCEPTION_CLASS(FileNotFoundException);
CAF_CM_DEFINE_EXCEPTION_CLASS(PathNotFoundException);
CAF_CM_DEFINE_EXCEPTION_CLASS(FileLockedException);

