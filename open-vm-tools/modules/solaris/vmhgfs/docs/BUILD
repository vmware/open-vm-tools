Solaris Hgfs Build Instructions
===============================

Last Updated: 11/22/2005 (biggadike@vmware.com)

Hgfs Kernel Module
------------------
0. One-time Guest Operations
1. Building with the Linux cross-compiler (Solaris 9)
2. Building from the Guest (Solaris 10)

Tools
-----
0. Building guestd and the toolbox
1. Running guestd
2. Running the toolbox


Hgfs Kernel Module
------------------

 0. One-time Guest Operations
 -------------------------
 Requirements: just a Solaris 9 or 10 guest

 o First, to build you'll need to make sure gmake, gcc, and binutils are
   installed.  The easiest way to install these is to use the pkg-get script
   from http://www.blastwave.org/pkg-get.php.  Follow the instructions there
   then do:

   # pkg-get -U
   # pkg-get -i gmake gcc2 binutils   
   # export PATH=$PATH:/opt/csw/bin     (for Bash)
   # alias make='gmake'                 (for Bash)

 o You'll need to add an entry to the device link table so that when the driver
   is loaded a symlink called /dev/vmware-hgfs is created that points to the
   entry in the Kernel's device tree (/devices).

   # cat docs/append-to-devlink.tab >> /etc/devlink.tab

 o You'll also need a copy of the driver's configuration file where the kernel
   looks for it.

   # cp docs/hgfs.conf /usr/kernel/drv

 o You also need to create the directory where guestd will mount the
   filesystem:

   # mkdir -p /mnt/hgfs

 These only need to be done once.


 1. Building with the Linux cross-compiler (Solaris 9)
 -----------------------------------------------------
 
 Note that the cross-compiler is currently only for Solaris 9.

 o From top of bora-vmsoft:
   
   # make tools-for-solaris

 o Respected build variables:
   VMX86_DEVEL : gets passed on to code
   VMX86_DEBUG : enables assertion checking
   VMX86_LOG   : enables logging
   SOL_VERSION : defaults to 9 because the Solaris 10 cross-compiler doesn't
                 exist yet

 o Installing Targets (run these from the guest):
   rem         : removes the driver from the system
   install     : installs the driver and driver config file to the proper
                 directories
   add         : adds the driver to the system
   rotate      : invokes rem, install, then add.

 Note that most of the time rotate will take care of everything for you, but if
 guestd is running this may cause problems because the driver will be added
 even if the module cannot be removed.  Just "kill" guestd then "make rotate".


 2. Building from the Guest (Solaris 10)
 ---------------------------------------

 Note that this is currently the only way to build in Solaris 10.

 o Make sure the BORA, BUILD, and BUILDROOT variables are set properly in the
   Makefile.guest.

   It's likely all you need know is:
    make -f Makefile.guest module
    make -f Makefile.guest install add

   If you want to disable debugging, use:
    make -f Makefile.guest module DEBUG=OFF

   If you get a "module already loaded" error, try:
    make -f Makefile.guest rotate
 
 o Building targets:
   module      : builds only the Kernel module (current default)
   clean       : cleans build files
   
 o Building variables:
   DEBUG=OFF  : turns off all debugging output from the module (faster!)
   ASSERT=OFF  : turns off ASSERT()s
   SOL_VERSION : defaults to 10 and automatically determines which build number
                 this is with uname -v (52 and 58 are the only builds tested)

 o Installing Targets (run these from the guest):
   rem         : removes the driver from the system
   install     : installs the driver and driver config file to the proper
                 directories
   add         : adds the driver to the system
   rotate      : invokes rem, install, then add.
   once        : appends necessary lines to /etc/devlink.tab (only needed once)
   initbuilddir: creates the build directory
 

Tools
-----

 0. Building guestd and the toolbox
 ------------------------------

 o guestd has been ported to Solaris and is built using the Solaris 9 cross
   compiler mentioned above.  The toolbox has not been ported to Solaris and is
   run using lxrun.  Building both is handled by the Makefiles in bora-vmsoft:

   # cd /path/to/bora-vmsoft
   # make tools-for-solaris

   The binaries will be located at $(BUILD)/obj/ws/tools-for-solaris/guestd/guestd
   and $(BUILD)/obj/ws/tools-for-solaris/toolbox/toolbox.

 1. Running guestd
 -----------------

 o guestd runs natively on Solaris 9.  It will mount Hgfs at /mnt/hgfs and your
   "Shared Folders" will be visible there.

 o I have not been able to use the bora-vmsoft Makefile infrastructure from
   within a Solaris 10 guest, so it is not currently possible to build guestd
   for Solaris 10.  Porting the Makefiles to run inside the guest should not be
   too difficult and a Solaris 10 cross compiler would allow guestd to be built
   for Solaris 10.
   
 2. Running the toolbox
 ----------------------

 o It is first necessary to set up lxrun on the guestd

   Setting up lxrun
   ----------------

   o lxrun enables you to run Linux binaries in Solaris by catching and
     translating all of their system calls.  This works relatively well for
     running guestd and toolbox, but you must install lxrun and build versions of
     the programs that have been "ported" to lxrun.

   o Install lxrun on the guest:
  
     The easiest way is to see if ~biggadike/public_html/lxrun/lxrun-environ.tgz
     exists.  If so, extract this to the root of your guest's filesystem:

     # cd /
     # gunzip /path/to/lxrun-environ.tgz
     # tar xvf /path/to/lxrun-environ.tar

     This will create two directories: /lx and /usr/local/lxrun.  /lx contains
     a linux environment (circa Red Hat 6.2) and the lxrun executable.
     /usr/local/lxrun contains a file called PATHMAP which tells lxrun where to
     get its environment files from (/lx).

     If the precompiled environment doesn't work, you'll need to get lxrun
     0.9.6pre1, compile it, and then use a Red Hat 6.2 Installation CD with the
     INSTALL-RH6 script in the lxrun directory.

     o Try ~biggadike/public_html/lxrun/lxrun-0.9.6pre1.tar.gz or
       http://www.ugcs.caltech.edu/~steven/lxrun/ for the lxrun source
     o Try exit15:/home/ISO-Images/vol1/redhat/RedHat6.2.iso for the Red Hat disc

     **Note that it is not necessary to patch lxrun for the toolbox.  The patch
       in the directory mentioned above is to enable lxrun to run guestd, which
       is no longer necessary since it has been ported.  The lxrun binary in
       the lxrun-environ.tgz file is a patched version, but this does also work
       for running the toolbox.

   o Set necessary environment variables:

     You will also need a few environment variables set.  For bash, add these
     lines to your .bashrc:
   
     export LXRUN_ROOT=/lx
     export PATH=$PATH:/lx

 o The following steps outline running the toolbox with lxrun

    Running the toolbox
    -------------------

    o Just copy (or make available) the above binaries on your guest and invoke
      them with lxrun:

      # lxrun /path/to/toolbox &

      (It is usually best to background this process.)

    o lxrun can also log the system calls it translates using its trace feature:

      # lxrun -t all /path/to/toolbox

      This will trace all the system calls guestd makes.  The output file is always
      placed in /tmp/lxrun.<pid>.  You can also restrict the system calls that are
      traced to a particular subset, run lxrun without any options to see the usage.

      It also appears possible to chroot the running process with -r, to specify the
      linux root directory with -R, and specify the uname of the emulated Linux
      system with -U.  I haven't ever needed to use these options.

 o Mouse grab/ungrab and device connection/disconnection are both known to work.




Notes from the old BUILD file that may still be helpful
-------------------------------------------------------
   
 o You can verify the module is installed with:

   # modinfo | grep hgfs

   This should output text of this format:
   <id#> <loadaddr> <size> <info> <revision#> hgfs (HGFS Device Interface)
   <id#> <loadaddr> <size> <info> <revision#> hgfs (Host/Guest Filesystem)

   There are two entries for the same ID number because we've placed both the
   device driver and the filesystem in a single loadable kernel module.

 o You can load the driver into the Kernel by hand with:
 
   # /usr/sbin/add_drv -v -m '* 0644 root sys' hgfs

   This should output something similar to:
   devfsadm[1032]: verbose: mknod /devices/pseudo/hgfs@0:vmware-hgfs 0l/3l/20644
   devfsadm[1032]: verbose: symlink /dev/vmware-hgfs -> ../devices/pseudo/hgfs@0:vmware-hgfs

 o You can remove the driver from the system by hand with:

   # /usr/sbin/rem_drv hgfs
