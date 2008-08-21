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

COMMON =
COMMON += ../lib/conf/libConf.a
COMMON += ../lib/eventManager/libEventManager.a
COMMON += ../lib/file/libFile.a
COMMON += ../lib/hgfs/libHgfs.a
COMMON += ../lib/procMgr/libProcMgr.a
COMMON += ../lib/rpcIn/libRpcIn.a
COMMON += ../lib/rpcOut/libRpcOut.a
COMMON += ../lib/string/libString.a
COMMON += ../lib/system/libSystem.a
COMMON += ../lib/user/libUser.a
COMMON += ../lib/vmCheck/libVmCheck.a
COMMON += ../lib/vmSignal/libVmSignal.a
COMMON += ../lib/wiper/libWiper.a
# In the absence of the linker options --start-group and --end-group (which
# can't be put in LDADD), we need to bring the following libraries out of
# the alphabetical order so their symbols are properly resolved.
COMMON += ../lib/err/libErr.a
COMMON += ../lib/backdoor/libBackdoor.a
COMMON += ../lib/dict/libDict.a
COMMON += ../lib/message/libMessage.a
COMMON += ../lib/unicode/libUnicode.a
COMMON += ../lib/sync/libSync.a
COMMON += ../lib/misc/libMisc.a
COMMON += ../lib/panicDefault/libPanicDefault.a
COMMON += ../lib/panic/libPanic.a
COMMON += ../lib/stubs/libStubs.a

bin_PROGRAMS =
if HAVE_X11
   bin_PROGRAMS += vmware-toolbox

   AM_CFLAGS = @GTK_CPPFLAGS@
   vmware_toolbox_LDADD =
   vmware_toolbox_LDADD += ../lib/guestApp/libGuestAppX11.a
   vmware_toolbox_LDADD += $(COMMON)

   vmware_toolbox_LDADD += @COMMON_XLIBS@
   vmware_toolbox_LDADD += @GTK_LIBS@

   vmware_toolbox_SOURCES =
   vmware_toolbox_SOURCES += debugStdio.c
   vmware_toolbox_SOURCES += toolbox-gtk.c
   vmware_toolbox_SOURCES += toolboxAbout.c
   vmware_toolbox_SOURCES += toolboxDevices.c
   vmware_toolbox_SOURCES += toolboxOptions.c
   vmware_toolbox_SOURCES += toolboxScripts.c
   vmware_toolbox_SOURCES += toolboxShrink.c
   vmware_toolbox_SOURCES += toolboxRecord.c

# Here's something wacky: automake will muck up the translation of this
# conditional if it is indented. For more details, see:
# http://osdir.com/ml/sysutils.automake.general/2002-10/msg00064.html
if HAVE_ICU
   vmware_toolbox_LDADD += @ICU_LIBS@
   vmware_toolbox_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                         $(LIBTOOLFLAGS) --mode=link $(CXX) $(AM_CXXFLAGS) \
                         $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
else
   vmware_toolbox_LINK = $(LINK)
endif

   install-exec-hook:
	$(INSTALL) -d $(DESTDIR)$(datadir)/applications/
	$(INSTALL) -m 644 ../scripts/common/vmware-toolbox.desktop \
		$(DESTDIR)$(datadir)/applications/
	$(SED) -i s/Exec=.*/Exec=vmware-toolbox/ \
		$(DESTDIR)$(datadir)/applications/vmware-toolbox.desktop
	$(INSTALL) -d $(DESTDIR)/usr/share/pixmaps/vmware
	$(INSTALL) -m 644 bigIcon.xpm \
		$(DESTDIR)/usr/share/pixmaps/vmware/vmware-toolbox.xpm
   uninstall-hook:
	-$(RM) -f $(DESTDIR)$(datadir)/share/applications/vmware-toolbox.desktop
	-$(RM) -rf $(DESTDIR)/usr/share/pixmaps/vmware
endif

bin_PROGRAMS += vmware-toolbox-cmd

vmware_toolbox_cmd_LDADD =
vmware_toolbox_cmd_LDADD += ../libguestlib/libguestlib.la
vmware_toolbox_cmd_LDADD += ../lib/guestInfo/libGuestInfo.a
vmware_toolbox_cmd_LDADD += ../lib/guestApp/libGuestApp.a
vmware_toolbox_cmd_LDADD += $(COMMON)

vmware_toolbox_cmd_SOURCES =
vmware_toolbox_cmd_SOURCES += debugStdio.c
vmware_toolbox_cmd_SOURCES += toolbox-cmd.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-devices.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-scripts.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-shrink.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-stat.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-time.c
vmware_toolbox_cmd_SOURCES += toolboxcmd-record.c

if HAVE_ICU
   vmware_toolbox_cmd_LDADD += @ICU_LIBS@
   vmware_toolbox_cmd_LINK = $(LIBTOOL) --tag=CXX $(AM_LIBTOOLFLAGS) \
                             $(LIBTOOLFLAGS) --mode=link $(CXX) \
                             $(AM_CXXFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) \
                             $(LDFLAGS) -o $@
else
   vmware_toolbox_cmd_LINK = $(LINK)
endif
