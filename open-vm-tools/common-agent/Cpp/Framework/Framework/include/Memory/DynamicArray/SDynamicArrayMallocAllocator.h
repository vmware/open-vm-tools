/*
 *	 Author: mdonahue
 *  Created: Feb 15, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef SDYNAMICARRAYMALLOCALLOCATOR_H_
#define SDYNAMICARRAYMALLOCALLOCATOR_H_

namespace Caf {

struct SDynamicArrayMallocAllocator {
	static void* allocMemory( const uint32 cdwAllocSize )
	{
		return ::malloc( cdwAllocSize );
	}

	static void freeMemory( void* pvFree )
	{
		if ( NULL != pvFree )
		{
			::free( pvFree );
		}
	}
};
}

#endif /* SDYNAMICARRAYMALLOCALLOCATOR_H_ */
