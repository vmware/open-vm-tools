#!/bin/sh

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts
configDir=$installDir/../config

prevCafenvAppconfig="$configDir/_previous_/cafenv-appconfig"
if [ ! -f "$prevCafenvAppconfig" ]; then
	echo "The backup file must exist! - $prevCafenvAppconfig"
	exit 1
fi
mv -f "$prevCafenvAppconfig" "$configDir"

#Temporary until we remove amqp_username/password
prevCommAppconfig="$configDir/_previous_/CommAmqpListener-appconfig"
if [ ! -f "$prevCommAppconfig" ]; then
	echo "The backup file must exist! - $prevCommAppconfig"
	exit 1
fi
mv -f "$prevCommAppconfig" "$configDir"

#Remove the now empty backup directory
rmdir "$configDir"/_previous_

. $scriptsDir/caf-common
sourceCafenv "$configDir"

# Make newer systemd systems (OpenSuSE 13.2) happy
#if [ -x /usr/bin/systemctl ]; then
#    /usr/bin/systemctl daemon-reload
#fi

#"$dir"/restartServices.sh

if [ ! -d $CAF_LIB_DIR ]; then
	echo "CAF_LIB_DIR not found - $CAF_LIB_DIR"
	exit 1
fi

cd $CAF_LIB_DIR
ln -sf libglib-2.0.so.0.3400.3 libglib-2.0.so
ln -sf libglib-2.0.so.0.3400.3 libglib-2.0.so.0
ln -sf libgthread-2.0.so.0.3400.3 libgthread-2.0.so
ln -sf libgthread-2.0.so.0.3400.3 libgthread-2.0.so.0
ln -sf liblog4cpp.so.5.0.6 liblog4cpp.so
ln -sf liblog4cpp.so.5.0.6 liblog4cpp.so.5
ln -sf librabbitmq.so.4.1.4 librabbitmq.so
ln -sf librabbitmq.so.4.1.4 librabbitmq.so.4
