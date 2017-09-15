#!/bin/sh

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts
configDir=$installDir/../config

prevCafenvConfig="$configDir/_previous_/cafenv.config"
if [ -f "$prevCafenvConfig" ]; then
   echo "Upgrading from a really old version of CAF - $prevCafenvConfig"
   . $prevCafenvConfig

   inputDir=$(echo "$CAF_INPUT_DIR" | sed 's:/vmware-caf/pme/data/input::')
   outputDir=$(echo "$CAF_OUTPUT_DIR" | sed 's:/vmware-caf/pme/data/output::')
   libDir=$(echo "$CAF_LIB_DIR" | sed 's:/vmware-caf/pme/lib::')
   binDir=$(echo "$CAF_BIN_DIR" | sed 's:/vmware-caf/pme/bin::')
   $installDir/install.sh -L -b "$CAF_BROKER_ADDRESS" -i "$inputDir" -o "$outputDir" -l "$libDir" -B "$binDir"

   rm -f "$prevCafenvConfig"
else
   prevCafenvAppconfig="$configDir/_previous_/cafenv-appconfig"
   if [ ! -f "$prevCafenvAppconfig" ]; then
      echo "The backup file must exist! - $prevCafenvAppconfig"
      exit 1
   fi
   mv -f "$prevCafenvAppconfig" "$configDir"
fi

#Remove the backup directory
rm -rf "$configDir"/_previous_

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
ln -sf librabbitmq.so.4.2.1 librabbitmq.so
ln -sf librabbitmq.so.4.2.1 librabbitmq.so.4
