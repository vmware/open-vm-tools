///////////////////////////////////////////////////////////////////////////////////////////
//
//  Author:		Michael Donahue
//
//  Created:	05/03/2004
//
//	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
//	-- VMware Confidential
// 
///////////////////////////////////////////////////////////////////////////////////////////

#ifndef DynamicArrayInc_h_
#define DynamicArrayInc_h_

#include "SDynamicArrayMallocAllocator.h"

namespace Caf {

template<typename T, typename Allocator = SDynamicArrayMallocAllocator>
class TDynamicArray;

template<typename T>
class TCharAdapter;

template<typename T>
class TWCharAdapter;

template<typename T>
class TByteAdapter;

typedef TDynamicArray<byte>										CDynamicByteArray;
typedef TCharAdapter<TByteAdapter<TDynamicArray<char> > >		CDynamicCharArray;
typedef TWCharAdapter<TByteAdapter<TDynamicArray<wchar_t> > >	CDynamicWCharArray;

}

#include "TDynamicArray.h"
#include "TByteAdapter.h"
#include "TCharAdapter.h"
#include "TWCharAdapter.h"

namespace Caf {

CAF_DECLARE_SMART_POINTER(CDynamicByteArray);
CAF_DECLARE_SMART_POINTER(CDynamicCharArray);
CAF_DECLARE_SMART_POINTER(CDynamicWCharArray);

}

#endif // #ifdef DynamicArrayInc_h_
