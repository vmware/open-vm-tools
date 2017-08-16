#!/bin/bash

function prtHeader() {
   local header=$1

   echo "*************************"
   echo "***"
   echo "*** $header"
   echo "***"
   echo "*************************"
}

function validateNotEmpty() {
   local value=$1
   local name=$2

   if [ "$value" = "" ]; then
      echo "Value cannot be empty - $name"
      exit 1
   fi
}

function configAmqp() {
   local username="$1"
   local password="$2"
   local brokerAddr="$3"
   validateNotEmpty "$username" "username"
   validateNotEmpty "$password" "password"
   validateNotEmpty "$brokerAddr" "brokerAddr"

   local uriAmqpFile="$CAF_INPUT_DIR/persistence/protocol/amqpBroker_default/uri_amqp.txt"
   sed -i -e "s/#amqpUsername#/${username}/g" -e "s/#amqpPassword#/${password}/g" -e "s/#brokerAddr#/$brokerAddr/g" "$uriAmqpFile"
}

function enableCaf() {
   local username="$1"
   local password="$2"
   validateNotEmpty "$username" "username"
   validateNotEmpty "$password" "password"

   local uriAmqpFile="$CAF_INPUT_DIR/persistence/protocol/amqpBroker_default/uri_amqp.txt"
   sed -i -e "s/#amqpUsername#/${username}/g" -e "s/#amqpPassword#/${password}/g" "$uriAmqpFile"
}

function setBroker() {
   local brokerAddr="$1"
   validateNotEmpty "$brokerAddr" "brokerAddr"

   local uriAmqpFile="$CAF_INPUT_DIR/persistence/protocol/amqpBroker_default/uri_amqp.txt"
   sed -i "s/#brokerAddr#/$brokerAddr/g" "$uriAmqpFile"
}

function setListenerConfigured() {
   mkdir -p "$CAF_INPUT_DIR/monitor"
   echo "Manual" > "$CAF_INPUT_DIR/monitor/listenerConfiguredStage1.txt"
}

function setListenerStartupType() {
   local startupType="$1"
   validateNotEmpty "$startupType" "startupType"

   local appconfigFile="$CAF_CONFIG_DIR/ma-appconfig"
   sed -i "s/listener_startup_type=.*/listener_startup_type=$startupType/g" "$appconfigFile"
}

function prtHelp() {
   echo "*** $(basename $0) cmd - Runs commands that help with debugging CAF"
   echo "  * configAmqp brokerUsername brokerPassword brokerAddress  Configures AMQP"
   echo "  * enableCaf brokerUsername brokerPassword                 Enables CAF"
   echo "  * setBroker brokerAddress                                 Sets the Broker into the CAF config file"
   echo "  * setListenerConfigured                                   Indicates that the Listener is configured"
   echo "  * setListenerStartupType startupType                      Sets the startup type used by the Listener (Manual, Automatic)"
   echo ""
   echo "  * getAmqpQueueName                                        Gets the AMQP Queue Name"
   echo ""
   echo "  * checkTunnel                                             Checks the AMQP Tunnel "
   echo "  * checkCerts                                              Checks the certificates"
   echo "  * prtCerts                                                Prints the certificates"
   echo "  * checkVmwTools                                           Checks VMware Tools"
   echo ""
   echo "  * validateXml                                             Validates the XML files against the published schema"
   echo "  * validateInstall                                         Validates that the files are in the right locations and have the right permissions"
   echo "  * validateOVTInstall                                      Validates that the files are in the right locations for OVT and have the right permissions"
   echo ""
   echo "  * clearCaches                                             Clears the CAF caches"
}

function validateXml() {
   local schemaArea="$1"
   local schemaPrefix="$2"
   validateNotEmpty "$schemaArea" "schemaArea"
   validateNotEmpty "$schemaPrefix" "schemaPrefix"

   local schemaRoot="$CAF_INPUT_DIR/schemas/caf"

   for file in $(find "$CAF_OUTPUT_DIR" -name '*.xml' -print0 2>/dev/null | xargs -0 egrep -IH -lw "${schemaPrefix}.xsd"); do
      prtHeader "Validating $schemaArea/$schemaPrefix - $file"
      xmllint --schema "${schemaRoot}/${schemaArea}/${schemaPrefix}.xsd" "$file"; rc=$?
      if [ "$rc" != "0" ]; then
         exit $rc
      fi
   done
}

function checkCerts() {
   local localDir="$CAF_INPUT_DIR/persistence/local"
   local cacertFile="$CAF_INPUT_DIR/persistence/protocol/amqpBroker_default/tlsCertCollection/tlsCert0.pem"

   prtHeader "Checking private key - $localDir/privateKey.pem"
   openssl rsa -in "$localDir/privateKey.pem" -check -noout

   prtHeader "Checking cert - $localDir/cert.pem"
   openssl verify -check_ss_sig -x509_strict -CAfile "$cacertFile" "$localDir/cert.pem"

   prtHeader "Validating that private key and cert match - $localDir/cert.pem"
   local clientCertMd5=$(openssl x509 -noout -modulus -in "$localDir/cert.pem" | openssl md5 | cut -d' ' -f2)
   local clientKeyMd5=$(openssl rsa -noout -modulus -in "$localDir/privateKey.pem" | openssl md5 | cut -d' ' -f2)
   if [ "$clientCertMd5" == "$clientKeyMd5" ]; then
      echo "Public and Private Key md5's match"
   else
      echo "*** Public and Private Key md5's do not match"
      exit 1
   fi
}

function prtCerts() {
   local localDir="$CAF_INPUT_DIR/persistence/local"
   local cacertFile="$CAF_INPUT_DIR/persistence/protocol/amqpBroker_default/tlsCertCollection/tlsCert0.pem"

   prtHeader "Printing - $cacertFile"
   openssl x509 -in "$cacertFile" -text -noout

   prtHeader "Printing - $localDir/cert.pem"
   openssl x509 -in "$localDir/cert.pem" -text -noout
}

function checkTunnel() {
   local localDir="$CAF_INPUT_DIR/persistence/local"
   local cacertFile="$CAF_INPUT_DIR/persistence/protocol/amqpBroker_default/tlsCertCollection/tlsCert0.pem"

   prtHeader "Connecting to tunnel"
   openssl s_client -connect localhost:6672 -key "$localDir/privateKey.pem" -cert "$localDir/cert.pem" -CAfile "$cacertFile" -verify 10
}

function checkVmwTools() {
   local isToolboxCmd=$(which vmware-toolbox-cmd 2>/dev/null)
   if [ "$isToolboxCmd" != "" ]; then
      vmware-toolbox-cmd --version
   else
      echo "It doesn't appear as though VMware Tools is installed"
   fi

   local isSystemctl=$(which systemctl 2>/dev/null)
   if [ "$isSystemctl" != "" ]; then
      systemctl status vmware-tools.service
   fi
}

function validateInstall() {
   checkFsPermsAll
   checkFileExistsBin
   checkFileExistsLib
   checkFileExistsConfig
   checkFileExistsScripts
   checkFileExistsInstall
   checkFileExistsInvokers
   checkFileExistsProviderReg
}

function checkFsPermsAll() {
   checkFsPerms "$CAF_INPUT_DIR" "755"
   checkFsPerms "$CAF_OUTPUT_DIR" "755"
   checkFsPerms "$CAF_LOG_DIR" "755"
   checkFsPerms "$CAF_BIN_DIR" "755"
   checkFsPerms "$CAF_LIB_DIR" "755"
}

function checkFileExistsBin() {
   checkFileExists "$CAF_BIN_DIR/CommAmqpListener"
   checkFileExists "$CAF_BIN_DIR/ConfigProvider"
   checkFileExists "$CAF_BIN_DIR/InstallProvider"
   checkFileExists "$CAF_BIN_DIR/ManagementAgentHost"
   checkFileExists "$CAF_BIN_DIR/RemoteCommandProvider"
   checkFileExists "$CAF_BIN_DIR/TestInfraProvider"
   checkFileExists "$CAF_BIN_DIR/VGAuthService"
   checkFileExists "$CAF_BIN_DIR/vmware-vgauth-cmd"
}

function checkFileExistsLib() {
   checkFileExists "$CAF_LIB_DIR/libCafIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libCommAmqpIntegration.so"
   checkFileExists "$CAF_LIB_DIR/libCommAmqpIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libCommIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libFramework.so"
   checkFileExists "$CAF_LIB_DIR/libIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libMaIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libProviderFx.so"
   checkFileExists "$CAF_LIB_DIR/libVgAuthIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libcom_err.so.3"
   checkFileExists "$CAF_LIB_DIR/libcrypto.so.1.0.2"
   checkFileExists "$CAF_LIB_DIR/libgcc_s.so.1"
   checkFileExists "$CAF_LIB_DIR/libglib-2.0.so.0.4800.1"
   checkFileExists "$CAF_LIB_DIR/libgthread-2.0.so.0.4800.1"
   checkFileExists "$CAF_LIB_DIR/liblog4cpp.so.5.0.6"
   checkFileExists "$CAF_LIB_DIR/librabbitmq.so.4.2.1"
   checkFileExists "$CAF_LIB_DIR/libssl.so.1.0.2"
   checkFileExists "$CAF_LIB_DIR/libstdc++.so.6.0.13"
   checkFileExists "$CAF_LIB_DIR/libvgauth.so"
   checkFileExists "$CAF_LIB_DIR/libxerces-c-3.1.so"
   checkFileExists "$CAF_LIB_DIR/libxml-security-c.so.16"
   checkFileExists "$CAF_LIB_DIR/libpcre.so.1"
   checkFileExists "$CAF_LIB_DIR/libiconv.so.2"
   checkFileExists "$CAF_LIB_DIR/libz.so.1.2.8"
   checkFileExists "$CAF_LIB_DIR/libffi.so.6.0.4"
}

function checkFileExistsConfig() {
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-appconfig"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-context-amqp.xml"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-context-common.xml"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-context-tunnel.xml"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-log4cpp_config"
   checkFileExists "$CAF_CONFIG_DIR/IntBeanConfigFile.xml"
   checkFileExists "$CAF_CONFIG_DIR/cafenv-appconfig"
   checkFileExists "$CAF_CONFIG_DIR/ma-appconfig"
   checkFileExists "$CAF_CONFIG_DIR/ma-context.xml"
   checkFileExists "$CAF_CONFIG_DIR/ma-log4cpp_config"
   checkFileExists "$CAF_CONFIG_DIR/providerFx-appconfig"
   checkFileExists "$CAF_CONFIG_DIR/providerFx-log4cpp_config"
}

function checkFileExistsScripts() {
   checkFileExists "$CAF_CONFIG_DIR/../scripts/caf-common"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/caf-processes.sh"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/setUpVgAuth"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/start-VGAuthService"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/start-listener"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/start-ma"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/startTestProc"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/stop-VGAuthService"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/stop-listener"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/stop-ma"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/tearDownVgAuth"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/vgAuth"
}
function checkFileExistsInstall() {
   checkFileExists "$CAF_CONFIG_DIR/../install/caf-c-communication-service"
   checkFileExists "$CAF_CONFIG_DIR/../install/caf-c-management-agent"
   checkFileExists "$CAF_CONFIG_DIR/../install/caf-dbg.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/caf-vgauth"
   checkFileExists "$CAF_CONFIG_DIR/../install/commonenv.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/install.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/postinstallInstall.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/postinstallUpgrade.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/preinstallUpgrade.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/preremoveUninstall.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/preuninstall.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/preupgrade.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/restartServices.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/stopAndRemoveServices.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/upgrade.sh"
}

function checkFileExistsInvokers() {
   checkFileExists "$CAF_INVOKERS_DIR/cafTestInfra_CafTestInfraProvider_1_0_0.sh"
   checkFileExists "$CAF_INVOKERS_DIR/caf_ConfigProvider_1_0_0.sh"
   checkFileExists "$CAF_INVOKERS_DIR/caf_InstallProvider_1_0_0.sh"
   checkFileExists "$CAF_INVOKERS_DIR/caf_RemoteCommandProvider_1_0_0.sh"
}

function checkFileExistsProviderReg() {
   checkFileExists "$CAF_INPUT_DIR/providerReg/cafTestInfra_CafTestInfraProvider_1_0_0.xml"
   checkFileExists "$CAF_INPUT_DIR/providerReg/caf_ConfigProvider_1_0_0.xml"
   checkFileExists "$CAF_INPUT_DIR/providerReg/caf_InstallProvider_1_0_0.xml"
   checkFileExists "$CAF_INPUT_DIR/providerReg/caf_RemoteCommandProvider_1_0_0.xml"
}

function checkFileExists() {
   local path="$1"
   validateNotEmpty "$path" "path"

   if [ ! -f "$path" ]; then
      echo "*** File existence check failed - expected: $path"
      exit 1
   fi
}

function checkFsPerms() {
   local dirOrFile="$1"
   local permExp="$2"
   local userExp="$3"
   local groupExp="$4"
   validateNotEmpty "$dirOrFile" "dirOrFile"
   validateNotEmpty "$permExp" "permExp"

   if [ "$userExp" = "" ]; then
      userExp="root"
   fi
   if [ "$groupExp" = "" ]; then
      groupExp="root"
   fi

   local statInfo=( $(stat -c "%a %U %G" $dirOrFile) )
   local permFnd=${statInfo[0]}
   local userFnd=${statInfo[1]}
   local groupFnd=${statInfo[2]}

   if [ "$permExp" != "$permFnd" ]; then
      echo "*** Perm check failed - expected: $permExp, found: $permFnd, dir/file: $dirOrFile"
      exit 1
   fi

   if [ "$userExp" != "$userFnd" ]; then
      echo "*** User check failed - expected: $userExp, found: $userFnd, dir/file: $dirOrFile"
      exit 1
   fi

   if [ "$groupExp" != "$groupFnd" ]; then
      echo "*** Group check failed - expected: $groupExp, found: $groupFnd, dir/file: $dirOrFile"
      exit 1
   fi
}

function clearCaches() {
   validateNotEmpty "$CAF_OUTPUT_DIR" "CAF_OUTPUT_DIR"
   validateNotEmpty "$CAF_LOG_DIR" "CAF_LOG_DIR"

   prtHeader "Clearing the CAF caches"
   rm -rf \
      $CAF_OUTPUT_DIR/schemaCache/* \
      $CAF_OUTPUT_DIR/comm-wrk/* \
      $CAF_OUTPUT_DIR/providerHost/* \
      $CAF_OUTPUT_DIR/responses/* \
      $CAF_OUTPUT_DIR/requests/* \
      $CAF_OUTPUT_DIR/split-requests/* \
      $CAF_OUTPUT_DIR/request_state/* \
      $CAF_OUTPUT_DIR/events/* \
      $CAF_OUTPUT_DIR/tmp/* \
      $CAF_OUTPUT_DIR/att/* \
      $CAF_OUTPUT_DIR/cache/* \
      $CAF_LOG_DIR/* \
      $CAF_BIN_DIR/*.log
}

function validateInstall() {
#   checkFsPermsAll
   checkFileExistsBin
   checkFileExistsLib "$1"
   checkFileExistsConfig
   checkFileExistsScripts
   checkFileExistsInstall
   checkFileExistsInvokers
   checkFileExistsProviderReg
}

function checkFsPermsAll() {
   checkFsPerms "$CAF_INPUT_DIR" "755"
   checkFsPerms "$CAF_OUTPUT_DIR" "755"
   checkFsPerms "$CAF_CONFIG_DIR" "755"
   checkFsPerms "$CAF_LOG_DIR" "755"
   checkFsPerms "$CAF_BIN_DIR" "755"
   checkFsPerms "$CAF_LIB_DIR" "755"
}

function checkFileExistsBin() {
   checkFileExists "$CAF_BIN_DIR/InstallProvider"
   checkFileExists "$CAF_BIN_DIR/TestInfraProvider"
   checkFileExists "$CAF_BIN_DIR/RemoteCommandProvider"
   checkFileExists "$CAF_BIN_DIR/ConfigProvider"
   checkFileExists "$CAF_BIN_DIR/CommAmqpListener"
   checkFileExists "$CAF_BIN_DIR/ManagementAgentHost"
}

function checkFileExistsLib() {
   checkFileExists "$CAF_LIB_DIR/libProviderFx.so"
   checkFileExists "$CAF_LIB_DIR/libMaIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libCommAmqpIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libCommAmqpIntegration.so"
   checkFileExists "$CAF_LIB_DIR/libFramework.so"
   checkFileExists "$CAF_LIB_DIR/libCafIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libVgAuthIntegrationSubsys.so"
   checkFileExists "$CAF_LIB_DIR/libCommIntegrationSubsys.so"

   local OVT=$1
   if [ "$OVT" != "true" ]; then
      checkFileExists "$CAF_LIB_DIR/libCommAmqpListener.so"
      checkFileExists "$CAF_LIB_DIR/libManagementAgentHost.so"
      checkFileExists "$CAF_LIB_DIR/liblog4cpp.so.5.0.6"
      checkFileExists "$CAF_LIB_DIR/librabbitmq.so.4.2.1"
      checkFileExists "$CAF_LIB_DIR/libgthread-2.0.so.0.4800.1"
      checkFileExists "$CAF_LIB_DIR/libglib-2.0.so.0.4800.1"
   fi
}

function checkFileExistsConfig() {
   checkFileExists "$CAF_CONFIG_DIR/ma-log4cpp_config"
   checkFileExists "$CAF_CONFIG_DIR/ma-context.xml"
   checkFileExists "$CAF_CONFIG_DIR/IntBeanConfigFile.xml"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-context-amqp.xml"
   checkFileExists "$CAF_CONFIG_DIR/ma-appconfig"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-context-tunnel.xml"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-context-common.xml"
   checkFileExists "$CAF_CONFIG_DIR/providerFx-appconfig"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-log4cpp_config"
   checkFileExists "$CAF_CONFIG_DIR/CommAmqpListener-appconfig"
   checkFileExists "$CAF_CONFIG_DIR/providerFx-log4cpp_config"
   checkFileExists "$CAF_CONFIG_DIR/cafenv-appconfig"
}

function checkFileExistsScripts() {
   checkFileExists "$CAF_CONFIG_DIR/../scripts/startTestProc"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/caf-processes.sh"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/stop-ma"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/vgAuth"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/start-listener"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/stop-VGAuthService"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/start-ma"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/setUpVgAuth"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/start-VGAuthService"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/tearDownVgAuth"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/stop-listener"
   checkFileExists "$CAF_CONFIG_DIR/../scripts/caf-common"
}

function checkFileExistsInstall() {
   checkFileExists "$CAF_CONFIG_DIR/../install/caf-dbg.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/stopAndRemoveServices.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/preuninstall.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/postinstallUpgrade.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/install.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/preremoveUninstall.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/preupgrade.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/restartServices.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/caf-vgauth"
   checkFileExists "$CAF_CONFIG_DIR/../install/preinstallUpgrade.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/caf-c-management-agent"
   checkFileExists "$CAF_CONFIG_DIR/../install/upgrade.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/commonenv.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/postinstallInstall.sh"
   checkFileExists "$CAF_CONFIG_DIR/../install/caf-c-communication-service"
}

function checkFileExistsInvokers() {
   checkFileExists "$CAF_INVOKERS_DIR/caf_RemoteCommandProvider_1_0_0.sh"
   checkFileExists "$CAF_INVOKERS_DIR/caf_InstallProvider_1_0_0.sh"
   checkFileExists "$CAF_INVOKERS_DIR/caf_ConfigProvider_1_0_0.sh"
   checkFileExists "$CAF_INVOKERS_DIR/cafTestInfra_CafTestInfraProvider_1_0_0.sh"
}

function checkFileExistsProviderReg() {
   checkFileExists "$CAF_INPUT_DIR/providerReg/caf_RemoteCommandProvider_1_0_0.xml"
   checkFileExists "$CAF_INPUT_DIR/providerReg/cafTestInfra_CafTestInfraProvider_1_0_0.xml"
   checkFileExists "$CAF_INPUT_DIR/providerReg/caf_ConfigProvider_1_0_0.xml"
   checkFileExists "$CAF_INPUT_DIR/providerReg/caf_InstallProvider_1_0_0.xml"
}

function checkFileExists() {
   local path="$1"
   validateNotEmpty "$path" "path"

   if [ ! -f "$path" ]; then
      echo "*** File existence check failed - expected: $path"
      exit 1
   fi
}

function getAmqpQueueName() {
   grep "^reactive_request_amqp_queue_id" "$CAF_CONFIG_DIR/persistence-appconfig" | cut -d'=' -f2
}

if [ $# -lt 1 -o "$1" = "--help" ]; then
   prtHelp
   exit 1
fi

cmd=$1
shift

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts
configDir=$installDir/../config

. $scriptsDir/caf-common
sourceCafenv "$configDir"

case "$cmd" in
   "validateXml")
      validateXml "fx" "CafInstallRequest"
      validateXml "fx" "DiagRequest"
      validateXml "fx" "Message"
      validateXml "fx" "MgmtRequest"
      validateXml "fx" "MultiPmeMgmtRequest"
      validateXml "fx" "ProviderInfra"
      validateXml "fx" "ProviderRequest"
      validateXml "fx" "Response"
      validateXml "cmdl" "ProviderResults"
   ;;
   "checkCerts")
      checkCerts "$certDir"
   ;;
   "prtCerts")
      prtCerts "$certDir"
   ;;
   "checkTunnel")
      checkTunnel "$certDir"
   ;;
   "checkVmwTools")
      checkVmwTools
   ;;
   "getAmqpQueueName")
      getAmqpQueueName
   ;;
   "clearCaches")
      clearCaches
   ;;
   "configAmqp")
      configAmqp "$1" "$2" "$3"
   ;;
   "enableCaf")
      enableCaf "$1" "$2"
   ;;
   "setBroker")
      setBroker "$1"
   ;;
   "setListenerConfigured")
      setListenerConfigured
   ;;
   "setListenerStartupType")
      setListenerStartupType "$1"
   ;;
   "validateInstall")
      validateInstall
   ;;
   "validateOVTInstall")
      validateInstall "true"
   ;;
   *)
      echo "Bad command - $cmd"
      prtHelp
      exit 1
esac
