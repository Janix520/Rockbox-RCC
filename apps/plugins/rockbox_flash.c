/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* Plugin for reprogramming only the second image in Flash ROM.
* !!! DON'T MESS WITH THIS CODE UNLESS YOU'RE ABSOLUTELY SHURE WHAT YOU DO !!!
*
* Copyright (C) 2003 J�rg Hohensohn aka [IDC]Dragon
*
* All files in this archive are subject to the GNU General Public License.
* See the file COPYING in the source tree root for full license agreement.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
****************************************************************************/
#include "plugin.h"

/* Only build for target */
#ifndef SIMULATOR
#ifdef HAVE_LCD_BITMAP

#ifndef UINT8
#define UINT8 unsigned char
#endif

#ifndef UINT16
#define UINT16 unsigned short
#endif

#ifndef UINT32
#define UINT32 unsigned long
#endif

/* hard-coded values */
static volatile UINT8* FB = (UINT8*)0x02000000; /* Flash base address */
#define SECTORSIZE 4096 /* size of one flash sector */

#define ROCKBOX_DEST 0x09000000
#define ROCKBOX_EXEC 0x09000200
#define DEFAULT_FILENAME "/rockbox.ucl"
#define VERS_ADR 0xFE /* position of firmware version value in Flash */
#define UCL_HEADER 26 /* size of the header generated by uclpack */

typedef struct 
{
    UINT32 destination; /* address to copy it to */
    UINT32 size;        /* how many bytes of payload (to the next header) */
    UINT32 execute;     /* entry point */
    UINT32 flags;       /* uncompressed or compressed */
    /* end of header, now comes the payload */
} tImageHeader;

/* result of the CheckFirmwareFile() function */
typedef enum
{
    eOK = 0,
    eFileNotFound, /* errors from here on */
    eTooBig,
    eTooSmall,
    eReadErr,
    eNotUCL,
    eWrongAlgorithm,
    eMultiBlocks,
} tCheckResult;

typedef struct 
{
    UINT8 manufacturer;
    UINT8 id;
    int size;
    char name[32];
} tFlashInfo;

static struct plugin_api* rb; /* here is a global api struct pointer */

static UINT8* sector; /* better not place this on the stack... */

/***************** Flash Functions *****************/


/* read the manufacturer and device ID */
bool ReadID(volatile UINT8* pBase, UINT8* pManufacturerID, UINT8* pDeviceID)
{
    UINT8 not_manu, not_id; /* read values before switching to ID mode */
    UINT8 manu, id; /* read values when in ID mode */

    pBase = (UINT8*)((UINT32)pBase & 0xFFF80000); /* round down to 512k align,
                                                     to make sure */

    not_manu = pBase[0]; /* read the normal content */
    not_id   = pBase[1]; /* should be 'A' (0x41) and 'R' (0x52) from the
                            "ARCH" marker */

    pBase[0x5555] = 0xAA; /* enter command mode */
    pBase[0x2AAA] = 0x55;
    pBase[0x5555] = 0x90; /* ID command */
    rb->sleep(HZ/50);     /* Atmel wants 20ms pause here */

    manu = pBase[0];
    id   = pBase[1];
    
    pBase[0] = 0xF0;  /* reset flash (back to normal read mode) */
    rb->sleep(HZ/50); /* Atmel wants 20ms pause here */

    /* I assume success if the obtained values are different from
       the normal flash content. This is not perfectly bulletproof, they 
       could theoretically be the same by chance, causing us to fail. */
    if (not_manu != manu || not_id != id) /* a value has changed */
    {
        *pManufacturerID = manu; /* return the results */
        *pDeviceID = id;
        return true; /* success */
    }
    return false; /* fail */
}

/* erase the sector which contains the given address */
bool EraseSector(volatile UINT8* pAddr)
{
    volatile UINT8* pBase =
        (UINT8*)((UINT32)pAddr & 0xFFF80000); /* round down to 512k align */
    unsigned timeout = 43000; /* the timeout loop should be no less than
                                 25ms */

    pBase[0x5555] = 0xAA; /* enter command mode */
    pBase[0x2AAA] = 0x55;
    pBase[0x5555] = 0x80; /* erase command */
    pBase[0x5555] = 0xAA; /* enter command mode */
    pBase[0x2AAA] = 0x55;
    *pAddr = 0x30;        /* erase the sector */

    /* I counted 7 instructions for this loop -> min. 0.58 us per round
       Plus memory waitstates it will be much more, gives margin */
    while (*pAddr != 0xFF && --timeout); /* poll for erased */

    return (timeout != 0);
}

/* address must be in an erased location */
inline bool ProgramByte(volatile UINT8* pAddr, UINT8 data)
{
    unsigned timeout = 35; /* the timeout loop should be no less than 20us */

    if (~*pAddr & data) /* just a safety feature, not really necessary */
        return false; /* can't set any bit from 0 to 1 */

    FB[0x5555] = 0xAA; /* enter command mode */
    FB[0x2AAA] = 0x55;
    FB[0x5555] = 0xA0; /* byte program command */

    *pAddr = data;

    /* I counted 7 instructions for this loop -> min. 0.58 us per round
       Plus memory waitstates it will be much more, gives margin */
    while (*pAddr != data && --timeout); /* poll for programmed */

    return (timeout != 0);
}

/* this returns true if supported and fills the info struct */
bool GetFlashInfo(tFlashInfo* pInfo)
{
    rb->memset(pInfo, 0, sizeof(tFlashInfo));

    if (!ReadID(FB, &pInfo->manufacturer, &pInfo->id))
        return false;

    if (pInfo->manufacturer == 0xBF) /* SST */
    {
        if (pInfo->id == 0xD6)
        {
            pInfo->size = 256* 1024; /* 256k */
            rb->strcpy(pInfo->name, "SST39VF020");
            return true;
        }
        else if (pInfo->id == 0xD7)
        {
            pInfo->size = 512* 1024; /* 512k */
            rb->strcpy(pInfo->name, "SST39VF040");
            return true;
        }
        else
            return false;
    }
    return false;
}


/*********** Tool Functions ************/

/* place a 32 bit value into memory, big endian */
void Write32(UINT8* pByte, UINT32 value)
{
    pByte[0] = (UINT8)(value >> 24);	
    pByte[1] = (UINT8)(value >> 16);	
    pByte[2] = (UINT8)(value >> 8);	
    pByte[3] = (UINT8)(value);	
}

/* read a 32 bit value from memory, big endian */
UINT32 Read32(UINT8* pByte)
{
    UINT32 value;

    value = (UINT32)pByte[0] << 24;
    value |= (UINT32)pByte[1] << 16;
    value |= (UINT32)pByte[2] << 8;
    value |= (UINT32)pByte[3];

    return value;
}

/* get the start address of the second image */
tImageHeader* GetSecondImage(void)
{
    tImageHeader* pImage1;
    UINT32 pos = 0;	/* default: not found */
    UINT32* pFlash = (UINT32*)FB;

    UINT16 version = *(UINT16*)(FB + VERS_ADR);
    if (version < 200) /* at least 2.00 */
    {
        return 0; /* not our flash layout */
    }

    /* determine the first image position */
    pos = pFlash[2] + pFlash[3]; /* position + size of the bootloader = after
                                    it */
    pos = (pos + 3) & ~3; /* be sure it's 32 bit aligned */
    pImage1 = (tImageHeader*)pos;

    if (pImage1->destination != ROCKBOX_DEST ||
        pImage1->execute != ROCKBOX_EXEC)
        return 0; /* seems to be no Rockbox stuff in here */

    if (pImage1->size != 0)
    {
        /* success, we have a second image */
        pos = (UINT32)pImage1 + sizeof(tImageHeader) + pImage1->size;
        if (((pos + SECTORSIZE-1) & ~(SECTORSIZE-1)) != pos)
        {	/* not sector-aligned */
            pos = 0; // sanity check failed
        }
    }

    return (tImageHeader*)pos;
}


/*********** Image File Functions ************/

/* so far, only compressed images in UCL NRV algorithm 2e supported */
tCheckResult CheckImageFile(char* filename, int space, tImageHeader* pHeader)
{
    int i;
    int fd;
    int filesize; /* size info */

    int fileread = 0; /* total size as read from the file */
    int read; /* how many for this sector */

    /* magic file header for compressed files */
    static const UINT8 magic[8] = { 0x00,0xe9,0x55,0x43,0x4c,0xff,0x01,0x1a };
    UINT8 ucl_header[UCL_HEADER];

    fd = rb->open(filename, O_RDONLY);
    if (fd < 0)
        return eFileNotFound;

    filesize = rb->filesize(fd);
    if (filesize - (int)sizeof(ucl_header) - 8 > space)
    {
        rb->close(fd);
        return eTooBig;
    }
    else if (filesize < 50000) /* give it some reasonable lower limit */
    {
        rb->close(fd);
        return eTooSmall;
    }

    /* do some sanity checks */

    read = rb->read(fd, ucl_header, sizeof(ucl_header));
    fileread += read;
    if (read != sizeof(ucl_header))
    {
        rb->close(fd);
        return eReadErr;
    }

    /* compare the magic header */
    for (i=0; i<8; i++)
    {
        if (ucl_header[i] != magic[i])
        {
            rb->close(fd);
            return eNotUCL;
        }
    }

    /* check for supported algorithm */
    if (ucl_header[12] != 0x2E)
    {
        rb->close(fd);
        return eWrongAlgorithm;
    }

    pHeader->size = Read32(ucl_header + 22); /* compressed size */
    if (pHeader->size != filesize - sizeof(ucl_header) - 8)
    {
        rb->close(fd);
        return eMultiBlocks;
    }

    if (Read32(ucl_header + 18) > pHeader->size) /* compare with uncompressed
                                                    size */
    {   /* normal case */
        pHeader->flags = 0x00000001; /* flags for UCL compressed */
    }
    else
    {
        pHeader->flags = 0x00000000; /* very unlikely, content was not
                                        compressible */
    }
    
    /* check if we can read the whole file */
    do
    {
        read = rb->read(fd, sector, SECTORSIZE);
        fileread += read;
    } while (read == SECTORSIZE);
    
    rb->close(fd);

    if (fileread != filesize)
        return eReadErr;
    
    /* fill in the hardcoded rest of the header */
    pHeader->destination = ROCKBOX_DEST;
    pHeader->execute = ROCKBOX_EXEC;
    
    return eOK;
}


/* returns the # of failures, 0 on success */
unsigned ProgramImageFile(char* filename, UINT8* pos,
                          tImageHeader* pImageHeader, int start, int size)
{
    int i;
    int fd;
    int read; /* how many for this sector */
    unsigned failures = 0;

    fd = rb->open(filename, O_RDONLY);
    if (fd < 0)
        return false;

    /* no error checking necessary here, we checked for minimum size
       already */
    rb->lseek(fd, start, SEEK_SET); /* go to start position */

    *(tImageHeader*)sector = *pImageHeader; /* copy header into sector
                                               buffer */
    read = rb->read(fd, sector + sizeof(tImageHeader),
                    SECTORSIZE - sizeof(tImageHeader)); /* payload behind */
    size -= read;
    read += sizeof(tImageHeader); /* to be programmed, but not part of the
                                     file */

    do {
        if (!EraseSector(pos))
        {
            /* nothing we can do, let the programming count the errors */
        }
        
        for (i=0; i<read; i++)
        {
            if (!ProgramByte(pos + i, sector[i]))
            {
                failures++;
            }
        }

        pos += SECTORSIZE;
        read = rb->read(fd, sector, (size > SECTORSIZE) ? SECTORSIZE : size);
        /* payload for next sector */
        size -= read;
    } while (read > 0);
    
    rb->close(fd);

    return failures;
}

/* returns the # of failures, 0 on success */
unsigned VerifyImageFile(char* filename, UINT8* pos,
                         tImageHeader* pImageHeader, int start, int size)
{
    int i;
    int fd;
    int read; /* how many for this sector */
    unsigned failures = 0;

    fd = rb->open(filename, O_RDONLY);
    if (fd < 0)
        return false;

    /* no error checking necessary here, we checked for minimum size
       already */
    rb->lseek(fd, start, SEEK_SET); /* go to start position */

    *(tImageHeader*)sector = *pImageHeader; /* copy header into sector
                                               buffer */
    read = rb->read(fd, sector + sizeof(tImageHeader),
                    SECTORSIZE - sizeof(tImageHeader)); /* payload behind */

    size -= read;
    read += sizeof(tImageHeader); /* to be programmed, but not part of the
                                     file */

    do
    {
        for (i=0; i<read; i++)
        {
            if (pos[i] != sector[i])
            {
                failures++;
            }
        }
        
        pos += SECTORSIZE;
        read = rb->read(fd, sector, (size > SECTORSIZE) ? SECTORSIZE : size);
        /* payload for next sector */
        size -= read;
    } while (read);
    
    rb->close(fd);
    
    return failures;
}


/***************** User Interface Functions *****************/
/*                 (to be changed for Player)               */

/* helper for DoUserDialog() */
void ShowFlashInfo(tFlashInfo* pInfo, tImageHeader* pImageHeader)
{
    char buf[32];

    if (!pInfo->manufacturer)
    {
        rb->lcd_puts(0, 0, "Flash: M=?? D=??");
    }
    else
    {
        if (pInfo->size)
        {
            rb->snprintf(buf, sizeof(buf), "Flash size: %d KB",
                         pInfo->size / 1024);
            rb->lcd_puts(0, 0, buf);
        }
        else
        {
            rb->lcd_puts(0, 0, "Unsupported chip");
        }
        
    }

    if (pImageHeader)
    {
        rb->snprintf(buf, sizeof(buf), "Image at %d KB",
                     ((UINT8*)pImageHeader - FB) / 1024);
        rb->lcd_puts(0, 1, buf);
    }
    else
    {
            rb->lcd_puts(0, 1, "No image found!");
    }
    
    rb->lcd_update();
}


/* Kind of our main function, defines the application flow. */
void DoUserDialog(char* filename)
{
    tImageHeader ImageHeader;
    tFlashInfo FlashInfo;
    char buf[32];
    int button;
    int rc; /* generic return code */
    UINT32 space, aligned_size, true_size;
    UINT8* pos;
    int memleft;
    
    rb->lcd_setfont(FONT_SYSFIXED);

    /* "allocate" memory */
    sector = rb->plugin_get_buffer(&memleft);
    if (memleft < SECTORSIZE) /* need buffer for a flash sector */
    {
        rb->splash(HZ*3, 0, true, "Out of memory");
        return; /* exit */
    }

    pos = (void*)GetSecondImage();
    rc = GetFlashInfo(&FlashInfo);

    ShowFlashInfo(&FlashInfo, (void*)pos);
    if (FlashInfo.size == 0) /* no valid chip */
    {
        rb->splash(HZ*3, 0, true, "Not flashable");
        return; /* exit */
    }
    else if (pos == 0)
    {
        rb->splash(HZ*3, 0, true, "No Image");
        return; /* exit */
    }
    
    rb->lcd_puts(0, 3, "using file:");
    rb->lcd_puts_scroll(0, 4, filename); 
    rb->lcd_puts(0, 6, "[F1] to check file");
    rb->lcd_puts(0, 7, "other key to exit");
    rb->lcd_update();
    
    button = rb->button_get(true);
    button = rb->button_get(true);
    
    if (button != BUTTON_F1)
    {
        return;
    }

    rb->lcd_clear_display();
    rb->lcd_puts(0, 0, "checking...");
    rb->lcd_update();

    space = FlashInfo.size - (pos-FB + sizeof(ImageHeader));
    /* size minus start */
    
    rc = CheckImageFile(filename, space, &ImageHeader);
    rb->lcd_puts(0, 0, "checked:");
    switch (rc) {
        case eOK:
            rb->lcd_puts(0, 1, "File OK.");
            break;
    case eNotUCL:
            rb->lcd_puts(0, 1, "File not UCL ");
            rb->lcd_puts(0, 2, "compressed.");
            rb->lcd_puts(0, 3, "Use uclpack --2e");
            rb->lcd_puts(0, 4, " --10 rockbox.bin");
            break;
    case eWrongAlgorithm:
            rb->lcd_puts(0, 1, "Wrong algorithm");
            rb->lcd_puts(0, 2, "for compression.");
            rb->lcd_puts(0, 3, "Use uclpack --2e");
            rb->lcd_puts(0, 4, " --10 rockbox.bin");
            break;
    case eFileNotFound:
            rb->lcd_puts(0, 1, "File not found:");
            rb->lcd_puts_scroll(0, 2, filename);
            break;
    case eTooBig:
            rb->lcd_puts(0, 1, "File too big,");
            rb->lcd_puts(0, 2, "won't fit in chip.");
            break;
    case eTooSmall:
            rb->lcd_puts(0, 1, "File too small.");
            rb->lcd_puts(0, 2, "Incomplete?");
            break;
    case eReadErr:
            rb->lcd_puts(0, 1, "File read error.");
            break;
    case eMultiBlocks:
            rb->lcd_puts(0, 1, "File invalid.");
            rb->lcd_puts(0, 2, "Blocksize");
            rb->lcd_puts(0, 3, " too small?");
            break;
    default:
            rb->lcd_puts(0, 1, "Check failed.");
            break;
    }

    if (rc == eOK)
    {	/* was OK */
        rb->lcd_puts(0, 6, "[F2] to program");
        rb->lcd_puts(0, 7, "other key to exit");
    }
    else
    { /* error occured */
        rb->lcd_puts(0, 6, "Any key to exit");
    }

    rb->lcd_update();
    
    button = rb->button_get(true);
    button = rb->button_get(true);
    
    if (rc != eOK || button != BUTTON_F2)
    {
        return;
    }
    
    true_size = ImageHeader.size;
    aligned_size = ((sizeof(tImageHeader) + true_size + SECTORSIZE-1) &
                    ~(SECTORSIZE-1)) - sizeof(tImageHeader); /* round up to
                                                                next flash
                                                                sector */
    ImageHeader.size = aligned_size; /* increase image size such that we reach
                                        the next sector */
    
    rb->lcd_clear_display();
    rb->lcd_puts(0, 0, "Programming...");
    rb->lcd_update();

    rc = ProgramImageFile(filename, pos, &ImageHeader, UCL_HEADER, true_size);
    if (rc)
    {   /* errors */
        rb->lcd_clear_display();
        rb->lcd_puts(0, 0, "Error:");
        rb->lcd_puts(0, 1, "Programming fail!");
        rb->snprintf(buf, sizeof(buf), "%d errors", rc);
        rb->lcd_puts(0, 2, buf);
        rb->lcd_update();
        button = rb->button_get(true);
        button = rb->button_get(true);
    }
    
    rb->lcd_clear_display();
    rb->lcd_puts(0, 0, "Verifying...");
    rb->lcd_update();

    rc = VerifyImageFile(filename, pos, &ImageHeader, UCL_HEADER, true_size);

    rb->lcd_clear_display();
    if (rc == 0)
    {
        rb->lcd_puts(0, 0, "Verify OK.");
    }
    else
    {
        rb->lcd_puts(0, 0, "Error:");
        rb->lcd_puts(0, 1, "Verify fail!");
        rb->snprintf(buf, sizeof(buf), "%d errors", rc);
        rb->lcd_puts(0, 2, buf);
        rb->lcd_puts(0, 3, "Use safe image");
        rb->lcd_puts(0, 4, "if booting hangs:");
        rb->lcd_puts(0, 5, "F1 during power-on");
    }
    rb->lcd_puts(0, 7, "Any key to exit");
    rb->lcd_update();
    
    button = rb->button_get(true);
    button = rb->button_get(true);
}

/***************** Plugin Entry Point *****************/

enum plugin_status plugin_start(struct plugin_api* api, void* parameter)
{
    char* filename;

    /* this macro should be called as the first thing you do in the plugin.
       it test that the api version and model the plugin was compiled for
       matches the machine it is running on */
    TEST_PLUGIN_API(api);

    if (parameter == NULL)
        filename = DEFAULT_FILENAME;
    else
        filename = (char*) parameter; 
    
    rb = api; /* copy to global api pointer */

    /* now go ahead and have fun! */
    DoUserDialog(filename);

    return PLUGIN_OK;
}

#endif // #ifdef HAVE_LCD_BITMAP

#endif // #ifndef SIMULATOR
