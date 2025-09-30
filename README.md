#     Handle-new-cloud-init-error-code

Reported in [OVT issue #768](https://github.com/vmware/open-vm-tools/issues/768), the current Guest OS Customization treated the new cloud-init exit code introduced in cloud-init v23.4 as a failure.

```
   0 - success
   1 - unrecoverable error
   2 - recoverable error
```

This patch to "libDeployPkg/linuxDeployment.c" will accept an exit code of 2 as a recoverable error.  The DeployPackage plugin will wait for cloud-init execution as long as it's status is "running" before triggering a reboot.

This fix is targeted for the next minor release of open-vm-tools.

The following patch provided to the open-vm-tools community can be used to apply the this fix to previous open-vm-tools releases.


## For releases 12.1.5 through 13.0.5


*   **[0001-Handle-new-cloud-init-error-code.patch](https://github.com/vmware/open-vm-tools/blob/Handle-new-cloud-init-error-code.patch/0001-Handle-new-cloud-init-error-code.patch)**


## For releases 12.1.0 and earlier


*   Please open an issue at https://github.com/vmware/open-vm-tools/issues and indicate the specific open-vm-tools release needed.


The patch has been tested against the above open-vm-tools releases.  It applies cleanly with: 

    git am        for a git repository.
    patch -p2     in the top directory of an open-vm-tools source tree. 

 
