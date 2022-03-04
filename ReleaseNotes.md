#                Open-vm-tools 12.0.0 Release Notes

Updated:  3 MAR 2022

Open-vm-tools | 01 MAR 2022 | Build 19345655

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [Internationalization](#i18n)
*   [End of Feature Support Notice](#endoffeaturesupport)
*   [Compatibility Notes](#compatibility)
*   [Guest Operating System Customization Support](#guestop)
*   [Interoperability Matrix](#interop)
*   [Resolved Issues](#resolvedissues)
*   [Known Issues](#knownissues)

## <a id="whatsnew" name="whatsnew"></a>What's New

*   Support for managing Salt Minion through guest variables. See [Enable Salt Minion Using VMware Tools](https://docs.vmware.com/en/VMware-Tools/12.0.0/com.vmware.vsphere.vmwaretools.doc/GUID-373CD922-AF80-4B76-B19B-17F83B8B0972.html).
*   Support for gathering and publishing a list of containers running inside Linux guests. See [Configure ContainerInfo for Linux](https://docs.vmware.com/en/VMware-Tools/12.0.0/com.vmware.vsphere.vmwaretools.doc/GUID-82490A5C-014C-46D9-815A-18B1C9E5312C.html).

To implement these two features, open-vm-tools 12.0.0 release introduces an optional setup script and two plugins (one optional)

 * [Salt Minion Setup](#saltminion)
 * [Component Manager plugin](#compmgr)
 * [ContainerInfo plugin (optional)](#continfo)

### <a id="saltminion" name="saltminion"></a>Salt Minion Setup
The Salt support on Linux consists of a single bash script to setup Salt Minion on VMware virtual machines.  The script requires the "curl" and "awk" commands to be available on the system.

Linux providers supplying open-vm-tools packages are recommended to provide Salt Minion support in a separate optional package - "open-vm-tools-salt-minion".

To include the Salt Minion Setup in the open-vm-tools build use the `--enable-salt-minion` option when invoking the configure script.
```
./configure --enable-salt-minion
```

### <a id="compmgr" name="compmgr"></a>Component Manager (componentMgr) plugin
The component Manager manages a preconfigured set of components available from VMware that can be made available on the Linux guest.  Currently the only component that can be managed is the Salt Minion Setup.

### <a id="continfo" name="continfo"></a>ContainerInfo (containerInfo) plugin
The optional containerInfo plugin retrieves a list of the containers running on a Linux guest and publishes the list to the guest variable "**guestinfo.vmtools.containerinfo**" in JSON format.  The containerInfo plugin communicates with the containerd daemon using gRPC to retrieve the desired information.  For containers that are managed by Docker, the plugin uses libcurl to communicate with the Docker daemon and get the names of the containers.

Since this plugin requires additional build and runtime dependencies, Linux vendors are recommended to release it in a separate, optional package - "open-vm-tools-containerinfo".  This avoids unnecessary dependencies for customers not using the feature.

#### Canonical, Debian, Ubuntu Linux
| Build Dependencies | Runtime |
|:------------------------:|:----------------:|
| `libcurl4-openssl-dev` | `curl` |
| `protobuf-compiler` | `protobuf` |
| `libprotobuf-dev` | `grpc++` |
| `protobuf-compiler-grpc` |
| `libgrpc++-dev` |
| `golang-github-containerd-containerd-dev` |
| `golang-github-gogo-protobuf-dev` |

#### Fedora, Red Hat Enterprise Linux, ...
| Build Dependencies | Runtime |
|:------------------------:|:----------------:|
| `libcurl-devel` | `curl` |
| `protobuf-compiler` | `protobuf` |
| `protobuf-devel` | `grpc-cpp` |
| `grpc-plugins` |
| `grpc-devel` |
| `containerd-devel` |


#### Configuring the build for the ContainerInfo plugin
The configure script defaults to building the ContainerInfo when all the needed dependencies are available.  ContainerInfo will not be built if there are missing dependencies.  Invoke the configure script with `--enable-containerinfo=no` to explicitly inhibit building the plugin.
```
./configure --enable-containerinfo=no
```
If the configure script is given the option `--enable-containerinfo=yes` and any necessary dependency is not available, the configure script will terminate with an error.
```
./configure --enable-containerinfo=yes
```



## Internationalization

Open-vm-tools 12.0.0 is available in the following languages:

*   English
*   French
*   German
*   Spanish
*   Italian
*   Japanese
*   Korean
*   Simplified Chinese
*   Traditional Chinese

## <a id="endoffeaturesupport" name="endoffeaturesupport"></a>End of Feature Support Notice

*   The tar tools (linux.iso) and OSPs shipped with VMware Tools 10.3.x release will continue to be supported. However, releases after VMware Tools 10.3.5 will only include critical and security fixes. No new feature support will be provided in these types of VMware Tools (tar tools and OSP's). It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information about open-vm-tools, see [https://kb.vmware.com/s/article/2073803](https://kb.vmware.com/s/article/2073803).

## <a id="guestop" name="guestop"></a>Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

## <a id="interop" name="interop"></a>Interoperability Matrix

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products.

## <a id="resolvedissues" name ="resolvedissues"></a> Resolved Issues

*   **Open-vm-tools can be built with either libfuse2 or libfuse3.**

    The open-vm-tools source has been updated to work with either the Fuse 2.x or Fuse 3.x software versions.  The `configure.ac` file has been modified to accept the following options which allow users to specify which version to use.  While the auto-options allow experimentation, select a specific version of fuse when building packages for distribution.

	| Option |    |
	|:--------------|:-----------------------|
	| **--without-fuse** | vmblock-fuse and vmhgfs-fuse will be disabled|
	| **--with-fuse=fuse3\|3** | use Fuse 3.x |
	| **--with-fuse=fuse\|2** | use Fuse 2.x |
	| **--with-fuse=auto** | check for Fuse 3 or Fuse 2 availability; disable vmblock-fuse and vmhgfs-fuse if unavailable |
	| **--with-fuse** | implicit "yes" |
	| **--with-fuse=yes** | check for Fuse 3 or Fuse 2 availability; disable vmblock-fuse and vmhgfs-fuse if unavailable |

*   A number of Coverity and Codacy reported issues have been addressed.

*   **The following issues and pull requests reported on [github.com/vmware/open-vm-tools](https://github.com/vmware/open-vm-tools)  have been addressed:**

    -   [Issue # 128](https://github.com/vmware/open-vm-tools/issues/128)
    -   [Issue # 314](https://github.com/vmware/open-vm-tools/issues/314)
    -   [Pull  # 513](https://github.com/vmware/open-vm-tools/pull/513)
    -   [Pull  # 544](https://github.com/vmware/open-vm-tools/pull/544)
    -   [Pull  # 573](https://github.com/vmware/open-vm-tools/pull/573)

*   **A complete list of the granular changes that are in the open-vm-tools 11.3.0 release is available at:**

    [Open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-12.0.0/open-vm-tools/ChangeLog)

## <a id="knownissues" name="knownissues"></a>Known Issues

The known issues are grouped as follows.

*   [Open-vm-tools Issues](#vmware-tools-issues-known)
*   [Open-vm-tools Issues in VMware Workstation or Fusion](#vmware-tools-issues-in-vmware-workstation-or-fusion-known)

**<a id="vmware-tools-issues-known" name="vmware-tools-issues-known"></a>Open-vm-tools Issues**

*   **[FTBFS] glibc 2.35 and GCC 11 & 12 reporting possible array bounds overflow - bora/lib/asyncsocket/asyncksocket.c: AsyncTCPSocketPollWork()**

    Reported in [open-vm-tools issue #570](https://github.com/vmware/open-vm-tools/issues/570), the fix did not make the code freeze date.   The fix is available in the open-vm-tools "devel" branch at [commit de6d129](https://github.com/vmware/open-vm-tools/commit/de6d129476724668b8903e2a87654f50ba21b1b2) and can be applied directly to the 12.0.0, 11.3.5 and 11.3.0 open-vm-tools sources.

    The fix will be in the next open-vm-tools source release.

**<a id="vmware-tools-issues-in-vmware-workstation-or-fusion-known" name="vmware-tools-issues-in-vmware-workstation-or-fusion-known"></a>Open-vm-tools Issues in VMware Workstation or Fusion**

*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>

