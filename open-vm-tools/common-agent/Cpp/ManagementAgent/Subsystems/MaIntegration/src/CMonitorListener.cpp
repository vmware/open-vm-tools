/*
 *   Copyright (C) 2010-2018 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Exception/CCafException.h"
#include "CMonitorListener.h"

using namespace Caf;

CMonitorListener::CMonitorListener() :
      _isInitialized(false),
      CAF_CM_INIT_LOG("CMonitorListener") {
   CAF_CM_INIT_THREADSAFE;
}

CMonitorListener::~CMonitorListener() {
}


void CMonitorListener::initialize() {
   CAF_CM_LOCK_UNLOCK;

   if (!_isInitialized) {
      _monitorDir = AppConfigUtils::getRequiredString("monitor_dir");
      _restartListenerPath = FileSystemUtils::buildPath(_monitorDir, "restartListener.txt");
      _listenerConfiguredStage1Path = FileSystemUtils::buildPath(
            _monitorDir, "listenerConfiguredStage1.txt");
      _listenerConfiguredStage2Path = FileSystemUtils::buildPath(
            _monitorDir, "listenerConfiguredStage2.txt");
      _listenerPreConfiguredPath = FileSystemUtils::buildPath(
            _monitorDir, "listenerPreConfigured.txt");

      _listenerCtrlPreConfigure = AppConfigUtils::getRequiredUint32("monitor",
            "listener_ctrl_preconfigure") ? true : false;
      _listenerCtrlFollowTunnel = AppConfigUtils::getRequiredUint32("monitor",
            "listener_ctrl_follow_tunnel") ? true : false;
      _listenerPreConfigured = FileSystemUtils::doesFileExist(_listenerPreConfiguredPath) ? true : false;

      _scriptOutputDir = AppConfigUtils::getRequiredString(_sConfigTmpDir);
      const std::string installDir = AppConfigUtils::getRequiredString("install_dir");
      const std::string scriptsDir = AppConfigUtils::getRequiredString("scripts_dir");
#ifdef _WIN32
      _stopListenerScript = FileSystemUtils::buildPath(scriptsDir, "stop-listener.bat");
      _startListenerScript = FileSystemUtils::buildPath(scriptsDir, "start-listener.bat");
      _preConfigureListenerScript = FileSystemUtils::buildPath(installDir, "preconfigure-listener.bat");
      _isListenerRunningScript = FileSystemUtils::buildPath(scriptsDir, "is-listener-running.bat");
#else
      _stopListenerScript = FileSystemUtils::buildPath(scriptsDir, "stop-listener");
      _startListenerScript = FileSystemUtils::buildPath(scriptsDir, "start-listener");
      _preConfigureListenerScript = FileSystemUtils::buildPath(installDir, "preconfigure-listener.sh");
      _isListenerRunningScript = FileSystemUtils::buildPath(scriptsDir, "is-listener-running");
#endif
      _isInitialized = true;
   }
}

bool CMonitorListener::preConfigureListener() {
   CAF_CM_FUNCNAME_VALIDATE("preConfigureListener");

   bool rc = true;
   if (!_listenerCtrlPreConfigure) {
      rc = false;
      CAF_CM_LOG_DEBUG_VA0("monitor/listener_ctrl_preconfigure is not enabled.");
   } else if (!isListenerPreConfigured()) {
      CAF_CM_LOG_DEBUG_VA0("Pre-configuring the listener...");
      const std::string stdoutStr = FileSystemUtils::executeScript(
         _preConfigureListenerScript, _scriptOutputDir);
      if (stdoutStr.compare("true") == 0) {
         CAF_CM_LOG_DEBUG_VA0("Pre-configured the listener.");
         std::string reason = "PreConfiguredByMA";
         listenerConfiguredStage1("Automatic");
         listenerConfiguredStage2(reason);
         listenerPreConfigured(reason);
      } else {
         rc = false;
         CAF_CM_LOG_ERROR_VA1("Failed to pre-configure the listener. errstr: %s", stdoutStr.c_str());
      }
   }

   return rc;
}

/*
 * Returns
 *     true if listener is stopped/started upon tunnel and sets listenerStartupType
 */
bool CMonitorListener::followTunnel(std::string& listenerStartupType) {
   CAF_CM_FUNCNAME_VALIDATE("followTunnel");

   // true - followed the tunnel
   bool rc = false;
   std::string reason;
   if (!_listenerCtrlFollowTunnel) {
      // If Listener is pre-configured and Tunnel enabled, start listener
      if (isListenerPreConfigured()) {
         // 1. Start the listener if tunnel is enabled
         // 2. Stop the listener otherwise
         if (CConfigEnvMerge::isTunnelEnabledFunc()) {
            CAF_CM_LOG_DEBUG_VA1("Listener is pre-configured and tunnel is enabled. "
                  "Starting the listener. PreConfiguredPath=%s", _listenerPreConfiguredPath.c_str());
            listenerConfiguredStage1(LISTENER_STARTUP_TYPE_AUTOMATIC);
            listenerConfiguredStage2(LISTENER_STARTUP_TYPE_AUTOMATIC);
            listenerStartupType = LISTENER_STARTUP_TYPE_AUTOMATIC;
         } else {
            CAF_CM_LOG_DEBUG_VA1("Listener is pre-configured and tunnel is disabled. "
               "PreConfiguredPath=%s", _listenerPreConfiguredPath.c_str());
            if (isListenerRunning()) {
               reason = "Listener pre-configured, tunnel disabled, and listener is running. Stopping it";
               CAF_CM_LOG_DEBUG_VA0(reason.c_str());
               stopListener(reason);
            }
            listenerUnConfiguredStage1();
            listenerUnConfiguredStage2();
         }
         rc = true;
      }
   }
   return rc;
}

bool CMonitorListener::canListenerBeStarted() {
   bool rc = false;

   if (CConfigEnvMerge::isTunnelEnabledFunc()) {
      if (_listenerCtrlFollowTunnel) {
         rc = true;
      }
   } else {
      //TODO: Implement non-tunnel case. Currently it is not a priority
   }

   return rc;
}

bool CMonitorListener::isListenerRunning() {
   const std::string stdoutStr = FileSystemUtils::executeScript(
            _isListenerRunningScript, _scriptOutputDir);
   return (stdoutStr.compare("true") == 0);
}

void CMonitorListener::stopListener(
      const std::string& reason) {
   CAF_CM_FUNCNAME_VALIDATE("stopListener");

   CAF_CM_LOG_DEBUG_VA1(
         "Stopping the listener - reason: %s", reason.c_str());
   FileSystemUtils::executeScript(_stopListenerScript, _scriptOutputDir);
}

void CMonitorListener::startListener(
      const std::string& reason) {
   CAF_CM_FUNCNAME_VALIDATE("startListener");

   if (canListenerBeStarted()) {
      CAF_CM_LOG_DEBUG_VA1("Starting the listener - reason: %s", reason.c_str());
      FileSystemUtils::executeScript(_startListenerScript, _scriptOutputDir);
   } else {
      CAF_CM_LOG_DEBUG_VA0("Listener is not allowed to start. Check setting...");
   }
}

void CMonitorListener::restartListener(
      const std::string& reason) {
   FileSystemUtils::saveTextFile(_restartListenerPath, reason);
}

void CMonitorListener::listenerConfiguredStage1(
      const std::string& reason) const {
   FileSystemUtils::saveTextFile(_listenerConfiguredStage1Path, reason);
}

void CMonitorListener::listenerUnConfiguredStage1() {
   FileSystemUtils::removeFile(_listenerConfiguredStage1Path);
}

void CMonitorListener::listenerConfiguredStage2(
      const std::string& reason) const {
   FileSystemUtils::saveTextFile(_listenerConfiguredStage2Path, reason);
}

void CMonitorListener::listenerUnConfiguredStage2() {
   FileSystemUtils::removeFile(_listenerConfiguredStage2Path);
}

void CMonitorListener::listenerPreConfigured(
      const std::string& reason) const {
   FileSystemUtils::saveTextFile(_listenerPreConfiguredPath, reason);
}

bool CMonitorListener::isListenerPreConfigured() {
   // Invalidate the flag
   if (!_listenerPreConfigured) {
      _listenerPreConfigured = FileSystemUtils::doesFileExist(_listenerPreConfiguredPath);
   }
   return _listenerPreConfigured;
}

