# open-vm-tools 10.1.15 Release Notes

## What's in the Release Notes

The release notes cover the following topics:

*   What's New
*   Internationalization
*   Compatibility Notes
*   Resolved Issues
*   Known Issues from Earlier Releases

## What's New

open-vm-tools is a suite of utilities that enhances the performance of the virtual machine's guest operating system and improves management of the virtual machine.

*   **Resolved Issues**: This release of open-vm-tools 10.1.15 addresses issues that have been documented in the Resolved Issues section.

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

Nothing applicable to open-vm-tools.

## Known Issues

*   **VMware Tools service stops running in the virtual machine**

    When tools.conf configuration file is configured with an entry similar to the following, without specifying the filename, VMware Tools service might stop running with a segmentation fault.

    ```
    [logging]
    vmsvc.handler=file
    ```

    To work around this issue, set a file name. For example,

    ```vmsvc.data=/var/log/vmware-vmsvc.log```

## Known Issues from Earlier Releases

*   **Common Agent Framework in open-vm-tools fails to build in Ubuntu 14.04**  
    Common Agent Framework in open-vm-tools fails to build in Ubuntu 14.04 with <tt>rabbitmq-c</tt> version lower than 0.8.0  

    Workaround: Upgrade <tt>rabbitmq-c</tt> to version 0.8.0 or later for TLS1.2 support in Common Agent Framework.

