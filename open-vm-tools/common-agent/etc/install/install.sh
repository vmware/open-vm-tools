#!/bin/sh

#Args
#brokerAddr
#	- default:
#baseLibDir
#	- default: /usr/lib
#	- expand to "$baseLibDir"/vmware-caf/pme
#
#baseInputDir
#	- default: /var/lib
#	- expand to "$baseInputDir"/vmware-caf/pme/data/input
#
#baseOutputDir
#	- default: /var/lib
#	- expand to "$baseOutputDir"/vmware-caf/pme/data/output

#Standard env
SCRIPT=`basename "$0"`

#Set defaults
baseLibDir='/usr/lib'
baseBinDir='/usr/lib'
baseInputDir='/var/lib'
baseOutputDir='/var/lib'
brokerAddr='@brokerAddr@'
linkSo='yes'

#Help function
HELP() {
	echo -e \\n"Help documentation for ${SCRIPT}."\\n
	echo -e "Basic usage: $SCRIPT"\\n
	echo "Command line switches are optional. The following switches are recognized."
	echo "b  --Sets the value for the broker address. Default is '$brokerAddr'."
	echo "i  --Sets the base location for the input data. Default is '$baseInputDir'."
	echo "l  --Sets the base location for the libraries. Default is '$baseLibDir'."
	echo "B  --Sets the location for the binaries. Default is '$baseLibDir'/bin or 'bin' in base location of libraries."
	echo "o  --Sets the base location for the output data. Default is '$baseOutputDir'."
	echo "L  --Do not create symlinks for libraries."
	echo -e "h  --Displays this help message. No further functions are performed."\\n
	echo -e "Example: $SCRIPT -b 10.25.91.81 -i \"/usr/lib\" -i \"/var/lib\" -o \"/var/lib\""\\n
	exit 1
}

#Replace tokens with install values
setupCafConfig() {
	local pattern="$1"
	local value="$2"
	local rconfigDir="$3"

	if [ ! -n "$pattern" ]; then
		echo 'The pattern cannot be empty!'
		exit 1
	fi
	if [ -n "$value" ]; then
		if [ ! -f "$rconfigDir/cafenv-appconfig" ]; then
			echo "The config file must exist! - $rconfigDir/cafenv-appconfig"
			exit 1
		fi

		sed -i "s?$pattern?$value?g" "$rconfigDir/cafenv-appconfig"
	fi
}

##BEGIN Main

#Get Optional overrides
while getopts ":b:i:l:B:o:hL" opt; do
	case $opt in
		b)
			brokerAddr="$OPTARG"
			;;
		i)
			baseInputDir="$OPTARG"
			;;
		l)
			baseLibDir="$OPTARG"
			;;
		B)
			baseBinDir="$OPTARG"
			;;
		o)
			baseOutputDir="$OPTARG"
			;;
		L)
			linkSo='no'
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

stdQuals="vmware-caf/pme"

libDir="${D}/$baseLibDir/$stdQuals/lib"
binDir="${D}/$baseBinDir/$stdQuals/bin"
inputDir="${D}/$baseInputDir/$stdQuals/data/input"
outputDir="${D}/$baseOutputDir/$stdQuals/data/output"

providersDir="$inputDir/providers"
invokersDir="$inputDir/invokers"
persistenceDir="$inputDir/persistence"
protocolDir="$persistenceDir/protocol"
localDir="$persistenceDir/local"
amqpBrokerDir="$protocolDir/amqpBroker_default"
tlsCertCollectionDir="$amqpBrokerDir/tlsCertCollection"

logDir="${D}/var/log/$stdQuals"

baseEtcDir="${D}/etc/$stdQuals"
configDir="$baseEtcDir/config"
installDir="$baseEtcDir/install"
scriptDir="$baseEtcDir/scripts"

brokerUriOpts="vhost=caf;connection_timeout=150000;connection_retries=10;connection_seconds_to_wait=10;channel_cache_size=3"

#Ensure directories exist
mkdir -p "$outputDir"
mkdir -p "$persistenceDir"
mkdir -p "$protocolDir"
mkdir -p "$amqpBrokerDir"
mkdir -p "$tlsCertCollectionDir"
mkdir -p "$localDir"
mkdir -p "$invokersDir"
mkdir -p "$providersDir"
mkdir -p "$logDir"

if [ -f "/etc/vmware-tools/GuestProxyData/server/cert.pem" ]; then
	cp -f "/etc/vmware-tools/GuestProxyData/server/cert.pem" "$tlsCertCollectionDir/cacert.pem"
fi

vcidPath="/etc/vmware-tools/GuestProxyData/VmVcUuid/vm.vc.uuid"
if [ -f "$vcidPath" ]; then
	reactiveRequestAmqpQueueId=$(cat "$vcidPath")-agentId1
else
	reactiveRequestAmqpQueueId=`uuidgen`
fi

tunnelPort=$(netstat -ldn | egrep ":6672 ")
if [ -f "$vcidPath" -a "$tunnelPort" != "" ]; then
	commAmqpListenerContext="$configDir/CommAmqpListener-context-tunnel.xml"
	brokerUri="tunnel:agentId1:bogus@localhost:6672/${reactiveRequestAmqpQueueId}?${brokerUriOpts}"
else
	commAmqpListenerContext="$configDir/CommAmqpListener-context-amqp.xml"
	brokerUri="amqp:@amqpUsername@:@amqpPassword@@${brokerAddr}:5672/${reactiveRequestAmqpQueueId}?${brokerUriOpts}"
fi

echo "[globals]" > "$configDir/persistence-appconfig"
echo "reactive_request_amqp_queue_id=$reactiveRequestAmqpQueueId" >> "$configDir/persistence-appconfig"
echo "comm_amqp_listener_context=$commAmqpListenerContext" >> "$configDir/persistence-appconfig"

echo -n "$brokerUri" > "$amqpBrokerDir/uri.txt"
echo -n "$reactiveRequestAmqpQueueId" > "$localDir/localId.txt"

#Substitute values into config files
setupCafConfig '@installDir@' "$installDir" "$configDir"
setupCafConfig '@libDir@' "$libDir" "$configDir"
setupCafConfig '@binDir@' "$binDir" "$configDir"
setupCafConfig '@configDir@' "$configDir" "$configDir"
setupCafConfig '@inputDir@' "$inputDir" "$configDir"
setupCafConfig '@outputDir@' "$outputDir" "$configDir"
setupCafConfig '@providersDir@' "$providersDir" "$configDir"
setupCafConfig '@invokersDir@' "$invokersDir" "$configDir"
setupCafConfig '@logDir@' "$logDir" "$configDir"

#Set default permissions
if [ -d "$libDir" ]; then
	for directory in $(find "$libDir" -type d); do
		chmod 755 "$directory"
	done

	for file in $(find "$libDir" -type f); do
		chmod 555 "$file"
	done
fi

if [ -d "$inputDir" ]; then
	for file in $(find "$inputDir" -type f); do
		chmod 644 "$file"
	done

	if [ -d "$persistenceDir" ]; then
		for file in $(find "$persistenceDir" -type f); do
			chmod 440 "$file"
		done
	fi

	if [ -d "$invokersDir" ]; then
		for file in $(find "$invokersDir" -type f); do
			chmod 555 "$file"
		done
	fi
fi

if [ -d "$scriptDir" ]; then
	chmod 555 "$scriptDir"/*
fi

#Set up links
if [ "$linkSo" != "no" ] ; then
	cd "$libDir"
	ln -sf libglib-2.0.so.0.3400.3 libglib-2.0.so
	ln -sf libglib-2.0.so.0.3400.3 libglib-2.0.so.0
	ln -sf libgthread-2.0.so.0.3400.3 libgthread-2.0.so
	ln -sf libgthread-2.0.so.0.3400.3 libgthread-2.0.so.0
	ln -sf liblog4cpp.so.5.0.6 liblog4cpp.so
	ln -sf liblog4cpp.so.5.0.6 liblog4cpp.so.5
	ln -sf librabbitmq.so.4.1.4 librabbitmq.so
	ln -sf librabbitmq.so.4.1.4 librabbitmq.so.4
fi

#Run provider install logic
installPProviders="$installDir/installPythonProviders.sh"
if [ -e "$installPProviders" ]; then
	"$installPProviders"
fi

#if previous CAF installation
	#migrate config
	#migrate other state
