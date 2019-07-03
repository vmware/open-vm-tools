/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
   LOGLEVEL_VAR(smram), \
   LOGLEVEL_VAR(txt), \
   LOGLEVEL_VAR(gmm), \
   LOGLEVEL_VAR(sgx), \
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
   LOGLEVEL_VAR(vusbaudio), \
   LOGLEVEL_VAR(vusbccid), \
   LOGLEVEL_VAR(vusbhid), \
   LOGLEVEL_VAR(vusbkeyboard), \
   LOGLEVEL_VAR(vusbmouse), \
   LOGLEVEL_VAR(vusbtablet), \
   LOGLEVEL_VAR(vusbvideo),\
   LOGLEVEL_VAR(vusbrng),\
   LOGLEVEL_VAR(hidQueue), \
   LOGLEVEL_VAR(pci_vlance), \
   LOGLEVEL_VAR(pci_svga), \
   LOGLEVEL_VAR(pci_e1000), \
   LOGLEVEL_VAR(pci_hyper), \
   LOGLEVEL_VAR(pcibridge), \
   LOGLEVEL_VAR(vide), \
   LOGLEVEL_VAR(atapiCdrom), \
   LOGLEVEL_VAR(hostonly), \
   LOGLEVEL_VAR(oprom), \
   LOGLEVEL_VAR(http), \
   LOGLEVEL_VAR(vmci), \
   LOGLEVEL_VAR(pci_vmci), \
   LOGLEVEL_VAR(vmxnet3), \
   LOGLEVEL_VAR(pci_vmxnet3), \
   LOGLEVEL_VAR(vcpuhotplug), \
   LOGLEVEL_VAR(vcpuNUMA), \
   LOGLEVEL_VAR(pciplugin), \
   LOGLEVEL_VAR(vsock), \
   LOGLEVEL_VAR(vrdma), \
   LOGLEVEL_VAR(nvdimm), \
   LOGLEVEL_VAR(qat), \
   LOGLEVEL_VAR(vtpm), \
   LOGLEVEL_VAR(mor), \
   LOGLEVEL_VAR(precisionclock), \
   \
   /* user/disk */ \
   LOGLEVEL_VAR(aioMgr), \
   LOGLEVEL_VAR(aioWin32), \
   LOGLEVEL_VAR(aioWin32Completion), \
   LOGLEVEL_VAR(aioKernel), \
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
   LOGLEVEL_VAR(mksInput), \
   LOGLEVEL_VAR(mksSWB), \
   LOGLEVEL_VAR(mksClient), \
   LOGLEVEL_VAR(mksServer), \
   LOGLEVEL_VAR(mksControl), \
   LOGLEVEL_VAR(mksKeyboard), \
   LOGLEVEL_VAR(keymap), \
   LOGLEVEL_VAR(mksMouse), \
   LOGLEVEL_VAR(mksHostCursor), \
   LOGLEVEL_VAR(mksCursorPosition), \
   LOGLEVEL_VAR(mksBasicOps), \
   LOGLEVEL_VAR(mksRenderOps), \
   LOGLEVEL_VAR(mksFrame), \
   LOGLEVEL_VAR(mksGLBasic), \
   LOGLEVEL_VAR(mksGLManager), \
   LOGLEVEL_VAR(mksGLFBO), \
   LOGLEVEL_VAR(mksGLShader), \
   LOGLEVEL_VAR(mksGLState), \
   LOGLEVEL_VAR(mksGLWindow), \
   LOGLEVEL_VAR(mksGLContextMux), \
   LOGLEVEL_VAR(mksGLDraw), \
   LOGLEVEL_VAR(mksGLQuery), \
   LOGLEVEL_VAR(mksGLTextureView), \
   LOGLEVEL_VAR(mksWinBSOD), \
   LOGLEVEL_VAR(mksDX11Renderer), \
   LOGLEVEL_VAR(mksDX11ResourceView), \
   LOGLEVEL_VAR(mksDX11ShimOps), \
   LOGLEVEL_VAR(mksMTLRenderer), \
   LOGLEVEL_VAR(mksVulkanRenderer), \
   LOGLEVEL_VAR(vaBasicOps), \
   LOGLEVEL_VAR(vdpPlugin), \
   LOGLEVEL_VAR(vncServer), \
   LOGLEVEL_VAR(viewClient), \
  \
   /* user/sound */ \
   LOGLEVEL_VAR(sound), \
   LOGLEVEL_VAR(hdaudio), \
   LOGLEVEL_VAR(pci_hdaudio), \
   LOGLEVEL_VAR(hdaudio_alsa), \
   \
   /* user video */ \
   LOGLEVEL_VAR(AVCapture), \
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
   LOGLEVEL_VAR(policy), \
   LOGLEVEL_VAR(poll), \
   LOGLEVEL_VAR(barrier), \
   LOGLEVEL_VAR(mstat), \
   LOGLEVEL_VAR(vmLock), \
   LOGLEVEL_VAR(buslogic), \
   LOGLEVEL_VAR(lsilogic), \
   LOGLEVEL_VAR(pvscsi), \
   LOGLEVEL_VAR(ahci), \
   LOGLEVEL_VAR(nvme), \
   LOGLEVEL_VAR(diskVmnix), \
   LOGLEVEL_VAR(hbaCommon), \
   LOGLEVEL_VAR(backdoor), \
   LOGLEVEL_VAR(buslogicMdev), \
   LOGLEVEL_VAR(hgfs), \
   LOGLEVEL_VAR(hgfsServer), \
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
   LOGLEVEL_VAR(vmgenc), \
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
   LOGLEVEL_VAR(vncBlit),   \
   LOGLEVEL_VAR(libconnect), \
   LOGLEVEL_VAR(state3d), \
   LOGLEVEL_VAR(vmGL), \
   LOGLEVEL_VAR(guest_msg), \
   LOGLEVEL_VAR(guest_rpc), \
   LOGLEVEL_VAR(guestVars), \
   LOGLEVEL_VAR(vmkEvent), \
   LOGLEVEL_VAR(authenticode), \
   LOGLEVEL_VAR(tpm2Verification), \
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
   LOGLEVEL_VAR(vprobe), \
   LOGLEVEL_VAR(VProbeClient), \
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
   LOGLEVEL_VAR(digestlib), \
   LOGLEVEL_VAR(inputdevtap), \
   LOGLEVEL_VAR(objlib), \
   LOGLEVEL_VAR(vsanobj), \
   LOGLEVEL_VAR(vvolbe), \
   LOGLEVEL_VAR(upitbe), \
   LOGLEVEL_VAR(slotfs), \
   LOGLEVEL_VAR(svgadevtap), \
   LOGLEVEL_VAR(masReceipt), /* lib/masReceipt */ \
   LOGLEVEL_VAR(serviceImpl), /* lib/serviceImpl */ \
   LOGLEVEL_VAR(serviceUser), /* lib/serviceUser */ \
   LOGLEVEL_VAR(ssl), \
   LOGLEVEL_VAR(namespaceDb), \
   LOGLEVEL_VAR(namespaceMgr), \
   LOGLEVEL_VAR(grainTrack), \
   LOGLEVEL_VAR(shim3D), \
   LOGLEVEL_VAR(crc32), \
   LOGLEVEL_VAR(vmkmgmtlib), \
   LOGLEVEL_VAR(vflash), \
   LOGLEVEL_VAR(ftConfig), /*lib/ftConfig */ \
   LOGLEVEL_VAR(vmname),  /* lib/vmname */ \
   LOGLEVEL_VAR(gpumgmt), \
   LOGLEVEL_VAR(unityMsg),  /* mks/remote/vdpUnityVmdb */ \
   LOGLEVEL_VAR(sharedFolderMgr),  /* mks/remote/vdpFolderSharedMgrVmdb */ \
   LOGLEVEL_VAR(crtbora),  /* apps/crtbora */ \
   LOGLEVEL_VAR(mirror), \
   LOGLEVEL_VAR(filtlib), \
   LOGLEVEL_VAR(epd), \
   LOGLEVEL_VAR(ddecomd), \
   LOGLEVEL_VAR(vdfs), \
   LOGLEVEL_VAR(vdfs_9p), \
   LOGLEVEL_VAR(hostctl), \
   LOGLEVEL_VAR(pmemobj), \
   LOGLEVEL_VAR(secureBoot), \
   LOGLEVEL_VAR(upitd), \
   LOGLEVEL_VAR(promotedisk), \
   LOGLEVEL_VAR(efivarstore), \
   LOGLEVEL_VAR(toolsIso), \
   LOGLEVEL_VAR(toolsversion), \
   LOGLEVEL_VAR(vmva), \
   LOGLEVEL_VAR(udpfec),    /* lib/udpfec */ \
   LOGLEVEL_VAR(maclatency), \
   LOGLEVEL_VAR(tpm2emu), \
   LOGLEVEL_VAR(tarReader),\
   LOGLEVEL_VAR(nvramMgr), \
   LOGLEVEL_VAR(hbr), \
   LOGLEVEL_VAR(vvtd), \
   LOGLEVEL_VAR(amdIommu), \
   LOGLEVEL_VAR(vmOvhd), \
   LOGLEVEL_VAR(assignHw), \
   LOGLEVEL_VAR(directBoot), \
   LOGLEVEL_VAR(vwdt), \
   LOGLEVEL_VAR(keypersist),

   /* end of list */

LOGLEVEL_EXTENSION_DECLARE(LOGLEVEL_USER);

#endif /* _LOGLEVEL_USER_H_ */
