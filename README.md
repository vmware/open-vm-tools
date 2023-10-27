#    CVE-2023-34059

A security issue in the vmware-user-suid-wrapper in open-vm-tools has been announced by VMware in CVE-2023-34059.

open-vm-tools contains a file descriptor hijack vulnerability in the vmware-user-suid-wrapper command.  A malicious actor with non-root privileges may be able to hijack the /dev/uinput file descriptor allowing them to simulate user inputs.

The issue has been fixed in open-vm-tools 12.3.5 released on October 26, 2023.

The following patch provided to the open-vm-tools community can be used to apply the security fix to previous open-vm-tools releases.

## For all open-vm-tools releases 11.0.0 through 12.3.0


*   **[CVE-2023-34059.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-34059.patch/CVE-2023-34059.patch)**

 

The patch has been tested against all open-vm-tools releases in the range shown.  Each applies cleanly with: 

    git am        for a git repository 
    patch -p1     in the top directory of an open-vm-tools source tree. 

 
