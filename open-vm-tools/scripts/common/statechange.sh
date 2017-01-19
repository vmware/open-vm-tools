#!/bin/sh
##########################################################
# Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation version 2.1 and no later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
#
##########################################################

##########################################################################
# DO NOT modify this file directly as it will be overwritten the next
# time the VMware Tools are installed.
##########################################################################

#
# statechange.sh
#
# This script is a refactored version of the legacy power scripts (e.g.,
# poweron-vm-default).  It expects to be installed in their places --
# in other words, `basename "$0"` might be poweron-vm-default.
#
# Handy reference/shorthand used in this doc/scripts:
#    TOOLS_CONFDIR ::= Depends on platform and installation settings.  Likely
#                      "/etc/vmware-tools" or
#                      "/Library/Application Support/VMware Tools"
#    powerOp       ::= One of "poweron-vm", "poweroff-vm", "suspend-vm", and
#                      "resume-vm".
#    vmwScriptDir  ::= $TOOLS_CONFDIR/scripts/vmware
#    userScriptDir ::= $TOOLS_CONFDIR/scripts/${powerOp}-default.d
#
# End users may install scripts of their own under $userScriptDir.  They
# are executed in alphabetical order with "$powerOp" as the only argument.
#
# NB:  This directory layout remains to preserve backwards compatibility. End
# users are free to write a single script which uses its only parameter
# (${powerOp}) as a discriminator, and then install symlinks to it in each
# of the ${powerOp}-default.d directories.
#
# On power-on and resume, VMware's scripts execute before the end user's.  On
# suspend and power-off, the end user's execute before VMware's.  (This way,
# VMware stops services only after the user's scripts have finished their
# work, and conversely restores the same services before the user's scripts
# attempt to use them.)
#
# Should any script exit non-zero, only its value will be saved to exitCode.
# (Any further non-zero exits will have no effect on exitCode.)  This script
# exits with $exitCode.
#
# XXX Consider using the available/enabled pattern for VMware's scripts.
#
# XXX This should be staged as a single executable whereby the desired
# power operation is passed in as a parameter.  (I.e., one would run
# "/path/to/statechange.sh suspend-vm" rather than having to install
# statechange.sh as suspend-vm-default.)
#

echo `date` ": Executing '$0'"

# See above.
TOOLS_CONFDIR=`dirname "$0"`
export TOOLS_CONFDIR

# Pull in subroutines like Panic.
. "$TOOLS_CONFDIR"/statechange.subr


#
# RunScripts --
#
#    Executes scripts installed under $scriptDir.
#
# Side effects:
#    exitCode may be incremented.
#

RunScripts() {
   scriptDir="$1"

   if [ -d "$scriptDir" ]; then
      for scriptFile in "$scriptDir"/*; do
         if [ -x "$scriptFile" ]; then
            "$scriptFile" $powerOp
            exitCode=`expr $exitCode \| $?`
         fi
      done
   fi
}


#
# main --
#
#    Entry point.  See comments at top of file for details.
#
# Results:
#    Exits with $exitCode.
#

main() {
   # This is sanity checked in the case/esac bit below.
   powerOp=`basename "$0" | sed 's,-default,,'`
   exitCode=0

   vmwScriptDir="$TOOLS_CONFDIR/scripts/vmware"
   userScriptDir="$TOOLS_CONFDIR/scripts/${powerOp}-default.d"

   case "$powerOp" in
      poweron-vm|resume-vm)
         RunScripts "$vmwScriptDir"
         RunScripts "$userScriptDir"
         ;;
      poweroff-vm|suspend-vm)
         RunScripts "$userScriptDir"
         RunScripts "$vmwScriptDir"
         ;;
      *)
         Panic "Invalid argument: $powerOp"
         ;;
   esac

   return $exitCode
}

main
