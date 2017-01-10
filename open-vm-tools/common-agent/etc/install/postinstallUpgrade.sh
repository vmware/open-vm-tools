#!/bin/sh

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts

$installDir/upgrade.sh
$scriptsDir/start-ma
