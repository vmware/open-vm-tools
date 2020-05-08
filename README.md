# General
## What is the open-vm-tools project?
open-vm-tools is a set of services and modules that enable several features in VMware products for better management of, and seamless user interactions with, guests. It includes kernel modules for enhancing the performance of virtual machines running Linux or other VMware supported Unix like guest operating systems. 
 
open-vm-tools enables the following features in VMware products:

- The ability to perform virtual machine power operations gracefully.
- Execution of VMware provided or user configured scripts in guests during various power operations.
- The ability to run programs, commands and file system operation in guests to enhance guest automation.
- Authentication for guest operations. 
- Periodic collection of network, disk, and memory usage information from the guest.
- Generation of heartbeat from guests to hosts so VMware's HA solution can determine guests' availability.
- Clock synchronization between guests and hosts or client desktops.
- Quiescing guest file systems to allow hosts to capture file-system-consistent guest snapshots.
- Execution of pre-freeze and post-thaw scripts while quiescing guest file systems.
- The ability to customize guest operating systems immediately after powering on virtual machines.
- Enabling shared folders between host and guest file systems on VMware Workstation and VMware Fusion.
- Copying and pasting text, graphics, and files between guests and hosts or client desktops.

## Can you provide more details on the actual code being released?
The following components have been released as open source software:
- Linux, Solaris and FreeBSD drivers for various devices and file system access.
- The memory balloon driver for reclaiming memory from guests.
- The PowerOps plugin to perform graceful power operation and run power scripts.
- The VIX plugin to run programs and commands, and perform file system operations in guests.
- The GuestInfo plugin to periodically collect various statistics from guests.
- The TimeSync plugin to perform time synchronization.
- The dndcp plugin to support drag and drop, and text and file copy/paste operations.
- The ResolutionSet plugin to adjust guest screen resolutions automatically based on window sizes.
- The guest authentication service.
- The toolbox command to perform disk wiping and shrinking, manage power scripts, and time synchronization.
- The guest SDK libraries to provide information about virtual machines to guests.
- Clients and servers for shared folders support.
- Multiple monitor support.
- The GTK Toolbox UI.
 
## Is open-vm-tools available with Linux distributions?
Yes. open-vm-tools packages for user space components are available with new versions of major Linux distributions, and are installed as part of the OS installation in several cases. Please refer to VMware KB article http://kb.vmware.com/kb/2073803 for details. All leading Linux vendors support open-vm-tools and bundle it with their products. For information about OS compatibility for open-vm-tools, see the 
VMware Compatibility Guide at http://www.vmware.com/resources/compatibility
Automatic installation of open-vm-tools along with the OS installation eliminates the need to separately install open-vm-tools in guests. If open-vm-tools is not installed automatically, you may be able to manually install it from the guest OS vendor's public repository. Installing open-vm-tools from the Linux vendor's repository reduces virtual machine downtime because future updates to open-vm-tools are included with the OS maintenance patches and updates.
**NOTE**: Most of the Linux distributions ship two or more open-vm-tools packages. "open-vm-tools" is the core package without any dependencies on X libraries and "open-vm-tools-desktop" is an additional package with dependencies on "open-vm-tools" core package and X libraries. The "open-vm-tools-sdmp" package contains a plugin for Service Discovery. There may be additional packages, please refer to the documentation of the OS vendor. Note that the open-vm-tools packages available with Linux distributions do not include Linux drivers because Linux drivers are available as part of Linux kernel itself. Linux kernel versions 3.10 and later include all of the Linux drivers present in open-vm-tools except the vmhgfs driver. The vmhgfs driver was required for enabling shared folders feature, but is superseded by vmhgfs-fuse which does not require a kernel driver.

## Will there be continued support for VMware Tools and OSP? 
VMware Tools will continue to be available under a commercial license. It is recommended that open-vm-tools be used for the Linux distributions where open-vm-tools is available. VMware will not provide OSPs for operating systems where open-vm-tools is available.

## How does this benefit other open source projects?
Under the terms of the GPL, open source community members are able to use the open-vm-tools code to develop their own applications, extend it, and contribute to the community. They can also incorporate some or all of the code into their projects, provided they comply with the terms of the GPL.

# License Related
## What license is the code being released under?
The code is being released under GPL v2 and GPL v2 compatible licenses. To be more specific, the Linux kernel modules are being released under the GPL v2, while almost all of the user level components are being released under the LGPL v2.1. The SVGA and mouse drivers have been available under the X11 license for quite some time. There are certain third party components released under BSD style licenses, to which VMware has in some cases contributed, and will continue to distribute with open-vm-tools.
 
## Why did you choose these licenses?
We chose the GPL v2 for the kernel components to be consistent with the Linux kernel's license. We chose the LGPL v2.1 for the user level components because some of the code is implemented as shared libraries and we do not wish to restrict proprietary code from linking against those libraries. For consistency, we decided to license the rest of the userlevel code under the LGPL v2.1 as well.

## What are the obligations that the license(s) impose?
Each of these licenses have different obligations.
For questions about the GPL, LGPL licenses, the Free Software Foundation's GPL FAQ page provides lots of useful information. 
For questions about the other licenses like the X11, BSD licenses, the Open Source Initiative has numerous useful resources including mailing lists. 
The Software Freedom Law Center provides legal expertise and consulting for free and open source software (FOSS) developers.

## Can I use all or part of this code in my proprietary software? Do I have to release the source code if I do?
Different open source licenses have different requirements regarding the release of source code. Since the code is being released under various open source licenses, you will need to comply with the terms of the corresponding licenses.

## Am I required to contribute back any changes I make to the code?
No, you aren't required to contribute any changes that you make back to the open-vm-tools project. However, we encourage you to do so.

## Can I use all or part of this code in another open source package?
Yes, as long as you comply with the appropriate license(s).
 
## Can I package this for my favorite operating system?
Yes! Please do. 

## Will the commercial version (VMware Tools) differ from the open source version (open-vm-tools)? If so, how?
Our goal is to work towards making the open source version as close to the commercial version as possible. However, we do currently make use of certain components licensed from third parties as well as components from other VMware products which are only available in binary form.

## If I use the code from the open-vm-tools project in my project/product, can I call my project/product VMware Tools?
No, since your project/product is not a VMware project/product.

# Building open-vm-tools
## How do I build open-vm-tools?
open-vm-tools uses the GNU Automake tool for generating Makefiles to build all sources. More information about Automake can be found here: http://www.gnu.org/software/automake/
## Project build information:
The following steps will work on most recent Linux distributions:
```
autoreconf -i
./configure
make
sudo make install
sudo ldconfig
```

To build the optional sdmp (Service Discovery) plugin use the `--enable-servicediscovery` option to invoke the configure script:
```
./configure --enable-servicediscovery
```

## Getting configure options and help
If you are looking for help or additional settings for the building of this project, the following configure command will display a list of help options:
```
./configure --help
```
When using configure in the steps above it is only necessary to call ./configure once unless there was a problem after the first invocation.

# Getting Involved
## How can I get involved today?
You can get involved today in several different ways:
- Start using open-vm-tools today and give us feedback.
- Suggest feature enhancements.
- Identify and submit bugs under issues section: https://github.com/vmware/open-vm-tools/issues
- Start porting the code to other operating systems.   Here is the list of operating systems with open-vm-tools:

  * Red Hat Enterprise Linux 7.0 and later releases
  * SUSE Linux Enterprise 12 and later releases
  * Ubuntu 14.04 and later releases
  * CentOS 7 and later releases
  * Debian 7.x and later releases
  * Oracle Linux 7 and later 
  * Fedora 19 and later releases
  * openSUSE 11.x and later releases
 
## Will external developers be allowed to become committers to the project?
Yes. Initially, VMware engineers will be the only committers. As we roll out our development infrastructure, we will be looking to add external committers to the project as well.

## How can I submit code changes like bug fixes, patches, new features to the project?
Initially, you can submit bug fixes, patches and new features to the project development mailing list as attachments to emails or bug reports. To contribute source code, you will need to fill out a contribution agreement form as part of the submission process. We will have more details on this process shortly.

## What is the governance model for managing this as an open source project?
The feature roadmap and schedules for the open-vm-tools project will continue to be defined by VMware. Initially, VMware engineers will be the only approved committers. We will review incoming submissions for suitability for merging into the project. We will be looking to add community committers to the project based on their demonstrated contributions to the project. Finally, we also plan to set up a process for enhancement proposals, establishing sub-projects and so on.

## Will you ship code that I contribute with VMware products? If so, will I get credit for my contributions?
Contributions that are accepted into the open-vm-tools project's main source tree will likely be a part of VMware Tools. We also recognize the value of attribution and value your contributions. Consequently, we will acknowledge contributions from the community that are distributed with VMware's products.

## Do I need to sign something before making a contribution?
Yes. We have a standard contribution agreement that covers all contributions made to the project. It gives VMware and you joint copyright interests in the code you are contributing. The agreement also gives VMware flexibility with licensing and also helps avoid any copyright/licensing related issues that may arise in the future. In order for us to include your contribution in our source tree, we ask that you send us a signed copy of the agreement. You can do this in one of two ways:
Fax to +1.650.427.5003, Attn: Product & Technology Law Group
Scan and email it to oss-queries_at_vmware.com
Agreement: http://open-vm-tools.sourceforge.net/files/vca.pdf

# Compatibilty

## What Operating Systems are supported for customization?
The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

## Which versions of open-vm-tools are compatible with other VMware products?

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of different versions of VMware Tools (includes open-vm-tools) and other VMware Products.

# Internationalization
## Which languages are supported?

open-vm-tools supports the following languages:
- English
- French
- German
- Spanish
- Italian
- Japanese
- Korean
- Simplified Chinese
- Traditional Chinese

# Other
## Mailing Lists
Please send an email to one of these mailing lists based on the nature of your question.
- Development related questions : open-vm-tools-devel@lists.sourceforge.net
- Miscellaneous questions: open-vm-tools-discuss@lists.sourceforge.net
- General project announcements: open-vm-tools-announce@lists.sourceforge.net
