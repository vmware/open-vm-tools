/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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
 * unicodeSimpleTypes.c --
 *
 *      Basic types and cache handling for simple UTF-8 implementation
 *      of Unicode library interface.
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "unicodeBase.h"
#include "unicodeInt.h"
#include "codeset.h"

#include "vm_assert.h"
#include "util.h"
#include "hashTable.h"
#include "vm_atomic.h"

static char *UnicodeNormalizeEncodingName(const char *encoding);

/*
 * Cross reference of IANA character set names, windows code pages
 * and ICU encodings
 *
 * See: http://www.iana.org/assignments/character-sets
 *      http://msdn2.microsoft.com/en-us/library/ms776446(VS.85).aspx
 *      http://demo.icu-project.org/icu-bin/convexp
 *
 * If you add new StringEncodings to this table, you must keep the
 * StringEncoding enum in lib/public/unicodeTypes.h in sync!
 */

#define MAXCHARSETNAMES 21
#define MIBUNDEF (-1)
#define WINUNDEF (-1)
#define SUPPORTED TRUE
#define UNSUPPORTED FALSE
#define IN_FULL_ICU FALSE

static struct xRef {
   int MIBenum;                   // Assigned by IANA
   int winACP;                    // Windows code page from GetACP()
   StringEncoding encoding;       // ICU encoding enum
   Bool isSupported;              // VMware supported encoding
   int preferredMime;             // Index of preferred MIME name 
   const char *names[MAXCHARSETNAMES];  // Encoding name and aliases
} xRef[] = {
   /*
    * Source: ECMA registry
    */
   { 3, 20127, STRING_ENCODING_US_ASCII, SUPPORTED, 4,
      { "ANSI_X3.4-1968", "iso-ir-6", "ANSI_X3.4-1986", "ISO_646.irv:1991",
        "ASCII", "ISO646-US", "US-ASCII", "us", "IBM367", "cp367", "csASCII",
        "646", "ascii7", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 4, 28591, STRING_ENCODING_ISO_8859_1, SUPPORTED, 3,
      { "ISO_8859-1:1987", "iso-ir-100", "ISO_8859-1", "ISO-8859-1", "latin1",
        "l1", "IBM819", "CP819", "csISOLatin1", "8859_1", "819", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 5, 28592, STRING_ENCODING_ISO_8859_2, SUPPORTED, 3,
      { "ISO_8859-2:1987", "iso-ir-101", "ISO_8859-2", "ISO-8859-2", "latin2",
        "l2", "csISOLatin2", "ibm-912_P100-1995", "ibm-912", "8859_2", "cp912",
        "912", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 6, 28593, STRING_ENCODING_ISO_8859_3, SUPPORTED, 3,
      { "ISO_8859-3:1988", "iso-ir-109", "ISO_8859-3", "ISO-8859-3", "latin3",
        "l3", "csISOLatin3", "ibm-913_P100-2000", "ibm-913", "8859_3", "cp913",
        "913", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 7, 28594, STRING_ENCODING_ISO_8859_4, SUPPORTED, 3,
      { "ISO_8859-4:1988", "iso-ir-110", "ISO_8859-4", "ISO-8859-4", "latin4",
        "l4", "csISOLatin4", "ibm-914_P100-1995", "ibm-914", "8859_4", "cp914",
        "914", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 8, 28595, STRING_ENCODING_ISO_8859_5, SUPPORTED, 3,
      { "ISO_8859-5:1988", "iso-ir-144", "ISO_8859-5", "ISO-8859-5", "cyrillic",
        "csISOLatinCyrillic", "ibm-915_P100-1995", "ibm-915", "8859_5", "cp915",
        "915", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 9, 28596, STRING_ENCODING_ISO_8859_6, SUPPORTED, 3,
      { "ISO_8859-6:1987", "iso-ir-127", "ISO_8859-6", "ISO-8859-6", "ECMA-114",
        "ASMO-708", "arabic", "csISOLatinArabic", "ibm-1089_P100-1995",
        "ibm-1089", "8859_6", "cp1089", "1089", NULL }
   },
   /*
    * Source: Windows duplicate of ISO-8859-6
    */
   { 9, 708, STRING_ENCODING_ISO_8859_6, SUPPORTED, 0,
      { "ASMO-708", NULL }
   },
   /*
    * Source: ECMA registry, ICU almost completely duplicates this entry with
    *         ibm-813 (see below), which is an older version
    */
   { 10, 28597, STRING_ENCODING_ISO_8859_7, SUPPORTED, 3,
      { "ISO_8859-7:1987", "iso-ir-126", "ISO_8859-7", "ISO-8859-7", "ELOT_928",
        "ECMA-118", "greek", "greek8", "csISOLatinGreek", "ibm-9005_X110-2007",
        "ibm-9005", "sun_eu_greek", NULL }
   },
   /*
    * Source: ICU
    */
   { MIBUNDEF, WINUNDEF, STRING_ENCODING_IBM_813, IN_FULL_ICU, 0,
      { "ibm-813_P100-1995", "ibm-813", "cp813", "813", "8859_7", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 11, 28598, STRING_ENCODING_ISO_8859_8, SUPPORTED, 3,
      { "ISO_8859-8:1988", "iso-ir-138", "ISO_8859-8", "ISO-8859-8", "hebrew",
        "csISOLatinHebrew", "ibm-5012_P100-1999", "ibm-5012", "8859_8", 
        "hebrew8", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 12, 28599, STRING_ENCODING_ISO_8859_9, SUPPORTED, 3,
      { "ISO_8859-9:1989", "iso-ir-148", "ISO_8859-9", "ISO-8859-9", "latin5",
        "l5", "csISOLatin5", "ibm-920_P100-1995", "ibm-920", "8859_9", "cp920",
        "920", "ECMA-128", "turkish", "turkish8", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 13, WINUNDEF, STRING_ENCODING_ISO_8859_10, SUPPORTED, 0,
      { "ISO-8859-10", "iso-ir-157", "l6", "ISO_8859-10:1992", "csISOLatin6",
        "latin6", "iso-8859_10-1998", NULL }
   },
   /*
    * Source: ECMA registry and ISO 6937-2:1983, not supported by ICU
    */
   { 14, WINUNDEF, STRING_ENCODING_ISO_6937_2_ADD, UNSUPPORTED, 0,
      { "ISO_6937-2-add", "iso-ir-142", "csISOTextComm", NULL }
   },
   /*
    * Source: JIS X 0201-1976.   One byte only, this is equivalent to 
    *         JIS/Roman (similar to ASCII) plus eight-bit half-width 
    *         Katakana
    */
   { 15, WINUNDEF, STRING_ENCODING_JIS_X0201, IN_FULL_ICU, 0,
      { "JIS_X0201", "X0201", "csHalfWidthKatakana", NULL }
   },
   /*
    * Source: JIS X 0202-1991.  Uses ISO 2022 escape sequences to
    *         shift code sets as documented in JIS X 0202-1991.
    *         ICU maps this to ISO-2022-JP-1
    */
   { 16, WINUNDEF, STRING_ENCODING_JIS_ENCODING, IN_FULL_ICU, 0,
      { "JIS_Encoding", "csJISEncoding", "JIS", NULL }
   },
   /*
    * Source: This charset is an extension of csHalfWidthKatakana by
    *         adding graphic characters in JIS X 0208.  The CCS's are
    *         JIS X0201:1997 and JIS X0208:1997.  The
    *         complete definition is shown in Appendix 1 of JIS
    *         X0208:1997.
    *         This charset can be used for the top-level media type "text".
    */
   { 17, 932, STRING_ENCODING_SHIFT_JIS, SUPPORTED, 0,
      { "Shift_JIS", "MS_Kanji", "csShiftJIS", "ibm-943_P15A-2003", "ibm-943",
        "x-sjis", "x-ms-cp932", "cp932", "cp943c", "IBM-943C", "ms932", "pck",
        "sjis", "ibm-943_VSUB_VPUA", NULL }
   },
   /*
    * Source: ICU, older version of Shift_JIS, use newer version above for
    *         common entries between the two
    *
    */
   { MIBUNDEF, WINUNDEF, STRING_ENCODING_IBM_943_P130_1999, SUPPORTED, 0,
      { "ibm-943_P130-1999", "cp943", "943", "ibm-943_VASCII_VSUB_VPUA",
         NULL }
   },
   /*
    * Source: Standardized by OSF, UNIX International, and UNIX Systems
    *         Laboratories Pacific.  Uses ISO 2022 rules to select
    *                code set 0: US-ASCII (a single 7-bit byte set)
    *                code set 1: JIS X0208-1990 (a double 8-bit byte set)
    *                            restricted to A0-FF in both bytes
    *                code set 2: Half Width Katakana (a single 7-bit byte set)
    *                            requiring SS2 as the character prefix
    *                code set 3: JIS X0212-1990 (a double 7-bit byte set)
    *                            restricted to A0-FF in both bytes
    *                            requiring SS3 as the character prefix
    */
   { 18, 20932, STRING_ENCODING_EUC_JP, IN_FULL_ICU, 2,
      { "Extended_UNIX_Code_Packed_Format_for_Japanese", "csEUCPkdFmtJapanese",
        "EUC-JP", "ibm-954_P101-2007", "ibm-954", "X-EUC-JP", "eucjis", "ujis", 
         NULL }
   },
   /*
    * Windows duplicate and older ICU version of EUC-JP
    */
   { 18, 51932, STRING_ENCODING_IBM_33722, IN_FULL_ICU, 0,
      { "ibm-33722_P12A_P12A-2004_U2", "ibm-33722", "ibm-5050", "ibm-33722_VPUA",
        "IBM-eucJP", NULL }
   },
   /*
    * Source: Used in Japan.  Each character is 2 octets.
    *                 code set 0: US-ASCII (a single 7-bit byte set)
    *                               1st byte = 00
    *                               2nd byte = 20-7E
    *                 code set 1: JIS X0208-1990 (a double 7-bit byte set)
    *                             restricted  to A0-FF in both bytes 
    *                 code set 2: Half Width Katakana (a single 7-bit byte set)
    *                               1st byte = 00
    *                               2nd byte = A0-FF
    *                 code set 3: JIS X0212-1990 (a double 7-bit byte set)
    *                             restricted to A0-FF in 
    *                             the first byte
    *                 and 21-7E in the second byte
    *
    *           Not supported by ICU
    */
   { 19, WINUNDEF, STRING_ENCODING_EXTENDED_UNIX_CODE_FIXED_WIDTH_FOR_JAPANESE,
     UNSUPPORTED, 0,
      { "Extended_UNIX_Code_Fixed_Width_for_Japanese", "csEUCFixWidJapanese",
         NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 20, WINUNDEF, STRING_ENCODING_BS_4730, IN_FULL_ICU, 0,
      { "BS_4730", "iso-ir-4", "ISO646-GB", "gb", "uk", "csISO4UnitedKingdom",
         NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 21, WINUNDEF, STRING_ENCODING_SEN_850200_C, UNSUPPORTED, 0,
      { "SEN_850200_C", "iso-ir-11", "ISO646-SE2", "se2",
        "csISO11SwedishForNames", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 22, WINUNDEF, STRING_ENCODING_IT, IN_FULL_ICU, 0,
      { "IT", "iso-ir-15", "ISO646-IT", "csISO15Italian", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 23, WINUNDEF, STRING_ENCODING_ES, IN_FULL_ICU, 0,
      { "ES", "iso-ir-17", "ISO646-ES", "csISO17Spanish", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 24, WINUNDEF, STRING_ENCODING_DIN_66003, IN_FULL_ICU, 0,
      { "DIN_66003", "iso-ir-21", "de", "ISO646-DE", "csISO21German", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 25, WINUNDEF, STRING_ENCODING_NS_4551_1, IN_FULL_ICU, 0,
      { "NS_4551-1", "iso-ir-60", "ISO646-NO", "no", "csISO60DanishNorwegian",
        "csISO60Norwegian1", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 26, WINUNDEF, STRING_ENCODING_NF_Z_62_010, IN_FULL_ICU, 0,
      { "NF_Z_62-010", "iso-ir-69", "ISO646-FR", "fr", "csISO69French", NULL }
   },
   /*
    * Source: Universal Transfer Format (1), this is the multibyte
    *         encoding, that subsets ASCII-7. It does not have byte
    *         ordering issues.
    *
    *         Not supported by ICU.
    */
   { 27, WINUNDEF, STRING_ENCODING_ISO_10646_UTF_1, UNSUPPORTED, 0,
      { "ISO-10646-UTF-1", "csISO10646UTF1", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 28, WINUNDEF, STRING_ENCODING_ISO_646_BASIC_1983, UNSUPPORTED, 0,
      { "ISO_646.basic:1983", "ref", "csISO646basic1983", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 29, WINUNDEF, STRING_ENCODING_INVARIANT, UNSUPPORTED, 0,
      { "INVARIANT", "csINVARIANT", NULL }
   },
   /*
    * Source: ECMA registry, ICU maps this to ASCII
    */
   { 30, WINUNDEF, STRING_ENCODING_ISO_646_IRV_1983, SUPPORTED, 0,
      { "ISO_646.irv:1983", "iso-ir-2", "irv", "csISO2IntlRefVersion", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 31, WINUNDEF, STRING_ENCODING_NATS_SEFI, UNSUPPORTED, 0,
      { "NATS-SEFI", "iso-ir-8-1", "csNATSSEFI", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 32, WINUNDEF, STRING_ENCODING_NATS_SEFI_ADD, UNSUPPORTED, 0,
      { "NATS-SEFI-ADD", "iso-ir-8-2", "csNATSSEFIADD", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 33, WINUNDEF, STRING_ENCODING_NATS_DANO, UNSUPPORTED, 0,
      { "NATS-DANO", "iso-ir-9-1", "csNATSDANO", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 34, WINUNDEF, STRING_ENCODING_NATS_DANO_ADD, UNSUPPORTED, 0,
      { "NATS-DANO-ADD", "iso-ir-9-2", "csNATSDANOADD", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 35, WINUNDEF, STRING_ENCODING_SEN_850200_B, IN_FULL_ICU, 0,
      { "SEN_850200_B", "iso-ir-10", "FI", "ISO646-FI", "ISO646-SE", "se",
        "csISO10Swedish", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 36, 51949, STRING_ENCODING_KS_C_5601_1987, SUPPORTED, 0,
      { "KS_C_5601-1987", "ibm-970_P110_P110-2006_U2", "ibm-970", "EUC-KR",
        "csEUCKR", "ibm-eucKR", "KSC_5601", "5601", "cp970", "970", 
        "ibm-970_VPUA", NULL }
   },
   /*
    * Windows-949 code page for KS_C_5601
    */
   { 36, 949, STRING_ENCODING_WINDOWS_949, SUPPORTED, 0,
      { "windows-949-2000", "KS_C_5601-1989", "KS_C_5601-1987", "KSC_5601", 
        "csKSC56011987", "korean", "iso-ir-149", "ms949", NULL }
   },
   /*
    * Another ICU converter for KS_C_5601
    */
   { 36, WINUNDEF, STRING_ENCODING_IBM_1363, SUPPORTED, 0,
      { "ibm-1363_P11B-1998", "ibm-1363", "cp1363", "5601", "ksc",
        "ibm-1363_VSUB_VPUA", NULL }
   },
   /*
    * Source: RFC-1557 (see also KS_C_5601-1987)
    */
   { 37, 50225, STRING_ENCODING_ISO_2022_KR, IN_FULL_ICU, 0,
      { "ISO-2022-KR", "csISO2022KR", NULL }
   },
   /*
    * Source: RFC-1468 (see also RFC-2237)
    *         Windows-50221 and 50222 are routed here
    */
   { 39, 50220, STRING_ENCODING_ISO_2022_JP, SUPPORTED, 0,
      { "ISO-2022-JP", "csISO2022JP", NULL }
   },
   /*
    * Source: VMware
    */
   { MIBUNDEF, WINUNDEF, STRING_ENCODING_ISO_2022_JP_1, 
     IN_FULL_ICU, 0,
      { "ISO-2022-JP-1", "ibm-5054", NULL }
   },
   /*
    * Source: RFC-1554
    */
   { 40, WINUNDEF, STRING_ENCODING_ISO_2022_JP_2, IN_FULL_ICU, 0,
      { "ISO-2022-JP-2", "csISO2022JP2", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 41, WINUNDEF, STRING_ENCODING_JIS_C6220_1969_JP, UNSUPPORTED, 0,
      { "JIS_C6220-1969-jp", "JIS_C6220-1969", "iso-ir-13", "katakana",
        "x0201-7", "csISO13JISC6220jp", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 42, WINUNDEF, STRING_ENCODING_JIS_C6220_1969_RO, UNSUPPORTED, 0,
      { "JIS_C6220-1969-ro", "iso-ir-14", "jp", "ISO646-JP",
        "csISO14JISC6220ro", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 43, WINUNDEF, STRING_ENCODING_PT, UNSUPPORTED, 0,
      { "PT", "iso-ir-16", "ISO646-PT", "csISO16Portuguese", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 44, WINUNDEF, STRING_ENCODING_GREEK7_OLD, UNSUPPORTED, 0,
      { "greek7-old", "iso-ir-18", "csISO18Greek7Old", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 45, WINUNDEF, STRING_ENCODING_LATIN_GREEK, UNSUPPORTED, 0,
      { "latin-greek", "iso-ir-19", "csISO19LatinGreek", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 46, WINUNDEF, STRING_ENCODING_NF_Z_62_010__1973_, IN_FULL_ICU, 0,
      { "NF_Z_62-010_(1973)", "iso-ir-25", "ISO646-FR1", "csISO25French", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 47, WINUNDEF, STRING_ENCODING_LATIN_GREEK_1, UNSUPPORTED, 0,
      { "Latin-greek-1", "iso-ir-27", "csISO27LatinGreek1", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 48, WINUNDEF, STRING_ENCODING_ISO_5427, UNSUPPORTED, 0,
      { "ISO_5427", "iso-ir-37", "csISO5427Cyrillic", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 49, WINUNDEF, STRING_ENCODING_JIS_C6226_1978, UNSUPPORTED, 0,
      { "JIS_C6226-1978", "iso-ir-42", "csISO42JISC62261978", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 50, WINUNDEF, STRING_ENCODING_BS_VIEWDATA, UNSUPPORTED, 0,
      { "BS_viewdata", "iso-ir-47", "csISO47BSViewdata", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 51, WINUNDEF, STRING_ENCODING_INIS, UNSUPPORTED, 0,
      { "INIS", "iso-ir-49", "csISO49INIS", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 52, WINUNDEF, STRING_ENCODING_INIS_8, UNSUPPORTED, 0,
      { "INIS-8", "iso-ir-50", "csISO50INIS8", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 53, WINUNDEF, STRING_ENCODING_INIS_CYRILLIC, UNSUPPORTED, 0,
      { "INIS-cyrillic", "iso-ir-51", "csISO51INISCyrillic", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 54, WINUNDEF, STRING_ENCODING_ISO_5427_1981, UNSUPPORTED, 0,
      { "ISO_5427:1981", "iso-ir-54", "ISO5427Cyrillic1981", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 55, WINUNDEF, STRING_ENCODING_ISO_5428_1980, UNSUPPORTED, 0,
      { "ISO_5428:1980", "iso-ir-55", "csISO5428Greek", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 56, WINUNDEF, STRING_ENCODING_GB_1988_80, UNSUPPORTED, 0,
      { "GB_1988-80", "iso-ir-57", "cn", "ISO646-CN", "csISO57GB1988", NULL }
   },
   /*
    * Source: ECMA registry
    *         Note that this encoding does not support ASCII as a subset
    */
   { 57, 20936, STRING_ENCODING_GB_2312_80, IN_FULL_ICU, 0,
      { "GB_2312-80", "iso-ir-58", "chinese", "csISO58GB231280", 
        "ibm-5478_P100-1995", "ibm-5478", "gb2312-1980", "GB2312.1980-0", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 58, WINUNDEF, STRING_ENCODING_NS_4551_2, UNSUPPORTED, 0,
      { "NS_4551-2", "ISO646-NO2", "iso-ir-61", "no2", "csISO61Norwegian2", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 59, WINUNDEF, STRING_ENCODING_VIDEOTEX_SUPPL, UNSUPPORTED, 0,
      { "videotex-suppl", "iso-ir-70", "csISO70VideotexSupp1", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 60, WINUNDEF, STRING_ENCODING_PT2, IN_FULL_ICU, 0,
      { "PT2", "iso-ir-84", "ISO646-PT2", "csISO84Portuguese2", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 61, WINUNDEF, STRING_ENCODING_ES2, IN_FULL_ICU, 0,
      { "ES2", "iso-ir-85", "ISO646-ES2", "csISO85Spanish2", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 62, WINUNDEF, STRING_ENCODING_MSZ_7795_3, UNSUPPORTED, 0,
      { "MSZ_7795.3", "iso-ir-86", "ISO646-HU", "hu", "csISO86Hungarian", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 63, WINUNDEF, STRING_ENCODING_JIS_C6226_1983, UNSUPPORTED, 0,
      { "JIS_C6226-1983", "iso-ir-87", "x0208", "JIS_X0208-1983",
        "csISO87JISX0208", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 64, WINUNDEF, STRING_ENCODING_GREEK7, UNSUPPORTED, 0,
      { "greek7", "iso-ir-88", "csISO88Greek7", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 65, WINUNDEF, STRING_ENCODING_ASMO_449, UNSUPPORTED, 0,
      { "ASMO_449", "ISO_9036", "arabic7", "iso-ir-89", "csISO89ASMO449", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 66, WINUNDEF, STRING_ENCODING_ISO_IR_90, UNSUPPORTED, 0,
      { "iso-ir-90", "csISO90", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 67, WINUNDEF, STRING_ENCODING_JIS_C6229_1984_A, UNSUPPORTED, 0,
      { "JIS_C6229-1984-a", "iso-ir-91", "jp-ocr-a", "csISO91JISC62291984a",
         NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 68, WINUNDEF, STRING_ENCODING_JIS_C6229_1984_B, UNSUPPORTED, 0,
      { "JIS_C6229-1984-b", "iso-ir-92", "ISO646-JP-OCR-B", "jp-ocr-b",
        "csISO92JISC62991984b", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 69, WINUNDEF, STRING_ENCODING_JIS_C6229_1984_B_ADD, UNSUPPORTED, 0,
      { "JIS_C6229-1984-b-add", "iso-ir-93", "jp-ocr-b-add",
        "csISO93JIS62291984badd", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 70, WINUNDEF, STRING_ENCODING_JIS_C6229_1984_HAND, UNSUPPORTED, 0,
      { "JIS_C6229-1984-hand", "iso-ir-94", "jp-ocr-hand",
        "csISO94JIS62291984hand", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 71, WINUNDEF, STRING_ENCODING_JIS_C6229_1984_HAND_ADD, UNSUPPORTED, 0,
      { "JIS_C6229-1984-hand-add", "iso-ir-95", "jp-ocr-hand-add",
        "csISO95JIS62291984handadd", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 72, WINUNDEF, STRING_ENCODING_JIS_C6229_1984_KANA, UNSUPPORTED, 0,
      { "JIS_C6229-1984-kana", "iso-ir-96", "csISO96JISC62291984kana", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 73, WINUNDEF, STRING_ENCODING_ISO_2033_1983, UNSUPPORTED, 0,
      { "ISO_2033-1983", "iso-ir-98", "e13b", "csISO2033", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 74, WINUNDEF, STRING_ENCODING_ANSI_X3_110_1983, UNSUPPORTED, 0,
      { "ANSI_X3.110-1983", "iso-ir-99", "CSA_T500-1983", "NAPLPS",
        "csISO99NAPLPS", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 75, WINUNDEF, STRING_ENCODING_T_61_7BIT, UNSUPPORTED, 0,
      { "T.61-7bit", "iso-ir-102", "csISO102T617bit", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 76, 20261, STRING_ENCODING_T_61_8BIT, UNSUPPORTED, 0,
      { "T.61-8bit", "T.61", "iso-ir-103", "csISO103T618bit", NULL }
   },
   /*
    * Source: ISO registry (formerly ECMA registry)
    *          http://www.itscj.ipsj.jp/ISO-IR/111.pdf
    *         Not supported by ICU
    */
   { 77, WINUNDEF, STRING_ENCODING_ECMA_CYRILLIC, UNSUPPORTED, 0,
      { "ECMA-cyrillic", "iso-ir-111", "KOI8-E", "csISO111ECMACyrillic", NULL }
   },
   /*
    * Source: ECMA registry
    */
   { 78, WINUNDEF, STRING_ENCODING_CSA_Z243_4_1985_1, IN_FULL_ICU, 0,
      { "CSA_Z243.4-1985-1", "iso-ir-121", "ISO646-CA", "csa7-1", "ca",
        "csISO121Canadian1", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 79, WINUNDEF, STRING_ENCODING_CSA_Z243_4_1985_2, UNSUPPORTED, 0,
      { "CSA_Z243.4-1985-2", "iso-ir-122", "ISO646-CA2", "csa7-2",
        "csISO122Canadian2", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 80, WINUNDEF, STRING_ENCODING_CSA_Z243_4_1985_GR, UNSUPPORTED, 0,
      { "CSA_Z243.4-1985-gr", "iso-ir-123", "csISO123CSAZ24341985gr", NULL }
   },
   /*
    * Source: RFC1556, ICU maps this to ISO-8859-6
    */
   { 81, WINUNDEF, STRING_ENCODING_ISO_8859_6_E, SUPPORTED, 2,
      { "ISO_8859-6-E", "csISO88596E", "ISO-8859-6-E", NULL }
   },
   /*
    * Source: RFC1556, ICU maps this to ISO-8859-6
    */
   { 82, WINUNDEF, STRING_ENCODING_ISO_8859_6_I, SUPPORTED, 2,
      { "ISO_8859-6-I", "csISO88596I", "ISO-8859-6-I", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 83, WINUNDEF, STRING_ENCODING_T_101_G2, UNSUPPORTED, 0,
      { "T.101-G2", "iso-ir-128", "csISO128T101G2", NULL }
   },
   /*
    * Source: RFC1556, ICU maps this to ISO-8859-8
    */
   { 84, WINUNDEF, STRING_ENCODING_ISO_8859_8_E, SUPPORTED, 2,
      { "ISO_8859-8-E", "csISO88598E", "ISO-8859-8-E", NULL }
   },
   /*
    * Source: RFC1556, ICU maps this to ISO-8859-8
    */
   { 85, WINUNDEF, STRING_ENCODING_ISO_8859_8_I, SUPPORTED, 2,
      { "ISO_8859-8-I", "csISO88598I", "ISO-8859-8-I", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 86, WINUNDEF, STRING_ENCODING_CSN_369103, UNSUPPORTED, 0,
      { "CSN_369103", "iso-ir-139", "csISO139CSN369103", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 87, WINUNDEF, STRING_ENCODING_JUS_I_B1_002, UNSUPPORTED, 0,
      { "JUS_I.B1.002", "iso-ir-141", "ISO646-YU", "js", "yu",
        "csISO141JUSIB1002", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 88, WINUNDEF, STRING_ENCODING_IEC_P27_1, UNSUPPORTED, 0,
      { "IEC_P27-1", "iso-ir-143", "csISO143IECP271", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 89, WINUNDEF, STRING_ENCODING_JUS_I_B1_003_SERB, UNSUPPORTED, 0,
      { "JUS_I.B1.003-serb", "iso-ir-146", "serbian", "csISO146Serbian", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 90, WINUNDEF, STRING_ENCODING_JUS_I_B1_003_MAC, UNSUPPORTED, 0,
      { "JUS_I.B1.003-mac", "macedonian", "iso-ir-147", "csISO147Macedonian",
         NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 91, WINUNDEF, STRING_ENCODING_GREEK_CCITT, UNSUPPORTED, 0,
      { "greek-ccitt", "iso-ir-150", "csISO150", "csISO150GreekCCITT", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 92, WINUNDEF, STRING_ENCODING_NC_NC00_10_81, UNSUPPORTED, 0,
      { "NC_NC00-10:81", "cuba", "iso-ir-151", "ISO646-CU", "csISO151Cuba",
         NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 93, WINUNDEF, STRING_ENCODING_ISO_6937_2_25, UNSUPPORTED, 0,
      { "ISO_6937-2-25", "iso-ir-152", "csISO6937Add", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 94, WINUNDEF, STRING_ENCODING_GOST_19768_74, UNSUPPORTED, 0,
      { "GOST_19768-74", "ST_SEV_358-88", "iso-ir-153", "csISO153GOST1976874",
         NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 95, WINUNDEF, STRING_ENCODING_ISO_8859_SUPP, UNSUPPORTED, 0,
      { "ISO_8859-supp", "iso-ir-154", "latin1-2-5", "csISO8859Supp", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 96, WINUNDEF, STRING_ENCODING_ISO_10367_BOX, UNSUPPORTED, 0,
      { "ISO_10367-box", "iso-ir-155", "csISO10367Box", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 97, WINUNDEF, STRING_ENCODING_LATIN_LAP, UNSUPPORTED, 0,
      { "latin-lap", "lap", "iso-ir-158", "csISO158Lap", NULL }
   },
   /*
    * Source: ECMA registry, not supported by ICU
    */
   { 98, WINUNDEF, STRING_ENCODING_JIS_X0212_1990, UNSUPPORTED, 0,
      { "JIS_X0212-1990", "x0212", "iso-ir-159", "csISO159JISX02121990", NULL }
   },
   /*
    * Source: Danish Standard, DS 2089, February 1974
    */
   { 99, WINUNDEF, STRING_ENCODING_DS_2089, IN_FULL_ICU, 0,
      { "DS_2089", "DS2089", "ISO646-DK", "dk", "csISO646Danish", NULL }
   },
   { 100, WINUNDEF, STRING_ENCODING_US_DK, UNSUPPORTED, 0,
      { "us-dk", "csUSDK", NULL }
   },
   { 101, WINUNDEF, STRING_ENCODING_DK_US, UNSUPPORTED, 0,
      { "dk-us", "csDKUS", NULL }
   },
   { 102, WINUNDEF, STRING_ENCODING_KSC5636, UNSUPPORTED, 0,
      { "KSC5636", "ISO646-KR", "csKSC5636", NULL }
   },
   /*
    * Source: RFC 1642, not supported by ICU
    */
   { 103, WINUNDEF, STRING_ENCODING_UNICODE_1_1_UTF_7, UNSUPPORTED, 0,
      { "UNICODE-1-1-UTF-7", "csUnicode11UTF7", NULL }
   },
   /*
    * Source: RFC-1922
    */
   { 104, 50227, STRING_ENCODING_ISO_2022_CN, IN_FULL_ICU, 0,
      { "ISO-2022-CN", "csISO2022CN", NULL }
   },
   /*
    * Source: RFC-1922
    */
   { 105, WINUNDEF, STRING_ENCODING_ISO_2022_CN_EXT, IN_FULL_ICU, 0,
      { "ISO-2022-CN-EXT", NULL }
   },
   /*
    * Source: RFC 3629
    */
   { 106, 65001, STRING_ENCODING_UTF8, SUPPORTED, 0,
      { "UTF-8", "ibm-1208", "ibm-1209", "ibm-5304", "ibm-5305", "ibm-13496",
        "ibm-13497", "ibm-17592", "ibm-17593", "cp1208", NULL }
   },
   /*
    * Source: ISO See 
    *     (http://www.iana.org/assignments/charset-reg/ISO-8859-13)[Tumasonis] 
    */
   { 109, 28603, STRING_ENCODING_ISO_8859_13, SUPPORTED, 0,
      { "ISO-8859-13", "ibm-921_P100-1995", "ibm-921", "8859_13", "cp921",
        "921", NULL }
   },
   /*
    * Source: ISO See
    *      (http://www.iana.org/assignments/charset-reg/ISO-8859-14) [Simonsen]
    */
   { 110, WINUNDEF, STRING_ENCODING_ISO_8859_14, SUPPORTED, 0,
      { "ISO-8859-14", "iso-ir-199", "ISO_8859-14:1998", "ISO_8859-14",
        "latin8", "iso-celtic", "l8", NULL }
   },
   /*
    * Source: ISO 
    *     Please see: <http://www.iana.org/assignments/charset-reg/ISO-8859-15>
    */
   { 111, 28605, STRING_ENCODING_ISO_8859_15, SUPPORTED, 0,
      { "ISO-8859-15", "ISO_8859-15", "Latin-9", "ibm-923_P100-1998", "ibm-923",
        "l9", "8859_15", "latin0", "csisolatin0", "csisolatin9", "cp923", "923",
        "iso8859_15_fdis", NULL }
   },
   /*
    * Windows duplicate of ISO-8859-15
    * windows-874: ANSI/OEM Thai
    */
   { 111, 874, STRING_ENCODING_IBM_874, SUPPORTED, 0,
      { "ibm-874", "ibm-874_P100-1995", "cp874", "ibm-9066", "TIS-620",
        "tis620.2533", "eucTH", NULL }
   },
   /*
    * Source: ISO
    */
   { 112, WINUNDEF, STRING_ENCODING_ISO_8859_16, IN_FULL_ICU, 0,
      { "ISO-8859-16", "iso-ir-226", "ISO_8859-16:2001", "ISO_8859-16",
        "latin10", "l10", NULL }
   },
   /*
    * Source: Chinese IT Standardization Technical Committee  
    *         Please see: <http://www.iana.org/assignments/charset-reg/GBK>
    */
   { 113, 936, STRING_ENCODING_GBK, SUPPORTED, 0,
      { "GBK", "CP936", "MS936", "windows-936", "windows-936-2000", NULL }
   },
   /*
    * Alternate ICU encoding for Windows-936
    */
   { MIBUNDEF, WINUNDEF, STRING_ENCODING_IBM_1386, SUPPORTED, 1,
      { "ibm-1386_P100-2001", "ibm-1386", "cp1386", "ibm-1386_VSUB_VPUA", NULL }
   },
   /*
    * Source: Chinese IT Standardization Technical Committee
    *         Please see: <http://www.iana.org/assignments/charset-reg/GB18030>
    */
   { 114, 54936, STRING_ENCODING_GB_18030, IN_FULL_ICU, 0,
      { "GB18030", "ibm-1392", NULL }
   },
   /*
    * Source:  Fujitsu-Siemens standard mainframe EBCDIC encoding
    *          Please see:
    *           <http://www.iana.org/assignments/charset-reg/OSD-EBCDIC-DF04-15
    *          Not supported by ICU
    */
   { 115, WINUNDEF, STRING_ENCODING_OSD_EBCDIC_DF04_15, UNSUPPORTED, 0,
      { "OSD_EBCDIC_DF04_15", NULL }
   },
   /*
    * Source:  Fujitsu-Siemens standard mainframe EBCDIC encoding
    *          Please see:
    *          <http://www.iana.org/assignments/charset-reg/OSD-EBCDIC-DF03-IRV>
    *          Not supported by ICU
    */
   { 116, WINUNDEF, STRING_ENCODING_OSD_EBCDIC_DF03_IRV, UNSUPPORTED, 0,
      { "OSD_EBCDIC_DF03_IRV", NULL }
   },
   /*
    * Source:  Fujitsu-Siemens standard mainframe EBCDIC encoding
    *          Please see:
    *          <http://www.iana.org/assignments/charset-reg/OSD-EBCDIC-DF04-1>
    *          Not supported by ICU
    */
   { 117, WINUNDEF, STRING_ENCODING_OSD_EBCDIC_DF04_1, UNSUPPORTED, 0,
      { "OSD_EBCDIC_DF04_1", NULL }
   },
   /*
    * Source: See <http://www.iana.org/assignments/charset-reg/ISO-11548-1>
    *            [Thibault]
    *          Not supported by ICU
    */
   { 118, WINUNDEF, STRING_ENCODING_ISO_11548_1, UNSUPPORTED, 0,
      { "ISO-11548-1", "ISO_11548-1", "ISO_TR_11548-1", "csISO115481", NULL }
   },
   /*
    * Source: See <http://www.iana.org/assignments/charset-reg/KZ-1048>  
    *    [Veremeev, Kikkarin]
    */
   { 119, WINUNDEF, STRING_ENCODING_KZ_1048, IN_FULL_ICU, 0,
      { "KZ-1048", "STRK1048-2002", "RK1048", "csKZ1048", NULL }
   },
   /*
    * Source: the 2-octet Basic Multilingual Plane, aka Unicode
    *         this needs to specify network byte order: the standard
    *         does not specify (it is a 16-bit integer space)
    */
   { 1000, WINUNDEF, STRING_ENCODING_ISO_10646_UCS_2, SUPPORTED, 0,
      { "ISO-10646-UCS-2", "csUnicode", "ibm-1204", "ibm-1205", "unicode",
        "ucs-2", NULL }
   },
   /*
    * Source: the full code space. (same comment about byte order,
    *         these are 31-bit numbers.
    */
   { 1001, WINUNDEF, STRING_ENCODING_ISO_10646_UCS_4, SUPPORTED, 0,
      { "ISO-10646-UCS-4", "csUCS4", "ibm-1236", "ibm-1237", "ucs-4", NULL }
   },
   /*
    * Source: ASCII subset of Unicode.  Basic Latin = collection 1
    *         See ISO 10646, Appendix A
    *          Not supported by ICU
    */
   { 1002, WINUNDEF, STRING_ENCODING_ISO_10646_UCS_BASIC, UNSUPPORTED, 0,
      { "ISO-10646-UCS-Basic", "csUnicodeASCII", NULL }
   },
   /*
    * Source: ISO Latin-1 subset of Unicode. Basic Latin and Latin-1 
    *          Supplement  = collections 1 and 2.  See ISO 10646, 
    *          Appendix A.  See RFC 1815.
    *          Not supported by ICU
    */
   { 1003, WINUNDEF, STRING_ENCODING_ISO_10646_UNICODE_LATIN1, UNSUPPORTED, 0,
      { "ISO-10646-Unicode-Latin1", "csUnicodeLatin1", "ISO-10646", NULL }
   },
   /*
    * Source: ISO 10646 Japanese, see RFC 1815.
    *          Not supported by ICU
    */
   { MIBUNDEF, WINUNDEF, STRING_ENCODING_ISO_10646_J_1, UNSUPPORTED, 0,
      { "ISO-10646-J-1", NULL }
   },
   /*
    * Source: IBM Latin-2, -3, -5, Extended Presentation Set, GCSGID: 1261
    *          Not supported by ICU
    */
   { 1005, WINUNDEF, STRING_ENCODING_ISO_UNICODE_IBM_1261, UNSUPPORTED, 0,
      { "ISO-Unicode-IBM-1261", "csUnicodeIBM1261", NULL }
   },
   /*
    * Source: IBM Latin-4 Extended Presentation Set, GCSGID: 1268
    *          Not supported by ICU
    */
   { 1006, WINUNDEF, STRING_ENCODING_ISO_UNICODE_IBM_1268, UNSUPPORTED, 0,
      { "ISO-Unicode-IBM-1268", "csUnicodeIBM1268", NULL }
   },
   /*
    * Source: IBM Cyrillic Greek Extended Presentation Set, GCSGID: 1276
    *          Not supported by ICU
    */
   { 1007, WINUNDEF, STRING_ENCODING_ISO_UNICODE_IBM_1276, UNSUPPORTED, 0,
      { "ISO-Unicode-IBM-1276", "csUnicodeIBM1276", NULL }
   },
   /*
    * Source: IBM Arabic Presentation Set, GCSGID: 1264
    *          Not supported by ICU
    */
   { 1008, WINUNDEF, STRING_ENCODING_ISO_UNICODE_IBM_1264, UNSUPPORTED, 0,
      { "ISO-Unicode-IBM-1264", "csUnicodeIBM1264", NULL }
   },
   /*
    * Source: IBM Hebrew Presentation Set, GCSGID: 1265
    *          Not supported by ICU
    */
   { 1009, WINUNDEF, STRING_ENCODING_ISO_UNICODE_IBM_1265, UNSUPPORTED, 0,
      { "ISO-Unicode-IBM-1265", "csUnicodeIBM1265", NULL }
   },
   /*
    * Source: RFC 1641, not supported by ICU
    */
   { 1010, WINUNDEF, STRING_ENCODING_UNICODE_1_1, UNSUPPORTED, 0,
      { "UNICODE-1-1", "csUnicode11", NULL }
   },
   /*
    * Source: SCSU See (http://www.iana.org/assignments/charset-reg/SCSU)
    *     [Scherer]
    */
   { 1011, WINUNDEF, STRING_ENCODING_SCSU, SUPPORTED, 0,
      { "SCSU", "ibm-1212", "ibm-1213", NULL }
   },
   /*
    * Source: RFC 2152
    */
   { 1012, 65000, STRING_ENCODING_UTF_7, SUPPORTED, 0,
      { "UTF-7", NULL }
   },
   /*
    * Source: RFC 2781
    */
   { 1013, 1201, STRING_ENCODING_UTF16_BE, SUPPORTED, 0,
      { "UTF-16BE", "x-utf-16be", "ibm-1200", "ibm-1201", "ibm-13488",
        "ibm-13489", "ibm-17584", "ibm-17585", "ibm-21680", "ibm-21681",
        "ibm-25776", "ibm-25777", "ibm-29872", "ibm-29873", "ibm-61955",
        "ibm-61956", "cp1200", "cp1201", "UTF16_BigEndian", 
        "UnicodeBigUnmarked", NULL }
   },
   /*
    * Source: RFC 2781
    */
   { 1014, 1200, STRING_ENCODING_UTF16_LE, SUPPORTED, 0,
      { "UTF-16LE", "x-utf-16le", "ibm-1202", "ibm-1203", "ibm-13490",
        "ibm-13491", "ibm-17586", "ibm-17587", "ibm-21682", "ibm-21683",
        "ibm-25778", "ibm-25779", "ibm-29874", "ibm-29875",
        "UTF16_LittleEndian", "UnicodeLittleUnmarked", NULL }
   },
   /*
    * Source: RFC 2781
    */
   { 1015, WINUNDEF, STRING_ENCODING_UTF16_XE, SUPPORTED, 0,
      { "UTF-16", NULL }
   },
   /*
    * Source: <http://www.unicode.org/unicode/reports/tr26>
    */
   { 1016, WINUNDEF, STRING_ENCODING_CESU_8, SUPPORTED, 0,
      { "CESU-8", "csCESU-8", "ibm-9400", NULL }
   },
   /*
    * Source: <http://www.unicode.org/unicode/reports/tr19/>
    */
   { 1017, WINUNDEF, STRING_ENCODING_UTF32_XE, SUPPORTED, 0,
      { "UTF-32", NULL }
   },
   /*
    * Source: <http://www.unicode.org/unicode/reports/tr19/>
    */
   { 1018, 12001, STRING_ENCODING_UTF32_BE, SUPPORTED, 0,
      { "UTF-32BE", "UTF32_BigEndian", "ibm-1232", "ibm-1233", "ibm-9424",
         NULL }
   },
   /*
    * Source: <http://www.unicode.org/unicode/reports/tr19/>
    */
   { 1019, 12000, STRING_ENCODING_UTF32_LE, SUPPORTED, 0,
      { "UTF-32LE", "UTF32_LittleEndian", "ibm-1234", "ibm-1235", NULL }
   },
   /*
    * Source: http://www.unicode.org/notes/tn6/
    */
   { 1020, WINUNDEF, STRING_ENCODING_BOCU_1, SUPPORTED, 0,
      { "BOCU-1", "csBOCU-1", "ibm-1214", "ibm-1215", NULL }
   },
   /*
    * Source: Extended ISO 8859-1 Latin-1 for Windows 3.0.  
    *         PCL Symbol Set id: 9U
    */
   { 2000, WINUNDEF, STRING_ENCODING_ISO_8859_1_WINDOWS_3_0_LATIN_1,
     UNSUPPORTED, 0,
      { "ISO-8859-1-Windows-3.0-Latin-1", "csWindows30Latin1", NULL }
   },
   /*
    * Source: Extended ISO 8859-1 Latin-1 for Windows 3.1.  
    *         PCL Symbol Set id: 19U
    *         Not supported by ICU
    */
   { 2001, WINUNDEF, STRING_ENCODING_ISO_8859_1_WINDOWS_3_1_LATIN_1,
     UNSUPPORTED, 0,
      { "ISO-8859-1-Windows-3.1-Latin-1", "csWindows31Latin1", NULL }
   },
   /*
    * Source: Extended ISO 8859-2.  Latin-2 for Windows 3.1.
    *         PCL Symbol Set id: 9E
    *         Not supported by ICU
    */
   { 2002, WINUNDEF, STRING_ENCODING_ISO_8859_2_WINDOWS_LATIN_2,
     UNSUPPORTED, 0,
      { "ISO-8859-2-Windows-Latin-2", "csWindows31Latin2", NULL }
   },
   /*
    * Source: Extended ISO 8859-9.  Latin-5 for Windows 3.1
    *         PCL Symbol Set id: 5T
    *         Not supported by ICU
    */
   { 2003, WINUNDEF, STRING_ENCODING_ISO_8859_9_WINDOWS_LATIN_5,
     UNSUPPORTED, 0,
      { "ISO-8859-9-Windows-Latin-5", "csWindows31Latin5", NULL }
   },
   /*
    * Source: LaserJet IIP Printer User's Manual, 
    *         HP part no 33471-90901, Hewlet-Packard, June 1989.
    */
   { 2004, WINUNDEF, STRING_ENCODING_HP_ROMAN8, IN_FULL_ICU, 0,
      { "hp-roman8", "roman8", "r8", "csHPRoman8", "ibm-1051_P100-1995",
        "ibm-1051", NULL }
   },
   /*
    * Source: PostScript Language Reference Manual
    *         PCL Symbol Set id: 10J
    */
   { 2005, WINUNDEF, STRING_ENCODING_ADOBE_STANDARD_ENCODING,
     IN_FULL_ICU, 0,
      { "Adobe-Standard-Encoding", "csAdobeStandardEncoding", 
        "ibm-1276_P100-1995", "ibm-1276", NULL }
   },
   /*
    * Source: Ventura US.  ASCII plus characters typically used in 
    *         publishing, like pilcrow, copyright, registered, trade mark, 
    *         section, dagger, and double dagger in the range A0 (hex) 
    *         to FF (hex).  
    *         PCL Symbol Set id: 14J
    *         Not supported by ICU
    */
   { 2006, WINUNDEF, STRING_ENCODING_VENTURA_US, UNSUPPORTED, 0,
      { "Ventura-US", "csVenturaUS", NULL }
   },
   /*
    * Source: Ventura International.  ASCII plus coded characters similar 
    *         to Roman8.
    *         PCL Symbol Set id: 13J
    *         Not supported by ICU
    */
   { 2007, WINUNDEF, STRING_ENCODING_VENTURA_INTERNATIONAL, UNSUPPORTED, 0,
      { "Ventura-International", "csVenturaInternational", NULL }
   },
   /*
    * Source: VAX/VMS User's Manual, 
    *         Order Number: AI-Y517A-TE, April 1986.
    */
   { 2008, WINUNDEF, STRING_ENCODING_DEC_MCS, IN_FULL_ICU, 0,
      { "DEC-MCS", "dec", "csDECMCS", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2009, 850, STRING_ENCODING_IBM_850, SUPPORTED, 0,
      { "IBM850", "cp850", "850", "csPC850Multilingual", 
        "ibm-850_P100-1995", NULL }
   },
   /*
    * Source: PC Danish Norwegian
    *         8-bit PC set for Danish Norwegian
    *         PCL Symbol Set id: 11U
    *         Not supported by ICU
    */
   { 2012, WINUNDEF, STRING_ENCODING_PC8_DANISH_NORWEGIAN, UNSUPPORTED, 0,
      { "PC8-Danish-Norwegian", "csPC8DanishNorwegian", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2013, 862, STRING_ENCODING_IBM_862, SUPPORTED, 0,
      { "IBM862", "cp862", "862", "csPC862LatinHebrew", 
        "ibm-862_P100-1995", "DOS-862", NULL }
   },
   /*
    * Source: PC Latin Turkish.  PCL Symbol Set id: 9T
    *         Not supported by ICU
    */
   { 2014, WINUNDEF, STRING_ENCODING_PC8_TURKISH, UNSUPPORTED, 0,
      { "PC8-Turkish", "csPC8Turkish", NULL }
   },
   /*
    * Source: Presentation Set, CPGID: 259
    *         Not supported by ICU
    */
   { 2015, WINUNDEF, STRING_ENCODING_IBM_SYMBOLS, UNSUPPORTED, 0,
      { "IBM-Symbols", "csIBMSymbols", NULL }
   },
   /*
    * Source: Presentation Set, CPGID: 838
    */
   { 2016, 20838, STRING_ENCODING_IBM_THAI, IN_FULL_ICU, 0,
      { "IBM-Thai", "csIBMThai", "ibm-838_P100-1995", "ibm-838", 
        "IBM838", "cp838", "838", "ibm-9030", NULL }
   },
   /*
    * Source: PCL 5 Comparison Guide, Hewlett-Packard,
    *         HP part number 5961-0510, October 1992
    *         PCL Symbol Set id: 1U
    *         Not supported by ICU
    */
   { 2017, WINUNDEF, STRING_ENCODING_HP_LEGAL, UNSUPPORTED, 0,
      { "HP-Legal", "csHPLegal", NULL }
   },
   /*
    * Source: PCL 5 Comparison Guide, Hewlett-Packard,
    *         HP part number 5961-0510, October 1992
    *         PCL Symbol Set id: 15U
    *         Not supported by ICU
    */
   { 2018, WINUNDEF, STRING_ENCODING_HP_PI_FONT, UNSUPPORTED, 0,
      { "HP-Pi-font", "csHPPiFont", NULL }
   },
   /*
    * Source: PCL 5 Comparison Guide, Hewlett-Packard,
    *         HP part number 5961-0510, October 1992
    *         PCL Symbol Set id: 8M
    *         Not supported by ICU
    */
   { 2019, WINUNDEF, STRING_ENCODING_HP_MATH8, UNSUPPORTED, 0,
      { "HP-Math8", "csHPMath8", NULL }
   },
   /*
    * Source: PostScript Language Reference Manual
    *         PCL Symbol Set id: 5M
    *         Not supported by ICU
    */
   { 2020, WINUNDEF, STRING_ENCODING_ADOBE_SYMBOL_ENCODING, UNSUPPORTED, 0,
      { "Adobe-Symbol-Encoding", "csHPPSMath", NULL }
   },
   /*
    * Source: PCL 5 Comparison Guide, Hewlett-Packard,
    *         HP part number 5961-0510, October 1992
    *         PCL Symbol Set id: 7J
    *         Not supported by ICU
    */
   { 2021, WINUNDEF, STRING_ENCODING_HP_DESKTOP, UNSUPPORTED, 0,
      { "HP-DeskTop", "csHPDesktop", NULL }
   },
   /*
    * Source: PCL 5 Comparison Guide, Hewlett-Packard,
    *         HP part number 5961-0510, October 1992
    *         PCL Symbol Set id: 6M
    *         Not supported by ICU
    */
   { 2022, WINUNDEF, STRING_ENCODING_VENTURA_MATH, UNSUPPORTED, 0,
      { "Ventura-Math", "csVenturaMath", NULL }
   },
   /*
    * Source: PCL 5 Comparison Guide, Hewlett-Packard,
    *         HP part number 5961-0510, October 1992
    *         PCL Symbol Set id: 6J
    *         Not supported by ICU
    */
   { 2023, WINUNDEF, STRING_ENCODING_MICROSOFT_PUBLISHING, UNSUPPORTED, 0,
      { "Microsoft-Publishing", "csMicrosoftPublishing", NULL }
   },
   /*
    * Source: Windows Japanese.  A further extension of Shift_JIS
    *         to include NEC special characters (Row 13), NEC
    *         selection of IBM extensions (Rows 89 to 92), and IBM
    *         extensions (Rows 115 to 119).  The CCS's are
    *         JIS X0201:1997, JIS X0208:1997, and these extensions.
    *         This charset can be used for the top-level media type "text",
    *         but it is of limited or specialized use (see RFC2278).
    *         PCL Symbol Set id: 19K
    */
   { 2024, WINUNDEF, STRING_ENCODING_WINDOWS_31J, SUPPORTED, 0,
      { "Windows-31J", "csWindows31J", NULL }
   },
   /*
    * Source: Chinese for People's Republic of China (PRC) mixed one byte, 
    *         two byte set: 
    *           20-7E = one byte ASCII 
    *           A1-FE = two byte PRC Kanji 
    *         See GB 2312-80 
    *         PCL Symbol Set Id: 18C
    */
   { 2025, WINUNDEF, STRING_ENCODING_GB_2312, IN_FULL_ICU, 0,
      { "GB2312", "csGB2312", "ibm-1383_P110-1999", "ibm-1383",
        "cp1383", "1383", "EUC-CN", "ibm-eucCN", "hp15CN",
        "ibm-1383_VPUA", NULL }
   },
   /*
    * Source: Chinese for Taiwan Multi-byte set.
    *         PCL Symbol Set Id: 18T
    */
   { 2026, 950, STRING_ENCODING_BIG_5, SUPPORTED, 0,
      { "Big5", "csBig5", "windows-950", "windows-950-2000",
        "x-big5", NULL }
   },
   /*
    * Alternate ICU converter for Windows-950 (Big5)
    */
   { MIBUNDEF, 950, STRING_ENCODING_IBM_1373, SUPPORTED, 0,
      { "ibm-1373_P100-2002", "ibm-1373", NULL }
   },
   /*
    * Source: The Unicode Standard ver1.0, ISBN 0-201-56788-1, Oct 1991
    */
   { 2027, WINUNDEF, STRING_ENCODING_MACINTOSH, IN_FULL_ICU, 0,
      { "macintosh", "mac", "csMacintosh", "macos-0_2-10.2", 
        "macroman", "x-macroman", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2028, 37, STRING_ENCODING_IBM_037, IN_FULL_ICU, 0,
      { "IBM037", "cp037", "ebcdic-cp-us", "ebcdic-cp-ca", "ebcdic-cp-wt",
        "ebcdic-cp-nl", "csIBM037", "ibm-37_P100-1995", "ibm-37",
        "037", "cpibm37", "cp37", NULL }
   },
   /*
    * Source: IBM 3174 Character Set Ref, GA27-3831-02, March 1990
    *         Not supported by ICU
    */
   { 2029, WINUNDEF, STRING_ENCODING_IBM_038, UNSUPPORTED, 0,
      { "IBM038", "EBCDIC-INT", "cp038", "csIBM038", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2030, 20273, STRING_ENCODING_IBM_273, IN_FULL_ICU, 0,
      { "IBM273", "CP273", "csIBM273", "ibm-273_P100-1995",
        "ebcdic-de", "273", NULL }
   },
   /*
    * Source: IBM 3174 Character Set Ref, GA27-3831-02, March 1990
    */
   { 2031, WINUNDEF, STRING_ENCODING_IBM_274, IN_FULL_ICU, 0,
      { "IBM274", "EBCDIC-BE", "CP274", "csIBM274", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2032, WINUNDEF, STRING_ENCODING_IBM_275, IN_FULL_ICU, 0,
      { "IBM275", "EBCDIC-BR", "cp275", "csIBM275", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2033, 20277, STRING_ENCODING_IBM_277, IN_FULL_ICU, 0,
      { "IBM277", "EBCDIC-CP-DK", "EBCDIC-CP-NO", "csIBM277",
        "ibm-277_P100-1995", "cp277", "ebcdic-dk", "277", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2034, 20278, STRING_ENCODING_IBM_278, IN_FULL_ICU, 0,
      { "IBM278", "CP278", "ebcdic-cp-fi", "ebcdic-cp-se", "csIBM278",
        "ibm-278_P100-1995", "ebcdic-sv", "278", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2035, 20280, STRING_ENCODING_IBM_280, IN_FULL_ICU, 0,
      { "IBM280", "CP280", "ebcdic-cp-it", "csIBM280",
        "ibm-280_P100-1995", "280", NULL }
   },
   /*
    * Source: IBM 3174 Character Set Ref, GA27-3831-02, March 1990
    *         Not supported by ICU
    */
   { 2036, WINUNDEF, STRING_ENCODING_IBM_281, UNSUPPORTED, 0,
      { "IBM281", "EBCDIC-JP-E", "cp281", "csIBM281", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2037, 20284, STRING_ENCODING_IBM_284, IN_FULL_ICU, 0,
      { "IBM284", "CP284", "ebcdic-cp-es", "csIBM284", 
        "ibm-284_P100-1995", "cpibm284", "284", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2038, 20285, STRING_ENCODING_IBM_285, IN_FULL_ICU, 0,
      { "IBM285", "CP285", "ebcdic-cp-gb", "csIBM285", 
        "ibm-284_P100-1995", "cpibm284", "284", NULL }
   },
   /*
    * Source: IBM 3174 Character Set Ref, GA27-3831-02, March 1990
    */
   { 2039, 20290, STRING_ENCODING_IBM_290, IN_FULL_ICU, 0,
      { "IBM290", "cp290", "EBCDIC-JP-kana", "csIBM290", 
        "ibm-290_P100-1995", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2040, 20297, STRING_ENCODING_IBM_297, IN_FULL_ICU, 0,
      { "IBM297", "cp297", "ebcdic-cp-fr", "csIBM297", 
        "ibm-297_P100-1995", "cpibm297", "297", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990, 
    *         IBM NLS RM p 11-11
    */
   { 2041, 20420, STRING_ENCODING_IBM_420, IN_FULL_ICU, 0,
      { "IBM420", "cp420", "ebcdic-cp-ar1", "csIBM420", 
        "ibm-420_X120-1999", "420", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    *         Not supporeted by ICU
    */
   { 2042, 20423, STRING_ENCODING_IBM_423, UNSUPPORTED, 0,
      { "IBM423", "cp423", "ebcdic-cp-gr", "csIBM423", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2043, 20424, STRING_ENCODING_IBM_424, IN_FULL_ICU, 0,
      { "IBM424", "cp424", "ebcdic-cp-he", "csIBM424", 
        "ibm-424_P100-1995", "424", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2011, 437, STRING_ENCODING_IBM_437, SUPPORTED, 0,
      { "IBM437", "cp437", "437", "csPC8CodePage437", 
        "ibm-437_P100-1995", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2044, 500, STRING_ENCODING_IBM_500, IN_FULL_ICU, 0,
      { "IBM500", "CP500", "ebcdic-cp-be", "ebcdic-cp-ch", 
        "csIBM500", "ibm-500_P100-1995", "500", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2045, WINUNDEF, STRING_ENCODING_IBM_851, IN_FULL_ICU, 0,
      { "IBM851", "cp851", "851", "csIBM851", "ibm-851_P100-1995",
        "csPC851", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2010, 852, STRING_ENCODING_IBM_852, SUPPORTED, 0,
      { "IBM852", "cp852", "852", "csPCp852", "ibm-852_P100-1995",
        NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2046, 855, STRING_ENCODING_IBM_855, IN_FULL_ICU, 0,
      { "IBM855", "cp855", "855", "csIBM855", "ibm-855_P100-1995",
        "csPCp855", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2047, 857, STRING_ENCODING_IBM_857, SUPPORTED, 0,
      { "IBM857", "cp857", "857", "csIBM857", "ibm-857_P100-1995",
         NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2048, 860, STRING_ENCODING_IBM_860, IN_FULL_ICU, 0,
      { "IBM860", "cp860", "860", "csIBM860", "ibm-860_P100-1995",
         NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2049, 861, STRING_ENCODING_IBM_861, IN_FULL_ICU, 0,
      { "IBM861", "cp861", "861", "cp-is", "csIBM861", 
        "ibm-861_P100-1995", NULL }
   },
   /*
    * Source: IBM Keyboard layouts and code pages, PN 07G4586 June 1991
    */
   { 2050, 863, STRING_ENCODING_IBM_863, IN_FULL_ICU, 0,
      { "IBM863", "cp863", "863", "csIBM863", "ibm-863_P100-1995",
         NULL }
   },
   /*
    * Source: IBM Keyboard layouts and code pages, PN 07G4586 June 1991
    */
   { 2051, 864, STRING_ENCODING_IBM_864, IN_FULL_ICU, 0,
      { "IBM864", "cp864", "csIBM864", "ibm-864_X110-1999", NULL }
   },
   /*
    * Source: IBM DOS 3.3 Ref (Abridged), 94X9575 (Feb 1987)
    */
   { 2052, 865, STRING_ENCODING_IBM_865, IN_FULL_ICU, 0,
      { "IBM865", "cp865", "865", "csIBM865", "ibm-865_P100-1995",
         NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2053, WINUNDEF, STRING_ENCODING_IBM_868, IN_FULL_ICU, 0,
      { "IBM868", "CP868", "cp-ar", "csIBM868", "ibm-868_P100-1995",
        "868", NULL }
   },
   /*
    * Source: IBM Keyboard layouts and code pages, PN 07G4586 June 1991
    */
   { 2054, 869, STRING_ENCODING_IBM_869, IN_FULL_ICU, 0,
      { "IBM869", "cp869", "869", "cp-gr", "csIBM869", "ibm-869_P100-1995",
         NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2055, 870, STRING_ENCODING_IBM_870, IN_FULL_ICU, 0,
      { "IBM870", "CP870", "ebcdic-cp-roece", "ebcdic-cp-yu", 
        "csIBM870", "ibm-870_P100-1995", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2056, 20871, STRING_ENCODING_IBM_871, IN_FULL_ICU, 0,
      { "IBM871", "CP871", "ebcdic-cp-is", "csIBM871",
        "ibm-871_P100-1995", "ebcdic-is", "871", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2057, 20880, STRING_ENCODING_IBM_880, IN_FULL_ICU, 0,
      { "IBM880", "cp880", "EBCDIC-Cyrillic", "csIBM880", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    *         Not supported by ICU
    */
   { 2058, WINUNDEF, STRING_ENCODING_IBM_891, UNSUPPORTED, 0,
      { "IBM891", "cp891", "csIBM891", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    *         Not supported by ICU
    */
   { 2059, WINUNDEF, STRING_ENCODING_IBM_903, UNSUPPORTED, 0,
      { "IBM903", "cp903", "csIBM903", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    *         Not supported by ICU
    */
   { 2060, WINUNDEF, STRING_ENCODING_IBM_904, UNSUPPORTED, 0,
      { "IBM904", "cp904", "904", "csIBBM904", NULL }
   },
   /*
    * Source: IBM 3174 Character Set Ref, GA27-3831-02, March 1990
    */
   { 2061, 20905, STRING_ENCODING_IBM_905, IN_FULL_ICU, 0,
      { "IBM905", "CP905", "ebcdic-cp-tr", "csIBM905", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2062, WINUNDEF, STRING_ENCODING_IBM_918, IN_FULL_ICU, 0,
      { "IBM918", "CP918", "ebcdic-cp-ar2", "csIBM918", 
        "ibm-918_P100-1995", NULL }
   },
   /*
    * Source: IBM NLS RM Vol2 SE09-8002-01, March 1990
    */
   { 2063, 1026, STRING_ENCODING_IBM_1026, IN_FULL_ICU, 0,
      { "IBM1026", "CP1026", "csIBM1026", "ibm-1026_P100-1995",
        "1026", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2064, WINUNDEF, STRING_ENCODING_EBCDIC_AT_DE, UNSUPPORTED, 0,
      { "EBCDIC-AT-DE", "csIBMEBCDICATDE", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987 
    */
   { 2065, WINUNDEF, STRING_ENCODING_EBCDIC_AT_DE_A, IN_FULL_ICU, 0,
      { "EBCDIC-AT-DE-A", "csEBCDICATDEA", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2066, WINUNDEF, STRING_ENCODING_EBCDIC_CA_FR, UNSUPPORTED, 0,
      { "EBCDIC-CA-FR", "csEBCDICCAFR", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2067, WINUNDEF, STRING_ENCODING_EBCDIC_DK_NO, UNSUPPORTED, 0,
      { "EBCDIC-DK-NO", "csEBCDICDKNO", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2068, WINUNDEF, STRING_ENCODING_EBCDIC_DK_NO_A, UNSUPPORTED, 0,
      { "EBCDIC-DK-NO-A", "csEBCDICDKNOA", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2069, WINUNDEF, STRING_ENCODING_EBCDIC_FI_SE, UNSUPPORTED, 0,
      { "EBCDIC-FI-SE", "csEBCDICFISE", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2070, WINUNDEF, STRING_ENCODING_EBCDIC_FI_SE_A, UNSUPPORTED, 0,
      { "EBCDIC-FI-SE-A", "csEBCDICFISEA", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2071, WINUNDEF, STRING_ENCODING_EBCDIC_FR, UNSUPPORTED, 0,
      { "EBCDIC-FR", "csEBCDICFR", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2072, WINUNDEF, STRING_ENCODING_EBCDIC_IT, UNSUPPORTED, 0,
      { "EBCDIC-IT", "csEBCDICIT", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2073, WINUNDEF, STRING_ENCODING_EBCDIC_PT, UNSUPPORTED, 0,
      { "EBCDIC-PT", "csEBCDICPT", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2074, WINUNDEF, STRING_ENCODING_EBCDIC_ES, UNSUPPORTED, 0,
      { "EBCDIC-ES", "csEBCDICES", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2075, WINUNDEF, STRING_ENCODING_EBCDIC_ES_A, UNSUPPORTED, 0,
      { "EBCDIC-ES-A", "csEBCDICESA", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2076, WINUNDEF, STRING_ENCODING_EBCDIC_ES_S, UNSUPPORTED, 0,
      { "EBCDIC-ES-S", "csEBCDICESS", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2077, WINUNDEF, STRING_ENCODING_EBCDIC_UK, UNSUPPORTED, 0,
      { "EBCDIC-UK", "csEBCDICUK", NULL }
   },
   /*
    * Source: IBM 3270 Char Set Ref Ch 10, GA27-2837-9, April 1987
    *         Not supported by ICU
    */
   { 2078, WINUNDEF, STRING_ENCODING_EBCDIC_US, UNSUPPORTED, 0,
      { "EBCDIC-US", "csEBCDICUS", NULL }
   },
   /*
    *         Not supported by ICU
    */
   { 2079, WINUNDEF, STRING_ENCODING_UNKNOWN_8BIT, UNSUPPORTED, 0,
      { "UNKNOWN-8BIT", "csUnknown8BiT", NULL }
   },
   /*
    * Source: RFC 1345, also known as "mnemonic+ascii+38"
    *         Not supported by ICU
    */
   { 2080, WINUNDEF, STRING_ENCODING_MNEMONIC, UNSUPPORTED, 0,
      { "MNEMONIC", "csMnemonic", NULL }
   },
   /*
    * Source: RFC 1345, also known as "mnemonic+ascii+8200"
    *         Not supported by ICU
    */
   { 2081, WINUNDEF, STRING_ENCODING_MNEM, UNSUPPORTED, 0,
      { "MNEM", "csMnem", NULL }
   },
   /*
    * Source: RFC 1456
    *         Not supported by ICU
    */
   { 2082, WINUNDEF, STRING_ENCODING_VISCII, UNSUPPORTED, 0,
      { "VISCII", "csVISCII", NULL }
   },
   /*
    * Source: RFC 1456
    *         Not supported by ICU
    */
   { 2083, WINUNDEF, STRING_ENCODING_VIQR, UNSUPPORTED, 0,
      { "VIQR", "csVIQR", NULL }
   },
   /*
    * Source: RFC 1489, based on GOST-19768-74, ISO-6937/8, 
    *         INIS-Cyrillic, ISO-5427.
    */
   { 2084, 20866, STRING_ENCODING_KOI8_R, IN_FULL_ICU, 0,
      { "KOI8-R", "csKOI8R", "koi8", "cp878", "ibm-878",
        "ibm-878_P100-1996", NULL }
   },
   /*
    * Source: RFC 1842, RFC 1843 [RFC1842, RFC1843]
    */
   { 2085, 52936, STRING_ENCODING_HZ_GB_2312, SUPPORTED, 0,
      { "HZ-GB-2312", "HZ", NULL }
   },
   /*
    * Source: IBM NLDG Volume 2 (SE09-8002-03) August 1994
    */
   { 2086, 866, STRING_ENCODING_IBM_866, SUPPORTED, 0,
      { "IBM866", "cp866", "866", "csIBM866", "ibm-866_P100-1995",
         NULL }
   },
   /*
    * Source: HP PCL 5 Comparison Guide (P/N 5021-0329) pp B-13, 1996
    */
   { 2087, 775, STRING_ENCODING_IBM_775, SUPPORTED, 0,
      { "IBM775", "cp775", "csPC775Baltic", "ibm-775_P100-1996",
        "775", NULL }
   },
   /*
    * Source: RFC 2319
    */
   { 2088, 21866, STRING_ENCODING_KOI8_U, IN_FULL_ICU, 0,
      { "KOI8-U", "ibm-1168", "ibm-1168_P100-2002", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM00858) 
    *   [Mahdi]
    */
   { 2089, 858, STRING_ENCODING_IBM_00858, SUPPORTED, 0,
      { "IBM00858", "CCSID00858", "CP00858", "PC-Multilingual-850+euro", 
        "ibm-858", "cp858", "ibm-858_P100-1997", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM00924)
    *   [Mahdi]
    */
   { 2090, 20924, STRING_ENCODING_IBM_00924, IN_FULL_ICU, 0,
      { "IBM00924", "CCSID00924", "CP00924", "ebcdic-Latin9--euro", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01140)
    *    [Mahdi]
    */
   { 2091, 1140, STRING_ENCODING_IBM_01140, IN_FULL_ICU, 0,
      { "IBM01140", "CCSID01140", "CP01140", "ebcdic-us-37+euro", 
        "ibm-1140", "cp1140", "ibm-1140_P100-1997", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01141)
    *    [Mahdi]
    */
   { 2092, 1141, STRING_ENCODING_IBM_01141, IN_FULL_ICU, 0,
      { "IBM01141", "CCSID01141", "CP01141", "ebcdic-de-273+euro",
        "ibm-1141", "cp1141", "ibm-1141_P100-1997", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01142)
    *    [Mahdi]
    */
   { 2093, 1142, STRING_ENCODING_IBM_01142, IN_FULL_ICU, 0,
      { "IBM01142", "CCSID01142", "CP01142", "ebcdic-dk-277+euro",
        "ebcdic-no-277+euro", "ibm-1142", "cp1142", "ibm-1142_P100-1997",
         NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01143)
    *    [Mahdi]
    */
   { 2094, 1143, STRING_ENCODING_IBM_01143, IN_FULL_ICU, 0,
      { "IBM01143", "CCSID01143", "CP01143", "ebcdic-fi-278+euro",
        "ebcdic-se-278+euro", "ibm-1143", "cp1143", "ibm-1143_P100-1997",
         NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01144)
    *    [Mahdi]
    */
   { 2095, 1144, STRING_ENCODING_IBM_01144, IN_FULL_ICU, 0,
      { "IBM01144", "CCSID01144", "CP01144", "ebcdic-it-280+euro", 
        "ibm-1144", "cp1144", "ibm-1144_P100-1997", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01145)
    *    [Mahdi]
    */
   { 2096, 1145, STRING_ENCODING_IBM_01145, IN_FULL_ICU, 0,
      { "IBM01145", "CCSID01145", "CP01145", "ebcdic-es-284+euro", 
        "ibm-1145", "cp1145", "ibm-1145_P100-1997", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01146)
    *    [Mahdi]
    */
   { 2097, 1146, STRING_ENCODING_IBM_01146, IN_FULL_ICU, 0,
      { "IBM01146", "CCSID01146", "CP01146", "ebcdic-gb-285+euro", 
        "ibm-1146", "cp1146", "ibm-1146_P100-1997", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01147)
    *    [Mahdi]
    */
   { 2098, 1147, STRING_ENCODING_IBM_01147, IN_FULL_ICU, 0,
      { "IBM01147", "CCSID01147", "CP01147", "ebcdic-fr-297+euro",
        "ibm-1147", "cp1147", "ibm-1147_P100-1997", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01148)
    *    [Mahdi]
    */
   { 2099, 1148, STRING_ENCODING_IBM_01148, IN_FULL_ICU, 0,
      { "IBM01148", "CCSID01148", "CP01148", "ebcdic-international-500+euro",
        "ibm-1148", "cp1148", "ibm-1148_P100-1997", NULL }
   },
   /*
    * Source: IBM See (http://www.iana.org/assignments/charset-reg/IBM01149)
    *    [Mahdi]
    */
   { 2100, 1149, STRING_ENCODING_IBM_01149, IN_FULL_ICU, 0,
      { "IBM01149", "CCSID01149", "CP01149", "ebcdic-is-871+euro",
        "ibm-1149", "cp1149", "ibm-1149_P100-1997", NULL }
   },
   /*
    * Source:   See (http://www.iana.org/assignments/charset-reg/Big5-HKSCS) 
    */
   { 2101, WINUNDEF, STRING_ENCODING_BIG_5_HK, IN_FULL_ICU, 0,
      { "Big5-HKSCS", "ibm-1375_P100-2007", "ibm-1375", "big5hk",
        "HKSCS-BIG5", NULL }
   },
   /*
    * Alternate ICU converter for Big-5-HKSCS 
    */
   { 2101, WINUNDEF, STRING_ENCODING_IBM_5471, IN_FULL_ICU, 0,
      { "ibm-5471_P100-2006", "ibm-5471", "MS950_HKSCS", "hkbig5",
         NULL }
   },
   /*
    * Source: IBM1047 (EBCDIC Latin 1/Open Systems)
    * http://www-1.ibm.com/servers/eserver/iseries/software/globalization/
    *        pdf/cp01047z.pdf
    */
   { 2102, 1047, STRING_ENCODING_IBM_1047, IN_FULL_ICU, 0,
      { "IBM1047", "IBM-1047", "cp1047", "1047", "ibm-1047_P100-1995",
         NULL }
   },
   /*
    * Source: See (http://www.iana.org/assignments/charset-reg/PTCP154)
    *         Not supported by ICU
    */
   { 2103, WINUNDEF, STRING_ENCODING_PTCP154, UNSUPPORTED, 0,
      { "PTCP154", "csPTCP154", "PT154", "CP154", "Cyrillic-Asian", NULL }
   },
   /*
    * Source:  See (http://www.amiga.ultranet.ru/Amiga-1251.html)
    *          Not supported by ICU
    */
   { 2104, WINUNDEF, STRING_ENCODING_AMIGA_1251, UNSUPPORTED, 0,
      { "Amiga-1251", "Ami1251", "Amiga1251", "Ami-1251", NULL }
   },
   /*
    * Source:  See <http://www.iana.org/assignments/charset-reg/KOI7-switched>
    * Aliases: None
    *          Not supported by ICU
    */
   { 2105, WINUNDEF, STRING_ENCODING_KOI7_SWITCHED, UNSUPPORTED, 0,
      { "KOI7-switched", NULL }
   },
   /*
    * Source: See <http://www.iana.org/assignments/charset-reg/BRF>
    * [Thibault]
    *         Not supported by ICU
    */
   { 2106, WINUNDEF, STRING_ENCODING_BRF, UNSUPPORTED, 0,
      { "BRF", "csBRF", NULL }
   },
   /*
    * Source: See <http://www.iana.org/assignments/charset-reg/TSCII>
    * [Kalyanasundaram]
    *         Not supported by ICU
    */
   { 2107, WINUNDEF, STRING_ENCODING_TSCII, UNSUPPORTED, 0,
      { "TSCII", "csTSCII", NULL }
   },
   /*
    * Source: Microsoft  
    *  (http://www.iana.org/assignments/charset-reg/windows-1250) [Lazhintseva]
    */
   { 2250, 1250, STRING_ENCODING_WINDOWS_1250, SUPPORTED, 0,
      { "windows-1250", "ibm-5346_P100-1998", "ibm-5346", "cp1250", 
        "ibm-1250_P100-1995", "ibm-1250", NULL }
   },
   /*
    * Source: Microsoft
    *  (http://www.iana.org/assignments/charset-reg/windows-1251) [Lazhintseva]
    */
   { 2251, 1251, STRING_ENCODING_WINDOWS_1251, SUPPORTED, 0,
      { "windows-1251", "ibm-5347_P100-1998", "ibm-5347", "cp1251", "ANSI1251",
        "ibm-1251_P100-1995", "ibm-1251", NULL }
   },
   /*
    * Source: Microsoft 
    *  (http://www.iana.org/assignments/charset-reg/windows-1252) [Wendt]
    */
   { 2252, 1252, STRING_ENCODING_WINDOWS_1252, SUPPORTED, 0,
      { "windows-1252", "ibm-5348_P100-1997", "ibm-5348", "cp1252", 
        "ibm-1252_P100-2000", "ibm-1252", NULL }
   },
   /*
    * Source: Microsoft 
    *  (http://www.iana.org/assignments/charset-reg/windows-1253) [Lazhintseva]
    */
   { 2253, 1253, STRING_ENCODING_WINDOWS_1253, SUPPORTED, 0,
      { "windows-1253", "ibm-5349_P100-1998", "ibm-5349", "cp1253",
        "ibm-1253", "ibm-1253_P100-1995", NULL }
   },
   /*
    * Source: Microsoft
    *  (http://www.iana.org/assignments/charset-reg/windows-1254) [Lazhintseva]
    */
   { 2254, 1254, STRING_ENCODING_WINDOWS_1254, SUPPORTED, 0,
      { "windows-1254", "ibm-5350_P100-1998", "ibm-5350", "cp1254", 
        "ibm-1254", "ibm-1254_P100-1995", NULL }
   },
   /*
    * Source: Microsoft 
    *  (http://www.iana.org/assignments/charset-reg/windows-1255) [Lazhintseva]
    */
   { 2255, 1255, STRING_ENCODING_WINDOWS_1255, SUPPORTED, 0,
      { "windows-1255", "ibm-9447_P100-2002", "ibm-9447", "cp1255", 
        "ibm-5351", "ibm-5351_P100-1998", NULL }
   },
   /*
    * Source: Microsoft
    *  (http://www.iana.org/assignments/charset-reg/windows-1256) [Lazhintseva]
    */
   { 2256, 1256, STRING_ENCODING_WINDOWS_1256, SUPPORTED, 0,
      { "windows-1256", "ibm-9448_X100-2005", "ibm-9448", "cp1256", 
        "ibm-5352", "ibm-5352_P100-1998", NULL }
   },
   /*
    * Source: Microsoft 
    *  (http://www.iana.org/assignments/charset-reg/windows-1257) [Lazhintseva]
    */
   { 2257, 1257, STRING_ENCODING_WINDOWS_1257, SUPPORTED, 0,
      { "windows-1257", "ibm-9449_P100-2002", "ibm-9449", "cp1257", 
        "ibm-5353", "ibm-5353_P100-1998", NULL }
   },
   /*
    * Source: Microsoft
    *  (http://www.iana.org/assignments/charset-reg/windows-1258) [Lazhintseva]
    */
   { 2258, 1258, STRING_ENCODING_WINDOWS_1258, SUPPORTED, 0,
      { "windows-1258", "ibm-5354_P100-1998", "ibm-5354", "cp1258", 
        "ibm-1258", "ibm-1258_P100-1997", NULL }
   },
   /*
    * Source: Thai Industrial Standards Institute (TISI)
    *    [Tantsetthi]
    */
   { 2259, WINUNDEF, STRING_ENCODING_TIS_620, SUPPORTED, 0,
      { "TIS-620", "windows-874-2000", "MS874", NULL }
   },
   /*
    * Windows specific entries for which there is no corresponding IANA mapping
    */
   
   /*
    * Windows-709: Arabic (ASMO-449+, BCON V4)
    *              Not supported by ICU
    */
   { MIBUNDEF, 709, STRING_ENCODING_WINDOWS_709, UNSUPPORTED, 0,
      { "Windows-709", "ASMO-449+", "BCON_V4", NULL }
   },
   /*
    * Windows-710: Arabic - Transparent Arabic
    *              Not supported by ICU
    */
   { MIBUNDEF, 710, STRING_ENCODING_WINDOWS_710, UNSUPPORTED, 0,
      { "Windows-710", NULL }
   },
   /*
    * DOS-720: Arabic (Transparent ASMO); Arabic (DOS)
    */
   { MIBUNDEF, 720, STRING_ENCODING_WINDOWS_720, SUPPORTED, 0,
      { "Windows-720", "DOS-720", "DOS_720", "ibm-720",
        "ibm-720_P100-1997", NULL }
   },
   /*
    * ibm737: OEM Greek (formerly 437G); Greek (DOS)
    */
   { MIBUNDEF, 737, STRING_ENCODING_WINDOWS_737, SUPPORTED, 0,
      { "Windows-737", "IBM737", "cp737", "737", "ibm-737_P100-1997",
         NULL }
   },
   /*
    * cp875: IBM EBCDIC Greek Modern
    *        ICU doesn't have "Windows-875" as an alias, use "cp875"
    */
   { MIBUNDEF, 875, STRING_ENCODING_WINDOWS_875, IN_FULL_ICU, 0,
      { "cp875", "ibm-875", "IBM875", "875", "ibm-875_P100-1995", NULL }
   },
   /*
    * Johab: Korean (Johab)
    *        Not supported by ICU
    */
   { MIBUNDEF, 1361, STRING_ENCODING_WINDOWS_1361, UNSUPPORTED, 0,
      { "Windows-1361", "Johab", NULL }
   },
   /*
    * macintosh: MAC Roman; Western European (Mac)
    * using the encoding names "mac" and "macintosh"
    * is probably a bad idea here
    */
   { MIBUNDEF, 10000, STRING_ENCODING_WINDOWS_10000, IN_FULL_ICU, 0,
      { "Windows-10000", NULL }
   },
   /*
    * x-mac-japanese: Japanese (Mac)
    *                 Not supported by ICU
    */
   { MIBUNDEF, 10001, STRING_ENCODING_WINDOWS_10001, UNSUPPORTED, 0,
      { "Windows-10001", "x-mac-japanese", NULL }
   },
   /*
    * x-mac-chinesetrad: MAC Traditional Chinese (Big5);
    *                    Chinese Traditional (Mac)
    *                    Not supported by ICU
    */
   { MIBUNDEF, 10002, STRING_ENCODING_WINDOWS_10002, UNSUPPORTED, 0,
      { "Windows-10002", "x-mac-chinesetrad", NULL }
   },
   /*
    * x-mac-korean: Korean (Mac)
    *               Not supported by ICU
    */
   { MIBUNDEF, 10003, STRING_ENCODING_WINDOWS_10003, UNSUPPORTED, 0,
      { "Windows-10003", "x-mac-korean", NULL }
   },
   /*
    * x-mac-arabic: Arabic (Mac)
    *               Not supported by ICU
    */
   { MIBUNDEF, 10004, STRING_ENCODING_WINDOWS_10004, UNSUPPORTED, 0,
      { "Windows-10004", "x-mac-arabic", NULL }
   },
   /*
    * x-mac-hebrew: Hebrew (Mac)
    *               Not supported by ICU
    */
   { MIBUNDEF, 10005, STRING_ENCODING_WINDOWS_10005, UNSUPPORTED, 0,
      { "Windows-10005", "x-mac-hebrew", NULL }
   },
   /*
    * x-mac-greek: Greek (Mac)
    */
   { MIBUNDEF, 10006, STRING_ENCODING_WINDOWS_10006, IN_FULL_ICU, 0,
      { "Windows-10006", "x-mac-greek", "macgr", "macos-6_2-10.4", NULL }
   },
   /*
    * x-mac-cyrillic: Cyrillic (Mac)
    */
   { MIBUNDEF, 10007, STRING_ENCODING_WINDOWS_10007, IN_FULL_ICU, 0,
      { "Windows-10007", "x-mac-cyrillic", "maccy", "mac-cyrillic",
        "macos-7_3-10.2", NULL }
   },
   /*
    * x-mac-chinesesimp: MAC Simplified Chinese (GB 2312);
    *                    Chinese Simplified (Mac)
    *                    Not supported by ICU
    */
   { MIBUNDEF, 10008, STRING_ENCODING_WINDOWS_10008, UNSUPPORTED, 0,
      { "Windows-10008", "x-mac-chinesesimp", NULL }
   },
   /*
    * x-mac-romanian: Romanian (Mac)
    *                 Not supported by ICU
    */
   { MIBUNDEF, 10010, STRING_ENCODING_WINDOWS_10010, UNSUPPORTED, 0,
      { "Windows-10010", "x-mac-romanian", NULL }
   },
   /*
    * x-mac-ukrainian: Ukrainian (Mac)
    *                  Not supported by ICU
    */
   { MIBUNDEF, 10017, STRING_ENCODING_WINDOWS_10017, UNSUPPORTED, 0,
      { "Windows-10017", "x-mac-ukrainian", NULL }
   },
   /*
    * x-mac-thai: Thai (Mac)
    *             Not supported by ICU
    */
   { MIBUNDEF, 10021, STRING_ENCODING_WINDOWS_10021, UNSUPPORTED, 0,
      { "Windows-10021", "x-mac-thai", NULL }
   },
   /*
    * x-mac-ce: MAC Latin 2; Central European (Mac)
    */
   { MIBUNDEF, 10029, STRING_ENCODING_WINDOWS_10029, IN_FULL_ICU, 0,
      { "Windows-10029", "x-mac-ce", "macce", "maccentraleurope",
        "x-mac-centraleurroman", "macos-29-10.2", NULL }
   },
   /*
    * x-mac-icelandic: Icelandic (Mac)
    *                  Not supported by ICU
    */
   { MIBUNDEF, 10079, STRING_ENCODING_WINDOWS_10079, UNSUPPORTED, 0,
      { "Windows-10079", "x-mac-icelandic", NULL }
   },
   /*
    * x-mac-turkish: Turkish (Mac)
    */
   { MIBUNDEF, 10081, STRING_ENCODING_WINDOWS_10081, IN_FULL_ICU, 0,
      { "Windows-10081", "x-mac-turkish", "mactr",
        "macos-35-10.2", NULL }
   },
   /*
    * x-mac-croatian: Croatian (Mac)
    *                 Not supported by ICU
    */
   { MIBUNDEF, 10082, STRING_ENCODING_WINDOWS_10082, UNSUPPORTED, 0,
      { "Windows-10082", "x-mac-croatian", NULL }
   },
   /*
    * x-Chinese_CNS: CNS Taiwan; Chinese Traditional (CNS)
    *                Not supported by ICU
    */
   { MIBUNDEF, 20000, STRING_ENCODING_WINDOWS_20000, UNSUPPORTED, 0,
      { "Windows-20000", "x-Chinese_CNS", NULL }
   },
   /*
    * x-cp20001: TCA Taiwan
    *            Not supported by ICU
    */
   { MIBUNDEF, 20001, STRING_ENCODING_WINDOWS_20001, UNSUPPORTED, 0,
      { "Windows-20001", "x-cp20001", NULL }
   },
   /*
    * x_Chinese-Eten: Eten Taiwan; Chinese Traditional (Eten)
    *                 Not supported by ICU
    */
   { MIBUNDEF, 20002, STRING_ENCODING_WINDOWS_20002, UNSUPPORTED, 0,
      { "Windows-20002", "x_Chinese-Eten", NULL }
   },
   /*
    * x-cp20003: IBM5550 Taiwan
    *            Not supported by ICU
    */
   { MIBUNDEF, 20003, STRING_ENCODING_WINDOWS_20003, UNSUPPORTED, 0,
      { "Windows-20003", "x-cp20003", NULL }
   },
   /*
    * x-cp20004: TeleText Taiwan
    *            Not supported by ICU
    */
   { MIBUNDEF, 20004, STRING_ENCODING_WINDOWS_20004, UNSUPPORTED, 0,
      { "Windows-20004", "x-cp20004", NULL }
   },
   /*
    * x-cp20005: Wang Taiwan
    *            Not supported by ICU
    */
   { MIBUNDEF, 20005, STRING_ENCODING_WINDOWS_20005, UNSUPPORTED, 0,
      { "Windows-20005", "x-cp20005", NULL }
   },
   /*
    * x-IA5: IA5 (IRV International Alphabet No. 5, 7-bit);
    *        Western European (IA5)
    *        Not supported by ICU
    */
   { MIBUNDEF, 20105, STRING_ENCODING_WINDOWS_20105, UNSUPPORTED, 0,
      { "Windows-20105", "x-IA5", NULL }
   },
   /*
    * x-IA5-German: IA5 German (7-bit)
    *               Not supported by ICU
    */
   { MIBUNDEF, 20106, STRING_ENCODING_WINDOWS_20106, UNSUPPORTED, 0,
      { "Windows-20106", "x-IA5-German", NULL }
   },
   /*
    * x-IA5-Swedish: IA5 Swedish (7-bit)
    *                Not supported by ICU
    */
   { MIBUNDEF, 20107, STRING_ENCODING_WINDOWS_20107, UNSUPPORTED, 0,
      { "Windows-20107", "x-IA5-Swedish", NULL }
   },
   /*
    * x-IA5-Norwegian: IA5 Norwegian (7-bit)
    *                  Not supported by ICU
    */
   { MIBUNDEF, 20108, STRING_ENCODING_WINDOWS_20108, UNSUPPORTED, 0,
      { "Windows-20108", "x-IA5-Norwegian", NULL }
   },
   /*
    * x-cp20269: ISO 6937 Non-Spacing Accent
    *            Not supported by ICU
    */
   { MIBUNDEF, 20269, STRING_ENCODING_WINDOWS_20269, UNSUPPORTED, 0,
      { "Windows-20269", "x-cp20269", NULL }
   },
   /*
    * x-EBCDIC-KoreanExtended: IBM EBCDIC Korean Extended
    *                          Not supported by ICU
    */
   { MIBUNDEF, 20833, STRING_ENCODING_WINDOWS_20833, UNSUPPORTED, 0,
      { "Windows-20833", "x-EBCDIC-KoreanExtended", NULL }
   },
   /*
    * x-cp20949: Korean Wansung
    *            Not supported by ICU
    */
   { MIBUNDEF, 20949, STRING_ENCODING_WINDOWS_20949, UNSUPPORTED, 0,
      { "Windows-20949", "x-cp20949", NULL }
   },
   /*
    * cp1025: IBM EBCDIC Cyrillic Serbian-Bulgarian
    *         ICU doesn't have alias "Windows-21025", use "cp1025"
    */
   { MIBUNDEF, 21025, STRING_ENCODING_WINDOWS_21025, IN_FULL_ICU, 0,
      { "cp1025", "ibm-1025", "1025", "ibm-1025_P100-1995", NULL }
   },
   /*
    * Windows-21027: (deprecated)
    *                Not supported by ICU
    */
   { MIBUNDEF, 21027, STRING_ENCODING_WINDOWS_21027, UNSUPPORTED, 0,
      { "Windows-21027", NULL }
   },
   /*
    * x-Europa: Europa 3
    *           Not supported by ICU
    */
   { MIBUNDEF, 29001, STRING_ENCODING_WINDOWS_29001, UNSUPPORTED, 0,
      { "Windows-29001", "x-Europa", NULL }
   },
   /*
    * iso-8859-8-i: ISO 8859-8 Hebrew; Hebrew (ISO-Logical)
    *               Windows duplicate of ISO-8859-8 (Windows-28598)
    *               ICU doesn't have alias "Windows-38598", use 
    *               "iso-8859-8-i"
    */
   { MIBUNDEF, 38598, STRING_ENCODING_WINDOWS_38598, IN_FULL_ICU, 0,
      { "iso-8859-8-i", NULL }
   },
   /*
    * csISO2022JP: ISO 2022 Japanese with halfwidth Katakana;
    *              Japanese (JIS-Allow 1 byte Kana)
    *              handled by ICU with ISO-2022-JP
    */
   { MIBUNDEF, 50221, STRING_ENCODING_WINDOWS_50221, SUPPORTED, 0,
      { "csISO2022JP", NULL }
   },
   /*
    * iso-2022-jp: ISO 2022 Japanese JIS X 0201-1989;
    *              Japanese (JIS-Allow 1 byte Kana - SO/SI)
    *              handled by ICU with ISO-2022-JP
    */
   { MIBUNDEF, 50222, STRING_ENCODING_WINDOWS_50222, IN_FULL_ICU, 0,
      { "ISO-2022-JP", NULL }
   },
   /*
    * Windows-50229: ISO 2022 Traditional Chinese
    *                Not supported by ICU
    */
   { MIBUNDEF, 50229, STRING_ENCODING_WINDOWS_50229, UNSUPPORTED, 0,
      { "Windows-50229", NULL }
   },
   /*
    * Windows-50930: EBCDIC Japanese (Katakana) Extended
    *                Not supported by ICU
    */
   { MIBUNDEF, 50930, STRING_ENCODING_WINDOWS_50930, UNSUPPORTED, 0,
      { "Windows-50930", NULL }
   },
   /*
    * Windows-50931: EBCDIC US-Canada and Japanese
    *                Not supported by ICU
    */
   { MIBUNDEF, 50931, STRING_ENCODING_WINDOWS_50931, UNSUPPORTED, 0,
      { "Windows-50931", NULL }
   },
   /*
    * Windows-50933: EBCDIC Korean Extended and Korean
    *                Not supported by ICU
    */
   { MIBUNDEF, 50933, STRING_ENCODING_WINDOWS_50933, UNSUPPORTED, 0,
      { "Windows-50933", NULL }
   },
   /*
    * Windows-50935: EBCDIC Simplified Chinese Extended and Simplified Chinese
    *                Not supported by ICU
    */
   { MIBUNDEF, 50935, STRING_ENCODING_WINDOWS_50935, UNSUPPORTED, 0,
      { "Windows-50935", NULL }
   },
   /*
    * Windows-50936: EBCDIC Simplified Chinese
    *                Not supported by ICU
    */
   { MIBUNDEF, 50936, STRING_ENCODING_WINDOWS_50936, UNSUPPORTED, 0,
      { "Windows-50936", NULL }
   },
   /*
    * Windows-50937: EBCDIC US-Canada and Traditional Chinese
    *                Not supported by ICU
    */
   { MIBUNDEF, 50937, STRING_ENCODING_WINDOWS_50937, UNSUPPORTED, 0,
      { "Windows-50937", NULL }
   },
   /*
    * Windows-50939: EBCDIC Japanese (Latin) Extended and Japanese
    *                Not supported by ICU
    */
   { MIBUNDEF, 50939, STRING_ENCODING_WINDOWS_50939, UNSUPPORTED, 0,
      { "Windows-50939", NULL }
   },
   /*
    * EUC-CN: EUC Simplified Chinese; Chinese Simplified (EUC)
    *         Route to GB2312
    */
   { MIBUNDEF, 51936, STRING_ENCODING_WINDOWS_51936, IN_FULL_ICU, 0,
      { "EUC-CN", NULL }
   },
   /*
    * Windows-51950: EUC Traditional Chinese
    *                Not supported by ICU
    */
   { MIBUNDEF, 51950, STRING_ENCODING_WINDOWS_51950, UNSUPPORTED, 0,
      { "Windows-51950", NULL }
   },
   /*
    * x-iscii-de: ISCII Devanagari
    */
   { MIBUNDEF, 57002, STRING_ENCODING_WINDOWS_57002, SUPPORTED, 0,
      { "Windows-57002", "x-iscii-de", "iscii-dev", "ibm-4902", NULL }
   },
   /*
    * x-iscii-be: ISCII Bengali
    */
   { MIBUNDEF, 57003, STRING_ENCODING_WINDOWS_57003, SUPPORTED, 0,
      { "Windows-57003", "x-iscii-be", "iscii-bng", NULL }
   },
   /*
    * x-iscii-ta: ISCII Tamil
    */
   { MIBUNDEF, 57004, STRING_ENCODING_WINDOWS_57004, SUPPORTED, 0,
      { "Windows-57004", "x-iscii-ta", "iscii-tml", NULL }
   },
   /*
    * x-iscii-te: ISCII Telugu
    */
   { MIBUNDEF, 57005, STRING_ENCODING_WINDOWS_57005, SUPPORTED, 0,
      { "Windows-57005", "x-iscii-te", "iscii-tlg", NULL }
   },
   /*
    * x-iscii-as: ISCII Assamese
    */
   { MIBUNDEF, 57006, STRING_ENCODING_WINDOWS_57006, SUPPORTED, 0,
      { "Windows-57006", "x-iscii-as", NULL }
   },
   /*
    * x-iscii-or: ISCII Oriya
    */
   { MIBUNDEF, 57007, STRING_ENCODING_WINDOWS_57007, SUPPORTED, 0,
      { "Windows-57007", "x-iscii-or", "iscii-ori", NULL }
   },
   /*
    * x-iscii-ka: ISCII Kannada
    */
   { MIBUNDEF, 57008, STRING_ENCODING_WINDOWS_57008, SUPPORTED, 0,
      { "Windows-57008", "x-iscii-ka", "iscii-knd", NULL }
   },
   /*
    * x-iscii-ma: ISCII Malayalam
    */
   { MIBUNDEF, 57009, STRING_ENCODING_WINDOWS_57009, SUPPORTED, 0,
      { "Windows-57009", "x-iscii-ma", "iscii-mlm", NULL }
   },
   /*
    * x-iscii-gu: ISCII Gujarati
    */
   { MIBUNDEF, 57010, STRING_ENCODING_WINDOWS_57010, SUPPORTED, 0,
      { "Windows-57010", "x-iscii-gu", "x-iscii-guj", NULL }
   },
   /*
    * x-iscii-pa: ISCII Punjabi
    */
   { MIBUNDEF, 57011, STRING_ENCODING_WINDOWS_57011, SUPPORTED, 0,
      { "Windows-57011", "x-iscii-pa", "iscii-gur", NULL }
   },
};


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeNormalizeEncodingName --
 *
 *      Normalizes a US-ASCII encoding name by discarding all
 *      non-alphanumeric characters and converting to lower-case.
 *
 * Results:
 *      The allocated, normalized encoding name in NUL-terminated
 *      US-ASCII bytes.  Caller must free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
UnicodeNormalizeEncodingName(const char *encodingName) // IN
{
   char *result;
   char *currentResult;

   ASSERT(encodingName);

   result = Util_SafeMalloc(strlen(encodingName) + 1);
   currentResult = result;

   for (currentResult = result; *encodingName != '\0'; encodingName++) {
      // The explicit cast from char to int is necessary for Netware builds.
      if (isalnum((int) *encodingName)) {
         *currentResult = tolower(*encodingName);
         currentResult++;
      }
   }

   *currentResult = '\0';

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeIANALookup --
 *
 *      Lookup an encoding name in the IANA cross reference table.
 *
 * Results:
 *      The index of the encoding within the table
 *      -1 if the encoding is not found
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int
UnicodeIANALookup(const char *encodingName) // IN
{
   /*
    * Thread-safe hash table to speed up encoding name -> IANA table
    * index lookups.
    */
   static Atomic_Ptr htPtr;
   static HashTable *encCache = NULL;

   char *name = NULL;
   char *candidate = NULL;
   const char *p;
   int i;
   int j;
   int acp;
   void *idx;
   size_t windowsPrefixLen = sizeof "windows-" - 1 /* NUL */;

   if (UNLIKELY(encCache == NULL)) {
      encCache = HashTable_AllocOnce(&htPtr, 128, HASH_ISTRING_KEY | HASH_FLAG_ATOMIC |
                                     HASH_FLAG_COPYKEY, free);
   }

   if (encCache && HashTable_Lookup(encCache, encodingName, &idx)) {
      return (int)(uintptr_t)idx;
   }

   /*
    * check for Windows-xxxx encoding names generated from GetACP()
    * code page numbers, see: CodeSetOld_GetCurrentCodeSet()
    */
   if (   strncmp(encodingName, "windows-", windowsPrefixLen) == 0
       || strncmp(encodingName, "Windows-", windowsPrefixLen) == 0) {
      p = encodingName + windowsPrefixLen;
      acp = 0;

      // The explicit cast from char to int is necessary for Netware builds.
      while (*p && isdigit((int)*p)) {
         acp *= 10;
         acp += *p - '0';
         p++;
      }
      if (!*p) {
         for (i = 0; i < ARRAYSIZE(xRef); i++) {
            if (xRef[i].winACP == acp) {
               goto done;
            }
         }
      }
   }

   // Try the raw names first to avoid the expense of normalizing everything.
   for (i = 0; i < ARRAYSIZE(xRef); i++) {
      for (j = 0; (p = xRef[i].names[j]) != NULL; j++) {
         if (strcmp(encodingName, p) == 0) {
            goto done;
         }
      }
   }

   name = UnicodeNormalizeEncodingName(encodingName);
   for (i = 0; i < ARRAYSIZE(xRef); i++) {
      for (j = 0; (p = xRef[i].names[j]) != NULL; j++) {
         candidate = UnicodeNormalizeEncodingName(p);
         if (strcmp(name, candidate) == 0) {
            goto done;
         }
         free(candidate);
      }
   }
   free(name);

   /*
    * Did not find a matching name.  Don't validate encoding names
    * here, unrecognized encoding will be caught when converting
    * from name to enum.
    */
   Log("%s: Did not find an IANA match for encoding \"%s\"\n",
       __FUNCTION__, encodingName);
   return -1;

done:
   free(name);
   free(candidate);

   if (encCache) {
      HashTable_Insert(encCache, encodingName, (void *)(uintptr_t)i);
   }

   return i;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_EncodingEnumToName --
 *
 *      Converts a StringEncoding enum value to the equivalent
 *      encoding name.
 *
 * Results:
 *      A NUL-terminated US-ASCII string containing the name of the
 *      encoding.  Encodings follow the preferred MIME encoding name
 *      from IANA's Character Sets standard.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const char *
Unicode_EncodingEnumToName(StringEncoding encoding) // IN
{
   int i;

   encoding = Unicode_ResolveEncoding(encoding);

   /* If you hit this, you probably need to call Unicode_Init() */
   ASSERT(encoding != STRING_ENCODING_UNKNOWN);

   /*
    * Look for a match in the xRef table. If found, return the
    * preferred MIME name. Whether ICU supports this encoding or
    * not isn't material here.
    */

   for (i = 0; i < ARRAYSIZE(xRef); i++) {
      if (encoding == xRef[i].encoding) {
	 return xRef[i].names[xRef[i].preferredMime];
      }
   }

   Log("%s: Unknown encoding %d.\n", __FUNCTION__, encoding);
   NOT_REACHED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_EncodingNameToEnum --
 *
 *      Converts a NUL-terminated US-ASCII string encoding name
 *      to the equivalent enum.
 *
 * Results:
 *      The StringEncoding enum value corresponding to the name, or
 *      STRING_ENCODING_UNKNOWN if the encoding name is not supported.
 *
 *      Inside tools all recognized local encodings are supported.
 *      If the local encoding is not available in our copy of ICU,
 *      fall back to the guest's facilities for converting between 
 *      the local encoding and UTF-8.
 *
 * Side effects:
 *      In tools, finding an unsupported encoding disables ICU and
 *      switches to codesetOld support.
 *
 *-----------------------------------------------------------------------------
 */

StringEncoding
Unicode_EncodingNameToEnum(const char *encodingName) // IN
{
   int idx;

   idx = UnicodeIANALookup(encodingName);
   if (idx < 0) {
      return STRING_ENCODING_UNKNOWN;
   }
   if (xRef[idx].isSupported) {
      return xRef[idx].encoding;
   }
#if defined(VMX86_TOOLS) && (!defined(OPEN_VM_TOOLS) || defined(USE_ICU))
   if (idx == UnicodeIANALookup(CodeSet_GetCurrentCodeSet())) {
      CodeSet_DontUseIcu();
      return xRef[idx].encoding;
   }
#endif
   return STRING_ENCODING_UNKNOWN;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeGetCurrentEncodingInternal --
 *
 *      Calls CodeSet_GetCurrentCodeSet() and returns the corresponding
 *      encoding.
 *
 * Results:
 *      The current encoding.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

StringEncoding
UnicodeGetCurrentEncodingInternal(void)
{
   StringEncoding encoding =
      Unicode_EncodingNameToEnum(CodeSet_GetCurrentCodeSet());

   ASSERT(Unicode_IsEncodingValid(encoding));
   return encoding;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetCurrentEncoding --
 *
 *      Return the current encoding (corresponding to
 *      CodeSet_GetCurrentCodeSet()).
 *
 * Results:
 *      The current encoding.
 *
 * Side effects:
 *      Since the return value of CodeSet_GetCurrentCodeSet() and our
 *      look-up table do not change, we memoize the value.
 *
 *-----------------------------------------------------------------------------
 */

StringEncoding
Unicode_GetCurrentEncoding(void)
{
   static StringEncoding encoding = STRING_ENCODING_UNKNOWN;

   if (UNLIKELY(encoding == STRING_ENCODING_UNKNOWN)) {
      encoding = UnicodeGetCurrentEncodingInternal();
   }

   return encoding;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_ResolveEncoding --
 *
 *      Resolves a meta-encoding enum value (e.g. STRING_ENCODING_DEFAULT) to
 *      a concrete one (e.g. STRING_ENCODING_UTF8).
 *
 * Results:
 *      A StringEncoding enum value.  May return STRING_ENCODING_UNKNOWN.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

StringEncoding
Unicode_ResolveEncoding(StringEncoding encoding)  // IN:
{
   if (encoding == STRING_ENCODING_DEFAULT) {
      encoding = Unicode_GetCurrentEncoding();
   }

   ASSERT(Unicode_IsEncodingValid(encoding));

   return encoding;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_IsEncodingValid --
 *
 *      Checks whether we support the given encoding.
 *
 * Results:
 *      TRUE if supported, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Unicode_IsEncodingValid(StringEncoding encoding)  // IN
{
   return encoding >= STRING_ENCODING_FIRST && 
          encoding < STRING_ENCODING_MAX_SPECIFIED;
}

/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeInitInternal --
 *
 *      Convert argv and environment from default encoding into
 *      unicode and initialize the cache of the native code set name
 *      used to resolve STRING_ENCODING_DEFAULT.
 *
 *      wargv takes precedence over argv as input if both are
 *      specified, likewise with wenvp/envp.
 *
 * Results:
 *      returns on success
 *      errors are terminal
 *
 * Side effects:
 *      Calling CodeSet_GetCurrentCodeSet() initializes the cache of the
 *      native code set name.  The cached name is used to resolve references
 *      to STRING_ENCODING_DEFAULT in unicode functions.
 *
 *      argv/envp will be rewritten to point to newly allocated memory. This
 *      memory should be freed by calling Unicode_Shutdown().
 *
 *-----------------------------------------------------------------------------
 */

static void
UnicodeInitInternal(int argc,               // IN
                    const char *icuDataDir, // IN
                    utf16_t **wargv,        // IN/OUT (OPT)
                    utf16_t **wenvp,        // IN/OUT (OPT)
                    char ***argv,           // IN/OUT (OPT)
                    char ***envp)           // IN/OUT (OPT)
{
#if !defined(__APPLE__) && !defined(VMX86_SERVER)
   char **list;
   StringEncoding encoding;
   const char *currentCodeSetName;
#endif
   Bool success = FALSE;
   char panicMsg[1024];
   static volatile Bool inited = FALSE;
   static Atomic_uint32 locked = {0};

   panicMsg[0] = '\0';

   /*
    * This function must be callable multiple times. We can't depend
    * on lib/sync, so cheese it.
    */
   while (1 == Atomic_ReadIfEqualWrite(&locked, 0, 1)) {
#if !defined(__FreeBSD__)
      usleep(250 * 1000);
#endif
   }

   if (inited) {
      success = TRUE;
      goto exit;
   }

   /*
    * Always init the codeset module first.
    */
   if (!CodeSet_Init(icuDataDir)) {
      snprintf(panicMsg, sizeof panicMsg, "Failed to initialize codeset.\n");
      goto exit;
   }

   // UTF-8 native encoding for these two
#if !defined(__APPLE__) && !defined(VMX86_SERVER)
   currentCodeSetName = CodeSet_GetCurrentCodeSet();
   encoding = Unicode_EncodingNameToEnum(currentCodeSetName);
   if (!Unicode_IsEncodingValid(encoding)) {
      snprintf(panicMsg, sizeof panicMsg,
              "Unsupported local character encoding \"%s\".\n",
               currentCodeSetName);
      goto exit;
   }

   if (wargv) {
      list = Unicode_AllocList((char **)wargv, argc + 1, STRING_ENCODING_UTF16);
      if (!list) {
         snprintf(panicMsg, sizeof panicMsg, "Unicode_AllocList1 failed.\n");
         goto exit;
      }
      *argv = list;
   } else if (argv) {
      list = Unicode_AllocList(*argv, argc + 1, STRING_ENCODING_DEFAULT);
      if (!list) {
         snprintf(panicMsg, sizeof panicMsg, "Unicode_AllocList2 failed.\n");
         goto exit;
      }
      *argv = list;
   }

   if (wenvp) {
      list = Unicode_AllocList((char **)wenvp, -1, STRING_ENCODING_UTF16);
      if (!list) {
         snprintf(panicMsg, sizeof panicMsg, "Unicode_AllocList3 failed.\n");
         goto exit;
      }
      *envp = list;
   } else if (envp) {
      list = Unicode_AllocList(*envp, -1, STRING_ENCODING_DEFAULT);
      if (!list) {
         snprintf(panicMsg, sizeof panicMsg, "Unicode_AllocList failed.\n");
         goto exit;
      }
      *envp = list;
   }
#endif // !__APPLE__ && !VMX86_SERVER

   inited = TRUE;
   success = TRUE;

  exit:
   Atomic_Write(&locked, 0);

   if (!success) {
      panicMsg[sizeof panicMsg - 1] = '\0';
      Panic("%s", panicMsg);
      exit(1);
   }
}


void
Unicode_InitW(int argc,                // IN
              utf16_t **wargv,         // IN/OUT (OPT)
              utf16_t **wenvp,         // IN/OUT (OPT)
              char ***argv,            // IN/OUT (OPT)
              char ***envp)            // IN/OUT (OPT)
{
   UnicodeInitInternal(argc, NULL, wargv, wenvp, argv, envp);
}


void
Unicode_InitEx(int argc,                // IN
               char ***argv,            // IN/OUT (OPT)
               char ***envp,            // IN/OUT (OPT)
               const char *icuDataDir)  // IN (OPT)
{
   UnicodeInitInternal(argc, icuDataDir, NULL, NULL, argv, envp);
}


void
Unicode_Init(int argc,        // IN
             char ***argv,    // IN/OUT (OPT)
             char ***envp)    // IN/OUT (OPT)
{
   UnicodeInitInternal(argc, NULL, NULL, NULL, argv, envp);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Shutdown --
 *
 *      Frees memory allocated by UnicodeInitInternal().
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
Unicode_Shutdown(int argc,              // IN
                 char **argv,           // IN (OPT)
                 char **envp)           // IN (OPT)
{
   if (argv != NULL) {
      Util_FreeStringList(argv, argc + 1);
   }

   if (envp != NULL) {
      Util_FreeStringList(envp, -1);
   }
}

#ifdef TEST_CUSTOM_ICU_DATA_FILE
/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeICUTest --
 *
 *      Test custom ICU data files
 *
 *      Checks string encodings for whether they are supported in 
 *      the xRef cross reference table and calls ICU with the 
 *      encodings to try to convert a simple ASCII string.  Note 
 *      that GB-2312-80 (Chinese) does not support ASCII, so it 
 *      is expected to fail the conversion.
 *
 *      To test custom ICU files, change the second arg in the call to
 *      UnicodeInitInternal() above to the *directory* containing the ICU
 *      data file, and add a call to this function.  Note that the name of
 *      the data file is hard coded to "icudt44l.dat" in lib/misc/codeset.c.
 *      Also note that in devel builds, lib/misc/codeset.c will override the 
 *      icu directory argument with a path to the toolchain, so that may need 
 *      to be disabled, too.
 *
 * Results:
 *      Prints results to stdout.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
 
void
UnicodeICUTest(void)
{
   StringEncoding enc, enc2;
   const char *name;
   Bool supported;
   Bool redirected;
   Bool canGetBytes;

   for (enc = STRING_ENCODING_FIRST; enc < STRING_ENCODING_MAX_SPECIFIED;
        enc++ ) {
      name =  Unicode_EncodingEnumToName(enc);
      enc2 = Unicode_EncodingNameToEnum(name);
      redirected = FALSE;
      if (enc2 == STRING_ENCODING_UNKNOWN) {
         supported = FALSE;
      } else {
         supported = TRUE;
         if (enc != enc2) {
            redirected = TRUE;  // xRef mapped to different entry
         }
      }
      canGetBytes = Unicode_CanGetBytesWithEncoding("Hello world", enc);
      printf("%s: supported:%s redirected:%s works:%s result:%s\n",
             name, supported ? "yes" : "no ", redirected ? "yes" : "no ", 
             canGetBytes ? "yes" : "no ",
             (supported && enc != STRING_ENCODING_GB_2312_80) == 
             canGetBytes ? "pass" : "FAIL");
   }
}
#endif

