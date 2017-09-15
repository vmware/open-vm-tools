/*
 *  Created on: Aug 13, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CObjectFactoryTables_h
#define CObjectFactoryTables_h

namespace Caf {

struct CObjectFactoryTables {
	static const ObjectCreatorMap objectCreatorMap;
	static const ObjectCreatorMap::value_type objectCreatorEntries[];

	static const MessageHandlerObjectCreatorMap messageHandlerObjectCreatorMap;
	static const MessageHandlerObjectCreatorMap::value_type messageHandlerObjectCreatorEntries[];
};

}

#endif /* CObjectFactoryTables_h */
