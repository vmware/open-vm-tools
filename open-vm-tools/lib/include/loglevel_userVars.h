/*********************************************************
 * Copyright (c) 1998-2022 VMware, Inc. All rights reserved.
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

#ifndef _LOGLEVEL_USER_VARS_H_
#define _LOGLEVEL_USER_VARS_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

/* KEEP IN SORTED ORDER! */

#define LOGLEVEL_USER(LOGLEVEL_VAR) \
   LOGLEVEL_VAR(acpi), \
   LOGLEVEL_VAR(acpiGPE), \
   LOGLEVEL_VAR(ahci), \
   LOGLEVEL_VAR(aio), \
   LOGLEVEL_VAR(aioGeneric), \
   LOGLEVEL_VAR(aioHttp), \
   LOGLEVEL_VAR(aioKernel), \
   LOGLEVEL_VAR(aioMgr), \
   LOGLEVEL_VAR(aioWin32), \
   LOGLEVEL_VAR(aioWin32Completion), \
   LOGLEVEL_VAR(amdIommu), \
   LOGLEVEL_VAR(appstate), \
   LOGLEVEL_VAR(assignHw), \
   LOGLEVEL_VAR(asyncsocket), \
   LOGLEVEL_VAR(atapiCdrom), \
   LOGLEVEL_VAR(authenticode), \
   LOGLEVEL_VAR(automation), \
   LOGLEVEL_VAR(AVCapture), \
   LOGLEVEL_VAR(backdoor), \
   LOGLEVEL_VAR(barrier), \
   LOGLEVEL_VAR(battery), \
   LOGLEVEL_VAR(blit), /* lib/blit */ \
   LOGLEVEL_VAR(brtalk), \
   LOGLEVEL_VAR(buslogic), \
   LOGLEVEL_VAR(buslogicMdev), \
   LOGLEVEL_VAR(button), \
   LOGLEVEL_VAR(cdrom), \
   LOGLEVEL_VAR(checkpoint), \
   LOGLEVEL_VAR(checksum), \
   LOGLEVEL_VAR(chipset), \
   LOGLEVEL_VAR(cmos), \
   LOGLEVEL_VAR(cptOps), \
   LOGLEVEL_VAR(cpucount), \
   LOGLEVEL_VAR(CpuidInfo), \
   LOGLEVEL_VAR(crc32), \
   LOGLEVEL_VAR(crtbora),  /* apps/crtbora */ \
   LOGLEVEL_VAR(cui), \
   LOGLEVEL_VAR(dataCache), \
   LOGLEVEL_VAR(dataSetsMgr), \
   LOGLEVEL_VAR(dataSetsStore),\
   LOGLEVEL_VAR(device), \
   LOGLEVEL_VAR(deviceGroup), \
   LOGLEVEL_VAR(devicePowerOn), \
   LOGLEVEL_VAR(deviceSwap), \
   LOGLEVEL_VAR(deviceThread), \
   LOGLEVEL_VAR(dict), \
   LOGLEVEL_VAR(digestlib), \
   LOGLEVEL_VAR(directBoot), \
   LOGLEVEL_VAR(disk), \
   LOGLEVEL_VAR(disklib), \
   LOGLEVEL_VAR(diskVmnix), \
   LOGLEVEL_VAR(dma), \
   LOGLEVEL_VAR(dmg), \
   LOGLEVEL_VAR(dnd), \
   LOGLEVEL_VAR(docker), \
   LOGLEVEL_VAR(dui), \
   LOGLEVEL_VAR(duiDevices), \
   LOGLEVEL_VAR(duiLocalization), \
   LOGLEVEL_VAR(duiMKS), \
   LOGLEVEL_VAR(duiProxyApps), \
   LOGLEVEL_VAR(dumper), \
   LOGLEVEL_VAR(dvx), \
   LOGLEVEL_VAR(e1000), \
   LOGLEVEL_VAR(efinv), \
   LOGLEVEL_VAR(efivarstore), \
   LOGLEVEL_VAR(ehci), \
   LOGLEVEL_VAR(enableDetTimer), \
   LOGLEVEL_VAR(epd), \
   LOGLEVEL_VAR(extcfgdevice), \
   LOGLEVEL_VAR(fakeDma), \
   LOGLEVEL_VAR(filtlib), \
   LOGLEVEL_VAR(FiltLibTestLog), \
   LOGLEVEL_VAR(flashram), \
   LOGLEVEL_VAR(floppy), \
   LOGLEVEL_VAR(fsresx), \
   LOGLEVEL_VAR(ftConfig), /*lib/ftConfig */ \
   LOGLEVEL_VAR(ftcpt), \
   LOGLEVEL_VAR(gmm), \
   LOGLEVEL_VAR(gpuManager), \
   LOGLEVEL_VAR(grainTrack), \
   LOGLEVEL_VAR(grm), \
   LOGLEVEL_VAR(guestAppMonitor), \
   LOGLEVEL_VAR(guestInstall), \
   LOGLEVEL_VAR(guest_msg), \
   LOGLEVEL_VAR(guest_rpc), \
   LOGLEVEL_VAR(guestVars), \
   LOGLEVEL_VAR(gui), \
   LOGLEVEL_VAR(guiWin32), \
   LOGLEVEL_VAR(Heap), \
   LOGLEVEL_VAR(hbaCommon), \
   LOGLEVEL_VAR(hbr), \
   LOGLEVEL_VAR(hdaudio), \
   LOGLEVEL_VAR(hdaudio_alsa), \
   LOGLEVEL_VAR(hgfs), \
   LOGLEVEL_VAR(hgfsServer), \
   LOGLEVEL_VAR(hidQueue), \
   LOGLEVEL_VAR(hostctl), \
   LOGLEVEL_VAR(hostonly), \
   LOGLEVEL_VAR(hpet), \
   LOGLEVEL_VAR(http), \
   LOGLEVEL_VAR(ich7m), \
   LOGLEVEL_VAR(inputdevtap), \
   LOGLEVEL_VAR(ipc), \
   LOGLEVEL_VAR(ipcMgr), \
   LOGLEVEL_VAR(keyboard), \
   LOGLEVEL_VAR(keymap), \
   LOGLEVEL_VAR(keypersist), \
   LOGLEVEL_VAR(largepage), \
   LOGLEVEL_VAR(libconnect), \
   LOGLEVEL_VAR(license), \
   LOGLEVEL_VAR(llc), \
   LOGLEVEL_VAR(lsilogic), \
   LOGLEVEL_VAR(lwdFilter), \
   LOGLEVEL_VAR(macbw), \
   LOGLEVEL_VAR(macfi), \
   LOGLEVEL_VAR(macfilter), \
   LOGLEVEL_VAR(machPoll), \
   LOGLEVEL_VAR(maclatency), \
   LOGLEVEL_VAR(main), \
   LOGLEVEL_VAR(mainMem), \
   LOGLEVEL_VAR(mainMemReplayCheck), \
   LOGLEVEL_VAR(masReceipt), /* lib/masReceipt */ \
   LOGLEVEL_VAR(memoryHotplug), \
   LOGLEVEL_VAR(memspace), \
   LOGLEVEL_VAR(migrate), \
   LOGLEVEL_VAR(migrateVM), \
   LOGLEVEL_VAR(mirror), \
   LOGLEVEL_VAR(mks), \
   LOGLEVEL_VAR(mksBasicOps), \
   LOGLEVEL_VAR(mksClient), \
   LOGLEVEL_VAR(mksControl), \
   LOGLEVEL_VAR(mksCursorPosition), \
   LOGLEVEL_VAR(mksDX11Window), \
   LOGLEVEL_VAR(mksDX11Renderer), \
   LOGLEVEL_VAR(mksDX11Basic), \
   LOGLEVEL_VAR(mksDX11ResourceView), \
   LOGLEVEL_VAR(mksDX11ShimOps), \
   LOGLEVEL_VAR(mksDX12Renderer), \
   LOGLEVEL_VAR(mksFrame), \
   LOGLEVEL_VAR(mksGLBasic), \
   LOGLEVEL_VAR(mksGLContextMux), \
   LOGLEVEL_VAR(mksGLDraw), \
   LOGLEVEL_VAR(mksGLFBO), \
   LOGLEVEL_VAR(mksGLManager), \
   LOGLEVEL_VAR(mksGLQuery), \
   LOGLEVEL_VAR(mksGLShader), \
   LOGLEVEL_VAR(mksGLState), \
   LOGLEVEL_VAR(mksGLTextureView), \
   LOGLEVEL_VAR(mksGLWindow), \
   LOGLEVEL_VAR(mksHostCursor), \
   LOGLEVEL_VAR(mksInput), \
   LOGLEVEL_VAR(mksKeyboard), \
   LOGLEVEL_VAR(mksMouse), \
   LOGLEVEL_VAR(mksMTLRenderer), \
   LOGLEVEL_VAR(mksRenderOps), \
   LOGLEVEL_VAR(mksServer), \
   LOGLEVEL_VAR(mksSWB), \
   LOGLEVEL_VAR(mksVulkanRenderer), \
   LOGLEVEL_VAR(mksVulkanCmds), \
   LOGLEVEL_VAR(mksWinBSOD), \
   LOGLEVEL_VAR(mor), \
   LOGLEVEL_VAR(mstat), \
   LOGLEVEL_VAR(msvga), \
   LOGLEVEL_VAR(mvnc), \
   LOGLEVEL_VAR(namespaceDb), \
   LOGLEVEL_VAR(namespaceMgr), \
   LOGLEVEL_VAR(netPkt), \
   LOGLEVEL_VAR(numa), \
   LOGLEVEL_VAR(numaHost), \
   LOGLEVEL_VAR(nvdimm), \
   LOGLEVEL_VAR(nvme), \
   LOGLEVEL_VAR(nvramMgr), \
   LOGLEVEL_VAR(objc), /* lib/objc */ \
   LOGLEVEL_VAR(objlib), \
   LOGLEVEL_VAR(oemDevice), \
   LOGLEVEL_VAR(opNotification), \
   LOGLEVEL_VAR(oprom), \
   LOGLEVEL_VAR(ovhdmem), \
   LOGLEVEL_VAR(parallel), \
   LOGLEVEL_VAR(passthrough), \
   LOGLEVEL_VAR(pci), \
   LOGLEVEL_VAR(pcibridge), \
   LOGLEVEL_VAR(pci_e1000), \
   LOGLEVEL_VAR(pci_ehci), \
   LOGLEVEL_VAR(pci_hdaudio), \
   LOGLEVEL_VAR(pci_hyper), \
   LOGLEVEL_VAR(pciPassthru), \
   LOGLEVEL_VAR(pciPlugin), \
   LOGLEVEL_VAR(pci_scsi), \
   LOGLEVEL_VAR(pci_svga), \
   LOGLEVEL_VAR(pci_uhci), \
   LOGLEVEL_VAR(pci_vide), \
   LOGLEVEL_VAR(pci_vlance), \
   LOGLEVEL_VAR(pci_vmci), \
   LOGLEVEL_VAR(pci_vmxnet3), \
   LOGLEVEL_VAR(pci_xhci), \
   LOGLEVEL_VAR(pmemobj), \
   LOGLEVEL_VAR(policy), \
   LOGLEVEL_VAR(poll), \
   LOGLEVEL_VAR(precisionclock), \
   LOGLEVEL_VAR(promotedisk), \
   LOGLEVEL_VAR(pvnvram), \
   LOGLEVEL_VAR(pvscsi), \
   LOGLEVEL_VAR(qat), \
   LOGLEVEL_VAR(remoteDevice), \
   LOGLEVEL_VAR(replayVMX), \
   LOGLEVEL_VAR(sbx), \
   LOGLEVEL_VAR(scsi), \
   LOGLEVEL_VAR(secureBoot), \
   LOGLEVEL_VAR(serial), \
   LOGLEVEL_VAR(serviceImpl), /* lib/serviceImpl */ \
   LOGLEVEL_VAR(serviceUser), /* lib/serviceUser */ \
   LOGLEVEL_VAR(sg), /* lib/sg */ \
   LOGLEVEL_VAR(sgx), \
   LOGLEVEL_VAR(sgxmpa), \
   LOGLEVEL_VAR(sgxRegistrationTool), \
   LOGLEVEL_VAR(shader), \
   LOGLEVEL_VAR(sharedFolderMgr),  /* mks/remote/vdpFolderSharedMgrVmdb */ \
   LOGLEVEL_VAR(shim3D), \
   LOGLEVEL_VAR(slotfs), \
   LOGLEVEL_VAR(smbios), \
   LOGLEVEL_VAR(smc), \
   LOGLEVEL_VAR(smram), \
   LOGLEVEL_VAR(snapshot), \
   LOGLEVEL_VAR(sound), \
   LOGLEVEL_VAR(sparseChecker), \
   LOGLEVEL_VAR(ssl), \
   LOGLEVEL_VAR(state3d), \
   LOGLEVEL_VAR(stats), \
   LOGLEVEL_VAR(svga), \
   LOGLEVEL_VAR(svgadevtap), \
   LOGLEVEL_VAR(svga_rect), \
   LOGLEVEL_VAR(syncWaitQ), \
   LOGLEVEL_VAR(tarReader),\
   LOGLEVEL_VAR(timer), \
   LOGLEVEL_VAR(tools), \
   LOGLEVEL_VAR(toolsIso), \
   LOGLEVEL_VAR(toolsversion), \
   LOGLEVEL_VAR(tpm2emu), \
   LOGLEVEL_VAR(tpm2Verification), \
   LOGLEVEL_VAR(txt), \
   LOGLEVEL_VAR(udpfec),    /* lib/udpfec */ \
   LOGLEVEL_VAR(uhci), \
   LOGLEVEL_VAR(undopoint), \
   LOGLEVEL_VAR(unityMsg),  /* mks/remote/vdpUnityVmdb */ \
   LOGLEVEL_VAR(upitbe), \
   LOGLEVEL_VAR(upitd), \
   LOGLEVEL_VAR(usb), \
   LOGLEVEL_VAR(usb_xhci), \
   LOGLEVEL_VAR(util), \
   LOGLEVEL_VAR(uwt), /* lib/unityWindowTracker */ \
   LOGLEVEL_VAR(vaBasicOps), \
   LOGLEVEL_VAR(vcpuhotplug), \
   LOGLEVEL_VAR(vcpuNUMA), \
   LOGLEVEL_VAR(vdfs), \
   LOGLEVEL_VAR(vdfs_9p), \
   LOGLEVEL_VAR(vdpPlugin), \
   LOGLEVEL_VAR(vdtiPciCfgSpc), \
   LOGLEVEL_VAR(vflash), \
   LOGLEVEL_VAR(vga), \
   LOGLEVEL_VAR(vide), \
   LOGLEVEL_VAR(viewClient), \
   LOGLEVEL_VAR(vigor), \
   LOGLEVEL_VAR(viommu), \
   LOGLEVEL_VAR(vlance), \
   LOGLEVEL_VAR(vmcf), \
   LOGLEVEL_VAR(vmci), \
   LOGLEVEL_VAR(vmgenc), \
   LOGLEVEL_VAR(vmGL), \
   LOGLEVEL_VAR(vmhs), \
   LOGLEVEL_VAR(vmIPC), \
   LOGLEVEL_VAR(vmkcfg), \
   LOGLEVEL_VAR(vmkEvent), \
   LOGLEVEL_VAR(vmkmgmtlib), \
   LOGLEVEL_VAR(vmLock), \
   LOGLEVEL_VAR(vmmouse), \
   LOGLEVEL_VAR(vmname),  /* lib/vmname */ \
   LOGLEVEL_VAR(vmnetBridge), \
   LOGLEVEL_VAR(vmOvhd), \
   LOGLEVEL_VAR(vmUpsellController), \
   LOGLEVEL_VAR(vmva), \
   LOGLEVEL_VAR(vmWindowController), \
   LOGLEVEL_VAR(vmxnet), \
   LOGLEVEL_VAR(vmxnet3), \
   LOGLEVEL_VAR(vmxvmdbCallbacks), \
   LOGLEVEL_VAR(vncBlit),   \
   LOGLEVEL_VAR(vncDecode), \
   LOGLEVEL_VAR(vncEncode), \
   LOGLEVEL_VAR(vncRegEnc), \
   LOGLEVEL_VAR(vncServer), \
   LOGLEVEL_VAR(vncServerOS), \
   LOGLEVEL_VAR(vnet), \
   LOGLEVEL_VAR(vprobe), \
   LOGLEVEL_VAR(VProbeClient), \
   LOGLEVEL_VAR(vrdma), \
   LOGLEVEL_VAR(vsanobj), \
   LOGLEVEL_VAR(vsock), \
   LOGLEVEL_VAR(vsockProxy), \
   LOGLEVEL_VAR(vthread), \
   LOGLEVEL_VAR(vtpm), \
   LOGLEVEL_VAR(vui), \
   LOGLEVEL_VAR(vusbaudio), \
   LOGLEVEL_VAR(vusbccid), \
   LOGLEVEL_VAR(vusbhid), \
   LOGLEVEL_VAR(vusbkeyboard), \
   LOGLEVEL_VAR(vusbmouse), \
   LOGLEVEL_VAR(vusbrng),\
   LOGLEVEL_VAR(vusbtablet), \
   LOGLEVEL_VAR(vusbvideo),\
   LOGLEVEL_VAR(vvolbe), \
   LOGLEVEL_VAR(vvtd), \
   LOGLEVEL_VAR(vwdt), \
   LOGLEVEL_VAR(wifi), /* macWireless and wpa_supplicant */ \
   LOGLEVEL_VAR(win32util), \
   LOGLEVEL_VAR(worker), \
   LOGLEVEL_VAR(xpmode)

   /* end of list */

#endif /* _LOGLEVEL_USER_VARS_H_ */
