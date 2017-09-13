#!/bin/sh

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts

#Shutdown any configured services
$scriptsDir/stop-ma
$scriptsDir/stop-listener
$installDir/preupgrade.sh
