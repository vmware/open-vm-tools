#!/bin/sh

installDir=$(dirname $(readlink -f $0))
configDir=$installDir/../config

#Preserve config
mkdir -p "$configDir"/_previous_
cp -pf "$configDir"/cafenv-appconfig "$configDir"/_previous_/ 2>/dev/null

#Temporary until we remove amqp_username/password
cp -pf "$configDir"/CommAmqpListener-appconfig "$configDir"/_previous_/ 2>/dev/null
