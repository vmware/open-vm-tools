#    CVE-2023-20867

A security issue in VMware Tools and open-vm-tools has been announced by VMware in security advisory [VMSA-2023-0013](https://www.vmware.com/security/advisories/VMSA-2023-0013.html).

The issue has been fixed in the open-vm-tools version 12.2.5 released on June 13, 2023.

The following patch provided to the open-vm-tools community can be used to apply the security fix to previous open-vm-tools releases.

## For releases 12.2.0, 12.1.5, 12.1.0, 12.0.5, 12.0.0, 11.3.5, 11.3.0


*   **[2023-20867-Remove-some-dead-code.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-20867.patch/2023-20867-Remove-some-dead-code.patch)**


## For releases 11.1.0, 11.1.5, 11.2.0, 11.2.5


*   **[2023-20867-Remove-some-dead-code-1110-1125.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-20867.patch/2023-20867-Remove-some-dead-code-1110-1125.patch)**


## For releases 11.0.0, 11.0.5


*   **[2023-20867-Remove-some-dead-code-1100-1105.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-20867.patch/2023-20867-Remove-some-dead-code-1100-1105.patch)**


## For releases 10.3.0, 10.3.5, 10.3.10


*   **[2023-20867-Remove-some-dead-code-1030-10310.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-20867.patch/2023-20867-Remove-some-dead-code-1030-10310.patch)**


The patches have been tested against the above open-vm-tools releases.  Each applies cleanly with: 

    git am        for a git repository.
    patch -p2     in the top directory of an open-vm-tools source tree. 

 
