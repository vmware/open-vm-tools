#!/bin/sh

#Get info on how the installation was configured
installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts
configDir=$installDir/../config

. $scriptsDir/caf-common
sourceCafenv "$configDir"

#Set a safety check string
VALIDATE_STRING='vmware-caf'

safe_rm() {
    #Only remove directory paths that contain the validate string
    if test "${1#*$VALIDATE_STRING}" != "$1"; then
        rm -rf "$1"
    fi
}

#The default of this should be /usr/lib/vmware-caf
#base_binary_dir=$(dirname $(dirname $CAF_BIN_DIR))
#safe_rm "$base_binary_dir"

#The default of this should be /var/lib/vmware-caf
#base_data_dir=$(dirname $(dirname $(dirname $CAF_INPUT_DIR)))
#safe_rm "$base_data_dir"

#The default of this should be /var/log/vmware-caf
base_log_dir=$(dirname $CAF_LOG_DIR)
safe_rm "$base_log_dir"

#07/21/2015
#Remove some log files that get put into the CAF bin dir.
#This is a hack until we fix the code to prevent this from happening.
base_binary_dir="$CAF_BIN_DIR"
safe_rm "$base_binary_dir/CommAmqpListener-log4cpp.log"
safe_rm "$base_binary_dir/CommAmqpListener-log4cpp_rolling.log"
safe_rm "$base_binary_dir/ma-log4cpp.log"
safe_rm "$base_binary_dir/ma-log4cpp_rolling.log"
