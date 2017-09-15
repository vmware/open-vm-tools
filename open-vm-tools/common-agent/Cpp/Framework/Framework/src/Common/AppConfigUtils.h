/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AppConfigUtils_H_
#define AppConfigUtils_H_

namespace Caf {

namespace AppConfigUtils {

	std::string COMMONAGGREGATOR_LINKAGE getRequiredString(const std::string& parameterName);
	uint32 COMMONAGGREGATOR_LINKAGE getRequiredUint32(const std::string& parameterName);
	int32 COMMONAGGREGATOR_LINKAGE getRequiredInt32(const std::string& parameterName);
	bool COMMONAGGREGATOR_LINKAGE getRequiredBoolean(const std::string& parameterName);

	std::string COMMONAGGREGATOR_LINKAGE getOptionalString(const std::string& parameterName);
	uint32 COMMONAGGREGATOR_LINKAGE getOptionalUint32(const std::string& parameterName);
	int32 COMMONAGGREGATOR_LINKAGE getOptionalInt32(const std::string& parameterName);
	bool COMMONAGGREGATOR_LINKAGE getOptionalBoolean(const std::string& parameterName);

	std::string COMMONAGGREGATOR_LINKAGE getRequiredString(
		const std::string& sectionName,
		const std::string& parameterName);
	uint32 COMMONAGGREGATOR_LINKAGE getRequiredUint32(
		const std::string& sectionName,
		const std::string& parameterName);
	int32 COMMONAGGREGATOR_LINKAGE getRequiredInt32(
		const std::string& sectionName,
		const std::string& parameterName);
	bool COMMONAGGREGATOR_LINKAGE getRequiredBoolean(
		const std::string& sectionName,
		const std::string& parameterName);

	std::string COMMONAGGREGATOR_LINKAGE getOptionalString(
		const std::string& sectionName,
		const std::string& parameterName);
	uint32 COMMONAGGREGATOR_LINKAGE getOptionalUint32(
		const std::string& sectionName,
		const std::string& parameterName);
	int32 COMMONAGGREGATOR_LINKAGE getOptionalInt32(
		const std::string& sectionName,
		const std::string& parameterName);
	bool COMMONAGGREGATOR_LINKAGE getOptionalBoolean(
		const std::string& sectionName,
		const std::string& parameterName);
};

}

#endif /* AppConfigUtils_H_ */
