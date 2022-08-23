#    CVE-2022-31676

A security issue in VMware Tools and open-vm-tools has been announced by VMware in a security advisory [VMSA-2022-0024](https://www.vmware.com/security/advisories/VMSA-2022-0024.html).

The issue has been fixed in the open-vm-tools release 12.1.0 made on August 23, 2022.

The following patches provided to the open-vm-tools community can be used to apply the security fix to previous open-vm-tools releases.

## For releases 12.0.5, 12.0.0, 11.3.5, 11.3.0 


*   **[1205-Properly-check-authorization-on-incoming-guestOps-re.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2022-31676.patch/1205-Properly-check-authorization-on-incoming-guestOps-re.patch)**


## For releases 11.2.5, 11.2.0, 11.1.5, 11.1.0, 11.0.5, 11.0.1, 11.0.0 
 

*   **[1125-Properly-check-authorization-on-incoming-guestOps-re.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2022-31676.patch/1125-Properly-check-authorization-on-incoming-guestOps-re.patch)**

 
## For releases 10.3.10, 10.3.5, 10.3.0 

*   **[10310-Properly-check-authorization-on-incoming-guestOps-r.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2022-31676.patch/10310-Properly-check-authorization-on-incoming-guestOps-r.patch)**


 

The patches have been tested against both ends of the corresponding release ranges shown. Each applies cleanly with: 

    git am       for a git repository 
    patch -p1     in the top directory of an open-vm-tools source tree. 

 
