#include <coreinit/filesystem_fsa.h>

#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

#define DEV_SD_REF		0
#define DEV_USB01_REF	1
#define DEV_USB02_REF	2

extern bool fatMounted[FF_VOLUMES];
extern const char* VolumeStr[FF_VOLUMES];
extern LBA_t g_sectorOffset[FF_VOLUMES];

typedef BYTE	DSTATUS;

typedef enum {
	RES_OK = 0,
	RES_ERROR,
	RES_WRPRT,
	RES_NOTRDY,
	RES_PARERR
} DRESULT;

FSError wiiu_readSectors(BYTE pdrv, LBA_t sectorIdx, UINT sectorCount, BYTE* outputBuff);
FSError wiiu_writeSectors(BYTE pdrv, LBA_t sectorIdx, UINT sectorCount, const BYTE* inputBuff);


DSTATUS disk_initialize (BYTE pdrv);
DSTATUS disk_shutdown (BYTE pdrv);
DSTATUS disk_status (BYTE pdrv);
DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count);
DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count);
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff);

#define STA_NOINIT		0x01
#define STA_NODISK		0x02
#define STA_PROTECT		0x04

#define CTRL_SYNC			0
#define GET_SECTOR_COUNT	1
#define GET_SECTOR_SIZE		2
#define GET_BLOCK_SIZE		3
#define CTRL_TRIM			4
#define SET_CACHE_COUNT		69
#define CTRL_FORCE_SYNC		70

#define CTRL_POWER			5
#define CTRL_LOCK			6
#define CTRL_EJECT			7
#define CTRL_FORMAT			8

#define MMC_GET_TYPE		10
#define MMC_GET_CSD			11
#define MMC_GET_CID			12
#define MMC_GET_OCR			13
#define MMC_GET_SDSTAT		14
#define ISDIO_READ			55
#define ISDIO_WRITE			56
#define ISDIO_MRITE			57

#define ATA_GET_REV			20
#define ATA_GET_MODEL		21
#define ATA_GET_SN			22

#ifdef __cplusplus
}
#endif

#endif
