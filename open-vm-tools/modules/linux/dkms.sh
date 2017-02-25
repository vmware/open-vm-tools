#!/bin/sh
################################################################################
### Copyright (C) 2009-2017 VMware, Inc.  All rights reserved.
###
### Script for creating a dmks-compliant source tree from an open-vm-tools
### distribution.
###
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

if test -z "$1" -o -z "$2"
then
   echo "usage: $0 src dst"
   echo
   echo "Where:"
   echo "   src:  root of unpacked open-vm-tools package"
   echo "   dst:  where to create the dkms tree"
   echo
   echo "The script will create an 'open-vm-tools' module with version 10.1.5."
   exit 1
fi

src=$1
dst=$2/open-vm-tools-10.1.5

SHARED_HEADERS="backdoor_def.h"
SHARED_HEADERS="$SHARED_HEADERS backdoor_types.h"
SHARED_HEADERS="$SHARED_HEADERS circList.h"
SHARED_HEADERS="$SHARED_HEADERS community_source.h"
SHARED_HEADERS="$SHARED_HEADERS dbllnklst.h"
SHARED_HEADERS="$SHARED_HEADERS guest_msg_def.h"
SHARED_HEADERS="$SHARED_HEADERS includeCheck.h"
SHARED_HEADERS="$SHARED_HEADERS vm_assert.h"
SHARED_HEADERS="$SHARED_HEADERS vm_atomic.h"
SHARED_HEADERS="$SHARED_HEADERS vm_basic_asm.h"
SHARED_HEADERS="$SHARED_HEADERS vm_basic_asm_x86.h"
SHARED_HEADERS="$SHARED_HEADERS vm_basic_asm_x86_64.h"
SHARED_HEADERS="$SHARED_HEADERS vm_basic_asm_x86_common.h"
SHARED_HEADERS="$SHARED_HEADERS vm_basic_defs.h"
SHARED_HEADERS="$SHARED_HEADERS vm_basic_math.h"
SHARED_HEADERS="$SHARED_HEADERS vm_basic_types.h"
SHARED_HEADERS="$SHARED_HEADERS vm_device_version.h"
SHARED_HEADERS="$SHARED_HEADERS vmci_sockets.h"
SHARED_HEADERS="$SHARED_HEADERS vmware.h"
SHARED_HEADERS="$SHARED_HEADERS vmware_pack_begin.h"
SHARED_HEADERS="$SHARED_HEADERS vmware_pack_end.h"
SHARED_HEADERS="$SHARED_HEADERS vmware_pack_init.h"
SHARED_HEADERS="$SHARED_HEADERS x86cpuid.h"
SHARED_HEADERS="$SHARED_HEADERS x86vendor.h"
SHARED_HEADERS="$SHARED_HEADERS x86cpuid_asm.h"

rm -rf $dst
mkdir -p $dst
cp -f `dirname $0`/dkms.conf $dst

for m in vmblock vmci vmhgfs vmsync vmxnet vsock
do
   mdst="$dst/$m"

   cp -rf $src/modules/linux/$m $mdst
   cp -rf $src/modules/linux/shared $mdst
   for h in $SHARED_HEADERS
   do
      cp -f $src/lib/include/$h $mdst/shared
   done

   # Shared vmblock code.
   if test $m = vmblock
   then
      cp -f $src/lib/include/vmblock.h $mdst/linux
      cp -rf $src/modules/shared/vmblock/* $mdst/linux
   fi

   # Backdoor library (for vmhgfs and vmmemctl).
   if test $m = vmhgfs -o $m = vmmemctl
   then
      cp -f $src/lib/include/backdoor.h $mdst
      cp -f $src/lib/backdoor/*.c $src/lib/backdoor/*.h $mdst
   fi

   # Other libraries used by vmhgfs
   if test $m = vmhgfs
   then
      cp -f $src/lib/include/cpName*.h $mdst
      cp -f $src/lib/include/escBitvector.h $mdst
      cp -f $src/lib/include/hgfs*.h $mdst
      cp -f $src/lib/include/message.h $mdst
      cp -f $src/lib/include/rpcout.h $mdst
      cp -f $src/lib/hgfs/*.c $src/lib/hgfs/*.h $mdst
      cp -f $src/lib/hgfsBd/*.c $mdst
      cp -f $src/lib/message/*.c $mdst
      cp -f $src/lib/rpcOut/*.c $mdst
   fi

   # Extra header file for vmsync.
   if test $m = vmsync
   then
      cp -f $src/lib/include/syncDriverIoc.h $mdst
   fi

   # Shared vmxnet headers.
   if test $m = vmxnet
   then
      cp -f $src/modules/shared/vmxnet/* $mdst/shared
   fi
done

