#!/bin/sh

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts

$installDir/install.sh -l /usr/lib -i /var/lib -o /var/lib
$scriptsDir/start-ma
