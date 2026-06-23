#include "ff.h"
#include "diskio.h"
#include "ffcache.h"
#include <coreinit/filesystem.h>
#include <coreinit/debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <mocha/mocha.h>
#include <mocha/fsa.h>
#include <coreinit/filesystem_fsa.h>
#include <whb/log.h>

#if USE_RAMDISK == 0

const char* fatDevPaths[FF_VOLUMES] = {"/dev/sdcard01", "/dev/usb01", "/dev/usb02"};
bool fatMounted[FF_VOLUMES] = {false, false, false};
const WORD fatSectorSizes[FF_VOLUMES] = {512, 512, 512};
WORD fatCacheSizes[FF_VOLUMES] = {32*8*4*4, 32*8*4*8, 32*8*4};
LBA_t g_sectorOffset[FF_VOLUMES] = {0, 0, 0};

static FSAClientHandle g_fatClient = -1;

static FSAClientHandle getOrCreateClient() {
    if (g_fatClient >= 0)
        return g_fatClient;
    g_fatClient = FSAAddClient(NULL);
    if (g_fatClient < 0)
        return -1;
    MochaUtilsStatus unlockRes = Mocha_UnlockFSClientEx(g_fatClient);
    if (unlockRes != MOCHA_RESULT_SUCCESS) {
        FSADelClient(g_fatClient);
        g_fatClient = -1;
    }
    return g_fatClient;
}

DSTATUS wiiu_mountDrive(BYTE pdrv) {
    // Used shared g_fatClient (created lazily by getOrCreateClient) instead of
    // a temp client which avoids consuming extra Mocha unlock whitelist slots.
    FSAClientHandle client = getOrCreateClient();
    if (client < 0)
        return STA_NOINIT;

    int32_t testHandle;
    FSError res = FSAEx_RawOpenEx(client, fatDevPaths[pdrv], &testHandle);
    WHBLogPrintf("[diskio] FSAEx_RawOpenEx(%d, %s) = %d, handle=%d", pdrv, fatDevPaths[pdrv], res, testHandle);
    if (res < 0) {
        return STA_NODISK;
    }
    FSAEx_RawCloseEx(client, testHandle);

    WHBLogPrintf("[diskio] Mounted drive %d (%s)", pdrv, fatDevPaths[pdrv]);
    OSReport("[diskio] Mounted drive %d (%s)\n", pdrv, fatDevPaths[pdrv]);
    fatMounted[pdrv] = true;
    return 0;
}

DSTATUS wiiu_unmountDrive(BYTE pdrv) {
    fatMounted[pdrv] = false;
    g_sectorOffset[pdrv] = 0;
    return 0;
}

// Raw sector I/O with per-drive sector offset for non-standard partition layouts.
FSError wiiu_readSectors(BYTE pdrv, LBA_t sectorIdx, UINT sectorCount, BYTE* outputBuff) {
    FSAClientHandle client = getOrCreateClient();
    if (client < 0) return FS_ERROR_MEDIA_NOT_READY;
    int32_t handle;
    FSError res = FSAEx_RawOpenEx(client, fatDevPaths[pdrv], &handle);
    if (res < 0) return FS_ERROR_MEDIA_NOT_READY;
    LBA_t physicalSector = sectorIdx + g_sectorOffset[pdrv];
    res = FSAEx_RawReadEx(client, outputBuff, fatSectorSizes[pdrv], sectorCount, physicalSector, handle);
    FSAEx_RawCloseEx(client, handle);
    if (res != FS_ERROR_OK) {
        OSReport("[wiiu_readSectors] FAILED pdrv=%d sector=%llu(+%llu) count=%u ret=%d\n", pdrv, (unsigned long long)sectorIdx, (unsigned long long)g_sectorOffset[pdrv], sectorCount, res);
    }
    return res;
}

FSError wiiu_writeSectors(BYTE pdrv, LBA_t sectorIdx, UINT sectorCount, const BYTE* inputBuff) {
    FSAClientHandle client = getOrCreateClient();
    if (client < 0) return FS_ERROR_MEDIA_NOT_READY;
    int32_t handle;
    FSError res = FSAEx_RawOpenEx(client, fatDevPaths[pdrv], &handle);
    if (res < 0) return FS_ERROR_MEDIA_NOT_READY;
    LBA_t physicalSector = sectorIdx + g_sectorOffset[pdrv];
    res = FSAEx_RawWriteEx(client, inputBuff, fatSectorSizes[pdrv], sectorCount, physicalSector, handle);
    FSAEx_RawCloseEx(client, handle);
    return res;
}

DSTATUS disk_status (
	BYTE pdrv
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES)
        return STA_NOINIT;
    if (!fatMounted[pdrv]) {
        return STA_NOINIT;
    }
    return 0;
}

DSTATUS disk_initialize (
	BYTE pdrv
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
    if (fatMounted[pdrv]) return 0;  /* Already initialized */
    // todo: Support drives with non-512 sector sizes
    if (ffcache_initialize(pdrv, fatSectorSizes[pdrv], fatCacheSizes[pdrv]) != RES_OK) {
        OSReport("[diskio] ffcache_initialize failed for drive %d\n", pdrv);
        return STA_NOINIT;
    }
    return wiiu_mountDrive(pdrv);
}

DSTATUS disk_shutdown (
        BYTE pdrv
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return STA_NOINIT;
    ffcache_shutdown(pdrv);
    if (!fatMounted[pdrv]) return STA_NOINIT;
    return wiiu_unmountDrive(pdrv);
}

DRESULT disk_read (
	BYTE pdrv,
	BYTE *buff,
	LBA_t sector,
	UINT count
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return RES_PARERR;
    if (!fatMounted[pdrv]) return RES_NOTRDY;

    DRESULT res = ffcache_readSectors(pdrv, sector, count, buff);
    if (res != RES_OK) {
        OSReport("[disk_read] FAILED pdrv=%d sector=%llu count=%u res=%d\n", pdrv, (unsigned long long)sector, count, res);
    }
    return res;
}

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,
	const BYTE *buff,
	LBA_t sector,
	UINT count
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return RES_PARERR;
    if (!fatMounted[pdrv]) return RES_NOTRDY;
    return ffcache_writeSectors(pdrv, sector, count, buff);
}

#endif

DRESULT disk_ioctl (
	BYTE pdrv,
	BYTE cmd,
	void *buff
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return RES_ERROR;
    if (!fatMounted[pdrv]) return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC: {
            DEBUG_OSReport("[disk_ioctl] Requested a sync, flushing cached sectors!");
            return RES_OK;
        }
        case CTRL_FORCE_SYNC: {
            DEBUG_OSReport("[disk_ioctl] Requested a forced sync, flushing cached sectors!");
            return RES_OK;
        }
        case SET_CACHE_COUNT: {
            DEBUG_OSReport("[disk_ioctl] Requested changing the cache size to %d", *((WORD*)buff));
            fatCacheSizes[pdrv] = *((WORD*)buff);
            return RES_OK;
        }
        case GET_SECTOR_COUNT: {
            DEBUG_OSReport("[disk_ioctl] Requested sector count!");
            return RES_OK;
        }
        case GET_SECTOR_SIZE: {
            DEBUG_OSReport("[disk_ioctl] Requested sector size which is currently %d!", fatSectorSizes[pdrv]);
            *(WORD*)buff = (WORD)fatSectorSizes[pdrv];
            return RES_OK;
        }
        case GET_BLOCK_SIZE: {
            DEBUG_OSReport("[disk_ioctl] Requested block size which is unknown!");
            *(WORD*)buff = 1;
            return RES_OK;
        }
        case CTRL_TRIM: {
            DEBUG_OSReport("[disk_ioctl] Requested trim!");
            return RES_OK;
        }
        default:
            return RES_PARERR;
    }

	return RES_PARERR;
}

DWORD get_fattime() {
    OSCalendarTime output;
    OSTicksToCalendarTime(OSGetTime(), &output);
    return (DWORD) (output.tm_year - 1980) << 25 |
           (DWORD) (output.tm_mon + 1) << 21 |
           (DWORD) output.tm_mday << 16 |
           (DWORD) output.tm_hour << 11 |
           (DWORD) output.tm_min << 5 |
           (DWORD) output.tm_sec >> 1;
}

#else

bool fatMounted[FF_VOLUMES] = {false};
LBA_t g_sectorOffset[FF_VOLUMES] = {0};

#define SPLIT_TOTAL_SIZE 10737418240
#define SPLIT_TOTAL_SIZE_SECTORS (SPLIT_TOTAL_SIZE/512)
#define SPLIT_IMAGE_COUNT 5
#define SPLIT_IMAGE_SIZE_SECTORS (SPLIT_TOTAL_SIZE/SPLIT_IMAGE_COUNT/512)

FSFileHandle fatHandles[FF_VOLUMES][SPLIT_IMAGE_COUNT];
const WORD fatSectorSizes[FF_VOLUMES] = {512};
WORD fatCacheSizes[FF_VOLUMES] = {32*8*4};

FSClient* client;
FSCmdBlock fsCmd;

char sMountPath[0x80];

DSTATUS wiiu_mountDrive(BYTE pdrv) {
    client = aligned_alloc(0x20, sizeof(FSClient));
    FSAddClient(client, 0);
    FSInitCmdBlock(&fsCmd);

    FSMountSource mountSource;
    FSGetMountSource(client, &fsCmd, FS_MOUNT_SOURCE_SD, &mountSource, -1);

    FSMount(client, &fsCmd, &mountSource, sMountPath, sizeof(sMountPath), FS_ERROR_FLAG_ALL);

    // Check raw disk image
    for (uint8_t i=0; i<SPLIT_IMAGE_COUNT; i++) {
        char name[255];
        snprintf(name, 254, "%s/split%u.img", sMountPath, i);
        FSStatus result = FSOpenFile(client, &fsCmd, name, "r+", &fatHandles[pdrv][i], FS_ERROR_FLAG_ALL);
        OSReport("Opened disk image %s with size of with result %d!\n", name, result);
        if (result != FS_STATUS_OK) {
            FSDelClient(client, 0);
            return STA_NOINIT;
        }
    }
    fatMounted[pdrv] = true;
    return 0;
}

DSTATUS wiiu_unmountDrive(BYTE pdrv) {
    for (uint8_t i=0; i<SPLIT_IMAGE_COUNT; i++) {
        FSCloseFile(client, &fsCmd, fatHandles[pdrv][i], FS_ERROR_FLAG_ALL);
    }
    FSDelClient(client, 0);
    free(client);

    fatMounted[pdrv] = false;
    return 0;
}

FSError wiiu_readSectors(BYTE pdrv, LBA_t sectorIdx, UINT sectorCount, BYTE* outputBuff) {
    void* tempBuff = aligned_alloc(0x40, 1*fatSectorSizes[pdrv]);
    for (uint32_t i=0; i<sectorCount; i++) {
        LBA_t currSectorIdx = sectorIdx + i;
        uint32_t fileIdx = 0;
        if (currSectorIdx != 0) {
            fileIdx = currSectorIdx / SPLIT_IMAGE_SIZE_SECTORS;
            currSectorIdx = currSectorIdx % SPLIT_IMAGE_SIZE_SECTORS;
        }

        FSStatus status = FSReadFileWithPos(client, &fsCmd, tempBuff, 1*fatSectorSizes[pdrv], 1, currSectorIdx*fatSectorSizes[pdrv], fatHandles[pdrv][fileIdx], 0, FS_ERROR_FLAG_ALL);
        memcpy(outputBuff+(i*fatSectorSizes[pdrv]), tempBuff, 1*fatSectorSizes[pdrv]);
    }
    free(tempBuff);
    return FS_ERROR_OK;
}

FSError wiiu_writeSectors(BYTE pdrv, LBA_t sectorIdx, UINT sectorCount, const BYTE* inputBuff) {
    void* tempBuff = aligned_alloc(0x40, 1*fatSectorSizes[pdrv]);
    for (uint32_t i=0; i<sectorCount; i++) {
        LBA_t currSectorIdx = sectorIdx + i;
        uint32_t fileIdx = 0;
        if (currSectorIdx != 0) {
            fileIdx = currSectorIdx / SPLIT_IMAGE_SIZE_SECTORS;
            currSectorIdx = currSectorIdx % SPLIT_IMAGE_SIZE_SECTORS;
        }

        memcpy(tempBuff, ((void*)inputBuff)+(i*fatSectorSizes[pdrv]), 1*fatSectorSizes[pdrv]);
        FSStatus status = FSWriteFileWithPos(client, &fsCmd, tempBuff, 1*fatSectorSizes[pdrv], 1, currSectorIdx*fatSectorSizes[pdrv], fatHandles[pdrv][fileIdx], 0, FS_ERROR_FLAG_ALL);
    }
    free(tempBuff);
    return FS_ERROR_OK;
}

DSTATUS disk_status (
        BYTE pdrv
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES)
        return STA_NOINIT;
    if (!fatMounted[pdrv]) {
        return STA_NOINIT;
    }
    return 0;
}

DSTATUS disk_initialize (
        BYTE pdrv
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES)
        return STA_NOINIT;
    if (fatMounted[pdrv])
        return STA_NOINIT;
    // todo: Get sector size instead of hard-coding it
    ffcache_initialize(pdrv, fatSectorSizes[pdrv], fatCacheSizes[pdrv]);
    return wiiu_mountDrive(pdrv);
}

DSTATUS disk_shutdown (
        BYTE pdrv
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES)
        return STA_NOINIT;
    ffcache_shutdown(pdrv);
    if (!fatMounted[pdrv])
        return STA_NOINIT;
    return wiiu_unmountDrive(pdrv);
}

DRESULT disk_read (
        BYTE pdrv,
        BYTE *buff,
        LBA_t sector,
        UINT count
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return RES_PARERR;
    if (!fatMounted[pdrv]) return RES_NOTRDY;

    return wiiu_readSectors(pdrv, sector, count, buff) == FS_ERROR_OK ? RES_OK : RES_ERROR;
}

#if FF_FS_READONLY == 0

DRESULT disk_write (
        BYTE pdrv,
        const BYTE *buff,
        LBA_t sector,
        UINT count
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return RES_PARERR;
    if (!fatMounted[pdrv]) return RES_NOTRDY;

    return wiiu_writeSectors(pdrv, sector, count, buff) == FS_ERROR_OK ? RES_OK : RES_ERROR;
}

#endif

DRESULT disk_ioctl (
        BYTE pdrv,
        BYTE cmd,
        void *buff
)
{
    if (pdrv < 0 || pdrv >= FF_VOLUMES) return RES_ERROR;
    if (!fatMounted[pdrv]) return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC: {
            DEBUG_OSReport("[disk_ioctl] Requested a sync, flushing currently cached sectors!");
            for (uint8_t i=0; i<SPLIT_IMAGE_COUNT; i++) {
                FSFlushFile(client, &fsCmd, fatHandles[pdrv][i], FS_ERROR_FLAG_ALL);
            }
            return RES_OK;
        }
        case CTRL_FORCE_SYNC: {
            DEBUG_OSReport("[disk_ioctl] Requested a forced sync, flushing currently cached sectors!");
            for (uint8_t i=0; i<SPLIT_IMAGE_COUNT; i++) {
                FSFlushFile(client, &fsCmd, fatHandles[pdrv][i], FS_ERROR_FLAG_ALL);
            }
            return RES_OK;
        }
        case SET_CACHE_COUNT: {
            DEBUG_OSReport("[disk_ioctl] Requested changing the cache size to %d", *((WORD*)buff));
            fatCacheSizes[pdrv] = *((WORD*)buff);
            return RES_OK;
        }
        case GET_SECTOR_COUNT: {
            DEBUG_OSReport("[disk_ioctl] Requested sector count!");
            *(LBA_t*)buff = SPLIT_TOTAL_SIZE_SECTORS;
            return RES_OK;
        }
        case GET_SECTOR_SIZE: {
            DEBUG_OSReport("[disk_ioctl] Requested sector size which is currently %d!", fatSectorSizes[pdrv]);
            *(WORD*)buff = (WORD)fatSectorSizes[pdrv];
            return RES_OK;
        }
        case GET_BLOCK_SIZE: {
            DEBUG_OSReport("[disk_ioctl] Requested block size which is unknown!");
            *(WORD*)buff = 1;
            return RES_OK;
        }
        case CTRL_TRIM: {
            DEBUG_OSReport("[disk_ioctl] Requested trim!");
        }
        default:
            return RES_PARERR;
    }

    return RES_PARERR;
}

DWORD get_fattime() {
    OSCalendarTime output;
    OSTicksToCalendarTime(OSGetTime(), &output);
    return (DWORD) (output.tm_year - 1980) << 25 |
           (DWORD) (output.tm_mon + 1) << 21 |
           (DWORD) output.tm_mday << 16 |
           (DWORD) output.tm_hour << 11 |
           (DWORD) output.tm_min << 5 |
           (DWORD) output.tm_sec >> 1;
}

#endif