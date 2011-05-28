/*********************************************************
 * Copyright (C) 1998-2003 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

#ifndef _LOGLEVEL_USER_H_
#define _LOGLEVEL_USER_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#define LOGLEVEL_EXTENSION user
#include "loglevel_defs.h"

#define LOGLEVEL_USER(LOGLEVEL_VAR) \
   /* user/main*/ \
   /* main has to be first. */ \
   LOGLEVEL_VAR(main), \
   LOGLEVEL_VAR(aio), \
   LOGLEVEL_VAR(passthrough), \
   LOGLEVEL_VAR(tools), \
   LOGLEVEL_VAR(license), \
   LOGLEVEL_VAR(vui), \
   LOGLEVEL_VAR(stats), \
   LOGLEVEL_VAR(cpucount), \
   LOGLEVEL_VAR(ovhdmem), \
   LOGLEVEL_VAR(vigor), \
   \
   /* user/io */ \
   LOGLEVEL_VAR(disk), \
   LOGLEVEL_VAR(keyboard), \
   LOGLEVEL_VAR(vmmouse), \
   LOGLEVEL_VAR(timer), \
   LOGLEVEL_VAR(vga), \
   LOGLEVEL_VAR(svga), \
   LOGLEVEL_VAR(svga_rect), \
   LOGLEVEL_VAR(enableDetTimer), \
   LOGLEVEL_VAR(dma), \
   LOGLEVEL_VAR(floppy), \
   LOGLEVEL_VAR(cmos), \
   LOGLEVEL_VAR(vlance), \
   LOGLEVEL_VAR(e1000), \
   LOGLEVEL_VAR(serial), \
   LOGLEVEL_VAR(parallel), \
   LOGLEVEL_VAR(chipset), \
   LOGLEVEL_VAR(smc), \
   LOGLEVEL_VAR(ich7m), \
   LOGLEVEL_VAR(hpet), \
   LOGLEVEL_VAR(extcfgdevice), \
   LOGLEVEL_VAR(flashram), \
   LOGLEVEL_VAR(efinv), \
   LOGLEVEL_VAR(pvnvram), \
   LOGLEVEL_VAR(pci), \
   LOGLEVEL_VAR(pci_vide), \
   LOGLEVEL_VAR(pci_uhci), \
   LOGLEVEL_VAR(uhci), \
   LOGLEVEL_VAR(pci_ehci), \
   LOGLEVEL_VAR(ehci), \
   LOGLEVEL_VAR(pci_xhci), \
   LOGLEVEL_VAR(usb_xhci), \
   LOGLEVEL_VAR(usb), \
   LOGLEVEL_VAR(vusbmouse), \
   LOGLEVEL_VAR(vusbkeyboard), \
   LOGLEVEL_VAR(vusbhid), \
   LOGLEVEL_VAR(vusbtablet), \
   LOGLEVEL_VAR(hidQueue), \
   LOGLEVEL_VAR(pci_1394), \
   LOGLEVEL_VAR(1394), \
   LOGLEVEL_VAR(pci_vlance), \
   LOGLEVEL_VAR(pci_svga), \
   LOGLEVEL_VAR(pci_e1000), \
   LOGLEVEL_VAR(pci_hyper), \
   LOGLEVEL_VAR(pcibridge), \
   LOGLEVEL_VAR(vide), \
   LOGLEVEL_VAR(ideCdrom), \
   LOGLEVEL_VAR(hostonly), \
   LOGLEVEL_VAR(oprom), \
   LOGLEVEL_VAR(http), \
   LOGLEVEL_VAR(vmci), \
   LOGLEVEL_VAR(pci_vmci), \
   LOGLEVEL_VAR(vmxnet3), \
   LOGLEVEL_VAR(pci_vmxnet3), \
   LOGLEVEL_VAR(vcpuhotplug), \
   LOGLEVEL_VAR(vcpuNUMA), \
   LOGLEVEL_VAR(heci), \
   LOGLEVEL_VAR(vmiopluginlib), \
   \
   /* user/disk */ \
   LOGLEVEL_VAR(aioMgr), \
   LOGLEVEL_VAR(aioWin32), \
   LOGLEVEL_VAR(aioWin32Completion), \
   LOGLEVEL_VAR(aioLinux), \
   LOGLEVEL_VAR(aioHttp), \
   LOGLEVEL_VAR(aioGeneric), \
   LOGLEVEL_VAR(cdrom), \
   LOGLEVEL_VAR(checksum), \
   \
   /* user/checkpoint */ \
   LOGLEVEL_VAR(checkpoint), \
   LOGLEVEL_VAR(dumper), \
   LOGLEVEL_VAR(migrate), \
   LOGLEVEL_VAR(fsresx), \
   \
   /* user/gui */ \
   LOGLEVEL_VAR(gui), \
   LOGLEVEL_VAR(guiWin32), \
   LOGLEVEL_VAR(mks), \
   LOGLEVEL_VAR(mksClient), \
   LOGLEVEL_VAR(mksServer), \
   LOGLEVEL_VAR(mksKeyboard), \
   LOGLEVEL_VAR(keymap), \
   LOGLEVEL_VAR(mksMouse), \
   LOGLEVEL_VAR(mksHostCursor), \
   LOGLEVEL_VAR(mksHostOps), \
   LOGLEVEL_VAR(mksGLManager), \
   LOGLEVEL_VAR(mksGLShader), \
   LOGLEVEL_VAR(vdpPlugin), \
   \
   /* user/sound */ \
   LOGLEVEL_VAR(sound), \
   LOGLEVEL_VAR(hdaudio), \
   LOGLEVEL_VAR(pci_hdaudio), \
   LOGLEVEL_VAR(hdaudio_alsa), \
   \
   /* user/disklib */ \
   LOGLEVEL_VAR(disklib), \
   LOGLEVEL_VAR(dmg), \
   LOGLEVEL_VAR(sparseChecker), \
   LOGLEVEL_VAR(dataCache), \
   /* more */ \
   LOGLEVEL_VAR(dict), \
   LOGLEVEL_VAR(pci_scsi), \
   LOGLEVEL_VAR(scsi), \
   LOGLEVEL_VAR(grm), \
   LOGLEVEL_VAR(vmxnet), \
   LOGLEVEL_VAR(pciPassthru), \
   LOGLEVEL_VAR(vnet), \
   LOGLEVEL_VAR(netPkt), \
   LOGLEVEL_VAR(macfilter), \
   LOGLEVEL_VAR(macbw), \
   LOGLEVEL_VAR(macfi), \
   LOGLEVEL_VAR(vmkcfg), \
   LOGLEVEL_VAR(poll), \
   LOGLEVEL_VAR(barrier), \
   LOGLEVEL_VAR(mstat), \
   LOGLEVEL_VAR(vmLock), \
   LOGLEVEL_VAR(buslogic), \
   LOGLEVEL_VAR(lsilogic), \
   LOGLEVEL_VAR(pvscsi), \
   LOGLEVEL_VAR(diskVmnix), \
   LOGLEVEL_VAR(hbaCommon), \
   LOGLEVEL_VAR(backdoor), \
   LOGLEVEL_VAR(buslogicMdev), \
   LOGLEVEL_VAR(hgfs), \
   LOGLEVEL_VAR(memspace), \
   LOGLEVEL_VAR(dnd), \
   LOGLEVEL_VAR(appstate), \
   LOGLEVEL_VAR(vthread), \
   LOGLEVEL_VAR(vmhs), \
   LOGLEVEL_VAR(undopoint), \
   LOGLEVEL_VAR(ipc), \
   LOGLEVEL_VAR(smbios), \
   LOGLEVEL_VAR(acpi), \
   LOGLEVEL_VAR(acpiGPE), \
   LOGLEVEL_VAR(xpmode), \
   LOGLEVEL_VAR(snapshot), \
   LOGLEVEL_VAR(asyncsocket), \
   LOGLEVEL_VAR(mainMem), \
   LOGLEVEL_VAR(mainMemReplayCheck), \
   LOGLEVEL_VAR(memoryHotplug), \
   LOGLEVEL_VAR(numa), \
   LOGLEVEL_VAR(numaHost), \
   LOGLEVEL_VAR(remoteDevice), \
   LOGLEVEL_VAR(vncDecode), \
   LOGLEVEL_VAR(vncEncode), \
   LOGLEVEL_VAR(libconnect), \
   LOGLEVEL_VAR(state3d), \
   LOGLEVEL_VAR(vmGL), \
   LOGLEVEL_VAR(guest_msg), \
   LOGLEVEL_VAR(guest_rpc), \
   LOGLEVEL_VAR(guestVars), \
   LOGLEVEL_VAR(vmkEvent), \
   LOGLEVEL_VAR(battery), \
   LOGLEVEL_VAR(fakeDma), \
   LOGLEVEL_VAR(shader), \
   LOGLEVEL_VAR(machPoll), \
   LOGLEVEL_VAR(replayVMX), \
   LOGLEVEL_VAR(vmWindowController), \
   LOGLEVEL_VAR(dui), \
   LOGLEVEL_VAR(duiMKS), \
   LOGLEVEL_VAR(worker), \
   LOGLEVEL_VAR(duiDevices), \
   LOGLEVEL_VAR(duiLocalization), \
   LOGLEVEL_VAR(duiProxyApps), \
   LOGLEVEL_VAR(docker), \
   LOGLEVEL_VAR(vmIPC), \
   LOGLEVEL_VAR(uwt), /* lib/unityWindowTracker */ \
   LOGLEVEL_VAR(cui), \
   LOGLEVEL_VAR(automation), \
   LOGLEVEL_VAR(oemDevice), \
   LOGLEVEL_VAR(cptOps), \
   LOGLEVEL_VAR(VProbeExec), \
   LOGLEVEL_VAR(VP), \
   LOGLEVEL_VAR(device), \
   LOGLEVEL_VAR(devicePowerOn), \
   LOGLEVEL_VAR(vmxvmdbCallbacks), \
   LOGLEVEL_VAR(guestInstall), \
   LOGLEVEL_VAR(migrateVM), \
   LOGLEVEL_VAR(vmUpsellController), \
   LOGLEVEL_VAR(objc), /* lib/objc */ \
   LOGLEVEL_VAR(blit), /* lib/blit */ \
   LOGLEVEL_VAR(vmnetBridge), \
   LOGLEVEL_VAR(wifi), /* macWireless and wpa_supplicant */ \
   LOGLEVEL_VAR(pvfslib), \
   LOGLEVEL_VAR(brtalk), \
   LOGLEVEL_VAR(button), \
   LOGLEVEL_VAR(util), \
   LOGLEVEL_VAR(vmcf), \
   LOGLEVEL_VAR(win32util), \
   LOGLEVEL_VAR(largepage), \
   LOGLEVEL_VAR(guestAppMonitor), \
   LOGLEVEL_VAR(syncWaitQ), \
   LOGLEVEL_VAR(sg), /* lib/sg */ \
   LOGLEVEL_VAR(ftcpt), \
   LOGLEVEL_VAR(wrapLib),  \
   LOGLEVEL_VAR(digestlib), \
   LOGLEVEL_VAR(inputdevtap), \
   LOGLEVEL_VAR(svgadevtap), \
   LOGLEVEL_VAR(masReceipt), \
   LOGLEVEL_VAR(ssl), \
   LOGLEVEL_VAR(vmrc), \
   /* end of list */

LOGLEVEL_EXTENSION_DECLARE(LOGLEVEL_USER);

#endif /* _LOGLEVEL_USER_H_ */
