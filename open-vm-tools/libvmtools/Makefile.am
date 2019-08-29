################################################################################
### Copyright (C) 2008-2019 VMware, Inc.  All rights reserved.
###
### This program is free software; you can redistribute it and/or modify
### it under the terms of version 2 of the GNU General Public License as
### published by the Free Software Foundation.
###
### This program is distributed in the hope that it will be useful,
### but WITHOUT ANY WARRANTY; without even the implied warranty of
### MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
### GNU General Public License for more details.
###
### You should have received a copy of the GNU General Public License
### along with this program; if not, write to the Free Software
### Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
################################################################################

lib_LTLIBRARIES = libvmtools.la

libvmtools_la_LIBADD =
libvmtools_la_LIBADD += ../lib/lock/libLock.la
libvmtools_la_LIBADD += ../lib/backdoor/libBackdoor.la
if HAVE_VSOCK
libvmtools_la_LIBADD += ../lib/asyncsocket/libAsyncSocket.la
endif
libvmtools_la_LIBADD += ../lib/sslDirect/libSslDirect.la
libvmtools_la_LIBADD += ../lib/pollGtk/libPollGtk.la
libvmtools_la_LIBADD += ../lib/poll/libPoll.la
libvmtools_la_LIBADD += ../lib/dataMap/libDataMap.la
libvmtools_la_LIBADD += ../lib/hashMap/libHashMap.la
libvmtools_la_LIBADD += ../lib/dict/libDict.la
libvmtools_la_LIBADD += ../lib/dynxdr/libDynxdr.la
libvmtools_la_LIBADD += ../lib/err/libErr.la
libvmtools_la_LIBADD += ../lib/file/libFile.la
libvmtools_la_LIBADD += ../lib/glibUtils/libGlibUtils.la
libvmtools_la_LIBADD += ../lib/guestApp/libGuestApp.la
libvmtools_la_LIBADD += ../lib/guestRpc/libGuestRpc.la
libvmtools_la_LIBADD += ../lib/message/libMessage.la
libvmtools_la_LIBADD += ../lib/netUtil/libNetUtil.la
libvmtools_la_LIBADD += ../lib/nicInfo/libNicInfo.la
libvmtools_la_LIBADD += ../lib/panic/libPanic.la
libvmtools_la_LIBADD += ../lib/procMgr/libProcMgr.la
libvmtools_la_LIBADD += ../lib/rpcChannel/libRpcChannel.la
libvmtools_la_LIBADD += ../lib/rpcIn/libRpcIn.la
libvmtools_la_LIBADD += ../lib/rpcOut/libRpcOut.la
libvmtools_la_LIBADD += ../lib/rpcVmx/libRpcVmx.la
libvmtools_la_LIBADD += ../lib/string/libString.la
libvmtools_la_LIBADD += ../lib/syncDriver/libSyncDriver.la
libvmtools_la_LIBADD += ../lib/system/libSystem.la
libvmtools_la_LIBADD += ../lib/stubs/libStubsCS.la
libvmtools_la_LIBADD += ../lib/unicode/libUnicode.la
libvmtools_la_LIBADD += ../lib/user/libUser.la
libvmtools_la_LIBADD += ../lib/vmCheck/libVmCheck.la
libvmtools_la_LIBADD += ../lib/vmSignal/libVmSignal.la
libvmtools_la_LIBADD += ../lib/wiper/libWiper.la
libvmtools_la_LIBADD += ../lib/misc/libMisc.la
libvmtools_la_LIBADD += @LIBVMTOOLS_LIBADD@
libvmtools_la_LIBADD += @GLIB2_LIBS@
libvmtools_la_LIBADD += @ICU_LIBS@

if USE_SLASH_PROC
libvmtools_la_LIBADD += ../lib/slashProc/libSlashProc.la
endif

libvmtools_la_SOURCES =
libvmtools_la_SOURCES += i18n.c
libvmtools_la_SOURCES += monotonicTimer.c
libvmtools_la_SOURCES += signalSource.c
libvmtools_la_SOURCES += vmtools.c
libvmtools_la_SOURCES += vmtoolsConfig.c
libvmtools_la_SOURCES += vmtoolsLog.c
libvmtools_la_SOURCES += vmxLogger.c
libvmtools_la_SOURCES += guestSDKLog.c

# Recompile the stub for Log_* functions, but not Log() itself (see -DNO_LOG_STUB).
libvmtools_la_SOURCES += $(top_srcdir)/lib/stubs/stub-log.c

libvmtools_la_CPPFLAGS =
libvmtools_la_CPPFLAGS += -DVMTOOLS_USE_GLIB
libvmtools_la_CPPFLAGS += -DNO_LOG_STUB
libvmtools_la_CPPFLAGS += -DVMTOOLS_DATA_DIR=\"$(datadir)/open-vm-tools\"
libvmtools_la_CPPFLAGS += @GLIB2_CPPFLAGS@

libvmtools_la_LDFLAGS =
# We require GCC, so we're fine passing compiler-specific flags.
libvmtools_la_LDFLAGS += -Wl,-z,defs
# Needed for OS's that don't link shared libraries against libc by
#default, e.g. FreeBSD
libvmtools_la_LDFLAGS += -Wl,-lc

