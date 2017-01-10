#!/bin/bash

function prtHeader() {
   local header=$1

   echo "*************************"
   echo "***"
   echo "*** $header"
   echo "***"
   echo "*************************"
}

function prtHelp() {
   echo "*** $0 cmd <args>"
   echo "  Runs various CAF commands"
   echo "    cmd: The CAF command to run:"
   echo "      * listServices                      Lists the CAF Services"
   echo "      * startServices                     Starts the Services"
   echo "      * stopServices                      Stops the Services"
   echo "      * killServices                      Kills the Services"
   echo ""
   echo "      * startListener                     Starts the Listener Service"
   echo "      * startListenerForeground           Starts the Listener in the foreground"
   echo "      * startListenerValgrindMemChecks    Starts the Listener with Valgrind Mem Checks"
   echo "      * stopListener                      Stops the Listener Service"
   echo "      * killListener                      Kills the Listener Service"
   echo ""
   echo "      * startMa                           Starts the Management Agent Service"
   echo "      * startMaForeground                 Starts the Management Agent in the foreground"
   echo "      * startMaValgrindMemChecks          Starts the Management Agent with Valgrind Mem Checks"
   echo "      * stopMa                            Stops the Management Agent Service"
   echo "      * killMa                            Kills the Management Agent Service"
}

function startProcess() {
   local process="$1"
   local enableConsoleLogging="$2"
   local cmd="$3"

   case "$process" in
      "listener")
         if [ -f "$scriptsDir/start-listener" ]; then
            prtHeader "Starting Listener - $cmd"
            if [ "$enableConsoleLogging" = "true" ]; then
               enableConsoleLogging "CommAmqpListener"
            else
               disableConsoleLogging "CommAmqpListener"
            fi
            $scriptsDir/start-listener "$cmd"
         fi
      ;;
      "ma")
         if [ -f "$scriptsDir/start-ma" ]; then
            prtHeader "Starting Management Agent - $cmd"
            if [ "$enableConsoleLogging" = "true" ]; then
               enableConsoleLogging "ma"
            else
               disableConsoleLogging "ma"
            fi
            $scriptsDir/start-ma "$cmd"
         fi
      ;;
      *)
         echo "Unknown process - $process"
         prtHelp
         exit 1
   esac

}

function stopListener() {
   if [ -f "$scriptsDir/stop-listener" ]; then
      $scriptsDir/stop-listener
   fi
}

function stopMa() {
   if [ -f "$scriptsDir/stop-ma" ]; then
      $scriptsDir/stop-ma
   fi
}

function killListener() {
   pid=$(ps -eo pid,cmd | egrep "CommAmqpListener" | egrep -v "egrep" | awk '{print $1}')
   if [ "$pid" != "" ]; then
      echo "Killing Listener - $pid"
      kill -9 $pid
   fi
}

function killMa() {
   pid=$(ps -eo pid,cmd | egrep "ManagementAgentHost" | egrep -v "egrep" | awk '{print $1}')
   if [ "$pid" != "" ]; then
      echo "Killing Management Agent - $pid"
      kill -9 $pid
   fi
}

function enableConsoleLogging() {
   component="$1"

   sed -i 's/^#log4j.rootCategory=DEBUG, console/log4j.rootCategory=DEBUG, console/g' "$CAF_CONFIG_DIR/${component}-log4cpp_config"
   sed -i 's/^log4j.rootCategory=DEBUG, logfile/#log4j.rootCategory=DEBUG, logfile/g' "$CAF_CONFIG_DIR/${component}-log4cpp_config"
}

function disableConsoleLogging() {
   component="$1"

   sed -i 's/^log4j.rootCategory=DEBUG, console/#log4j.rootCategory=DEBUG, console/g' "$CAF_CONFIG_DIR/${component}-log4cpp_config"
   sed -i 's/^#log4j.rootCategory=DEBUG, logfile/log4j.rootCategory=DEBUG, logfile/g' "$CAF_CONFIG_DIR/${component}-log4cpp_config"
}

if [ $# -lt 1 -o "$1" = "--help" ]; then
   prtHelp
   exit 1
fi

cmd=$1
shift
cmd_params=$@

scriptsDir=$(dirname $(readlink -f $0))
configDir=$scriptsDir/../config

. $scriptsDir/caf-common
sourceCafenv "$configDir"

case "$cmd" in
   "listServices")
      prtHeader "Listing services"
      ps -ef | egrep "CommAmqpListener|ManagementAgentHost|VGAuthService" | egrep -v "egrep"
   ;;
   "startListener")
      startProcess "listener" "false" "daemon"
   ;;
   "startMa")
      startProcess "ma" "false" "daemon"
   ;;
   "startServices")
      startProcess "listener" "false" "daemon"
      startProcess "ma" "false" "daemon"
   ;;
   "startListenerForeground")
      startProcess "listener" "true" "foreground"
   ;;
   "startMaForeground")
      startProcess "ma" "true" "foreground"
   ;;
   "startListenerValgrindMemChecks")
      startProcess "listener" "true" "valgrindMemChecks"
   ;;
   "startMaValgrindMemChecks")
      startProcess "ma" "true" "valgrindMemChecks"
   ;;
   "startListenerValgrindProfiling")
      startProcess "listener" "true" "valgrindProfiling"
   ;;
   "startMaValgrindProfiling")
      startProcess "ma" "true" "valgrindProfiling"
   ;;
   "startListenerValgrindThreading")
      startProcess "listener" "true" "valgrindThreading"
   ;;
   "startMaValgrindThreading")
      startProcess "ma" "true" "valgrindThreading"
   ;;
   "stopListener")
      stopListener
   ;;
   "stopMa")
      stopMa
   ;;
   "stopServices")
      stopListener
      stopMa
   ;;
   "killListener")
      killListener
   ;;
   "killMa")
      killMa
   ;;
   "killServices")
      killListener
      killMa
   ;;
   *)
      echo "Bad command - $cmd"
      prtHelp
      exit 1
esac
