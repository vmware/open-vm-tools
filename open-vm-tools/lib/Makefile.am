################################################################################
### Copyright (c) 2007-2016,2020-2021 VMware, Inc.  All rights reserved.
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

SUBDIRS =
SUBDIRS += guestRpc
if HAVE_X11
   SUBDIRS += appUtil
endif
SUBDIRS += auth
SUBDIRS += backdoor
if HAVE_VSOCK
SUBDIRS += asyncsocket
endif
SUBDIRS += sslDirect
SUBDIRS += pollGtk
SUBDIRS += poll
SUBDIRS += dataMap
SUBDIRS += hashMap
SUBDIRS += dict
SUBDIRS += dynxdr
SUBDIRS += err
SUBDIRS += file
SUBDIRS += foundryMsg
SUBDIRS += glibUtils
SUBDIRS += guestApp
if LINUX
   SUBDIRS += guestStoreClientHelper
   SUBDIRS += globalConfig
endif
SUBDIRS += hgfs
SUBDIRS += hgfsBd
SUBDIRS += hgfsHelper
SUBDIRS += hgfsServer
SUBDIRS += hgfsServerManagerGuest
SUBDIRS += hgfsServerPolicyGuest
if HAVE_GTKMM
SUBDIRS += hgfsUri
endif
SUBDIRS += impersonate
SUBDIRS += lock
SUBDIRS += message
SUBDIRS += misc
SUBDIRS += netUtil
SUBDIRS += nicInfo
SUBDIRS += panic
SUBDIRS += panicDefault
SUBDIRS += procMgr
SUBDIRS += rpcChannel
SUBDIRS += rpcIn
SUBDIRS += rpcOut
SUBDIRS += rpcVmx
if USE_SLASH_PROC
   SUBDIRS += slashProc
endif
SUBDIRS += string
SUBDIRS += jsmn
SUBDIRS += stubs
SUBDIRS += syncDriver
SUBDIRS += system
SUBDIRS += unicode
SUBDIRS += user
SUBDIRS += vmCheck
SUBDIRS += vmSignal
SUBDIRS += wiper
SUBDIRS += xdg
