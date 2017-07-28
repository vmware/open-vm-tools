# open-vm-tools 10.1.10 Release Notes

## What's in the Release Notes

The release notes cover the following topics:

*   What's New
*   Internationalization
*   Compatibility Notes
*   Resolved Issues
*   Known Issues from Earlier Releases

## What's New

open-vm-tools is a suite of utilities that enhances the performance of the virtual machine's guest operating system and improves management of the virtual machine.

*   **Resolved Issues**: This release of open-vm-tools 10.1.5 addresses issues that have been documented in the Resolved Issues section.

## Internationalization

open-vm-tools 10.1.10 is available in the following languages:

*   English
*   French
*   German
*   Spanish
*   Italian
*   Japanese
*   Korean
*   Simplified Chinese
*   Traditional Chinese

## Compatibility

Please refer to [VMware Compatibility Guide](http://www.vmware.com/resources/compatibility/search.php) for compatibility related information.

## Resolved Issues

*   **Fix for CVE-2015-5191**

    Open VMware Tools (CVE-2015-5191) contained multiple file system races in libDeployPkg, related to the use of hard-coded paths under /tmp.

    Successful exploitation may result in a local privilege escalation. The impact of this vulnerability is low for distributions which have enabled PrivateTmp for the affected service.

    We would like to thank Florian Weimer and Kurt Seifried of Red Hat Product Security for reporting this issue to us.

## Known Issues from Earlier Releases

*   **Common Agent Framework in open-vm-tools fails to build in Ubuntu 14.04**  
    Common Agent Framework in open-vm-tools fails to build in Ubuntu 14.04 with <tt>rabbitmq-c</tt> version lower than 0.8.0  

    Workaround: Upgrade <tt>rabbitmq-c</tt> to version 0.8.0 or later for TLS1.2 support in Common Agent Framework.

