/*
 *-----------------------------------------------------------------------------
 *  FatFs - FAT file system module  R0.10b                (C)ChaN, 2014
 *-----------------------------------------------------------------------------
 * FatFs module is a generic FAT file system module for small embedded systems.
 * This is a free software that opened for education, research and commercial
 * developments under license policy of following terms.
 *
 *  Copyright (c) 2014, 2015 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
 *  Copyright (C) 2014, ChaN, all right reserved.
 *
 * * The FatFs module is a free software and there is NO WARRANTY.
 * * No restriction on use. You can use, modify and redistribute it for
 *   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
 * * Redistributions of source code must retain the above copyright notice.
 *
 *-----------------------------------------------------------------------------
 */

#include <errno.h>
#include <sys/ioctl.h>
#include <kactype.h>
#include <kmalloc.h>
#include <kstring.h>
#include "ff.h"         /* Declarations of FatFs API */
#include "diskio.h"     /* Declarations of disk I/O functions */

/*
 * Module Private Definitions
 * --------------------------
 */

#define GET_INO(_dp_) \
    ((_dp_)->index + ld_clust((_dp_)->fs, (_dp_)->dir))

/*
 * Reentrancy related
 */
#define ENTER_FF(fs)        { if (lock_fs(fs)) return FR_TIMEOUT; }
#define LEAVE_FF(fs, res)   { unlock_fs(fs, res); return res; }

#define ABORT(fs, res)      { fp->err = (uint8_t)(res); LEAVE_FF(fs, res); }


/*
 * Definitions of sector size
 */
#if (_MAX_SS < _MIN_SS) || (_MAX_SS != 512 && _MAX_SS != 1024 && _MAX_SS != 2048 && _MAX_SS != 4096) || (_MIN_SS != 512 && _MIN_SS != 1024 && _MIN_SS != 2048 && _MIN_SS != 4096)
#error Wrong sector size configuration.
#endif
#define SS(fs)  ((fs)->ssize)   /* Variable sector size */


/*
 * File access control feature
 */
#if _FS_LOCK
typedef struct {
    FATFS * fs;     /* Object ID 1, volume (NULL:blank entry) */
    DWORD clu;      /* Object ID 2, directory (0:root) */
    WORD idx;       /* Object ID 3, directory index */
    WORD ctr;       /* Object open counter, 0:none, 0x01..0xFF:read mode open count, 0x100:write mode */
} FILESEM;
#endif



/*
 * DBCS code ranges and SBCS extend character conversion table
 */

#if _CODE_PAGE == 932   /* Japanese Shift-JIS */
#define _DF1S   0x81    /* DBC 1st byte range 1 start */
#define _DF1E   0x9F    /* DBC 1st byte range 1 end */
#define _DF2S   0xE0    /* DBC 1st byte range 2 start */
#define _DF2E   0xFC    /* DBC 1st byte range 2 end */
#define _DS1S   0x40    /* DBC 2nd byte range 1 start */
#define _DS1E   0x7E    /* DBC 2nd byte range 1 end */
#define _DS2S   0x80    /* DBC 2nd byte range 2 start */
#define _DS2E   0xFC    /* DBC 2nd byte range 2 end */

#elif _CODE_PAGE == 936 /* Simplified Chinese GBK */
#define _DF1S   0x81
#define _DF1E   0xFE
#define _DS1S   0x40
#define _DS1E   0x7E
#define _DS2S   0x80
#define _DS2E   0xFE

#elif _CODE_PAGE == 949 /* Korean */
#define _DF1S   0x81
#define _DF1E   0xFE
#define _DS1S   0x41
#define _DS1E   0x5A
#define _DS2S   0x61
#define _DS2E   0x7A
#define _DS3S   0x81
#define _DS3E   0xFE

#elif _CODE_PAGE == 950 /* Traditional Chinese Big5 */
#define _DF1S   0x81
#define _DF1E   0xFE
#define _DS1S   0x40
#define _DS1E   0x7E
#define _DS2S   0xA1
#define _DS2E   0xFE

#elif _CODE_PAGE == 437 /* U.S. (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x9A,0x90,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F,0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 720 /* Arabic (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x45,0x41,0x84,0x41,0x86,0x43,0x45,0x45,0x45,0x49,0x49,0x8D,0x8E,0x8F,0x90,0x92,0x92,0x93,0x94,0x95,0x49,0x49,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 737 /* Greek (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x92,0x92,0x93,0x94,0x95,0x96,0x97,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87, \
                0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0xAA,0x92,0x93,0x94,0x95,0x96,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0x97,0xEA,0xEB,0xEC,0xE4,0xED,0xEE,0xE7,0xE8,0xF1,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 775 /* Baltic (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x9A,0x91,0xA0,0x8E,0x95,0x8F,0x80,0xAD,0xED,0x8A,0x8A,0xA1,0x8D,0x8E,0x8F,0x90,0x92,0x92,0xE2,0x99,0x95,0x96,0x97,0x97,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
                0xA0,0xA1,0xE0,0xA3,0xA3,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xB5,0xB6,0xB7,0xB8,0xBD,0xBE,0xC6,0xC7,0xA5,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE3,0xE8,0xE8,0xEA,0xEA,0xEE,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 850 /* Multilingual Latin 1 (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x9A,0x90,0xB6,0x8E,0xB7,0x8F,0x80,0xD2,0xD3,0xD4,0xD8,0xD7,0xDE,0x8E,0x8F,0x90,0x92,0x92,0xE2,0x99,0xE3,0xEA,0xEB,0x59,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
                0xB5,0xD6,0xE0,0xE9,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE7,0xE7,0xE9,0xEA,0xEB,0xED,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 852 /* Latin 2 (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x9A,0x90,0xB6,0x8E,0xDE,0x8F,0x80,0x9D,0xD3,0x8A,0x8A,0xD7,0x8D,0x8E,0x8F,0x90,0x91,0x91,0xE2,0x99,0x95,0x95,0x97,0x97,0x99,0x9A,0x9B,0x9B,0x9D,0x9E,0x9F, \
                0xB5,0xD6,0xE0,0xE9,0xA4,0xA4,0xA6,0xA6,0xA8,0xA8,0xAA,0x8D,0xAC,0xB8,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBD,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC6,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD1,0xD1,0xD2,0xD3,0xD2,0xD5,0xD6,0xD7,0xB7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE3,0xD5,0xE6,0xE6,0xE8,0xE9,0xE8,0xEB,0xED,0xED,0xDD,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xEB,0xFC,0xFC,0xFE,0xFF}

#elif _CODE_PAGE == 855 /* Cyrillic (OEM) */
#define _DF1S   0
#define _EXCVT {0x81,0x81,0x83,0x83,0x85,0x85,0x87,0x87,0x89,0x89,0x8B,0x8B,0x8D,0x8D,0x8F,0x8F,0x91,0x91,0x93,0x93,0x95,0x95,0x97,0x97,0x99,0x99,0x9B,0x9B,0x9D,0x9D,0x9F,0x9F, \
                0xA1,0xA1,0xA3,0xA3,0xA5,0xA5,0xA7,0xA7,0xA9,0xA9,0xAB,0xAB,0xAD,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB6,0xB6,0xB8,0xB8,0xB9,0xBA,0xBB,0xBC,0xBE,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD1,0xD1,0xD3,0xD3,0xD5,0xD5,0xD7,0xD7,0xDD,0xD9,0xDA,0xDB,0xDC,0xDD,0xE0,0xDF, \
                0xE0,0xE2,0xE2,0xE4,0xE4,0xE6,0xE6,0xE8,0xE8,0xEA,0xEA,0xEC,0xEC,0xEE,0xEE,0xEF,0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF8,0xFA,0xFA,0xFC,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 857 /* Turkish (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x9A,0x90,0xB6,0x8E,0xB7,0x8F,0x80,0xD2,0xD3,0xD4,0xD8,0xD7,0x98,0x8E,0x8F,0x90,0x92,0x92,0xE2,0x99,0xE3,0xEA,0xEB,0x98,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9E, \
                0xB5,0xD6,0xE0,0xE9,0xA5,0xA5,0xA6,0xA6,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xDE,0x59,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 858 /* Multilingual Latin 1 + Euro (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x9A,0x90,0xB6,0x8E,0xB7,0x8F,0x80,0xD2,0xD3,0xD4,0xD8,0xD7,0xDE,0x8E,0x8F,0x90,0x92,0x92,0xE2,0x99,0xE3,0xEA,0xEB,0x59,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
                0xB5,0xD6,0xE0,0xE9,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD1,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE7,0xE7,0xE9,0xEA,0xEB,0xED,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 862 /* Hebrew (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0x21,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 866 /* Russian (OEM) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0x90,0x91,0x92,0x93,0x9d,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0xF0,0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 874 /* Thai (OEM, Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 1250 /* Central Europe (Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x8A,0x9B,0x8C,0x8D,0x8E,0x8F, \
                0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xA3,0xB4,0xB5,0xB6,0xB7,0xB8,0xA5,0xAA,0xBB,0xBC,0xBD,0xBC,0xAF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xFF}

#elif _CODE_PAGE == 1251 /* Cyrillic (Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x82,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x80,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x8A,0x9B,0x8C,0x8D,0x8E,0x8F, \
                0xA0,0xA2,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB2,0xA5,0xB5,0xB6,0xB7,0xA8,0xB9,0xAA,0xBB,0xA3,0xBD,0xBD,0xAF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF}

#elif _CODE_PAGE == 1252 /* Latin 1 (Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0xAd,0x9B,0x8C,0x9D,0xAE,0x9F, \
                0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0x9F}

#elif _CODE_PAGE == 1253 /* Greek (Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xA2,0xB8,0xB9,0xBA, \
                0xE0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xF2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xFB,0xBC,0xFD,0xBF,0xFF}

#elif _CODE_PAGE == 1254 /* Turkish (Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x8A,0x9B,0x8C,0x9D,0x9E,0x9F, \
                0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0x9F}

#elif _CODE_PAGE == 1255 /* Hebrew (Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 1256 /* Arabic (Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x8C,0x9D,0x9E,0x9F, \
                0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0x41,0xE1,0x41,0xE3,0xE4,0xE5,0xE6,0x43,0x45,0x45,0x45,0x45,0xEC,0xED,0x49,0x49,0xF0,0xF1,0xF2,0xF3,0x4F,0xF5,0xF6,0xF7,0xF8,0x55,0xFA,0x55,0x55,0xFD,0xFE,0xFF}

#elif _CODE_PAGE == 1257 /* Baltic (Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xA8,0xB9,0xAA,0xBB,0xBC,0xBD,0xBE,0xAF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xFF}

#elif _CODE_PAGE == 1258 /* Vietnam (OEM, Windows) */
#define _DF1S   0
#define _EXCVT {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0xAC,0x9D,0x9E,0x9F, \
                0xA0,0x21,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xEC,0xCD,0xCE,0xCF,0xD0,0xD1,0xF2,0xD3,0xD4,0xD5,0xD6,0xF7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xFE,0x9F}

#elif _CODE_PAGE == 1   /* ASCII (for only non-LFN cfg) */
#if configFATFS_LFN
#error Cannot use LFN feature without valid code page.
#endif
#define _DF1S   0

#else
#error Unknown code page

#endif

#if _DF1S       /* Code page is DBCS */

#ifdef _DF2S    /* Two 1st byte areas */
#define IsDBCS1(c)  (((uint8_t)(c) >= _DF1S && (uint8_t)(c) <= _DF1E) || ((uint8_t)(c) >= _DF2S && (uint8_t)(c) <= _DF2E))
#else           /* One 1st byte area */
#define IsDBCS1(c)  ((uint8_t)(c) >= _DF1S && (uint8_t)(c) <= _DF1E)
#endif

#ifdef _DS3S    /* Three 2nd byte areas */
#define IsDBCS2(c)  (((uint8_t)(c) >= _DS1S && (uint8_t)(c) <= _DS1E) || ((uint8_t)(c) >= _DS2S && (uint8_t)(c) <= _DS2E) || ((uint8_t)(c) >= _DS3S && (uint8_t)(c) <= _DS3E))
#else           /* Two 2nd byte areas */
#define IsDBCS2(c)  (((uint8_t)(c) >= _DS1S && (uint8_t)(c) <= _DS1E) || ((uint8_t)(c) >= _DS2S && (uint8_t)(c) <= _DS2E))
#endif

#else           /* Code page is SBCS */

#define IsDBCS1(c)  0
#define IsDBCS2(c)  0

#endif /* _DF1S */


/*
 * Name status flags
 */
#define NS          11      /* Index of name status byte in fn[] */
#define NS_LOSS     0x01    /* Out of 8.3 format */
#define NS_LFN      0x02    /* Force to create LFN entry */
#define NS_LAST     0x04    /* Last segment */
#define NS_BODY     0x08    /* Lower case flag (body) */
#define NS_EXT      0x10    /* Lower case flag (ext) */
#define NS_DOT      0x20    /* Dot entry */


/*
 * FAT sub-type boundaries
 */
#define MIN_FAT16   4086U   /* Minimum number of clusters for FAT16 */
#define MIN_FAT32   65526U  /* Minimum number of clusters for FAT32 */


/*
 * FatFs refers the members in the FAT structures as byte array instead of
 * structure member because the structure is not binary compatible between
 * different platforms
 */

#define BS_jmpBoot          0       /* Jump instruction (3) */
#define BS_OEMName          3       /* OEM name (8) */
#define BPB_BytsPerSec      11      /* Sector size [byte] (2) */
#define BPB_SecPerClus      13      /* Cluster size [sector] (1) */
#define BPB_RsvdSecCnt      14      /* Size of reserved area [sector] (2) */
#define BPB_NumFATs         16      /* Number of FAT copies (1) */
#define BPB_RootEntCnt      17      /* Number of root directory entries for FAT12/16 (2) */
#define BPB_TotSec16        19      /* Volume size [sector] (2) */
#define BPB_Media           21      /* Media descriptor (1) */
#define BPB_FATSz16         22      /* FAT size [sector] (2) */
#define BPB_SecPerTrk       24      /* Track size [sector] (2) */
#define BPB_NumHeads        26      /* Number of heads (2) */
#define BPB_HiddSec         28      /* Number of special hidden sectors (4) */
#define BPB_TotSec32        32      /* Volume size [sector] (4) */
#define BS_DrvNum           36      /* Physical drive number (2) */
#define BS_BootSig          38      /* Extended boot signature (1) */
#define BS_VolID            39      /* Volume serial number (4) */
#define BS_VolLab           43      /* Volume label (8) */
#define BS_FilSysType       54      /* File system type (1) */
#define BPB_FATSz32         36      /* FAT size [sector] (4) */
#define BPB_ExtFlags        40      /* Extended flags (2) */
#define BPB_FSVer           42      /* File system version (2) */
#define BPB_RootClus        44      /* Root directory first cluster (4) */
#define BPB_FSInfo          48      /* Offset of FSINFO sector (2) */
#define BPB_BkBootSec       50      /* Offset of backup boot sector (2) */
#define BS_DrvNum32         64      /* Physical drive number (2) */
#define BS_BootSig32        66      /* Extended boot signature (1) */
#define BS_VolID32          67      /* Volume serial number (4) */
#define BS_VolLab32         71      /* Volume label (8) */
#define BS_FilSysType32     82      /* File system type (1) */
#define FSI_LeadSig         0       /* FSI: Leading signature (4) */
#define FSI_StrucSig        484     /* FSI: Structure signature (4) */
#define FSI_Free_Count      488     /* FSI: Number of free clusters (4) */
#define FSI_Nxt_Free        492     /* FSI: Last allocated cluster (4) */
#define MBR_Table           446     /* MBR: Partition table offset (2) */
#define SZ_PTE              16      /* MBR: Size of a partition table entry */
#define BS_55AA             510     /* Signature word (2) */

#define DIR_Name            0       /* Short file name (11) */
#define DIR_Attr            11      /* Attribute (1) */
#define DIR_NTres           12      /* NT flag (1) */
#define DIR_CrtTimeTenth    13      /* Created time sub-second (1) */
#define DIR_CrtTime         14      /* Created time (2) */
#define DIR_CrtDate         16      /* Created date (2) */
#define DIR_LstAccDate      18      /* Last accessed date (2) */
#define DIR_FstClusHI       20      /* Higher 16-bit of first cluster (2) */
#define DIR_WrtTime         22      /* Modified time (2) */
#define DIR_WrtDate         24      /* Modified date (2) */
#define DIR_FstClusLO       26      /* Lower 16-bit of first cluster (2) */
#define DIR_FileSize        28      /* File size (4) */
#define LDIR_Ord            0       /* LFN entry order and LLE flag (1) */
#define LDIR_Attr           11      /* LFN attribute (1) */
#define LDIR_Type           12      /* LFN type (1) */
#define LDIR_Chksum         13      /* Sum of corresponding SFN entry */
#define LDIR_FstClusLO      26      /* Filled by zero (0) */
#define SZ_DIR              32      /* Size of a directory entry */
#define LLE                 0x40    /* Last long entry flag in LDIR_Ord */
#define DDE                 0xE5    /* Deleted directory entry mark in
                                     * DIR_Name[0] */
#define NDDE                0x05    /* Replacement of the character collides
                                     * with DDE */


/*
 * ------------------------------------------------------------
 * Module private work area
 * ------------------------------------------------------------
 * Note that uninitialized variables with static duration are
 * guaranteed zero/null as initial value. If not, either the
 * linker or start-up routine is out of ANSI-C standard.
 */

static WORD Fsid;           /* File system mount ID */

#if _FS_LOCK
static FILESEM * Files;     /* Open object lock semaphores */
#endif

#if configFATFS_LFN     /*
                         * LFN feature with dynamic working buffer on
                         * the heap.
                         */
#define DEF_NAMEBUF     uint8_t sfn[12]; WCHAR *lfn
#define INIT_BUF(dobj)  { lfn = kmalloc((_MAX_LFN + 1) * 2); \
                          if (!lfn) LEAVE_FF((dobj).fs, FR_NOT_ENOUGH_CORE); \
                            (dobj).lfn = lfn; (dobj).fn = sfn; }
#define FREE_BUF()      kfree(lfn)

#else                   /* No LFN feature */
#define DEF_NAMEBUF     uint8_t sfn[12]
#define INIT_BUF(dobj)  (dobj).fn = sfn
#define FREE_BUF()
#endif


#ifdef _EXCVT
/* Upper conversion table for extended characters */
static const uint8_t ExCvt[] = _EXCVT;
#endif

int ff_init(void)
{
#if _FS_LOCK
    Files = kcalloc(_FS_LOCK, sizeof(FILESEM));
    if (!Files)
        return -ENOMEM;
#endif
    return 0;
}


/*
 * Module Private Functions
 * ------------------------
 */

/*
 * Request/Release grant to access the volume
 *-------------------------------------------
 */

/**
 * @param fs File system object.
 */
#define lock_fs(_fs_) mtx_lock(&(_fs_)->sobj)

/**
 * @param fs File system object.
 * @param res Result code to be returned.
 */
static void unlock_fs(FATFS * fs, FRESULT res)
{
    if (fs &&
        res != FR_NOT_ENABLED &&
        res != FR_INVALID_DRIVE &&
        res != FR_INVALID_OBJECT &&
        res != FR_TIMEOUT) {
        mtx_unlock(&fs->sobj);
    }
}

/*
 * File lock control functions
 * ---------------------------
 */
#if _FS_LOCK

/**
 * Check if the file can be accessed.
 * @param dp Directory object pointing the file to be checked.
 * @param acc Desired access type (0:Read, 1:Write, 2:Delete/Rename).
 */
static FRESULT chk_lock(FF_DIR * dp, int acc)
{
    unsigned int i, be;

    /* Search file semaphore table */
    for (i = be = 0; i < _FS_LOCK; i++) {
        if (Files[i].fs) {  /* Existing entry */
            /* Check if the object matched with an open object */
            if (Files[i].fs == dp->fs &&
                Files[i].clu == dp->sclust &&
                Files[i].idx == dp->index) break;
        } else { /* Blank entry */
            be = 1;
        }
    }
    if (i == _FS_LOCK) { /* The object is not opened */
        /* Is there a blank entry for new object? */
        return (be || acc == 2) ? FR_OK : FR_TOO_MANY_OPEN_FILES;
    }

    /*
     * The object has been opened. Reject any open against writing file and all
     * write mode open.
     */
    return (acc || Files[i].ctr == 0x100) ? FR_LOCKED : FR_OK;
}

/**
 * Check if an entry is available for a new object.
 */
static int enq_lock(void)
{
    unsigned int i;

    for (i = 0; i < _FS_LOCK && Files[i].fs; i++) ;
    return i != _FS_LOCK;
}

/**
 * Increment object open counter and returns its index (0:Internal error).
 * @param dp Directory object pointing the file to register or increment.
 * @param acc Desired access (0:Read, 1:Write, 2:Delete/Rename).
 */
static unsigned int inc_lock(FF_DIR * dp, int acc)
{
    unsigned int i;

    for (i = 0; i < _FS_LOCK; i++) { /* Find the object */
        if (Files[i].fs == dp->fs &&
            Files[i].clu == dp->sclust &&
            Files[i].idx == dp->index)
            break;
    }

    if (i == _FS_LOCK) { /* Not opened. Register it as new. */
        for (i = 0; i < _FS_LOCK && Files[i].fs; i++) ;
        if (i == _FS_LOCK)
            return 0; /* No free entry to register (int err) */
        Files[i].fs = dp->fs;
        Files[i].clu = dp->sclust;
        Files[i].idx = dp->index;
        Files[i].ctr = 0;
    }

    if (acc && Files[i].ctr)
        return 0; /* Access violation (int err) */

    Files[i].ctr = acc ? 0x100 : Files[i].ctr + 1;  /* Set semaphore value */

    return i + 1;
}

/**
 * Decrement object open counter.
 * @param i Semaphore index (1..).
 */
static FRESULT dec_lock(unsigned int i)
{
    FRESULT res;

    if (--i < _FS_LOCK) {   /* Shift index number origin from 0 */
        WORD n = Files[i].ctr;

        if (n == 0x100)
            n = 0; /* If write mode open, delete the entry */
        if (n)
            n--; /* Decrement read mode open count */
        Files[i].ctr = n;
        if (!n)
            Files[i].fs = NULL; /* Delete the entry if open count gets zero */
        res = FR_OK;
    } else {
        res = FR_INT_ERR; /* Invalid index nunber */
    }

    return res;
}

/**
 * Clear lock entries of the volume.
 */
static void clear_lock(FATFS * fs)
{
    unsigned int i;

    for (i = 0; i < _FS_LOCK; i++) {
        if (Files[i].fs == fs)
            Files[i].fs = NULL;
    }
}
#endif

/**
 * Move/Flush disk access window in the file system object.
 * @param fs File system object.
 */
static FRESULT sync_window(FATFS * fs)
{
    DWORD wsect;
    unsigned int nf;

    if (fs->wflag) {    /* Write back the sector if it is dirty */
        wsect = fs->winsect;    /* Current sector number */
        if (fatfs_disk_write(fs->drv, fs->win, wsect, SS(fs)))
            return FR_DISK_ERR;
        fs->wflag = 0;
        if (wsect - fs->fatbase < fs->fsize) { /* Is it in the FAT area? */
            /* Reflect the change to all FAT copies */
            for (nf = fs->n_fats; nf >= 2; nf--) {
                wsect += fs->fsize;
                fatfs_disk_write(fs->drv, fs->win, wsect, SS(fs));
            }
        }
    }

    return FR_OK;
}

/**
 * @param fs File system object.
 * @param sector Sector number to make appearance in the fs->win[].
 */
static FRESULT move_window(FATFS * fs, DWORD sector)
{
    if (sector != fs->winsect) {    /* Changed current window */
        if (!fs->readonly && sync_window(fs) != FR_OK)
            return FR_DISK_ERR;
        if (fatfs_disk_read(fs->drv, fs->win, sector, SS(fs)))
            return FR_DISK_ERR;
        fs->winsect = sector;
    }

    return FR_OK;
}

/**
 * Synchronize file system and strage device.
 * @param fs File system object.
 * @return FR_OK: successful, FR_DISK_ERR: failed
 */
static FRESULT sync_fs(FATFS * fs)
{
    FRESULT res;

    res = sync_window(fs);
    if (res == FR_OK) {
        /* Update FSINFO sector if needed */
        if (fs->fs_type == FS_FAT32 && fs->fsi_flag == 1) {
            /* Create FSINFO structure */
            memset(fs->win, 0, SS(fs));
            ST_WORD(fs->win + BS_55AA, 0xAA55);
            ST_DWORD(fs->win + FSI_LeadSig, 0x41615252);
            ST_DWORD(fs->win + FSI_StrucSig, 0x61417272);
            ST_DWORD(fs->win + FSI_Free_Count, fs->free_clust);
            ST_DWORD(fs->win + FSI_Nxt_Free, fs->last_clust);
            /* Write it into the FSINFO sector */
            fs->winsect = fs->volbase + 1;
            fatfs_disk_write(fs->drv, fs->win, fs->winsect, SS(fs));
            fs->fsi_flag = 0;
        }
        /* Make sure that no pending write process in the physical drive */
        if (fatfs_disk_ioctl(fs->drv, CTRL_SYNC, NULL, 0) != RES_OK)
            res = FR_DISK_ERR;
    }

    return res;
}

/**
 * Get sector# from cluster#.
 * !=0: Sector number, 0: Failed - invalid cluster#
 * @param fs File system object.
 * @param clst Cluster# to be converted.
 */
DWORD clust2sect(FATFS * fs, DWORD clst)
{
    clst -= 2;
    if (clst >= (fs->n_fatent - 2))
        return 0; /* Invalid cluster# */
    return clst * fs->csize + fs->database;
}

/**
 * FAT access - Read value of a FAT entry.
 * 0xFFFFFFFF:Disk error, 1:Internal error, Else:Cluster status.
 * @param fs File system object.
 * @param clst Cluster# to get the link information.
 */
DWORD get_fat(FATFS * fs, DWORD clst)
{
    unsigned int wc, bc;
    uint8_t * p;

    if (clst < 2 || clst >= fs->n_fatent) /* Check range */
        return 1;

    switch (fs->fs_type) {
    case FS_FAT12 :
        bc = (unsigned int)clst; bc += bc / 2;
        if (move_window(fs, fs->fatbase + (bc / SS(fs))))
            break;
        wc = fs->win[bc % SS(fs)]; bc++;
        if (move_window(fs, fs->fatbase + (bc / SS(fs))))
            break;
        wc |= fs->win[bc % SS(fs)] << 8;
        return clst & 1 ? wc >> 4 : (wc & 0xFFF);

    case FS_FAT16 :
        if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 2))))
            break;
        p = &fs->win[clst * 2 % SS(fs)];
        return LD_WORD(p);

    case FS_FAT32 :
        if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 4))))
            break;
        p = &fs->win[clst * 4 % SS(fs)];
        return LD_DWORD(p) & 0x0FFFFFFF;

    default:
        return 1;
    }

    return 0xFFFFFFFF;  /* An error occurred at the disk I/O layer */
}

/**
 * FAT access - Change value of a FAT entry.
 * @param fs File system object.
 * @param clst Cluster# to be changed in range of 2 to fs->n_fatent - 1.
 * @param val New value to mark the cluster.
 */
FRESULT put_fat(FATFS * fs, DWORD clst, DWORD val)
{
    unsigned int bc;
    uint8_t * p;
    FRESULT res;

    if (clst < 2 || clst >= fs->n_fatent) { /* Check range */
        return FR_INT_ERR;
    }

    switch (fs->fs_type) {
    case FS_FAT12 :
        bc = (unsigned int)clst; bc += bc / 2;
        res = move_window(fs, fs->fatbase + (bc / SS(fs)));
        if (res != FR_OK)
            break;
        p = &fs->win[bc % SS(fs)];
        *p = (clst & 1) ? ((*p & 0x0F) | ((uint8_t)val << 4)) : (uint8_t)val;
        bc++;
        fs->wflag = 1;
        res = move_window(fs, fs->fatbase + (bc / SS(fs)));
        if (res != FR_OK)
            break;
        p = &fs->win[bc % SS(fs)];
        *p = (clst & 1) ? (uint8_t)(val >> 4) :
                          ((*p & 0xF0) | ((uint8_t)(val >> 8) & 0x0F));
        break;

    case FS_FAT16 :
        res = move_window(fs, fs->fatbase + (clst / (SS(fs) / 2)));
        if (res != FR_OK)
            break;
        p = &fs->win[clst * 2 % SS(fs)];
        ST_WORD(p, (WORD)val);
        break;

    case FS_FAT32 :
        res = move_window(fs, fs->fatbase + (clst / (SS(fs) / 4)));
        if (res != FR_OK)
            break;
        p = &fs->win[clst * 4 % SS(fs)];
        val |= LD_DWORD(p) & 0xF0000000;
        ST_DWORD(p, val);
        break;

    default :
        res = FR_INT_ERR;
    }

    fs->wflag = 1;

    return res;
}

/**
 * FAT handling - Remove a cluster chain.
 * @param fs File system object.
 * @param clst Cluster# to remove a chain from.
 */
static FRESULT remove_chain(FATFS * fs, DWORD clst)
{
    FRESULT res;
    DWORD nxt;
#if _USE_ERASE
    DWORD scl = clst, ecl = clst, rt[2];
#endif

    if (clst < 2 || clst >= fs->n_fatent) { /* Check range */
        res = FR_INT_ERR;

    } else {
        res = FR_OK;
        while (clst < fs->n_fatent) {           /* Not a last link? */
            nxt = get_fat(fs, clst);            /* Get cluster status */
            if (nxt == 0)
                break; /* Empty cluster? */
            if (nxt == 1) {
                res = FR_INT_ERR; /* Internal error? */
                break;
            }
            if (nxt == 0xFFFFFFFF) {
                res = FR_DISK_ERR; /* Disk error? */
                break;
            }
            res = put_fat(fs, clst, 0);         /* Mark the cluster "empty" */
            if (res != FR_OK)
                break;
            if (fs->free_clust != 0xFFFFFFFF) { /* Update FSINFO */
                fs->free_clust++;
                fs->fsi_flag |= 1;
            }
#if _USE_ERASE
            if (ecl + 1 == nxt) {   /* Is next cluster contiguous? */
                ecl = nxt;
            } else {                /* End of contiguous clusters */
                rt[0] = clust2sect(fs, scl);                  /* Start sector */
                rt[1] = clust2sect(fs, ecl) + fs->csize - 1;    /* End sector */
                fatfs_disk_ioctl(fs->drv, CTRL_ERASE_SECTOR, rt, sizeof(rt));
                scl = ecl = nxt;
            }
#endif
            clst = nxt; /* Next cluster */
        }
    }

    return res;
}

/**
 * FAT handling - Stretch or Create a cluster chain.
 * @param fs File system object.
 * @param clst Cluster# to stretch. 0 means create a new chain.
 * @return 0:No free cluster, 1:Internal error, 0xFFFFFFFF:Disk error, >=2:New cluster#
 */
static DWORD create_chain(FATFS * fs, DWORD clst)
{
    DWORD cs, ncl, scl;
    FRESULT res;

    if (clst == 0) { /* Create a new chain */
        scl = fs->last_clust; /* Get suggested start point */
        if (!scl || scl >= fs->n_fatent)
            scl = 1;
    }
    else { /* Stretch the current chain */
        cs = get_fat(fs, clst); /* Check the cluster status */
        if (cs < 2)
            return 1; /* Invalid value */
        if (cs == 0xFFFFFFFF)
            return cs; /* A disk error occurred */
        if (cs < fs->n_fatent)
            return cs; /* It is already followed by next cluster */
        scl = clst;
    }

    ncl = scl; /* Start cluster */
    for (;;) {
        ncl++;                          /* Next cluster */
        if (ncl >= fs->n_fatent) {      /* Check wrap around */
            ncl = 2;
            if (ncl > scl)
                return 0;    /* No free cluster */
        }
        cs = get_fat(fs, ncl);          /* Get the cluster status */
        if (cs == 0)
            break;             /* Found a free cluster */
        if (cs == 0xFFFFFFFF || cs == 1) /* An error occurred */
            return cs;
        if (ncl == scl)
            return 0;       /* No free cluster */
    }

    res = put_fat(fs, ncl, 0x0FFFFFFF); /* Mark the new cluster "last link" */
    if (res == FR_OK && clst != 0) {
        res = put_fat(fs, clst, ncl);   /* Link it to the previous one if needed */
    }
    if (res == FR_OK) {
        fs->last_clust = ncl;           /* Update FSINFO */
        if (fs->free_clust != 0xFFFFFFFF) {
            fs->free_clust--;
            fs->fsi_flag |= 1;
        }
    } else {
        ncl = (res == FR_DISK_ERR) ? 0xFFFFFFFF : 1;
    }

    return ncl; /* Return new cluster number or error code */
}

#if _USE_FASTSEEK
/**
 * FAT handling - Convert offset into cluster with link map table.
 * @param fp Pointer to the file object.
 * @param ofs File offset to be converted to cluster#.
 * @return <2:Error, >=2:Cluster number.
 */
static DWORD clmt_clust(FF_FIL * fp, DWORD ofs)
{
    DWORD cl, ncl;
    DWORD * tbl;

    tbl = fp->cltbl + 1;    /* Top of CLMT */
    cl = ofs / SS(fp->fs) / fp->fs->csize;  /* Cluster order from top of the file */
    for (;;) {
        ncl = *tbl++; /* Number of cluters in the fragment */
        if (!ncl)
            return 0; /* End of table? (error) */
        if (cl < ncl)
            break;    /* In this fragment? */
        cl -= ncl; tbl++; /* Next fragment */
    }
    return cl + *tbl; /* Return the cluster number */
}
#endif  /* _USE_FASTSEEK */

/**
 * Directory handling - Set directory index.
 * @param dp Pointer to directory object.
 * @param idx Index of directory table.
 */
static FRESULT dir_sdi(FF_DIR * dp, unsigned int idx)
{
    DWORD clst, sect;
    unsigned int ic;

    dp->index = (WORD)idx;  /* Current index */
    clst = dp->sclust;      /* Table start cluster (0:root) */
    if (clst == 1 || clst >= dp->fs->n_fatent)  /* Check start cluster range */
        return FR_INT_ERR;
    if (!clst && dp->fs->fs_type == FS_FAT32) {
        /* Replace cluster# 0 with root cluster# if in FAT32 */
        clst = dp->fs->dirbase;
    }

    if (clst == 0) {    /* Static table (root-directory in FAT12/16) */
        if (idx >= dp->fs->n_rootdir)   /* Is index out of range? */
            return FR_INT_ERR;
        sect = dp->fs->dirbase;
    } else {
        /* Dynamic table (root-directory in FAT32 or sub-directory) */
        ic = SS(dp->fs) / SZ_DIR * dp->fs->csize;   /* Entries per cluster */
        while (idx >= ic) { /* Follow cluster chain */
            clst = get_fat(dp->fs, clst);               /* Get next cluster */
            if (clst == 0xFFFFFFFF)
                return FR_DISK_ERR; /* Disk error */
            if (clst < 2 || clst >= dp->fs->n_fatent) {
                /* Reached to end of table or internal error */
                return FR_INT_ERR;
            }
            idx -= ic;
        }
        sect = clust2sect(dp->fs, clst);
    }
    dp->clust = clst;   /* Current cluster# */
    if (!sect)
        return FR_INT_ERR;
    /* Sector# of the directory entry */
    dp->sect = sect + idx / (SS(dp->fs) / SZ_DIR);
    /* Ptr to the entry in the sector */
    dp->dir = dp->fs->win + (idx % (SS(dp->fs) / SZ_DIR)) * SZ_DIR;

    return FR_OK;
}

/**
 * Directory handling - Move directory table index next.
 * @param dp Pointer to the directory object.
 * @param stretch 0: Do not stretch table, 1: Stretch table if needed.
 * @return FR_OK:Succeeded, FR_NO_FILE:End of table,
 *         FR_DENIED:Could not stretch.
 */
static FRESULT dir_next(FF_DIR * dp, int stretch)
{
    DWORD clst;
    unsigned int i;

    i = dp->index + 1;
    if (!(i & 0xFFFF) || !dp->sect) /* Report EOT when index has reached 65535 */
        return FR_NO_FILE;

    if (!(i % (SS(dp->fs) / SZ_DIR))) { /* Sector changed? */
        dp->sect++; /* Next sector */

        if (!dp->clust) { /* Static table */
            if (i >= dp->fs->n_rootdir) {
                /* Report EOT if it reached end of static table */
                return FR_NO_FILE;
            }
        }
        else { /* Dynamic table */
            if (((i / (SS(dp->fs) / SZ_DIR)) & (dp->fs->csize - 1)) == 0) {
                /* Cluster changed? */
                clst = get_fat(dp->fs, dp->clust); /* Get next cluster */
                if (clst <= 1)
                    return FR_INT_ERR;
                if (clst == 0xFFFFFFFF)
                    return FR_DISK_ERR;
                if (clst >= dp->fs->n_fatent) {
                    /* If it reached end of dynamic table, */
                    if (!dp->fs->readonly) {
                        unsigned int c;
                        if (!stretch) {
                            /* If do not stretch, report EOT */
                            return FR_NO_FILE;
                        }

                        /* Stretch cluster chain */
                        clst = create_chain(dp->fs, dp->clust);
                        if (clst == 0)
                            return FR_DENIED; /* No free cluster */
                        if (clst == 1)
                            return FR_INT_ERR;
                        if (clst == 0xFFFFFFFF)
                            return FR_DISK_ERR;

                        /* Clean-up stretched table */
                        if (sync_window(dp->fs))
                            return FR_DISK_ERR;/* Flush disk access window */
                        /* Clear window buffer */
                        memset(dp->fs->win, 0, SS(dp->fs));
                        /* Cluster start sector */
                        dp->fs->winsect = clust2sect(dp->fs, clst);
                        for (c = 0; c < dp->fs->csize; c++) {
                            /* Fill the new cluster with 0 */
                            dp->fs->wflag = 1;
                            if (sync_window(dp->fs))
                                return FR_DISK_ERR;
                            dp->fs->winsect++;
                        }
                        dp->fs->winsect -= c; /* Rewind window offset */
                    } else {
                        if (!stretch) {
                            /*
                             * If do not stretch, report EOT
                             * (this is to suppress a warning)
                             */
                            return FR_NO_FILE;
                        }
                        return FR_NO_FILE; /* Report EOT */
                    }
                }
                dp->clust = clst; /* Initialize data for new cluster */
                dp->sect = clust2sect(dp->fs, clst);
            }
        }
    }

    dp->index = (WORD)i; /* Current index */
    /* Current entry in the window */
    dp->dir = dp->fs->win + (i % (SS(dp->fs) / SZ_DIR)) * SZ_DIR;

    return FR_OK;
}

/**
 * Directory handling - Reserve directory entry.
 * @param dp Pointer to the directory object.
 * @param nent Number of contiguous entries to allocate (1-21).
 */
static FRESULT dir_alloc(FF_DIR * dp, unsigned int nent)
{
    FRESULT res;
    unsigned int n;

    res = dir_sdi(dp, 0);
    if (res == FR_OK) {
        n = 0;
        do {
            res = move_window(dp->fs, dp->sect);
            if (res != FR_OK)
                break;
            /* Is it a blank entry? */
            if (dp->dir[0] == DDE || dp->dir[0] == 0) {
                if (++n == nent)
                    break; /* A block of contiguous entries is found */
            } else {
                n = 0; /* Not a blank entry. Restart to search */
            }
            res = dir_next(dp, 1);      /* Next entry with table stretch enabled */
        } while (res == FR_OK);
    }
    if (res == FR_NO_FILE)
        res = FR_DENIED; /* No directory entry to allocate */
    return res;
}

/**
 * Directory handling - Load/Store start cluster number.
 * @param fs is a pointer to the fs object.
 * @param dir is a pointer to the directory entry.
 */
static DWORD ld_clust(FATFS * fs, uint8_t * dir)
{
    DWORD cl;

    cl = LD_WORD(dir + DIR_FstClusLO);
    if (fs->fs_type == FS_FAT32)
        cl |= (DWORD)LD_WORD(dir + DIR_FstClusHI) << 16;

    return cl;
}


/**
 * @param dir Pointer to the directory entry.
 * @param cl Value to be set.
 */
static void st_clust(uint8_t * dir, DWORD cl)
{
    ST_WORD(dir + DIR_FstClusLO, cl);
    ST_WORD(dir + DIR_FstClusHI, cl >> 16);
}

/*-----------------------------------------------------------------------*/
/* LFN handling - Test/Pick/Fit an LFN segment from/to directory entry   */
/*-----------------------------------------------------------------------*/
#if configFATFS_LFN
/**
 * Offset of LFN characters in the directory entry.
 */
static const uint8_t LfnOfs[] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

/**
 * @param lfnbuf Pointer to the LFN to be compared.
 * @param dir Pointer to the directory entry containing a part of LFN.
 * @return 1:Matched, 0:Not matched.
 */
static int cmp_lfn(WCHAR * lfnbuf, uint8_t * dir)
{
    unsigned int i, s;
    WCHAR wc, uc;

    i = ((dir[LDIR_Ord] & ~LLE) - 1) * 13;  /* Get offset in the LFN buffer */
    s = 0; wc = 1;
    do {
        /* Pick an LFN character from the entry */
        uc = LD_WORD(dir + LfnOfs[s]);

        if (wc) {   /* Last character has not been processed */
            wc = ff_wtoupper(uc);       /* Convert it to upper case */

            /* Compare it */
            if (i >= _MAX_LFN || wc != ff_wtoupper(lfnbuf[i++]))
                return 0;               /* Not matched */
        } else {
            if (uc != 0xFFFF)
                return 0; /* Check filler */
        }
    } while (++s < 13); /* Repeat until all characters in the entry are checked */

    if ((dir[LDIR_Ord] & LLE) && wc && lfnbuf[i]) {
        /* Last segment matched but different length */
        return 0;
    }
    return 1; /* The part of LFN matched */
}

/**
 * @param lfnbuf Pointer to the Unicode-LFN buffer.
 * @param dir Pointer to the directory entry.
 * @return 1:Succeeded, 0:Buffer overflow.
 */
static int pick_lfn(WCHAR * lfnbuf, uint8_t * dir)
{
    unsigned int i, s;
    WCHAR wc, uc;

    i = ((dir[LDIR_Ord] & 0x3F) - 1) * 13;  /* Offset in the LFN buffer */

    s = 0; wc = 1;
    do {
        uc = LD_WORD(dir + LfnOfs[s]); /* Pick an LFN character from the entry */
        if (wc) {   /* Last character has not been processed */
            if (i >= _MAX_LFN)
                return 0;    /* Buffer overflow? */
            lfnbuf[i++] = wc = uc;          /* Store it */
        } else {
            if (uc != 0xFFFF)
                return 0;     /* Check filler */
        }
    } while (++s < 13); /* Read all character in the entry */

    if (dir[LDIR_Ord] & LLE) {
        /* Put terminator if it is the last LFN part */
        if (i >= _MAX_LFN)
            return 0;        /* Buffer overflow? */
        lfnbuf[i] = 0;
    }

    return 1;
}

/**
 * @param lfnbuf Pointer to the LFN buffer.
 * @param dir Pointer to the directory entry.
 * @param ord LFN order (1-20).
 * @param sum SFN sum.
 */
static void fit_lfn(const WCHAR * lfnbuf, uint8_t * dir, uint8_t ord,
                    uint8_t sum)
{
    unsigned int i, s;
    WCHAR wc;

    dir[LDIR_Chksum] = sum;         /* Set check sum */
    dir[LDIR_Attr] = AM_LFN;        /* Set attribute. LFN entry */
    dir[LDIR_Type] = 0;
    ST_WORD(dir + LDIR_FstClusLO, 0);

    i = (ord - 1) * 13;             /* Get offset in the LFN buffer */
    s = wc = 0;
    do {
        if (wc != 0xFFFF)
            wc = lfnbuf[i++]; /* Get an effective character */
        ST_WORD(dir + LfnOfs[s], wc); /* Put it */
        if (!wc)
            wc = 0xFFFF; /* Padding characters following last character */
    } while (++s < 13);
    if (wc == 0xFFFF || !lfnbuf[i])
        ord |= LLE; /* Bottom LFN part is the start of LFN sequence */
    dir[LDIR_Ord] = ord;            /* Set the LFN order */
}

#endif /* configFATFS_LFN */

/*-----------------------------------------------------------------------*/
/* Create numbered name                                                  */
/*-----------------------------------------------------------------------*/
#if configFATFS_LFN
/**
 * Generate a numbered name.
 * @param dst Pointer to the buffer to store numbered SFN.
 * @param src Pointer to SFN.
 * @param lfn Pointer to LFN.
 * @param seq Sequence number.
 */
static void gen_numname(uint8_t * dst, const uint8_t * src, const WCHAR * lfn,
                        unsigned int seq)
{
    uint8_t ns[8], c;
    unsigned int i, j;

    memcpy(dst, src, 11);

    if (seq > 5) {
        /*
         * On many collisions, generate a hash number instead of sequential
         * number.
         */

        WCHAR wc;
        DWORD sr = seq;

        while (*lfn) {  /* Create a CRC */
            wc = *lfn++;
            for (i = 0; i < 16; i++) {
                sr = (sr << 1) + (wc & 1);
                wc >>= 1;
                if (sr & 0x10000) sr ^= 0x11021;
            }
        }
        seq = (unsigned int)sr;
    }

    /* itoa (hexdecimal) */
    i = 7;
    do {
        c = (seq % 16) + '0';
        if (c > '9') c += 7;
        ns[i--] = c;
        seq /= 16;
    } while (seq);
    ns[i] = '~';

    /* Append the number */
    for (j = 0; j < i && dst[j] != ' '; j++) {
        if (IsDBCS1(dst[j])) {
            if (j == i - 1) break;
            j++;
        }
    }
    do {
        dst[j++] = (i < 8) ? ns[i++] : ' ';
    } while (j < 8);
}
#endif

#if configFATFS_LFN
/**
 * Calculate sum of an SFN.
 * @param dir Pointer to the SFN entry.
 */
static uint8_t sum_sfn(const uint8_t * dir)
{
    uint8_t sum = 0;
    unsigned int n = 11;

    do {
        sum = (sum >> 1) + (sum << 7) + *dir++;
    } while (--n);

    return sum;
}
#endif

/**
 * Directory handling - Find an object in the directory.
 * @param dp Pointer to the directory object linked to the file name.
 */
static FRESULT dir_find(FF_DIR * dp)
{
    FRESULT res;
    uint8_t c;
    uint8_t * dir;
#if configFATFS_LFN
    uint8_t a, ord, sum;
#endif

    res = dir_sdi(dp, 0);           /* Rewind directory object */
    if (res != FR_OK)
        return res;

#if configFATFS_LFN
    ord = sum = 0xFF; dp->lfn_idx = 0xFFFF; /* Reset LFN sequence */
#endif
    do {
        res = move_window(dp->fs, dp->sect);
        if (res != FR_OK)
            break;
        dir = dp->dir;                  /* Ptr to the directory entry of current index */
        c = dir[DIR_Name];
        if (c == 0) {
            /* Reached to end of table */
            res = FR_NO_FILE;
            break;
        }
#if configFATFS_LFN    /* LFN configuration */
        a = dir[DIR_Attr] & AM_MASK;
        if (c == DDE || ((a & AM_VOL) && a != AM_LFN)) {    /* An entry without valid data */
            ord = 0xFF; dp->lfn_idx = 0xFFFF;   /* Reset LFN sequence */
        } else {
            if (a == AM_LFN) {          /* An LFN entry is found */
                if (dp->lfn) {
                    if (c & LLE) {      /* Is it start of LFN sequence? */
                        sum = dir[LDIR_Chksum];
                        c &= ~LLE; ord = c; /* LFN start order */
                        dp->lfn_idx = dp->index;    /* Start index of LFN */
                    }
                    /* Check validity of the LFN entry and compare it with given name */
                    ord = (c == ord && sum == dir[LDIR_Chksum] && cmp_lfn(dp->lfn, dir)) ? ord - 1 : 0xFF;
                }
            } else {                    /* An SFN entry is found */
                if (!ord && sum == sum_sfn(dir))
                    break; /* LFN matched? */
                if (!(dp->fn[NS] & NS_LOSS) && !memcmp(dir, dp->fn, 11))
                    break;    /* SFN matched? */
                ord = 0xFF; dp->lfn_idx = 0xFFFF;   /* Reset LFN sequence */
            }
        }
#else       /* Non LFN configuration */
        if (!(dir[DIR_Attr] & AM_VOL) && !memcmp(dir, dp->fn, 11)) /* Is it a valid entry? */
            break;
#endif
        res = dir_next(dp, 0);      /* Next entry */
    } while (res == FR_OK);

    return res;
}

/**
 * Read an object from the directory.
 * @param dp Pointer to the directory object.
 * @param vol Filtered by 0:file/directory or 1:volume label.
 */
static FRESULT dir_read(FF_DIR * dp, int vol)
{
    FRESULT res;
    uint8_t a, c;
    uint8_t * dir;
#if configFATFS_LFN
    uint8_t ord = 0xFF, sum = 0xFF;
#endif

    res = FR_NO_FILE;
    while (dp->sect) {
        res = move_window(dp->fs, dp->sect);
        if (res != FR_OK)
            break;
        dir = dp->dir;                  /* Ptr to the directory entry of current index */
        c = dir[DIR_Name];
        if (c == 0) {
            res = FR_NO_FILE;
            break;
        }    /* Reached to end of table */
        a = dir[DIR_Attr] & AM_MASK;
#if configFATFS_LFN    /* LFN configuration */
        if (c == DDE || c == '.' || (int)(a == AM_VOL) != vol) {
            /* An entry without valid data */
            ord = 0xFF;
        } else {
            if (a == AM_LFN) {          /* An LFN entry is found */
                if (c & LLE) {          /* Is it start of LFN sequence? */
                    sum = dir[LDIR_Chksum];
                    c &= ~LLE; ord = c;
                    dp->lfn_idx = dp->index;
                }
                /* Check LFN validity and capture it */
                ord = (c == ord && sum == dir[LDIR_Chksum] && pick_lfn(dp->lfn, dir)) ? ord - 1 : 0xFF;
            } else {                    /* An SFN entry is found */
                if (ord || sum != sum_sfn(dir)) /* Is there a valid LFN? */
                    dp->lfn_idx = 0xFFFF;       /* It has no LFN. */
                break;
            }
        }
#else       /*
             * Non LFN configuration.
             * Is it a valid entry?
             */
        if (c != DDE && c != '.' && a != AM_LFN && (int)(a == AM_VOL) == vol)
            break;
#endif
        res = dir_next(dp, 0);              /* Next entry */
        if (res != FR_OK)
            break;
    }

    if (res != FR_OK) dp->sect = 0;

    return res;
}

/**
 * Register an object to the directory.
 * @param dp Target directory with object name to be created.
 * @return FR_OK:Successful, FR_DENIED:No free entry or too many SFN collision,
 *         FR_DISK_ERR:Disk error.
 */
static FRESULT dir_register(FF_DIR * dp)
{
    FRESULT res;
#if configFATFS_LFN    /* LFN configuration */
    unsigned int n, nent;
    uint8_t sn[12], sum;
    uint8_t * fn;
    WCHAR * lfn;

    fn = dp->fn; lfn = dp->lfn;
    memcpy(sn, fn, 12);

    if (sn[NS] & NS_LOSS) {
        /* When LFN is out of 8.3 format, generate a numbered name */
        fn[NS] = 0; dp->lfn = 0; /* Find only SFN */
        for (n = 1; n < 100; n++) {
            gen_numname(fn, sn, lfn, n); /* Generate a numbered name */
            res = dir_find(dp); /* Check if the name collides with existing SFN */
            if (res != FR_OK)
                break;
        }
        if (n == 100)
            return FR_DENIED;     /* Abort if too many collisions */
        if (res != FR_NO_FILE)
            return res;  /* Abort if the result is other than 'not collided' */
        fn[NS] = sn[NS]; dp->lfn = lfn;
    }

    if (sn[NS] & NS_LFN) {
        /* When LFN is to be created, allocate entries for an SFN + LFNs. */
        for (n = 0; lfn[n]; n++) ;
        nent = (n + 25) / 13;
    } else {
        /* Otherwise allocate an entry for an SFN  */
        nent = 1;
    }
    res = dir_alloc(dp, nent);      /* Allocate entries */

    if (res == FR_OK && --nent) {   /* Set LFN entry if needed */
        res = dir_sdi(dp, dp->index - nent);
        if (res == FR_OK) {
            sum = sum_sfn(dp->fn);  /* Sum value of the SFN tied to the LFN */
            do {                    /* Store LFN entries in bottom first */
                res = move_window(dp->fs, dp->sect);
                if (res != FR_OK)
                    break;
                fit_lfn(dp->lfn, dp->dir, (uint8_t)nent, sum);
                dp->fs->wflag = 1;
                res = dir_next(dp, 0);  /* Next entry */
            } while (res == FR_OK && --nent);
        }
    }
#else   /* Non LFN configuration */
    res = dir_alloc(dp, 1);     /* Allocate an entry for SFN */
#endif

    if (res == FR_OK) {             /* Set SFN entry */
        res = move_window(dp->fs, dp->sect);
        if (res == FR_OK) {
            memset(dp->dir, 0, SZ_DIR);    /* Clean the entry */
            memcpy(dp->dir, dp->fn, 11);   /* Put SFN */
#if configFATFS_LFN
            dp->dir[DIR_NTres] = dp->fn[NS] & (NS_BODY | NS_EXT);   /* Put NT flag */
#endif
            dp->fs->wflag = 1;
        }
    }

    return res;
}

/**
 * Remove an object from the directory.
 * @param dp Directory object pointing the entry to be removed.
 * @return FR_OK: Successful, FR_DISK_ERR: A disk error.
 */
static FRESULT dir_remove (FF_DIR * dp)
{
    FRESULT res;
#if configFATFS_LFN    /* LFN configuration */
    unsigned int i;

    i = dp->index;  /* SFN index */
    res = dir_sdi(dp, (dp->lfn_idx == 0xFFFF) ? i : dp->lfn_idx);   /* Goto the SFN or top of the LFN entries */
    if (res == FR_OK) {
        do {
            res = move_window(dp->fs, dp->sect);
            if (res != FR_OK)
                break;
            memset(dp->dir, 0, SZ_DIR);    /* Clear and mark the entry "deleted" */
            *dp->dir = DDE;
            dp->fs->wflag = 1;
            if (dp->index >= i)
                break;  /* When reached SFN, all entries of the object has been deleted. */
            res = dir_next(dp, 0);      /* Next entry */
        } while (res == FR_OK);
        if (res == FR_NO_FILE)
            res = FR_INT_ERR;
    }

#else           /* Non LFN configuration */
    res = dir_sdi(dp, dp->index);
    if (res == FR_OK) {
        res = move_window(dp->fs, dp->sect);
        if (res == FR_OK) {
            memset(dp->dir, 0, SZ_DIR); /* Clear and mark the entry "deleted" */
            *dp->dir = DDE;
            dp->fs->wflag = 1;
        }
    }
#endif

    return res;
}

/**
 * Get file information from directory entry.
 * @param dp Pointer to the directory object.
 * @param fno Pointer to the file information to be filled.
 */
static void get_fileinfo(FF_DIR * dp, FILINFO * fno)
{
    unsigned int i;
    TCHAR c;
    TCHAR * p;

    p = fno->fname;
    if (dp->sect) { /* Get SFN */
        uint8_t *dir = dp->dir;

        i = 0;
        while (i < 11) { /* Copy name body and extension */
            c = (TCHAR)dir[i++];
            if (c == ' ')
                continue;         /* Skip padding spaces */
            if (c == NDDE)
                c = (TCHAR)DDE;  /* Restore replaced DDE character */
            if (i == 9)
                *p++ = '.';         /* Insert a . if extension is exist */
#if configFATFS_LFN
            if (ka_isupper(c) && (dir[DIR_NTres] & (i >= 9 ? NS_EXT : NS_BODY)))
                c += 0x20;          /* To lower */
#if _LFN_UNICODE
            if (IsDBCS1(c) && i != 8 && i != 11 && IsDBCS2(dir[i]))
                c = c << 8 | dir[i++];
            c = ff_convert(c, 1);   /* OEM -> Unicode */
            if (!c)
                c = '?';
#endif
#endif
            *p++ = c;
        }
        fno->fattrib = dir[DIR_Attr];               /* Attribute */
        fno->fsize = LD_DWORD(dir + DIR_FileSize);  /* Size */
        fno->fdate = LD_WORD(dir + DIR_WrtDate);    /* Date */
        fno->ftime = LD_WORD(dir + DIR_WrtTime);    /* Time */
        fno->ino = GET_INO(dp);
    }
    *p = 0;     /* Terminate SFN string by a \0 */

#if configFATFS_LFN
    if (fno->lfname) {
        WCHAR w, *lfn;

        i = 0; p = fno->lfname;
        if (dp->sect && fno->lfsize && dp->lfn_idx != 0xFFFF) {
            /* Get LFN if available */
            lfn = dp->lfn;
            while ((w = *lfn++) != 0) {     /* Get an LFN character */
#if !_LFN_UNICODE
                w = ff_convert(w, 0);       /* Unicode -> OEM */
                if (!w) {
                    i = 0;
                    break;
                }   /* No LFN if it could not be converted */
                if (_DF1S && w >= 0x100)    /* Put 1st byte if it is a DBC (always false on SBCS cfg) */
                    p[i++] = (TCHAR)(w >> 8);
#endif
                if (i >= fno->lfsize - 1) {
                    i = 0;
                    break;
                } /* No LFN if buffer overflow */
                p[i++] = (TCHAR)w;
            }
        }
        p[i] = 0;   /* Terminate LFN string by a \0 */
    }
#endif
}

/**
 * Pick a segment and create the object name in directory form.
 * @param dp Pointer to the directory object.
 * @param path Pointer to pointer to the segment in the path string.
 */
static FRESULT create_name (FF_DIR * dp, const TCHAR ** path)
{
#if configFATFS_LFN    /* LFN configuration */
    uint8_t b, cf;
    WCHAR w, * lfn;
    unsigned int i, ni, si, di;
    const TCHAR * p;

    /* Create LFN in Unicode */
    for (p = *path; *p == '/' || *p == '\\'; p++) ; /* Strip duplicated separator */
    lfn = dp->lfn;
    si = di = 0;
    for (;;) {
        w = p[si++]; /* Get a character */
        if (w < ' ' || w == '/' || w == '\\')
            break; /* Break on end of segment */
        if (di >= _MAX_LFN) /* Reject too long name */
            return FR_INVALID_NAME;
#if !_LFN_UNICODE
        w &= 0xFF;

        /* Check if it is a DBC 1st byte (always false on SBCS cfg) */
        if (IsDBCS1(w)) {
            b = (uint8_t)p[si++];          /* Get 2nd byte */
            if (!IsDBCS2(b))
                return FR_INVALID_NAME; /* Reject invalid sequence */
            w = (w << 8) + b;           /* Create a DBC */
        }
        w = ff_convert(w, 1);           /* Convert ANSI/OEM to Unicode */
        if (!w)
            return FR_INVALID_NAME; /* Reject invalid code */
#endif
        /* Reject illegal characters for LFN */
        if (w < 0x80 && kstrchr("\"*:<>\?|\x7F", w))
            return FR_INVALID_NAME;
        lfn[di++] = w;                  /* Store the Unicode character */
    }
    *path = &p[si];                     /* Return pointer to the next segment */
    cf = (w < ' ') ? NS_LAST : 0;       /* Set last segment flag if end of path */

    while (di) {                        /* Strip trailing spaces and dots */
        w = lfn[di-1];
        if (w != ' ' && w != '.')
            break;
        di--;
    }
    if (!di)
        return FR_INVALID_NAME;    /* Reject nul string */

    lfn[di] = 0;                        /* LFN is created */

    /* Create SFN in directory form */
    memset(dp->fn, ' ', 11);
    for (si = 0; lfn[si] == ' ' || lfn[si] == '.'; si++) ;  /* Strip leading spaces and dots */
    if (si)
        cf |= NS_LOSS | NS_LFN;
    while (di && lfn[di - 1] != '.')
        di--;  /* Find extension (di<=si: no extension) */

    b = i = 0; ni = 8;
    for (;;) {
        w = lfn[si++];                  /* Get an LFN character */
        if (!w)
            break;                  /* Break on end of the LFN */
        if (w == ' ' || (w == '.' && si != di)) {   /* Remove spaces and dots */
            cf |= NS_LOSS | NS_LFN;
            continue;
        }

        if (i >= ni || si == di) {      /* Extension or end of SFN */
            if (ni == 11) {             /* Long extension */
                cf |= NS_LOSS | NS_LFN;
                break;
            }
            if (si != di)
                cf |= NS_LOSS | NS_LFN;   /* Out of 8.3 format */
            if (si > di)
                break;         /* No extension */
            si = di; i = 8; ni = 11;    /* Enter extension section */
            b <<= 2;
            continue;
        }

        if (w >= 0x80) {                /* Non ASCII character */
#ifdef _EXCVT
            w = ff_convert(w, 0);       /* Unicode -> OEM code */
            if (w)
                w = ExCvt[w - 0x80]; /* Convert extended character to upper (SBCS) */
#else
            w = ff_convert(ff_wtoupper(w), 0);  /* Upper converted Unicode -> OEM code */
#endif
            cf |= NS_LFN;               /* Force create LFN entry */
        }

        if (_DF1S && w >= 0x100) {      /* Double byte character (always false on SBCS cfg) */
            if (i >= ni - 1) {
                cf |= NS_LOSS | NS_LFN;
                i = ni;
                continue;
            }
            dp->fn[i++] = (uint8_t)(w >> 8);
        } else {                        /* Single byte character */
            if (!w || kstrchr("+,;=[]", w)) {   /* Replace illegal characters for SFN */
                w = '_'; cf |= NS_LOSS | NS_LFN;/* Lossy conversion */
            } else {
                if (ka_isupper(w)) {       /* ASCII large capital */
                    b |= 2;
                } else {
                    if (ka_islower(w)) {   /* ASCII small capital */
                        b |= 1; w -= 0x20;
                    }
                }
            }
        }
        dp->fn[i++] = (uint8_t)w;
    }

    if (dp->fn[0] == DDE)
        dp->fn[0] = NDDE; /* If the first character collides with deleted mark, replace it with 0x05 */

    if (ni == 8) b <<= 2;
    if ((b & 0x0C) == 0x0C || (b & 0x03) == 0x03)   /* Create LFN entry when there are composite capitals */
        cf |= NS_LFN;
    if (!(cf & NS_LFN)) {
        /* When LFN is in 8.3 format without extended character, NT flags are created */
        if ((b & 0x03) == 0x01)
            cf |= NS_EXT;   /* NT flag (Extension has only small capital) */
        if ((b & 0x0C) == 0x04)
            cf |= NS_BODY;  /* NT flag (Filename has only small capital) */
    }

    dp->fn[NS] = cf;    /* SFN is created */

    return FR_OK;


#else   /* Non-LFN configuration */
    uint8_t b, c, d, *sfn;
    unsigned int ni, si, i;
    const char *p;

    /* Create file name in directory form */
    for (p = *path; *p == '/' || *p == '\\'; p++) ; /* Strip duplicated separator */
    sfn = dp->fn;
    memset(sfn, ' ', 11);
    si = i = b = 0; ni = 8;

    for (;;) {
        c = (uint8_t)p[si++];
        if (c <= ' ' || c == '/' || c == '\\')
            break;   /* Break on end of segment */
        if (c == '.' || i >= ni) {
            if (ni != 8 || c != '.')
                return FR_INVALID_NAME;
            i = 8; ni = 11;
            b <<= 2;
            continue;
        }
        if (c >= 0x80) {                /* Extended character? */
            b |= 3;                     /* Eliminate NT flag */
#ifdef _EXCVT
            c = ExCvt[c - 0x80];        /* To upper extended characters (SBCS cfg) */
#else
#if !_DF1S
            return FR_INVALID_NAME;     /* Reject extended characters (ASCII cfg) */
#endif
#endif
        }
        if (IsDBCS1(c)) {               /* Check if it is a DBC 1st byte (always false on SBCS cfg) */
            d = (uint8_t)p[si++];          /* Get 2nd byte */
            if (!IsDBCS2(d) || i >= ni - 1) /* Reject invalid DBC */
                return FR_INVALID_NAME;
            sfn[i++] = c;
            sfn[i++] = d;
        } else {                        /* Single byte code */
            if (kstrchr("\"*+,:;<=>\?[]|\x7F", c))  /* Reject illegal chrs for SFN */
                return FR_INVALID_NAME;
            if (ka_isupper(c)) {           /* ASCII large capital? */
                b |= 2;
            } else {
                if (ka_islower(c)) {       /* ASCII small capital? */
                    b |= 1; c -= 0x20;
                }
            }
            sfn[i++] = c;
        }
    }
    *path = &p[si];                     /* Return pointer to the next segment */
    c = (c <= ' ') ? NS_LAST : 0;       /* Set last segment flag if end of path */

    if (!i)
        return FR_INVALID_NAME;     /* Reject nul string */
    if (sfn[0] == DDE)
        sfn[0] = NDDE;   /* When first character collides with DDE, replace it with 0x05 */

    if (ni == 8) b <<= 2;
    if ((b & 0x03) == 0x01)
        c |= NS_EXT;    /* NT flag (Name extension has only small capital) */
    if ((b & 0x0C) == 0x04)
        c |= NS_BODY;   /* NT flag (Name body has only small capital) */

    sfn[NS] = c;        /* Store NT flag, File name is created */

    return FR_OK;
#endif
}

/**
 * Follow a file path.
 * @param dp Directory object to return last directory and found object.
 * @param path Full-path string to find a file or directory.
 * @return FR_OK(0): successful, !=0: error code.
 */
static FRESULT follow_path(FF_DIR * dp, const TCHAR * path)
{
    uint8_t ns;
    uint8_t * dir;
    FRESULT res;

    if (*path == '/' || *path == '\\')      /* Strip heading separator if exist */
        path++;
    dp->sclust = 0;                         /* Always start from the root directory */

    if ((unsigned int)*path < ' ') {
        /* Null path name is the origin directory itself */
        res = dir_sdi(dp, 0);
        dp->dir = 0;
    } else {                                /* Follow path */
        for (;;) {
            res = create_name(dp, &path);   /* Get a segment name of the path */
            if (res != FR_OK)
                break;
            res = dir_find(dp);             /* Find an object with the sagment name */
            ns = dp->fn[NS];
            if (res != FR_OK) {             /* Failed to find the object */
                if (res == FR_NO_FILE) {    /* Object is not found */
                    /* Could not find the object */
                    if (!(ns & NS_LAST)) {
                        /* Adjust error code if not last segment */
                        res = FR_NO_PATH;
                    }
                }
                break;
            }
            if (ns & NS_LAST)
                break;            /* Last segment matched. Function completed. */
            dir = dp->dir;                      /* Follow the sub-directory */
            if (!(dir[DIR_Attr] & AM_DIR)) {    /* It is not a sub-directory and cannot follow */
                res = FR_NO_PATH;
                break;
            }
            dp->sclust = ld_clust(dp->fs, dir);
        }
    }

    return res;
}

/**
 * TODO Remove this function when not needed anymore.
 * Get logical drive number from path name.
 * @param path Pointer to pointer to the path name.
 * @return Returns logical drive number (-1:invalid drive).
 */
static int get_ldnumber(const TCHAR ** path)
{
    const TCHAR * tp, * tt;
    unsigned int i;
    int vol = -1;

    if (*path) {    /* If the pointer is not a null */
        for (tt = *path; (unsigned int)*tt >= (configFATFS_LFN ? ' ' : '!') &&
                         *tt != ':'; tt++) ; /* Find ':' in the path */
        if (*tt == ':') {   /* If a ':' is exist in the path name */
            tp = *path;
            i = *tp++ - '0';
            if (i < 10 && tp == tt) {   /* Is there a numeric drive id? */
                if (i < _VOLUMES) {
                    /* If a drive id is found, get the value and strip it */
                    vol = (int)i;
                    *path = ++tt;
                }
            }

            return vol;
        }

        vol = 0;        /* Drive 0 */
    }

    return vol;
}

/**
 * Load a sector and check if it is an FAT boot sector.
 * @param fs File system object.
 * @param sect Sector# (lba) to check if it is an FAT boot record or not.
 * @return 0:FAT boor sector, 1:Valid boor sector but not FAT, 2:Not a boot sector,
 *         3:Disk error.
 */
static uint8_t check_fs(FATFS * fs, DWORD sect)
{
    fs->wflag = 0; fs->winsect = 0xFFFFFFFF;    /* Invaidate window */
    if (move_window(fs, sect) != FR_OK)         /* Load boot record */
        return 3;

    if (LD_WORD(&fs->win[BS_55AA]) != 0xAA55)   /* Check boot record signature (always placed at offset 510 even if the sector size is >512) */
        return 2;

    if ((LD_DWORD(&fs->win[BS_FilSysType]) & 0xFFFFFF) == 0x544146)     /* Check "FAT" string */
        return 0;
    if ((LD_DWORD(&fs->win[BS_FilSysType32]) & 0xFFFFFF) == 0x544146)   /* Check "FAT" string */
        return 0;

    return 1;
}

/**
 * Lock the logical drive and check if the volume is mounted.
 * @param vol is the volume number. RFE Is this actually needed?
 * @param wmode !=0: Check write protection for write access.
 * @return FR_OK(0): successful, !=0: any error occurred.
 */
static FRESULT find_volume(FATFS * fs, int vol, uint8_t wmode)
{
    uint8_t fmt;
    DSTATUS stat;
    DWORD bsect, fasize, tsect, sysect, nclst, szbfat;
    WORD nrsv;
    DRESULT derr;

    if (vol < 0)
        return FR_INVALID_DRIVE;

    if (!fs)
        return FR_NOT_ENABLED;

    ENTER_FF(fs); /* Lock the volume */

    if (fs->fs_type) { /* If the volume has been mounted */
        stat = fatfs_disk_status(fs->drv);
        /* and the physical drive is kept initialized */
        if (!(stat & STA_NOINIT)) {
            /* Check write protection if needed */
            if (!fs->readonly && (wmode & FA_READ) && (stat & STA_PROTECT))
                return FR_WRITE_PROTECTED;
            return FR_OK;               /* The file system object is valid */
        }
    }

    /* The file system object is not valid. */
    /* Following code attempts to mount the volume. (analyze BPB and initialize the fs object) */

    fs->fs_type = 0;                    /* Clear the file system object */
    fs->drv = LD2PD(vol);               /* Bind the logical drive and a physical drive */
    stat = fatfs_disk_initialize(fs->drv);    /* Initialize the physical drive */
    if (stat & STA_NOINIT)              /* Check if the initialization succeeded */
        return FR_NOT_READY;            /* Failed to initialize due to no medium or hard error */

    /* Check disk write protection if needed */
    if (!fs->readonly && wmode && (stat & STA_PROTECT))
        return FR_WRITE_PROTECTED;

    /*
     * Get sector size
     */
    derr = fatfs_disk_ioctl(fs->drv, IOCTL_GETBLKSIZE, &SS(fs), sizeof(SS(fs)));
    if (derr != RES_OK || SS(fs) < _MIN_SS || SS(fs) > _MAX_SS) {
#ifdef configFATFS_DEBUG
        KERROR(KERROR_DEBUG, "err %d, ss: %d < %d < %d\n", derr,
               (int)_MIN_SS, (int)SS(fs), (int)_MAX_SS);
#endif
        return FR_DISK_ERR;
    }

    /* Find an FAT partition on the drive. Supports only generic partitioning, FDISK and SFD. */
    bsect = 0;
    /* Load sector 0 and check if it is an FAT boot sector as SFD */
    fmt = check_fs(fs, bsect);
    if (fmt == 1 || (!fmt && (LD2PT(vol)))) {
        /* Not a FAT boot sector or forced partition number */
        unsigned int i;
        DWORD br[4];

        for (i = 0; i < 4; i++) {           /* Get partition offset */
            uint8_t *pt = fs->win + MBR_Table + i * SZ_PTE;
            br[i] = pt[4] ? LD_DWORD(&pt[8]) : 0;
        }
        i = LD2PT(vol);                     /* Partition number: 0:auto, 1-4:forced */
        if (i)
            i--;
        do {                                /* Find an FAT volume */
            bsect = br[i];
            fmt = bsect ? check_fs(fs, bsect) : 2;  /* Check the partition */
        } while (!LD2PT(vol) && fmt && ++i < 4);
    }
    if (fmt == 3) {
        return FR_DISK_ERR;       /* An error occured in the disk I/O layer */
    }
    if (fmt)
        return FR_NO_FILESYSTEM;       /* No FAT volume is found */

    /* An FAT volume is found. Following code initializes the file system object */

    if (LD_WORD(fs->win + BPB_BytsPerSec) != SS(fs)) {
        /* (BPB_BytsPerSec must be equal to the physical sector size) */
        return FR_NO_FILESYSTEM;
    }

    fasize = LD_WORD(fs->win+BPB_FATSz16); /* Number of sectors per FAT */
    if (!fasize)
        fasize = LD_DWORD(fs->win + BPB_FATSz32);
    fs->fsize = fasize;

    fs->n_fats = fs->win[BPB_NumFATs]; /* Number of FAT copies */
    if (fs->n_fats != 1 && fs->n_fats != 2) /* (Must be 1 or 2) */
        return FR_NO_FILESYSTEM;
    fasize *= fs->n_fats;                               /* Number of sectors for FAT area */

    fs->csize = fs->win[BPB_SecPerClus];                /* Number of sectors per cluster */
    if (!fs->csize || (fs->csize & (fs->csize - 1)))    /* (Must be power of 2) */
        return FR_NO_FILESYSTEM;

    fs->n_rootdir = LD_WORD(fs->win+BPB_RootEntCnt);    /* Number of root directory entries */
    if (fs->n_rootdir % (SS(fs) / SZ_DIR))              /* (Must be sector aligned) */
        return FR_NO_FILESYSTEM;

    tsect = LD_WORD(fs->win+BPB_TotSec16);              /* Number of sectors on the volume */
    if (!tsect)
        tsect = LD_DWORD(fs->win+BPB_TotSec32);

    nrsv = LD_WORD(fs->win+BPB_RsvdSecCnt);             /* Number of reserved sectors */
    if (!nrsv)
        return FR_NO_FILESYSTEM;                 /* (Must not be 0) */

    /* Determine the FAT sub type */
    sysect = nrsv + fasize + fs->n_rootdir / (SS(fs) / SZ_DIR); /* RSV+FAT+DIR */
    if (tsect < sysect)
        return FR_NO_FILESYSTEM;        /* (Invalid volume size) */
    nclst = (tsect - sysect) / fs->csize;               /* Number of clusters */
    if (!nclst)
        return FR_NO_FILESYSTEM;                /* (Invalid volume size) */
    fmt = FS_FAT12;
    if (nclst >= MIN_FAT16)
        fmt = FS_FAT16;
    if (nclst >= MIN_FAT32)
        fmt = FS_FAT32;

    /* Boundaries and Limits */
    fs->n_fatent = nclst + 2;                           /* Number of FAT entries */
    fs->volbase = bsect;                                /* Volume start sector */
    fs->fatbase = bsect + nrsv;                         /* FAT start sector */
    fs->database = bsect + sysect;                      /* Data start sector */
    if (fmt == FS_FAT32) {
        if (fs->n_rootdir)
            return FR_NO_FILESYSTEM;     /* (BPB_RootEntCnt must be 0) */
        fs->dirbase = LD_DWORD(fs->win+BPB_RootClus);   /* Root directory start cluster */
        szbfat = fs->n_fatent * 4;                      /* (Needed FAT size) */
    } else {
        if (!fs->n_rootdir)
            return FR_NO_FILESYSTEM;    /* (BPB_RootEntCnt must not be 0) */
        fs->dirbase = fs->fatbase + fasize;             /* Root directory start sector */
        szbfat = (fmt == FS_FAT16) ?                    /* (Needed FAT size) */
            fs->n_fatent * 2 : fs->n_fatent * 3 / 2 + (fs->n_fatent & 1);
    }
    if (fs->fsize < (szbfat + (SS(fs) - 1)) / SS(fs)) {
        /* (BPB_FATSz must not be less than needed) */
        return FR_NO_FILESYSTEM;
    }

    if (!fs->readonly) {
        /* Initialize cluster allocation information */
        fs->last_clust = fs->free_clust = 0xFFFFFFFF;

        /* Get fsinfo if available */
        fs->fsi_flag = 0x80;
#if (_FS_NOFSINFO & 3) != 3
        /* Enable FSINFO only if FAT32 and BPB_FSInfo is 1 */
        if (fmt == FS_FAT32 && LD_WORD(fs->win+BPB_FSInfo) == 1 &&
            move_window(fs, bsect + 1) == FR_OK) {
            fs->fsi_flag = 0;
            /* Load FSINFO data if available */
            if (LD_WORD(fs->win+BS_55AA) == 0xAA55 &&
                LD_DWORD(fs->win+FSI_LeadSig) == 0x41615252 &&
                LD_DWORD(fs->win+FSI_StrucSig) == 0x61417272) {
#if (_FS_NOFSINFO & 1) == 0
                fs->free_clust = LD_DWORD(fs->win+FSI_Free_Count);
#endif
#if (_FS_NOFSINFO & 2) == 0
                fs->last_clust = LD_DWORD(fs->win+FSI_Nxt_Free);
#endif
            }
        }
#endif
    }
    fs->fs_type = fmt;  /* FAT sub-type */
    fs->id = ++Fsid;    /* File system mount ID */
#if _FS_LOCK            /* Clear file lock semaphores */
    clear_lock(fs);
#endif

    return FR_OK;
}

/**
 * Check if the file/directory object is valid or not.
 * @param obj Pointer to the object FIL/DIR to check validity.
 * @return FR_OK(0): The object is valid, !=0: Invalid.
 */
static FRESULT validate(void * obj)
{

   /* Assuming offset of .fs and .id in the FIL/DIR structure is identical */
   FF_FIL * fil = (FF_FIL *)obj;

    if (!fil || !fil->fs || !fil->fs->fs_type || fil->fs->id != fil->id ||
            (fatfs_disk_status(fil->fs->drv) & STA_NOINIT)) {
        return FR_INVALID_OBJECT;
    }

    ENTER_FF(fil->fs);      /* Lock file system */

    return FR_OK;
}


/*
 * Public Functions
 * ----------------
 */

/**
 * Mount a Logical Drive
 */
FRESULT f_mount(FATFS * fs, const TCHAR * path, uint8_t opt)
{
    int vol;
    FRESULT res;
    const TCHAR * rp = path;

    vol = get_ldnumber(&rp);
    if (vol < 0)
        return FR_INVALID_DRIVE;

    fs->fs_type = 0;
    fs->readonly = (opt & FATFS_READONLY) == FATFS_READONLY;
    mtx_init(&fs->sobj, MTX_TYPE_TICKET, MTX_OPT_PRICEIL);

    res = find_volume(fs, vol, 0);
    LEAVE_FF(fs, res);
}

/**
 * Open or Create a File.
 * @param fp Pointer to the blank file object.
 * @param path Pointer to the file name.
 * @param mode Access mode and file open mode flags.
 */
FRESULT f_open(FF_FIL * fp, FATFS * fs, const TCHAR * path, uint8_t mode)
{
    FRESULT res;
    FF_DIR dj;
    uint8_t * dir;
    DEF_NAMEBUF;

    if (!fp)
        return FR_INVALID_OBJECT;
    fp->fs = NULL;
    dj.fs = fs;

    mode &= FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS |
            FA_CREATE_NEW;
    res = find_volume(dj.fs, get_ldnumber(&path), mode);
    if (res == FR_OK) {
        INIT_BUF(dj);
        res = follow_path(&dj, path);   /* Follow the file path */
        dir = dj.dir;
        if ((mode & FA_WRITE) && res == FR_OK) {
            if (!dir)   /* Default directory itself */
                res = FR_INVALID_NAME;
#if _FS_LOCK
            else
                res = chk_lock(&dj, (mode & ~FA_READ) ? 1 : 0);
#endif
        }
        /* Create or Open a file */
        if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
            DWORD dw, cl;

            if (res != FR_OK) {                 /* No file, create new */
                if (res == FR_NO_FILE)          /* There is no file to open, create a new entry */
#if _FS_LOCK
                    res = enq_lock() ? dir_register(&dj) : FR_TOO_MANY_OPEN_FILES;
#else
                    res = dir_register(&dj);
#endif
                mode |= FA_CREATE_ALWAYS;       /* File is created */
                dir = dj.dir;                   /* New entry */
            } else { /* Any object is already existing */
                if (dir[DIR_Attr] & (AM_RDO | AM_DIR)) {    /* Cannot overwrite it (R/O or DIR) */
                    res = FR_DENIED;
                } else {
                    if (mode & FA_CREATE_NEW)   /* Cannot create as new file */
                        res = FR_EXIST;
                }
            }
            if (res == FR_OK && (mode & FA_CREATE_ALWAYS)) {    /* Truncate it if overwrite mode */
                dw = get_fattime();             /* Created time */
                ST_DWORD(dir+DIR_CrtTime, dw);
                dir[DIR_Attr] = 0;              /* Reset attribute */
                ST_DWORD(dir+DIR_FileSize, 0);  /* size = 0 */
                cl = ld_clust(dj.fs, dir);      /* Get start cluster */
                st_clust(dir, 0);               /* cluster = 0 */
                dj.fs->wflag = 1;
                if (cl) {                       /* Remove the cluster chain if exist */
                    dw = dj.fs->winsect;
                    res = remove_chain(dj.fs, cl);
                    if (res == FR_OK) {
                        dj.fs->last_clust = cl - 1; /* Reuse the cluster hole */
                        res = move_window(dj.fs, dw);
                    }
                }
            }
        }
        else {  /* Open an existing file */
            if (res == FR_OK) {                 /* Follow succeeded */
                if (dir[DIR_Attr] & AM_DIR) {   /* It is a directory */
                    res = FR_NO_FILE;
                } else {
                    if ((mode & FA_WRITE) && (dir[DIR_Attr] & AM_RDO)) /* R/O violation */
                        res = FR_DENIED;
                }
            }
        }
        if (res == FR_OK) {
            if (mode & FA_CREATE_ALWAYS)        /* Set file change flag if created or overwritten */
                mode |= FA__WRITTEN;
            fp->dir_sect = dj.fs->winsect;      /* Pointer to the directory entry */
            fp->dir_ptr = dir;
#if _FS_LOCK
            fp->lockid = inc_lock(&dj, (mode & ~FA_READ) ? 1 : 0);
            if (!fp->lockid) res = FR_INT_ERR;
#endif
        }

        /* R/O configuration */
        if ((mode & FA_READ) && res == FR_OK) {
            /* Follow succeeded */
            dir = dj.dir;
            if (!dir) {                     /* Current directory itself */
                res = FR_INVALID_NAME;
            } else {
                if (dir[DIR_Attr] & AM_DIR) /* It is a directory */
                    res = FR_NO_FILE;
            }
        }

        FREE_BUF();

        if (res == FR_OK) {
            fp->flag = mode;                    /* File access mode */
            fp->err = 0;                        /* Clear error flag */
            fp->ino = GET_INO(&dj);
            fp->sclust = ld_clust(dj.fs, dir);  /* File start cluster */
            fp->fsize = LD_DWORD(dir+DIR_FileSize); /* File size */
            fp->fptr = 0;                       /* File pointer */
            fp->dsect = 0;
#if _USE_FASTSEEK
            fp->cltbl = 0;                      /* Normal seek mode */
#endif
            fp->fs = dj.fs;                     /* Validate file object */
            fp->id = fp->fs->id;
        }
    }

    LEAVE_FF(dj.fs, res);
}

/**
 * Read File.
 * @param fp Pointer to the file object.
 * @param buff Pointer to data buffer.
 * @param btr Number of bytes to read.
 * @param br Pointer to number of bytes read.
 */
FRESULT f_read(FF_FIL * fp, void * buff, unsigned int btr, unsigned int * br)
{
    FRESULT res;
    DWORD clst, sect, remain;
    unsigned int rcnt, cc;
    uint8_t csect;
    uint8_t * rbuff = (uint8_t *)buff;

    *br = 0;    /* Clear read byte counter */

    res = validate(fp);                         /* Check validity */
    if (res != FR_OK)
        LEAVE_FF(fp->fs, res);
    if (fp->err)                                /* Check error */
        LEAVE_FF(fp->fs, (FRESULT)fp->err);
    if (!(fp->flag & FA_READ))                  /* Check access mode */
        LEAVE_FF(fp->fs, FR_DENIED);
    remain = fp->fsize - fp->fptr;
    if (btr > remain) btr = (unsigned int)remain;       /* Truncate btr by remaining bytes */

    for ( ;  btr;                               /* Repeat until all data read */
        rbuff += rcnt, fp->fptr += rcnt, *br += rcnt, btr -= rcnt) {
        if ((fp->fptr % SS(fp->fs)) == 0) {     /* On the sector boundary? */
            csect = (uint8_t)(fp->fptr / SS(fp->fs) & (fp->fs->csize - 1));    /* Sector offset in the cluster */
            if (!csect) {                       /* On the cluster boundary? */
                if (fp->fptr == 0) {            /* On the top of the file? */
                    clst = fp->sclust;          /* Follow from the origin */
                } else {                        /* Middle or end of the file */
#if _USE_FASTSEEK
                    if (fp->cltbl)
                        clst = clmt_clust(fp, fp->fptr);    /* Get cluster# from the CLMT */
                    else
#endif
                        clst = get_fat(fp->fs, fp->clust);  /* Follow cluster chain on the FAT */
                }
                if (clst < 2)
                    ABORT(fp->fs, FR_INT_ERR);
                if (clst == 0xFFFFFFFF)
                    ABORT(fp->fs, FR_DISK_ERR);
                fp->clust = clst;               /* Update current cluster */
            }
            sect = clust2sect(fp->fs, fp->clust);   /* Get current sector */
            if (!sect)
                ABORT(fp->fs, FR_INT_ERR);
            sect += csect;
            cc = btr / SS(fp->fs);              /* When remaining bytes >= sector size, */
            if (cc) {                           /* Read maximum contiguous sectors directly */
                if (csect + cc > fp->fs->csize) /* Clip at cluster boundary */
                    cc = fp->fs->csize - csect;
                if (fatfs_disk_read(fp->fs->drv, rbuff, sect, cc * SS(fp->fs)))
                    ABORT(fp->fs, FR_DISK_ERR);
                /*
                 * Replace one of the read sectors with cached data if it
                 * contains a dirty sector
                 */
                if (!fp->fs->readonly && (fp->flag & FA__DIRTY) &&
                    fp->dsect - sect < cc) {
                    memcpy(rbuff + ((fp->dsect - sect) * SS(fp->fs)), fp->buf,
                           SS(fp->fs));
                }

                rcnt = SS(fp->fs) * cc;         /* Number of bytes transferred */
                continue;
            }
            if (fp->dsect != sect) {            /* Load data sector if not in cache */
                if (!fp->fs->readonly && fp->flag & FA__DIRTY) {
                    /* Write-back dirty sector cache */
                    if (fatfs_disk_write(fp->fs->drv, fp->buf, fp->dsect, SS(fp->fs)))
                        ABORT(fp->fs, FR_DISK_ERR);
                    fp->flag &= ~FA__DIRTY;
                }

                if (fatfs_disk_read(fp->fs->drv, fp->buf, sect, SS(fp->fs)))   /* Fill sector cache */
                    ABORT(fp->fs, FR_DISK_ERR);
            }
            fp->dsect = sect;
        }
        rcnt = SS(fp->fs) - ((unsigned int)fp->fptr % SS(fp->fs));  /* Get partial sector data from sector buffer */
        if (rcnt > btr) rcnt = btr;
        memcpy(rbuff, &fp->buf[fp->fptr % SS(fp->fs)], rcnt);  /* Pick partial sector */
    }

    LEAVE_FF(fp->fs, FR_OK);
}

/**
 * Write File.
 * @param fp Pointer to the file object.
 * @param buff Pointer to the data to be written.
 * @param btw Number of bytes to write.
 * @param bw Pointer to number of bytes written.
 */
FRESULT f_write(FF_FIL * fp, const void * buff, unsigned int btw,
                unsigned int * bw)
{
    FRESULT res;
    DWORD clst, sect;
    unsigned int wcnt, cc;
    const uint8_t * wbuff = (const uint8_t *)buff;
    uint8_t csect;

    *bw = 0;    /* Clear write byte counter */

    res = validate(fp);                     /* Check validity */
    if (res != FR_OK)
        LEAVE_FF(fp->fs, res);
    if (fp->err)                            /* Check error */
        LEAVE_FF(fp->fs, (FRESULT)fp->err);
    if (!(fp->flag & FA_WRITE))             /* Check access mode */
        LEAVE_FF(fp->fs, FR_DENIED);
    if (fp->fptr + btw < fp->fptr) btw = 0; /* File size cannot reach 4GB */

    for ( ;  btw;                           /* Repeat until all data written */
        wbuff += wcnt, fp->fptr += wcnt, *bw += wcnt, btw -= wcnt) {
        if ((fp->fptr % SS(fp->fs)) == 0) { /* On the sector boundary? */
            csect = (uint8_t)(fp->fptr / SS(fp->fs) & (fp->fs->csize - 1));    /* Sector offset in the cluster */
            if (!csect) {                   /* On the cluster boundary? */
                if (fp->fptr == 0) {        /* On the top of the file? */
                    clst = fp->sclust;      /* Follow from the origin */
                    if (clst == 0)          /* When no cluster is allocated, */
                        clst = create_chain(fp->fs, 0); /* Create a new cluster chain */
                } else {                    /* Middle or end of the file */
#if _USE_FASTSEEK
                    if (fp->cltbl)
                        clst = clmt_clust(fp, fp->fptr);    /* Get cluster# from the CLMT */
                    else
#endif
                        clst = create_chain(fp->fs, fp->clust); /* Follow or stretch cluster chain on the FAT */
                }
                if (clst == 0) break;       /* Could not allocate a new cluster (disk full) */
                if (clst == 1) ABORT(fp->fs, FR_INT_ERR);
                if (clst == 0xFFFFFFFF) ABORT(fp->fs, FR_DISK_ERR);
                fp->clust = clst;           /* Update current cluster */
                if (fp->sclust == 0) fp->sclust = clst; /* Set start cluster if the first write */
            }

            if (fp->flag & FA__DIRTY) {     /* Write-back sector cache */
                if (fatfs_disk_write(fp->fs->drv, fp->buf, fp->dsect,
                                     SS(fp->fs))) {
                    ABORT(fp->fs, FR_DISK_ERR);
                }
                fp->flag &= ~FA__DIRTY;
            }

            sect = clust2sect(fp->fs, fp->clust);   /* Get current sector */
            if (!sect) ABORT(fp->fs, FR_INT_ERR);
            sect += csect;
            cc = btw / SS(fp->fs); /* When remaining bytes >= sector size, */
            if (cc) { /* Write maximum contiguous sectors directly */
                if (csect + cc > fp->fs->csize) /* Clip at cluster boundary */
                    cc = fp->fs->csize - csect;
                if (fatfs_disk_write(fp->fs->drv, wbuff, sect, cc * SS(fp->fs)))
                    ABORT(fp->fs, FR_DISK_ERR);
                if (fp->dsect - sect < cc) {
                    /*
                     * Refill sector cache if it gets invalidated by the direct
                     * write
                     */
                    memcpy(fp->buf, wbuff + ((fp->dsect - sect) * SS(fp->fs)),
                           SS(fp->fs));
                    fp->flag &= ~FA__DIRTY;
                }
                wcnt = SS(fp->fs) * cc;     /* Number of bytes transferred */
                continue;
            }

            if (fp->dsect != sect) {
                /* Fill sector cache with file data */
                if (fp->fptr < fp->fsize &&
                    fatfs_disk_read(fp->fs->drv, fp->buf, sect, SS(fp->fs))) {
                        ABORT(fp->fs, FR_DISK_ERR);
                }
            }
            fp->dsect = sect;
        }
        /* Put partial sector into file I/O buffer */
        wcnt = SS(fp->fs) - ((unsigned int)fp->fptr % SS(fp->fs));
        if (wcnt > btw)
            wcnt = btw;

        /* Fit partial sector */
        memcpy(&fp->buf[fp->fptr % SS(fp->fs)], wbuff, wcnt);
        fp->flag |= FA__DIRTY;
    }

    if (fp->fptr > fp->fsize)
        fp->fsize = fp->fptr;   /* Update file size if needed */
    fp->flag |= FA__WRITTEN;    /* Set file change flag */

    LEAVE_FF(fp->fs, FR_OK);
}

/**
 * Synchronize the File.
 * @param fp Pointer to the file object.
 */
FRESULT f_sync(FF_FIL * fp)
{
    FRESULT res;
    DWORD tm;
    uint8_t * dir;

    res = validate(fp);                 /* Check validity of the object */
    if (res != FR_OK)
        goto fail;

    if (fp->flag & FA__WRITTEN) {   /* Has the file been written? */
        /* Write-back dirty buffer */
        if (fp->flag & FA__DIRTY) {
            if (fatfs_disk_write(fp->fs->drv, fp->buf, fp->dsect, SS(fp->fs)))
                LEAVE_FF(fp->fs, FR_DISK_ERR);
            fp->flag &= ~FA__DIRTY;
        }

        /* Update the directory entry */
        res = move_window(fp->fs, fp->dir_sect);
        if (res != FR_OK)
            goto fail;

        dir = fp->dir_ptr;
        dir[DIR_Attr] |= AM_ARC;                    /* Set archive bit */
        ST_DWORD(dir+DIR_FileSize, fp->fsize);      /* Update file size */
        st_clust(dir, fp->sclust);                  /* Update start cluster */
        tm = get_fattime();                         /* Update updated time */
        ST_DWORD(dir+DIR_WrtTime, tm);
        ST_WORD(dir+DIR_LstAccDate, 0);
        fp->flag &= ~FA__WRITTEN;
        fp->fs->wflag = 1;
        res = sync_fs(fp->fs);
    }

fail:
    LEAVE_FF(fp->fs, res);
}

/**
 * Close File.
 * @param fp Pointer to the file object to be closed.
 */
FRESULT f_close(FF_FIL * fp)
{
    FATFS * fs;
    FRESULT res;

    res = validate(fp);
    if (res != FR_OK)
        return res;
    fs = fp->fs;

    if (!fs->readonly) {
        res = f_sync(fp); /* Flush cached data */
        if (res != FR_OK)
            return res;
    }


#if _FS_LOCK
    res = dec_lock(fp->lockid); /* Decrement file open counter */
    if (res == FR_OK)
#endif
        fp->fs = NULL;          /* Invalidate file object */

    unlock_fs(fs, FR_OK);       /* Unlock volume */

    return res;
}

/**
 * Seek File R/W Pointer.
 * @param fp Pointer to the file object.
 * @param ofs File pointer from top of file.
 */
FRESULT f_lseek(FF_FIL * fp, DWORD ofs)
{
    FRESULT res;

    res = validate(fp);                 /* Check validity of the object */
    if (res != FR_OK)
        LEAVE_FF(fp->fs, res);
    if (fp->err)                        /* Check error */
        LEAVE_FF(fp->fs, (FRESULT)fp->err);

#if _USE_FASTSEEK
    if (fp->cltbl) {    /* Fast seek */
        DWORD cl, pcl, ncl, tcl, dsc, tlen, ulen, *tbl;

        if (ofs == CREATE_LINKMAP) {    /* Create CLMT */
            tbl = fp->cltbl;
            tlen = *tbl++; ulen = 2;    /* Given table size and required table size */
            cl = fp->sclust;            /* Top of the chain */
            if (cl) {
                do {
                    /* Get a fragment */
                    tcl = cl; ncl = 0; ulen += 2;   /* Top, length and used items */
                    do {
                        pcl = cl; ncl++;
                        cl = get_fat(fp->fs, cl);
                        if (cl <= 1) ABORT(fp->fs, FR_INT_ERR);
                        if (cl == 0xFFFFFFFF) ABORT(fp->fs, FR_DISK_ERR);
                    } while (cl == pcl + 1);
                    if (ulen <= tlen) {     /* Store the length and top of the fragment */
                        *tbl++ = ncl; *tbl++ = tcl;
                    }
                } while (cl < fp->fs->n_fatent);    /* Repeat until end of chain */
            }
            *fp->cltbl = ulen;  /* Number of items used */
            if (ulen <= tlen)
                *tbl = 0;       /* Terminate table */
            else
                res = FR_NOT_ENOUGH_CORE;   /* Given table size is smaller than required */

        } else {                        /* Fast seek */
            if (ofs > fp->fsize)        /* Clip offset at the file size */
                ofs = fp->fsize;
            fp->fptr = ofs;             /* Set file pointer */
            if (ofs) {
                fp->clust = clmt_clust(fp, ofs - 1);
                dsc = clust2sect(fp->fs, fp->clust);
                if (!dsc) ABORT(fp->fs, FR_INT_ERR);
                dsc += (ofs - 1) / SS(fp->fs) & (fp->fs->csize - 1);
                if (fp->fptr % SS(fp->fs) && dsc != fp->dsect) {    /* Refill sector cache if needed */
                    if (!fp->fs->readonly && fp->flag & FA__DIRTY) {
                        /* Write-back dirty sector cache */
                        if (fatfs_disk_write(fp->fs->drv, fp->buf, fp->dsect, SS(fp->fs)))
                            ABORT(fp->fs, FR_DISK_ERR);
                        fp->flag &= ~FA__DIRTY;
                    }

                    if (fatfs_disk_read(fp->fs->drv, fp->buf, dsc, SS(fp->fs)))    /* Load current sector */
                        ABORT(fp->fs, FR_DISK_ERR);
                    fp->dsect = dsc;
                }
            }
        }
    } else
#endif

    /* Normal Seek */
    {
        DWORD clst, bcs, nsect, ifptr;

        if (fp->fs->readonly) {
            if (ofs > fp->fsize) {
                /* In read-only mode, clip offset with the file size */
                ofs = fp->fsize;
            }
        } else if (ofs > fp->fsize && !(fp->flag & FA_WRITE)) {
            ofs = fp->fsize;
        }

        ifptr = fp->fptr;
        fp->fptr = nsect = 0;
        if (ofs) {
            bcs = (DWORD)fp->fs->csize * SS(fp->fs);    /* Cluster size (byte) */
            if (ifptr > 0 &&
                (ofs - 1) / bcs >= (ifptr - 1) / bcs) { /* When seek to same or following cluster, */
                fp->fptr = (ifptr - 1) & ~(bcs - 1);    /* start from the current cluster */
                ofs -= fp->fptr;
                clst = fp->clust;
            } else {                                    /* When seek to back cluster, */
                clst = fp->sclust;                      /* start from the first cluster */
                if (!fp->fs->readonly && clst == 0) {
                    /* If no cluster chain, create a new chain */
                    clst = create_chain(fp->fs, 0);
                    if (clst == 1) ABORT(fp->fs, FR_INT_ERR);
                    if (clst == 0xFFFFFFFF) ABORT(fp->fs, FR_DISK_ERR);
                    fp->sclust = clst;
                }
                fp->clust = clst;
            }
            if (clst != 0) {
                while (ofs > bcs) {                     /* Cluster following loop */
                    /* Check if in write mode or not */
                    if (!fp->fs->readonly && fp->flag & FA_WRITE) {
                        clst = create_chain(fp->fs, clst);  /* Force stretch if in write mode */
                        if (clst == 0) {                /* When disk gets full, clip file size */
                            ofs = bcs; break;
                        }
                    } else {
                        clst = get_fat(fp->fs, clst);   /* Follow cluster chain if not in write mode */
                    }
                    if (clst == 0xFFFFFFFF) ABORT(fp->fs, FR_DISK_ERR);
                    if (clst <= 1 || clst >= fp->fs->n_fatent) ABORT(fp->fs, FR_INT_ERR);
                    fp->clust = clst;
                    fp->fptr += bcs;
                    ofs -= bcs;
                }
                fp->fptr += ofs;
                if (ofs % SS(fp->fs)) {
                    nsect = clust2sect(fp->fs, clst);   /* Current sector */
                    if (!nsect) ABORT(fp->fs, FR_INT_ERR);
                    nsect += ofs / SS(fp->fs);
                }
            }
        }
        if (fp->fptr % SS(fp->fs) && nsect != fp->dsect) {  /* Fill sector cache if needed */
            if (!fp->fs->readonly && fp->flag & FA__DIRTY) {
                /* Write-back dirty sector cache */
                if (fatfs_disk_write(fp->fs->drv, fp->buf, fp->dsect, SS(fp->fs)))
                    ABORT(fp->fs, FR_DISK_ERR);
                fp->flag &= ~FA__DIRTY;
            }

            if (fatfs_disk_read(fp->fs->drv, fp->buf, nsect, SS(fp->fs)))  /* Fill sector cache */
                ABORT(fp->fs, FR_DISK_ERR);
            fp->dsect = nsect;
        }

        if (!fp->fs->readonly && fp->fptr > fp->fsize) {
            /* Set file change flag if the file size is extended */
            fp->fsize = fp->fptr;
            fp->flag |= FA__WRITTEN;
        }
    }

    LEAVE_FF(fp->fs, res);
}

/**
 * Create a Directory Object.
 * @param dp Pointer to directory object to create.
 * @param path Pointer to the directory path.
 */
FRESULT f_opendir(FF_DIR * dp, FATFS * fs, const TCHAR* path)
{
    FRESULT res;
    DEF_NAMEBUF;

    res = find_volume(fs, get_ldnumber(&path), 0);
    if (res != FR_OK)
        goto fail;

    dp->fs = fs;
    INIT_BUF(*dp);
    res = follow_path(dp, path); /* Follow the path to the directory */
    FREE_BUF();
    if (res != FR_OK)
        goto fail;

    dp->ino = GET_INO(dp);

    /* Follow completed */
    if (dp->dir) { /* It is not the origin directory itself */
        /*
         * The following check is unfortunately ambiguous because AM_DIR
         * is actually meant to mark a subdirectory but eg. the root is not
         * a subdirectory.
         */
#if 0
        if ((dp->dir[DIR_Attr] & AM_DIR) == 0) /* The object is a file */
            res = FR_NO_PATH;
        else
#endif
        dp->sclust = ld_clust(fs, dp->dir);
    }
    if (res != FR_OK)
        goto fail;

    dp->id = fs->id;
    res = dir_sdi(dp, 0); /* Rewind directory */

#if _FS_LOCK
    if (res != FR_OK)
        goto fail;

    if (dp->sclust) {
        dp->lockid = inc_lock(dp, 0);   /* Lock the sub directory */
        if (!dp->lockid)
            res = FR_TOO_MANY_OPEN_FILES;
    } else {
        dp->lockid = 0; /* Root directory need not to be locked */
    }
#endif

    if (res == FR_NO_FILE)
        res = FR_NO_PATH;
fail:
    if (res != FR_OK)
        dp->fs = NULL; /* Invalidate the directory object if function faild */

    LEAVE_FF(fs, res);
}

/**
 * Close Directory.
 * @param dp Pointer to the directory object to be closed.
 */
FRESULT f_closedir(FF_DIR * dp)
{
    FATFS * fs;
    FRESULT res;

    res = validate(dp);
    if (res != FR_OK)
        return res;

    fs = dp->fs;
#if _FS_LOCK
    if (dp->lockid)             /* Decrement sub-directory open counter */
        res = dec_lock(dp->lockid);
    if (res == FR_OK)
#endif
        dp->fs = NULL;          /* Invalidate directory object */

    unlock_fs(fs, FR_OK);       /* Unlock volume */

    return res;
}

/**
 * Read Directory Entries in Sequence.
 * @param dp Pointer to the open directory object.
 * @param fno Pointer to file information to return.
 */
FRESULT f_readdir(FF_DIR * dp, FILINFO * fno)
{
    FRESULT res;
    DEF_NAMEBUF;

    res = validate(dp);                 /* Check validity of the object */
    if (res != FR_OK)
        goto fail;

    if (!fno) {
        res = dir_sdi(dp, 0);           /* Rewind the directory object */
    } else {
        INIT_BUF(*dp);
        res = dir_read(dp, 0);          /* Read an item */
        if (res == FR_NO_FILE) {        /* Reached end of directory */
            dp->sect = 0;
            res = FR_OK;
        }
        if (res == FR_OK) {             /* A valid entry is found */
            get_fileinfo(dp, fno);      /* Get the object information */
            res = dir_next(dp, 0);      /* Increment index for next */
            if (res == FR_NO_FILE) {
                dp->sect = 0;
                res = FR_OK;
            }
        }
        FREE_BUF();
    }

fail:
    LEAVE_FF(dp->fs, res);
}

/**
 * Get File Status.
 * @param path Pointer to the file path.
 * @param fno Pointer to file information to return.
 */
FRESULT f_stat(FATFS * fs, const TCHAR * path, FILINFO * fno)
{
    FRESULT res;
    FF_DIR dj;
    DEF_NAMEBUF;

    dj.fs = fs;

    /* Get logical drive number */
    res = find_volume(dj.fs, get_ldnumber(&path), 0);
    if (res != FR_OK)
        goto fail;

    INIT_BUF(dj);
    res = follow_path(&dj, path);   /* Follow the file path */
    if (res == FR_OK) {             /* Follow completed */
        if (dj.dir) {       /* Found an object */
            if (fno)
                get_fileinfo(&dj, fno);
        } else {            /* It is root directory */
            res = FR_INVALID_NAME;
        }
    }
    FREE_BUF();

fail:
    LEAVE_FF(dj.fs, res);
}



/**
 * Get Number of Free Clusters.
 * @param path Path name of the logical drive number.
 * @param nclst Pointer to a variable to return number of free clusters.
 */
FRESULT f_getfree(FATFS * fs, const TCHAR * path, DWORD * nclst)
{
    FRESULT res;
    DWORD n, clst, sect, stat;
    unsigned int i;
    uint8_t fat;
    uint8_t * p;

    /* Get logical drive number */
    res = find_volume(fs, get_ldnumber(&path), 0);

    if (res == FR_OK) {
        /* If free_clust is valid, return it without full cluster scan */
        if (fs->free_clust <= fs->n_fatent - 2) {
            *nclst = fs->free_clust;
        } else {
            /* Get number of free clusters */
            fat = fs->fs_type;
            n = 0;
            if (fat == FS_FAT12) {
                clst = 2;
                do {
                    stat = get_fat(fs, clst);
                    if (stat == 0xFFFFFFFF) { res = FR_DISK_ERR; break; }
                    if (stat == 1) { res = FR_INT_ERR; break; }
                    if (stat == 0) n++;
                } while (++clst < fs->n_fatent);
            } else {
                clst = fs->n_fatent;
                sect = fs->fatbase;
                i = 0; p = 0;
                do {
                    if (!i) {
                        res = move_window(fs, sect++);
                        if (res != FR_OK) break;
                        p = fs->win;
                        i = SS(fs);
                    }
                    if (fat == FS_FAT16) {
                        if (LD_WORD(p) == 0) n++;
                        p += 2; i -= 2;
                    } else {
                        if ((LD_DWORD(p) & 0x0FFFFFFF) == 0) n++;
                        p += 4; i -= 4;
                    }
                } while (--clst);
            }
            fs->free_clust = n;
            fs->fsi_flag |= 1;
            *nclst = n;
        }
    }
    LEAVE_FF(fs, res);
}

/**
 * Truncate File.
 * @param fp Pointer to the file object.
 */
FRESULT f_truncate(FF_FIL * fp)
{
    FRESULT res;
    DWORD ncl;


    res = validate(fp);                     /* Check validity of the object */
    if (res == FR_OK) {
        if (fp->err) {                      /* Check error */
            res = (FRESULT)fp->err;
        } else {
            if (!(fp->flag & FA_WRITE))     /* Check access mode */
                res = FR_DENIED;
        }
    }
    if (res == FR_OK) {
        if (fp->fsize > fp->fptr) {
            fp->fsize = fp->fptr;   /* Set file size to current R/W point */
            fp->flag |= FA__WRITTEN;
            if (fp->fptr == 0) {    /* When set file size to zero, remove entire cluster chain */
                res = remove_chain(fp->fs, fp->sclust);
                fp->sclust = 0;
            } else {                /* When truncate a part of the file, remove remaining clusters */
                ncl = get_fat(fp->fs, fp->clust);
                res = FR_OK;
                if (ncl == 0xFFFFFFFF) res = FR_DISK_ERR;
                if (ncl == 1) res = FR_INT_ERR;
                if (res == FR_OK && ncl < fp->fs->n_fatent) {
                    res = put_fat(fp->fs, fp->clust, 0x0FFFFFFF);
                    if (res == FR_OK) res = remove_chain(fp->fs, ncl);
                }
            }

            if (res == FR_OK && (fp->flag & FA__DIRTY)) {
                if (fatfs_disk_write(fp->fs->drv, fp->buf, fp->dsect, SS(fp->fs)))
                    res = FR_DISK_ERR;
                else
                    fp->flag &= ~FA__DIRTY;
            }
        }
        if (res != FR_OK) fp->err = (FRESULT)res;
    }

    LEAVE_FF(fp->fs, res);
}

/**
 * Delete a File or Directory.
 * @param path Pointer to the file or directory path.
 */
FRESULT f_unlink(FATFS * fs, const TCHAR * path)
{
    FRESULT res;
    FF_DIR dj, sdj;
    uint8_t *dir;
    DWORD dclst;
    DEF_NAMEBUF;

    dj.fs = fs;

    res = find_volume(dj.fs, get_ldnumber(&path), 1);
    if (res == FR_OK) {
        INIT_BUF(dj);
        res = follow_path(&dj, path);       /* Follow the file path */
#if _FS_LOCK
        if (res == FR_OK) res = chk_lock(&dj, 2);   /* Cannot remove open file */
#endif
        if (res == FR_OK) {                 /* The object is accessible */
            dir = dj.dir;
            if (!dir) {
                res = FR_INVALID_NAME;      /* Cannot remove the start directory */
            } else {
                if (dir[DIR_Attr] & AM_RDO)
                    res = FR_DENIED;        /* Cannot remove R/O object */
            }
            dclst = ld_clust(dj.fs, dir);
            if (res == FR_OK && (dir[DIR_Attr] & AM_DIR)) { /* Is it a sub-dir? */
                if (dclst < 2) {
                    res = FR_INT_ERR;
                } else {
                    memcpy(&sdj, &dj, sizeof (DIR));   /* Check if the sub-directory is empty or not */
                    sdj.sclust = dclst;
                    res = dir_sdi(&sdj, 2);     /* Exclude dot entries */
                    if (res == FR_OK) {
                        res = dir_read(&sdj, 0);    /* Read an item */
                        if (res == FR_OK) /* Not empty directory */
                            res = FR_DENIED;
                        if (res == FR_NO_FILE) res = FR_OK; /* Empty */
                    }
                }
            }
            if (res == FR_OK) {
                res = dir_remove(&dj);      /* Remove the directory entry */
                if (res == FR_OK) {
                    if (dclst)              /* Remove the cluster chain if exist */
                        res = remove_chain(dj.fs, dclst);
                    if (res == FR_OK) res = sync_fs(dj.fs);
                }
            }
        }
        FREE_BUF();
    }

    LEAVE_FF(dj.fs, res);
}

/**
 * Create a Directory.
 * @param path Pointer to the directory path.
 */
FRESULT f_mkdir(FATFS * fs, const TCHAR * path)
{
    FRESULT res;
    FF_DIR dj;
    uint8_t * dir, n;
    DWORD dsc, dcl, pcl, tm = get_fattime();
    DEF_NAMEBUF;

    dj.fs = fs;

    res = find_volume(dj.fs, get_ldnumber(&path), 1);
    if (res == FR_OK) {
        INIT_BUF(dj);
        res = follow_path(&dj, path);           /* Follow the file path */
        if (res == FR_OK) {
            /* Any object with same name is already existing */
            res = FR_EXIST;
        }
        if (res == FR_NO_FILE) {                /* Can create a new directory */
            dcl = create_chain(dj.fs, 0);       /* Allocate a cluster for the new directory table */
            res = FR_OK;
            if (dcl == 0)
                res = FR_DENIED; /* No space to allocate a new cluster */
            if (dcl == 1)
                res = FR_INT_ERR;
            if (dcl == 0xFFFFFFFF)
                res = FR_DISK_ERR;
            if (res == FR_OK)                   /* Flush FAT */
                res = sync_window(dj.fs);
            if (res == FR_OK) {                 /* Initialize the new directory table */
                dsc = clust2sect(dj.fs, dcl);
                dir = dj.fs->win;
                memset(dir, 0, SS(dj.fs));
                memset(dir + DIR_Name, ' ', 11); /* Create "." entry */
                dir[DIR_Name] = '.';
                dir[DIR_Attr] = AM_DIR;
                ST_DWORD(dir + DIR_WrtTime, tm);
                st_clust(dir, dcl);
                memcpy(dir + SZ_DIR, dir, SZ_DIR);   /* Create ".." entry */
                dir[SZ_DIR + 1] = '.'; pcl = dj.sclust;
                if (dj.fs->fs_type == FS_FAT32 && pcl == dj.fs->dirbase)
                    pcl = 0;
                st_clust(dir + SZ_DIR, pcl);
                for (n = dj.fs->csize; n; n--) {    /* Write dot entries and clear following sectors */
                    dj.fs->winsect = dsc++;
                    dj.fs->wflag = 1;
                    res = sync_window(dj.fs);
                    if (res != FR_OK)
                        break;
                    memset(dir, 0, SS(dj.fs));
                }
            }
            if (res == FR_OK) {
                /* Register the object to the directory. */
                res = dir_register(&dj);
            }
            if (res != FR_OK) {
                /* Could not register, remove cluster chain */
                remove_chain(dj.fs, dcl);
            } else {
                dir = dj.dir;
                dir[DIR_Attr] = AM_DIR;             /* Attribute */
                ST_DWORD(dir+DIR_WrtTime, tm);      /* Created time */
                st_clust(dir, dcl);                 /* Table start cluster */
                dj.fs->wflag = 1;
                res = sync_fs(dj.fs);
            }
        }
        FREE_BUF();
    }

    LEAVE_FF(dj.fs, res);
}

/**
 * Change Attribute.
 * @param path Pointer to the file path.
 * @param value Attribute bits.
 * @param mask  Attribute mask to change.
 */
FRESULT f_chmod(FATFS * fs, const TCHAR * path, uint8_t value, uint8_t mask)
{
    FRESULT res;
    FF_DIR dj;
    uint8_t * dir;
    DEF_NAMEBUF;

    dj.fs = fs;

    res = find_volume(dj.fs, get_ldnumber(&path), 1);
    if (res == FR_OK) {
        INIT_BUF(dj);
        res = follow_path(&dj, path);       /* Follow the file path */
        FREE_BUF();
        if (res == FR_OK) {
            dir = dj.dir;
            if (!dir) {                     /* Is it a root directory? */
                res = FR_INVALID_NAME;
            } else {                        /* File or sub directory */
                mask &= AM_RDO|AM_HID|AM_SYS|AM_ARC;    /* Valid attribute mask */
                dir[DIR_Attr] = (value & mask) | (dir[DIR_Attr] & (uint8_t)~mask); /* Apply attribute change */
                dj.fs->wflag = 1;
                res = sync_fs(dj.fs);
            }
        }
    }

    LEAVE_FF(dj.fs, res);
}

/**
 * Change Timestamp.
 * @param path Pointer to the file/directory name.
 * @param fno Pointer to the time stamp to be set.
 */
FRESULT f_utime(FATFS * fs, const TCHAR * path, const FILINFO * fno)
{
    FRESULT res;
    FF_DIR dj;
    uint8_t * dir;
    DEF_NAMEBUF;

    dj.fs = fs;

    res = find_volume(dj.fs, get_ldnumber(&path), 1);
    if (res == FR_OK) {
        INIT_BUF(dj);
        res = follow_path(&dj, path);   /* Follow the file path */
        FREE_BUF();
        if (res == FR_OK) {
            dir = dj.dir;
            if (!dir) {                 /* Root directory */
                res = FR_INVALID_NAME;
            } else {                    /* File or sub-directory */
                ST_WORD(dir+DIR_WrtTime, fno->ftime);
                ST_WORD(dir+DIR_WrtDate, fno->fdate);
                dj.fs->wflag = 1;
                res = sync_fs(dj.fs);
            }
        }
    }

    LEAVE_FF(dj.fs, res);
}

/**
 * Rename File/Directory.
 * @param path_old Pointer to the object to be renamed.
 * @param path_new Pointer to the new name.
 */
FRESULT f_rename(FATFS * fs, const TCHAR * path_old, const TCHAR * path_new)
{
    FRESULT res;
    FF_DIR djo, djn;
    uint8_t buf[21], *dir;
    DWORD dw;
    DEF_NAMEBUF;

    djo.fs = fs;

    res = find_volume(djo.fs, get_ldnumber(&path_old), 1);
    if (res == FR_OK) {
        djn.fs = djo.fs;
        INIT_BUF(djo);
        res = follow_path(&djo, path_old);      /* Check old object */
#if _FS_LOCK
        if (res == FR_OK) res = chk_lock(&djo, 2);
#endif
        if (res == FR_OK) {                     /* Old object is found */
            if (!djo.dir) {                     /* Is root dir? */
                res = FR_NO_FILE;
            } else {
                memcpy(buf, djo.dir+DIR_Attr, 21);     /* Save the object information except name */
                memcpy(&djn, &djo, sizeof (DIR));      /* Duplicate the directory object */
                if (get_ldnumber(&path_new) >= 0)       /* Snip drive number off and ignore it */
                    res = follow_path(&djn, path_new);  /* and check if new object is exist */
                else
                    res = FR_INVALID_DRIVE;
                if (res == FR_OK) res = FR_EXIST;       /* The new object name is already existing */
                if (res == FR_NO_FILE) {                /* Is it a valid path and no name collision? */
/* Start critical section that any interruption can cause a cross-link */
                    res = dir_register(&djn);           /* Register the new entry */
                    if (res == FR_OK) {
                        dir = djn.dir;                  /* Copy object information except name */
                        memcpy(dir+13, buf+2, 19);
                        dir[DIR_Attr] = buf[0] | AM_ARC;
                        djo.fs->wflag = 1;
                        if (djo.sclust != djn.sclust && (dir[DIR_Attr] & AM_DIR)) {     /* Update .. entry in the directory if needed */
                            dw = clust2sect(djo.fs, ld_clust(djo.fs, dir));
                            if (!dw) {
                                res = FR_INT_ERR;
                            } else {
                                res = move_window(djo.fs, dw);
                                dir = djo.fs->win+SZ_DIR;   /* .. entry */
                                if (res == FR_OK && dir[1] == '.') {
                                    dw = (djo.fs->fs_type == FS_FAT32 && djn.sclust == djo.fs->dirbase) ? 0 : djn.sclust;
                                    st_clust(dir, dw);
                                    djo.fs->wflag = 1;
                                }
                            }
                        }
                        if (res == FR_OK) {
                            res = dir_remove(&djo);     /* Remove old entry */
                            if (res == FR_OK)
                                res = sync_fs(djo.fs);
                        }
                    }
/* End critical section */
                }
            }
        }
        FREE_BUF();
    }

    LEAVE_FF(djo.fs, res);
}

/**
 * Get volume label.
 * @param path Path name of the logical drive number.
 * @param label Pointer to a buffer to return the volume label.
 * @param vsn Pointer to a variable to return the volume serial number.
 */
FRESULT f_getlabel(FATFS * fs, const TCHAR * path, TCHAR * label, DWORD * vsn)
{
    FRESULT res;
    FF_DIR dj;
    unsigned int i, j;

    dj.fs = fs;

    res = find_volume(dj.fs, get_ldnumber(&path), 0);

    /* Get volume label */
    if (res == FR_OK && label) {
        dj.sclust = 0;                  /* Open root directory */
        res = dir_sdi(&dj, 0);
        if (res == FR_OK) {
            res = dir_read(&dj, 1);     /* Get an entry with AM_VOL */
            if (res == FR_OK) {         /* A volume label is exist */
#if configFATFS_LFN && _LFN_UNICODE
                WCHAR w;
                i = j = 0;
                do {
                    w = (i < 11) ? dj.dir[i++] : ' ';
                    if (IsDBCS1(w) && i < 11 && IsDBCS2(dj.dir[i]))
                        w = w << 8 | dj.dir[i++];
                    label[j++] = ff_convert(w, 1);  /* OEM -> Unicode */
                } while (j < 11);
#else
                memcpy(label, dj.dir, 11);
#endif
                j = 11;
                do {
                    label[j] = 0;
                    if (!j) break;
                } while (label[--j] == ' ');
            }
            if (res == FR_NO_FILE) {    /* No label, return nul string */
                label[0] = 0;
                res = FR_OK;
            }
        }
    }

    /* Get volume serial number */
    if (res == FR_OK && vsn) {
        res = move_window(dj.fs, dj.fs->volbase);
        if (res == FR_OK) {
            i = dj.fs->fs_type == FS_FAT32 ? BS_VolID32 : BS_VolID;
            *vsn = LD_DWORD(&dj.fs->win[i]);
        }
    }

    LEAVE_FF(dj.fs, res);
}

/**
 * Set volume label.
 * @param label Pointer to the volume label to set.
 */
FRESULT f_setlabel(FATFS * fs, const TCHAR * label)
{
    FRESULT res;
    FF_DIR dj;
    uint8_t vn[11];
    unsigned int i, j, sl;
    WCHAR w;
    DWORD tm;

    dj.fs = fs;

    res = find_volume(dj.fs, get_ldnumber(&label), 1);
    if (res) LEAVE_FF(dj.fs, res);

    /* Create a volume label in directory form */
    vn[0] = 0;
    for (sl = 0; label[sl]; sl++) ;             /* Get name length */
    for ( ; sl && label[sl-1] == ' '; sl--) ;   /* Remove trailing spaces */
    if (sl) {   /* Create volume label in directory form */
        i = j = 0;
        do {
#if configFATFS_LFN && _LFN_UNICODE
            w = ff_convert(ff_wtoupper(label[i++]), 0);
#else
            w = (uint8_t)label[i++];
            if (IsDBCS1(w))
                w = (j < 10 && i < sl && IsDBCS2(label[i])) ? w << 8 | (uint8_t)label[i++] : 0;
#if configFATFS_LFN
            w = ff_convert(ff_wtoupper(ff_convert(w, 1)), 0);
#else
            if (ka_islower(w))
                w -= 0x20; /* To upper ASCII characters */
#ifdef _EXCVT
            if (w >= 0x80) {
                /* To upper extended characters (SBCS cfg) */
                w = ExCvt[w - 0x80];
            }
#else
            if (!_DF1S && w >= 0x80)
                w = 0; /* Reject extended characters (ASCII cfg) */
#endif
#endif
#endif
            if (!w || kstrchr("\"*+,.:;<=>\?[]|\x7F", w) ||
                j >= (unsigned int)((w >= 0x100) ? 10 : 11)) {
                /* Reject invalid characters for volume label */
                LEAVE_FF(dj.fs, FR_INVALID_NAME);
            }
            if (w >= 0x100) vn[j++] = (uint8_t)(w >> 8);
            vn[j++] = (uint8_t)w;
        } while (i < sl);
        while (j < 11) vn[j++] = ' ';
    }

    /* Set volume label */
    dj.sclust = 0;                  /* Open root directory */
    res = dir_sdi(&dj, 0);
    if (res == FR_OK) {
        res = dir_read(&dj, 1);     /* Get an entry with AM_VOL */
        if (res == FR_OK) {         /* A volume label is found */
            if (vn[0]) {
                memcpy(dj.dir, vn, 11);    /* Change the volume label name */
                tm = get_fattime();
                ST_DWORD(dj.dir+DIR_WrtTime, tm);
            } else {
                dj.dir[0] = DDE;            /* Remove the volume label */
            }
            dj.fs->wflag = 1;
            res = sync_fs(dj.fs);
        } else {                    /* No volume label is found or error */
            if (res == FR_NO_FILE) {
                res = FR_OK;
                if (vn[0]) {                /* Create volume label as new */
                    res = dir_alloc(&dj, 1);    /* Allocate an entry for volume label */
                    if (res == FR_OK) {
                        memset(dj.dir, 0, SZ_DIR); /* Set volume label */
                        memcpy(dj.dir, vn, 11);
                        dj.dir[DIR_Attr] = AM_VOL;
                        tm = get_fattime();
                        ST_DWORD(dj.dir+DIR_WrtTime, tm);
                        dj.fs->wflag = 1;
                        res = sync_fs(dj.fs);
                    }
                }
            }
        }
    }

    LEAVE_FF(dj.fs, res);
}
