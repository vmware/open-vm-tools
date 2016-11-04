#!/bin/sh

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts

$scriptsDir/stop-ma
$scriptsDir/stop-listener
