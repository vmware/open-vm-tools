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

SUBDIRS =
SUBDIRS += guestRpc
if ENABLE_UNITY
   SUBDIRS += appUtil
endif
SUBDIRS += auth
SUBDIRS += backdoor
SUBDIRS += conf
SUBDIRS += dict
SUBDIRS += deployPkg
SUBDIRS += dnd
if HAVE_GTKMM
   SUBDIRS += dndGuest
endif
SUBDIRS += dynxdr
SUBDIRS += err
SUBDIRS += eventManager
SUBDIRS += file
SUBDIRS += foundryMsg
if ENABLE_UNITY
   SUBDIRS += ghIntegration
else
   SUBDIRS += ghIntegrationStub
endif
SUBDIRS += guestApp
SUBDIRS += guestInfo
SUBDIRS += hgfs
SUBDIRS += hgfsBd
SUBDIRS += hgfsHelper
SUBDIRS += hgfsServer
SUBDIRS += hgfsServerManagerGuest
SUBDIRS += hgfsServerPolicyGuest
if ENABLE_UNITY
   SUBDIRS += image
endif
SUBDIRS += impersonate
SUBDIRS += message
SUBDIRS += misc
SUBDIRS += netUtil
SUBDIRS += panic
SUBDIRS += panicDefault
SUBDIRS += printer
SUBDIRS += procMgr
if ENABLE_UNITY
   SUBDIRS += raster
   SUBDIRS += region
endif
if HAVE_X11
   SUBDIRS += resolution
endif
SUBDIRS += rpcChannel
SUBDIRS += rpcIn
SUBDIRS += rpcOut
SUBDIRS += rpcVmx
SUBDIRS += SLPv2Parser
SUBDIRS += socketMgr
SUBDIRS += string
SUBDIRS += stubs
SUBDIRS += sync
SUBDIRS += syncDriver
SUBDIRS += system
SUBDIRS += unicode
if ENABLE_UNITY
   SUBDIRS += unity
   SUBDIRS += unityWindowTracker
else
   SUBDIRS += unityStub
endif
SUBDIRS += user
SUBDIRS += vixTools
SUBDIRS += vmCheck
SUBDIRS += vmSignal
SUBDIRS += wiper
