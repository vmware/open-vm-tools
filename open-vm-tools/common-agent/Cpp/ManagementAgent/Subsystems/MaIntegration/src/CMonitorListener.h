/*
 *   Copyright (C) 2017-2018 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _MaIntegration_CMonitorListener_h_
#define _MaIntegration_CMonitorListener_h_

using namespace Caf;

#define LISTENER_STARTUP_TYPE_AUTOMATIC      "Automatic"
#define LISTENER_STARTUP_TYPE_MANUAL         "Manual"

/*
 * Manages the listener LCM
 */
class CMonitorListener {

public:
   CMonitorListener();
   ~CMonitorListener();

   void initialize();

   bool preConfigureListener();

   bool isListenerPreConfigured();

   bool followTunnel(std::string& listenerStartupType);

   void stopListener(const std::string& reason);

   bool isListenerRunning();

   bool canListenerBeStarted();

   void startListener(const std::string& reason);

   void restartListener(const std::string& reason);

   void listenerConfiguredStage1(const std::string& reason) const;

   void listenerUnConfiguredStage1();

   void listenerConfiguredStage2(const std::string& reason) const;

   void listenerUnConfiguredStage2();

   void listenerPreConfigured(const std::string& reason) const;



private:
   bool _isInitialized;
   bool _listenerCtrlPreConfigure;
   bool _listenerCtrlFollowTunnel;
   bool _listenerPreConfigured;

   std::string _startListenerScript;
   std::string _restartListenerPath;
        std::string _listenerConfiguredStage1Path;
        std::string _listenerConfiguredStage2Path;
        std::string _listenerPreConfiguredPath;
   std::string _stopListenerScript;
   std::string _isListenerRunningScript;
        std::string _preConfigureListenerScript;
   std::string _monitorDir;
   std::string _scriptOutputDir;

private:
   CAF_CM_CREATE;
   CAF_CM_CREATE_LOG;
   CAF_CM_CREATE_THREADSAFE;
   CAF_CM_DECLARE_NOCOPY(CMonitorListener);
};
CAF_DECLARE_SMART_POINTER(CMonitorListener);

#endif // #ifndef _MaIntegration_CMonitorListener_h_
