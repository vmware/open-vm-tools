#!/bin/sh

# Copyright (C) 2017 VMware, Inc.  All rights reserved. -- VMware Confidential

installDir=$(dirname $(readlink -f $0))
configDir=$installDir/../config

#Preserve config
mkdir -p "$configDir"/_previous_
cp -pf "$configDir"/cafenv-appconfig "$configDir"/_previous_/ 2>/dev/null
cp -pf "$configDir"/cafenv.config "$configDir"/_previous_/cafenv.config 2>/dev/null
cat /tmp/_cafenv-appconfig_ >> "$configDir"/_previous_/cafenv.config

#Temporary until we remove amqp_username/password
cp -pf "$configDir"/CommAmqpListener-appconfig "$configDir"/_previous_/ 2>/dev/null
