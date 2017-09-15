/*
 *	Author: bwilliams
 *  Created: Nov 17, 2014
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "CTimeUnit.h"

using namespace Caf;

int32 CTimeUnit::MILLISECONDS::toDays(const int32 milliseconds) {
	return milliseconds / (1000*60*60*24);
}

int32 CTimeUnit::MILLISECONDS::toHours(const int32 milliseconds) {
	return milliseconds / (1000*60*60);
}

int32 CTimeUnit::MILLISECONDS::toMinutes(const int32 milliseconds) {
	return milliseconds / (1000*60);
}

int32 CTimeUnit::MILLISECONDS::toSeconds(const int32 milliseconds) {
	return milliseconds / 1000;
}

int32 CTimeUnit::SECONDS::toDays(const int32 seconds) {
	return seconds / (60*60*24);
}

int32 CTimeUnit::SECONDS::toHours(const int32 seconds) {
	return seconds / (60*60);
}

int32 CTimeUnit::SECONDS::toMinutes(const int32 seconds) {
	return seconds / 60;
}

int32 CTimeUnit::SECONDS::toMilliseconds(const int32 seconds) {
	return seconds * 1000;
}

int32 CTimeUnit::MINUTES::toDays(const int32 seconds) {
	return seconds / (60*24);
}

int32 CTimeUnit::MINUTES::toHours(const int32 minutes) {
	return minutes / 60;
}

int32 CTimeUnit::MINUTES::toSeconds(const int32 minutes) {
	return minutes * 60;
}

int32 CTimeUnit::MINUTES::toMilliseconds(const int32 minutes) {
	return minutes * 1000 * 60;
}

int32 CTimeUnit::HOURS::toDays(const int32 hours) {
	return hours / 24;
}

int32 CTimeUnit::HOURS::toMinutes(const int32 hours) {
	return hours * 60;
}

int32 CTimeUnit::HOURS::toSeconds(const int32 hours) {
	return hours * (60*60);
}

int32 CTimeUnit::HOURS::toMilliseconds(const int32 hours) {
	return hours * (60*60*1000);
}

int32 CTimeUnit::DAYS::toHours(const int32 days) {
	return days * 24;
}

int32 CTimeUnit::DAYS::toMinutes(const int32 days) {
	return days * (24*60);
}

int32 CTimeUnit::DAYS::toSeconds(const int32 days) {
	return days * (24*60*60);
}

int32 CTimeUnit::DAYS::toMilliseconds(const int32 days) {
	return days * (24*60*60*1000);
}
