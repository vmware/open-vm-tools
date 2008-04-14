/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * checkvm.c --
 *
 *      Check if we are running in a VM or not
 */


#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <wtypes.h>
#include <winbase.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "vm_version.h"
#include "backdoor_def.h"
#include "vm_basic_types.h"

#include "checkvm_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(CHECKVM_VERSION_STRING);

#ifdef __GNUC__

/*
 *  outl and inl: Output or input a 32-bit word
 */
static __inline__ void
outl(
    const uint32 port,
    uint32       val
)
{
  __asm__ volatile("out%L0 (%%dx)" : :"a" (val), "d" (port));
}

static __inline__ uint32
inl(
    const uint32 port
)
{
  uint32 ret;

  __asm__ volatile("in%L0 (%%dx)" : "=a" (ret) : "d" (port));
  return ret;
}


/*
 *  getVersion  -  Read VM version & product code through backdoor
 */
void
getVersion(uint32 *version)
{
   uint32 eax, ebx, ecx, edx;
   
   __asm__ volatile("inl (%%dx)" :
   	            "=a"(eax), "=c"(ecx), "=d"(edx), "=b"(ebx) :
		    "0"(BDOOR_MAGIC), "1"(BDOOR_CMD_GETVERSION),
		    "2"(BDOOR_PORT) : "memory");
   version[0] = eax;
   version[1] = ebx;
   version[2] = ecx;
}

/*
 *  getHWVersion  -  Read VM HW version through backdoor
 */
void
getHWVersion(uint32 *hwVersion)
{
   uint32 eax, ebx, ecx, edx;
   
   __asm__ volatile("inl (%%dx)" :
   	            "=a"(eax), "=c"(ecx), "=d"(edx), "=b"(ebx) :
		    "0"(BDOOR_MAGIC), "1"(BDOOR_CMD_GETHWVERSION),
		    "2"(BDOOR_PORT) : "memory");
   *hwVersion = eax;
}


/*
 *  getScreenSize  -  Get screen size of the host
 */
void
getScreenSize(uint32 *screensize)
{
   uint32 eax, ebx, ecx, edx;
   
   __asm__ volatile("inl (%%dx)" :
   		    "=a"(eax), "=c"(ecx), "=d"(edx), "=b"(ebx) :
		    "0"(BDOOR_MAGIC), "1"(BDOOR_CMD_GETSCREENSIZE),
		    "2"(BDOOR_PORT) : "memory");
   *screensize = eax;
}



#elif _WIN32


/*
 *  getVersion  -  Read VM version & product code through backdoor
 */
void
getVersion(uint32 *version)
{		
   uint32 v;
   uint32 m;
   uint32 p;
   
   _asm {	  
         push eax
         push ebx
         push ecx
         push edx
         
         mov eax, BDOOR_MAGIC
         mov ecx, BDOOR_CMD_GETVERSION
         mov dx, BDOOR_PORT
         in eax, dx
         mov v, eax
         mov m, ebx
	 mov p, ecx
         
         pop edx
         pop ecx
         pop ebx
         pop eax
   }
   
   version[0] = v;
   version[1] = m;
   version[2] = p;
}


/*
 *  getHWVersion  -  Read VM HW Version through backdoor
 */
void
getHWVersion(uint32 *hwVersion)
{		
   uint32 v;

   _asm {	  
         push eax
         push ecx
         push edx

         mov eax, BDOOR_MAGIC
         mov ecx, BDOOR_CMD_GETHWVERSION
         mov dx, BDOOR_PORT
	 in eax, dx
	 mov v, eax

         pop edx
         pop ecx
         pop eax
   }
   
   *hwVersion = v;
}


/*
 *  getScreenSize  -  Get screen size of the host
 */
void
getScreenSize(uint32 *screensize)
{		
   uint32 v;

   _asm {	  
         push eax
         push ecx
         push edx

         mov eax, BDOOR_MAGIC
         mov ecx, BDOOR_CMD_GETSCREENSIZE
         mov dx, BDOOR_PORT
         in eax, dx
	 mov v, eax

         pop edx
         pop ecx
         pop eax
   }
   
   *screensize = v;
}


#endif

/*
 *  Simulate the getopt function for windows
 */
#ifdef _WIN32
static char *optarg;
static int optind = 1;

static int
getopt(int argc, char *argv[], char *opts)
{
   char *ptr = argv[optind];
   int chr;
   if ((ptr) && (*ptr == '-')) {
      ptr++;
      while (*opts) {
         if (*ptr == *opts) {
            if (*(opts+1) == ':') {
               optarg = ptr+1;
               if (*optarg == 0) {
                  optind++;
                  optarg = argv[optind];
               }
            }
            chr = *ptr & 0xff;
            optind++;
            return chr;
         }
         opts++;
      }

      /*
       *  Not a valid option
       */
      return '?';
   }
   return EOF;
}
#endif


static void handler(int sig)
{
   fprintf (stdout, "Couldn't get version\n");
   exit(1);
}


/*
 *  Start of main program.  Check if we are in a VM, by reading
 *  a backdoor port.  Then process any other commands.
 */
int main(int argc, char *argv[])
{
   uint32 version[3];
   int opt;
   int width, height;
   uint32 screensize = 0;
   uint32 hwVersion;

#ifdef _WIN32
   __try {
      getVersion(&version[0]);
   } __except(EXCEPTION_EXECUTE_HANDLER) {
      fprintf (stdout, "Couldn't get version\n");
      return 1;
   }
#else
#ifdef __FreeBSD__
#  define ERROR_SIGNAL SIGBUS
#else
#  define ERROR_SIGNAL SIGSEGV
#endif
   signal(ERROR_SIGNAL, handler);
   getVersion(&version[0]);
   signal(ERROR_SIGNAL, SIG_DFL);
#endif

   if (version[1] != BDOOR_MAGIC) {
      fprintf (stdout, "Incorrect virtual machine version\n");
      return 1;
   }

   if (version[0] != VERSION_MAGIC) {
      fprintf (stdout, "%s version %d (should be %d)\n",
			PRODUCT_LINE_NAME, version[0], VERSION_MAGIC);
      return 1;
   }

   /*
    *  OK, we're in a VM, check if there are any other requests
    */
   while ((opt = getopt (argc, argv, "rph")) != EOF) {
      switch (opt) {
      case 'r':
         getScreenSize (&screensize);
         width = (screensize >> 16) & 0xffff;
         height = screensize & 0xffff;
         if ((width <= 0x7fff) && (height <= 0x7fff)) {
	    printf("%d %d\n", width, height);
         } else {
	    printf("0 0\n");
         }
	 return 0;

      case 'p':
	 /*
	  * Print out product that we're running on based on code
	  * obtained from getVersion().
	  */
         switch (version[2]) {
	    case VMX_TYPE_EXPRESS:
	       printf("Express\n");
	       break;
	       
	    case VMX_TYPE_SCALABLE_SERVER:
	       printf("ESX Server\n");
	       break;
	       
	    case VMX_TYPE_WGS:
	       printf("VMware Server\n");
	       break;
	       
	    case VMX_TYPE_WORKSTATION:
	       printf("Workstation\n");
	       break;
	       
            default:
	       printf("Unknown\n");
	       break;
	 }
	 return 0;

      case 'h':
	 getHWVersion(&hwVersion);
	 printf("VM's hw version is %u\n", hwVersion);
	 break;
      default:
	 break;
      }
   }

   printf("%s version %d (good)\n", PRODUCT_LINE_NAME, version[0]);
   return 0;
}
