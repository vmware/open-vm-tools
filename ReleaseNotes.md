#                      open-vm-tools 13.0.0 Release Notes

Updated on: 17 June 2025

open-vm-tools | 17 JUNE 2025 | Build 24696409

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

* [What's New](#whatsnew) 
* [Internationalization](#i18n) 
* [Product Support Notice](#suppnote)
* [Guest Operating System Customization Support](#guestop) 
* [Interoperability Matrix](#interop) 
* [Resolved Issues](#resolvedissues) 
* [Known Issues](#knownissues)

## <a id="whatsnew" name="whatsnew"></a>What's New

*   The vm-support script has been improved (version 0.98).

    To aid in triaging open-vm-tools issues, the vm-support script has been updated to:
      * now collect all current open-vm-tools log files as configured in the [logging] section of tools.conf.
      * collect one month of information from the systemd journal.

*   Please see the [Resolved Issues](#resolvedissues) and [Known Issues](#knownissues) sections below.

*   A complete list of the granular changes in the open-vm-tools 13.0.0 release is available at:

    [open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-13.0.0/open-vm-tools/ChangeLog)

## <a id="i18n" name="i18n"></a>Internationalization

open-vm-tools 13.0.0 is available in the following languages:

* English
* French
* Japanese
* Spanish

## <a id="guestop" name="guestop"></a>Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](https://compatibilityguide.broadcom.com/search?program=software&persona=live&customization=Guest+Customization&column=osVendors&order=asc) provides details about the guest operating systems supported for customization.


## <a id="interop" name="interop"></a>Interoperability Matrix

The [Broadcom Product Interoperability Matrix](https://interopmatrix.broadcom.com/Interoperability) provides details about the compatibility of current and earlier versions of VMware Products. 

## <a id="resolvedissues" name ="resolvedissues"></a> Resolved Issues

*   **The following github.com/vmware/open-vm-tools pull requests and issues has been addressed.**

    * FTBFS: --std=c23 conflicting types between function definition and declaration MXUserTryAcquireForceFail()

      [Fixes Issue #750](https://github.com/vmware/open-vm-tools/issues/750)<br>
      [Pull request #751](https://github.com/vmware/open-vm-tools/pull/751)

    * Provide tools.conf settings to deactivate one-time and periodic time synchronization

      The new tools.conf settings `disable-all` and `disable-periodic` allow the guest OS administrator to deactivate one-time and periodic time synchronization without rebooting the VM or restarting the guest OS.

      [Fixes Issue #302](https://github.com/vmware/open-vm-tools/issues/302)

    * Fix xmlsec detection when cross-compiling with pkg-config

      [Pull request #732](https://github.com/vmware/open-vm-tools/pull/732)

*   **After October 25, 2024, with open-vm-tools earlier than 13.0.0, the salt-minion component is not installed or fails to install in a guest operating system through the VMware Component Manager**

    When you configure the salt-minion component in the present state, its last status is set to 102 (not installed) or 103 (installation failed), never reaching the installed state 100.

    * The VM advanced setting with the key "guestinfo./vmware.components.salt_minion.desiredstate" has a value present.
    * The VM advanced setting with the key "guestinfo.vmware.components.salt_minion.laststatus" has a value 102 or 103.

    The salt-minion component installs a log file with traces indicating failure to access the online salt repository on https://repo.saltproject.io.  The "vmware-svtminion.sh-install-*.log" file for the failed install shows a trace similar to:

    ```
    <date+time> INFO: /usr/lib64/open-vm-tools/componentMgr/saltMinion/svtminion.sh:_curl_download attempting download of file 'repo.json'
    <date+time> WARNING: /usr/lib64/open-vm-tools/componentMgr/saltMinion/svtminion.sh:_curl_download failed to download file 'repo.json' from 'https://repo.saltproject.io/salt/py3/onedir/repo.json' on '0' attempt, retcode '6' 
    <date+time> WARNING: /usr/lib64/open-vm-tools/componentMgr/saltMinion/svtminion.sh:_curl_download failed to download file 'repo.json' from 'https://repo.saltproject.io/salt/py3/onedir/repo.json' on '1' attempt, retcode '6' 
    <date+time> WARNING: /usr/lib64/open-vm-tools/componentMgr/saltMinion/svtminion.sh:_curl_download failed to download file 'repo.json' from 'https://repo.saltproject.io/salt/py3/onedir/repo.json' on '2' attempt, retcode '6' 
    <date+time> WARNING: /usr/lib64/open-vm-tools/componentMgr/saltMinion/svtminion.sh:_curl_download failed to download file 'repo.json' from 'https://repo.saltproject.io/salt/py3/onedir/repo.json' on '3' attempt, retcode '6' 
    <date+time> WARNING: /usr/lib64/open-vm-tools/componentMgr/saltMinion/svtminion.sh:_curl_download failed to download file 'repo.json' from 'https://repo.saltproject.io/salt/py3/onedir/repo.json' on '4' attempt, retcode '6' 
    <date+time> ERROR: /usr/lib64/open-vm-tools/componentMgr/saltMinion/svtminion.sh:_curl_download failed to download file 'repo.json' from 'https://repo.saltproject.io/salt/py3/onedir/repo.json' after '5' attempts
    ```

    This issue is resolved in this release.

    The new versions of the salt-minion integration scripts supporting the new Salt Project repository locations are available at:

    * [https://packages.broadcom.com/artifactory/saltproject-generic/onedir/](https://packages.broadcom.com/artifactory/saltproject-generic/onedir/)

## <a id="knownissues" name="knownissues"></a>Known Issues

*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Note: This issue is applicable to open-vm-tools running on VMware Workstation and VMware Fusion.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>

    For more information on how to configure VMware Tools Shared Folders, see [KB 60262](https://knowledge.broadcom.com/external/article?legacyId=60262)
