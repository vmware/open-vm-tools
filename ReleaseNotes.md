# open-vm-tools 10.1.5 Release Notes

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

open-vm-tools 10.1.5 is available in the following languages:

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

*   **Authentication failure is reported as unknown general system error.**  
    Attempts to authenticate through <tt>VGAuth</tt> service might result in an authentication-specific error such as an expired account or password. The authentication-specific error might then be incorrectly reported as an unknown general system error, similar to the following:  

    <tt>CommunicationException: Failed to create temp file on target <IP_ADDRESS>: A general system error occurred: Unknown error</tt>

    This issue is resolved in this release.

*   **Unable to backup virtual machines with active Docker containers**.  

    Attempts to take quiesced snapshots may fail on RHEL 7 guest operating systems that are running Docker containers. Docker version 1.12.x and later create special mount points for containers. These mount points are recorded as 'net:[NUMBER]' instead of absolute paths in <tt>/proc/self/mounts</tt> on the system.

    Note: To see the issue tracked by Red Hat, see [Red Hat Bugzilla.](https://bugzilla.redhat.com/show_bug.cgi?id=1418962)

    This issue is resolved in this release.

## Known Issues from Earlier Releases

*   **Common Agent Framework in open-vm-tools fails to build in Ubuntu 14.04**  
    Common Agent Framework in open-vm-tools fails to build in Ubuntu 14.04 with <tt>rabbitmq-c</tt> version lower than 0.8.0  

    Workaround: Upgrade <tt>rabbitmq-c</tt> to version 0.8.0 or later for TLS1.2 support in Common Agent Framework.

