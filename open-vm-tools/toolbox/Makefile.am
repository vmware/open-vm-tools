################################################################################
### Copyright 2007 VMware, Inc.  All rights reserved.
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

bin_PROGRAMS = vmware-toolbox

AM_CFLAGS = @GTK_CPPFLAGS@

vmware_toolbox_LDADD =
vmware_toolbox_LDADD += ../lib/conf/libConf.a
vmware_toolbox_LDADD += ../lib/eventManager/libEventManager.a
vmware_toolbox_LDADD += ../lib/file/libFile.a
vmware_toolbox_LDADD += ../lib/guestApp/libGuestApp.a
vmware_toolbox_LDADD += ../lib/hgfs/libHgfs.a
vmware_toolbox_LDADD += ../lib/procMgr/libProcMgr.a
vmware_toolbox_LDADD += ../lib/rpcIn/libRpcIn.a
vmware_toolbox_LDADD += ../lib/rpcOut/libRpcOut.a
vmware_toolbox_LDADD += ../lib/strUtil/libStrUtil.a
vmware_toolbox_LDADD += ../lib/string/libString.a
vmware_toolbox_LDADD += ../lib/system/libSystem.a
vmware_toolbox_LDADD += ../lib/user/libUser.a
vmware_toolbox_LDADD += ../lib/vmCheck/libVmCheck.a
vmware_toolbox_LDADD += ../lib/vmSignal/libVmSignal.a
vmware_toolbox_LDADD += ../lib/wiper/libWiper.a
# In the absence of the linker options --start-group and --end-group (which
# can't be put in LDADD), we need to bring the following libraries out of
# the alphabetical order so their symbols are properly resolved.
vmware_toolbox_LDADD += ../lib/err/libErr.a
vmware_toolbox_LDADD += ../lib/backdoor/libBackdoor.a
vmware_toolbox_LDADD += ../lib/dict/libDict.a
vmware_toolbox_LDADD += ../lib/message/libMessage.a
vmware_toolbox_LDADD += ../lib/unicode/libUnicode.a
vmware_toolbox_LDADD += ../lib/sync/libSync.a
vmware_toolbox_LDADD += ../lib/misc/libMisc.a
vmware_toolbox_LDADD += ../lib/panicDefault/libPanicDefault.a
vmware_toolbox_LDADD += ../lib/panic/libPanic.a
vmware_toolbox_LDADD += -lX11
vmware_toolbox_LDADD += @GTK_LIBS@
vmware_toolbox_LDADD += @ICU_LIBS@

vmware_toolbox_SOURCES =
vmware_toolbox_SOURCES += debugStdio.c
vmware_toolbox_SOURCES += stub.c
vmware_toolbox_SOURCES += toolbox-gtk.c
vmware_toolbox_SOURCES += toolboxAbout.c
vmware_toolbox_SOURCES += toolboxDevices.c
vmware_toolbox_SOURCES += toolboxOptions.c
vmware_toolbox_SOURCES += toolboxScripts.c
vmware_toolbox_SOURCES += toolboxShrink.c

install-exec-hook:
	$(INSTALL) -d $(datadir)/applications/
	$(INSTALL) -m 644 ../scripts/common/vmware-toolbox.desktop \
		$(datadir)/applications/
	$(SED) -i s/Exec=.*/Exec=vmware-toolbox/ \
		$(datadir)/applications/vmware-toolbox.desktop
	$(INSTALL) -d /usr/share/pixmaps/vmware
	$(INSTALL) -m 644 bigIcon.xpm \
		/usr/share/pixmaps/vmware/vmware-toolbox.xpm
uninstall-hook:
	-$(RM) -f $(datadir)/share/applications/vmware-toolbox.desktop
	-$(RM) -rf /usr/share/pixmaps/vmware
