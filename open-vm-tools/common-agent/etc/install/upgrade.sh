#!/bin/sh

#Standard env
SCRIPT=`basename "$0"`

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts
configDir=$installDir/../config
toolsLibDir='/usr/lib/vmware-tools/lib' # lib is symlink to either lib64 or lib32


#Help function
HELP() {
	echo -e \\n"Help documentation for ${SCRIPT}."\\n
	echo -e "Basic usage: $SCRIPT"\\n
	echo "Command line switches are optional. The following switches are recognized."
	echo "t  --Sets the location for the tools lib dir. Default is '$toolsLibDir'."
	echo -e "h  --Displays this help message. No further functions are performed."\\n
	echo -e "Example: $SCRIPT -t \"/usr/lib/vmware-tools/lib\""\\n
	exit 1
}


##BEGIN Main

#Get Optional overrides
while getopts ":t:h" opt; do
	case $opt in
		t)
			toolsLibDir="$OPTARG"
			;;
		h)
			HELP
			;;
		\?)
			echo "Invalid option: -$OPTARG" >&2
			HELP
			;;
	esac
done


prevCafenvConfig="$configDir/_previous_/cafenv.config"
if [ -f "$prevCafenvConfig" ]; then
   echo "Upgrading from a really old version of CAF - $prevCafenvConfig"
   . $prevCafenvConfig

   inputDir=$(echo "$CAF_INPUT_DIR" | sed 's:/vmware-caf/pme/data/input::')
   outputDir=$(echo "$CAF_OUTPUT_DIR" | sed 's:/vmware-caf/pme/data/output::')
   libDir=$(echo "$CAF_LIB_DIR" | sed 's:/vmware-caf/pme/lib::')
   binDir=$(echo "$CAF_BIN_DIR" | sed 's:/vmware-caf/pme/bin::')
   if [ -n "$CAF_TOOLS_LIB_DIR" ]; then
     toolsLibDir="$CAF_TOOLS_LIB_DIR"
   fi
   $installDir/install.sh -L -b "$CAF_BROKER_ADDRESS" -i "$inputDir" \
                          -o "$outputDir" -l "$libDir" -B "$binDir" -t "$toolsLibDir"

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
ln -sf libglib-2.0.so.0.4800.1 libglib-2.0.so
ln -sf libglib-2.0.so.0.4800.1 libglib-2.0.so.0
ln -sf libgthread-2.0.so.0.4800.1 libgthread-2.0.so
ln -sf libgthread-2.0.so.0.4800.1 libgthread-2.0.so.0
ln -sf liblog4cpp.so.5.0.6 liblog4cpp.so
ln -sf liblog4cpp.so.5.0.6 liblog4cpp.so.5
ln -sf librabbitmq.so.4.2.1 librabbitmq.so
ln -sf librabbitmq.so.4.2.1 librabbitmq.so.4
ln -sf libpcre.so.1 libpcre.so
ln -sf libiconv.so.2 libiconv.so
ln -sf libz.so.1.2.8 libz.so
ln -sf libz.so.1.2.8 libz.so.1
ln -sf libffi.so.6.0.4 libffi.so
ln -sf libffi.so.6.0.4 libffi.so.6
