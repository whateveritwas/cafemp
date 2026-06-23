#ifndef PLATFORM_WIIU_LEGACY
#include "utils/usb.hpp"

#include "logger/logger.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fatfs/diskio.h>
#include <fatfs/ff.h>
#include <fcntl.h>
#include <limits.h>
#include <mocha/mocha.h>
#include <sys/errno.h>
#include <sys/iosupport.h>
#include <unistd.h>

#define USB_DEVICE_NAME "usb"
#define MAX_FAT_DRIVES 3
#define FAT_DRIVE_FIRST 1
#define FAT_DRIVE_LAST 2
#define FAT_SECTOR_OFFSET 2048

static bool mocha_initialized = false;
static bool drive_mounted[MAX_FAT_DRIVES] = {false, false, false};
static FATFS *fatfs_objs[MAX_FAT_DRIVES] = {nullptr, nullptr, nullptr};
static int active_drive = -1;

typedef struct {
    devoptab_t tab;
    char name[32];
    int drive_num;
    bool registered;
} FatDevEntry;
static FatDevEntry fat_devs[FF_VOLUMES];
static int fat_dev_count = 0;

static void build_fat_path(int drv, char *out, size_t sz, const char *path) {
    const char *c = strchr(path, ':');
    if (c) path = c + 1;
    if (path[0] == '/')
        snprintf(out, sz, "%d:%s", drv, path);
    else
        snprintf(out, sz, "%d:/%s", drv, path);
}

static BYTE posix_to_fat_flags(int flags) {
    BYTE mode = 0;
    int acc = flags & O_ACCMODE;
    if (acc == O_RDONLY)
        mode |= FA_READ;
    else if (acc == O_WRONLY)
        mode |= FA_WRITE;
    else
        mode |= FA_READ | FA_WRITE;
    if (flags & O_CREAT) {
        if (flags & O_EXCL)
            mode |= FA_CREATE_NEW;
        else if (flags & O_TRUNC)
            mode |= FA_CREATE_ALWAYS;
        else
            mode |= FA_OPEN_ALWAYS;
    } else {
        mode |= FA_OPEN_EXISTING;
    }
    return mode;
}

static int fatfs_to_errno(FRESULT fr) {
    switch (fr) {
        case FR_OK:
            return 0;
        case FR_DISK_ERR:
            return EIO;
        case FR_INT_ERR:
            return EIO;
        case FR_NOT_READY:
            return ENXIO;
        case FR_NO_FILE:
            return ENOENT;
        case FR_NO_PATH:
            return ENOENT;
        case FR_INVALID_NAME:
            return EINVAL;
        case FR_DENIED:
            return EACCES;
        case FR_EXIST:
            return EEXIST;
        case FR_INVALID_OBJECT:
            return EBADF;
        case FR_WRITE_PROTECTED:
            return EROFS;
        case FR_INVALID_DRIVE:
            return ENODEV;
        case FR_NOT_ENABLED:
            return ENODEV;
        case FR_NO_FILESYSTEM:
            return ENODEV;
        case FR_MKFS_ABORTED:
            return EIO;
        case FR_TIMEOUT:
            return ETIMEDOUT;
        case FR_LOCKED:
            return EBUSY;
        case FR_NOT_ENOUGH_CORE:
            return ENOMEM;
        case FR_TOO_MANY_OPEN_FILES:
            return ENFILE;
        case FR_INVALID_PARAMETER:
            return EINVAL;
        default:
            return EIO;
    }
}

static int fatfs_close_r(struct _reent *r, void *fd) {
    FRESULT fr = f_close((FIL *)fd);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    return 0;
}
static ssize_t fatfs_write_r(struct _reent *r, void *fd, const char *ptr, size_t len) {
    UINT bw;
    FRESULT fr = f_write((FIL *)fd, ptr, len, &bw);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    return (ssize_t)bw;
}
static ssize_t fatfs_read_r(struct _reent *r, void *fd, char *ptr, size_t len) {
    UINT br;
    FRESULT fr = f_read((FIL *)fd, ptr, len, &br);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    return (ssize_t)br;
}
static int fatfs_ftruncate_r(struct _reent *r, void *fd, off_t len) {
    FIL *fp = (FIL *)fd;
    FRESULT fr = f_lseek(fp, (FSIZE_t)len);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    fr = f_truncate(fp);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    return 0;
}
static int fatfs_fsync_r(struct _reent *r, void *fd) {
    FRESULT fr = f_sync((FIL *)fd);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    return 0;
}

static off_t fatfs_seek_r(struct _reent *r, void *fd, off_t pos, int dir) {
    FIL *fp = (FIL *)fd;
    FSIZE_t np;
    switch (dir) {
        case SEEK_SET:
            np = (FSIZE_t)pos;
            break;
        case SEEK_CUR:
            np = f_tell(fp) + (FSIZE_t)pos;
            break;
        case SEEK_END:
            np = f_size(fp) + (FSIZE_t)pos;
            break;
        default:
            r->_errno = EINVAL;
            return -1;
    }
    FRESULT fr = f_lseek(fp, np);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    return (off_t)f_tell(fp);
}

static int fatfs_fstat_r(struct _reent *r, void *fd, struct stat *st) {
    (void)r;
    FIL *fp = (FIL *)fd;
    memset(st, 0, sizeof(struct stat));
    st->st_mode = S_IRUSR | S_IRGRP | S_IROTH;
    st->st_size = (off_t)f_size(fp);
    if (fp->flag & FA_WRITE) st->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
    return 0;
}

static int fatfs_dirreset_r(struct _reent *r, DIR_ITER *ds) {
    DIR *dp = (DIR *)ds->dirStruct;
    FRESULT fr = f_rewinddir(dp);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    return 0;
}

static int fatfs_dirnext_r(struct _reent *r, DIR_ITER *ds, char *filename, struct stat *st) {
    FILINFO fno;
    DIR *dp = (DIR *)ds->dirStruct;
    FRESULT fr = f_readdir(dp, &fno);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    if (fno.fname[0] == '\0') {
        r->_errno = 0;
        return -1;
    }
    strncpy(filename, fno.fname, NAME_MAX);
    if (st) {
        memset(st, 0, sizeof(struct stat));
        st->st_size = (off_t)fno.fsize;
        if (fno.fattrib & AM_DIR)
            st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        else {
            st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
            if (!(fno.fattrib & AM_RDO)) st->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
        }
    }
    return 0;
}

static int fatfs_dirclose_r(struct _reent *r, DIR_ITER *ds) {
    DIR *dp = (DIR *)ds->dirStruct;
    FRESULT fr = f_closedir(dp);
    if (fr != FR_OK) {
        r->_errno = fatfs_to_errno(fr);
        return -1;
    }
    return 0;
}

static long fatfs_fpathconf_r(struct _reent *r, void *fd, int name) {
    (void)fd;
    switch (name) {
        case _PC_LINK_MAX:
            return 1;
        case _PC_MAX_CANON:
            return 255;
        case _PC_MAX_INPUT:
            return 255;
        case _PC_NAME_MAX:
            return FF_MAX_LFN;
        case _PC_PATH_MAX:
            return 260;
        case _PC_PIPE_BUF:
            return 512;
        case _PC_CHOWN_RESTRICTED:
            return 1;
        case _PC_NO_TRUNC:
            return 1;
        case _PC_VDISABLE:
            return 0;
        default:
            r->_errno = EINVAL;
            return -1;
    }
}

static int fatfs_stub_link_r(struct _reent *r, const char *e, const char *n) {
    (void)e;
    (void)n;
    r->_errno = ENOTSUP;
    return -1;
}
static int fatfs_stub_chdir_r(struct _reent *r, const char *p) {
    (void)r;
    (void)p;
    return 0;
}
static int fatfs_stub_chmod_r(struct _reent *r, const char *p, mode_t m) {
    (void)r;
    (void)p;
    (void)m;
    return 0;
}
static int fatfs_stub_fchmod_r(struct _reent *r, void *fd, mode_t m) {
    (void)r;
    (void)fd;
    (void)m;
    return 0;
}
static int fatfs_stub_utimes_r(struct _reent *r, const char *f, const struct timeval t[2]) {
    (void)r;
    (void)f;
    (void)t;
    return 0;
}
static long fatfs_stub_pathconf_r(struct _reent *r, const char *p, int name) {
    (void)p;
    return fatfs_fpathconf_r(r, nullptr, name);
}
static int fatfs_stub_symlink_r(struct _reent *r, const char *t, const char *l) {
    (void)t;
    (void)l;
    r->_errno = ENOTSUP;
    return -1;
}
static ssize_t fatfs_stub_readlink_r(struct _reent *r, const char *p, char *b, size_t s) {
    (void)p;
    (void)b;
    (void)s;
    r->_errno = ENOTSUP;
    return -1;
}

#define DRIVE_FUNCS(D) \
[[maybe_unused]] static int      fatfs_open_r_##D    (struct _reent *r, void *fs, const char *path, int flags, int m) { (void)m; char fp[256]; build_fat_path(D, fp, sizeof(fp), path); FRESULT fr = f_open((FIL *)fs, fp, posix_to_fat_flags(flags)); if (fr != FR_OK) { r->_errno = fatfs_to_errno(fr); return -1; } if (flags & O_APPEND) f_lseek((FIL *)fs, f_size((FIL *)fs)); return 0; } \
[[maybe_unused]] static int      fatfs_stat_r_##D    (struct _reent *r, const char *file, struct stat *st) { char fp[256]; build_fat_path(D, fp, sizeof(fp), file); FILINFO fno; FRESULT fr = f_stat(fp, &fno); if (fr != FR_OK) { r->_errno = fatfs_to_errno(fr); return -1; } memset(st, 0, sizeof(struct stat)); st->st_size = (off_t)fno.fsize; if (fno.fattrib & AM_DIR) st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO; else { st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH; if (!(fno.fattrib & AM_RDO)) st->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH; } return 0; } \
[[maybe_unused]] static int      fatfs_unlink_r_##D  (struct _reent *r, const char *name) { char fp[256]; build_fat_path(D, fp, sizeof(fp), name); FRESULT fr = f_unlink(fp); if (fr != FR_OK) { r->_errno = fatfs_to_errno(fr); return -1; } return 0; } \
[[maybe_unused]] static int      fatfs_rename_r_##D  (struct _reent *r, const char *oldN, const char *newN) { char fo[256], fn[256]; build_fat_path(D, fo, sizeof(fo), oldN); build_fat_path(D, fn, sizeof(fn), newN); FRESULT fr = f_rename(fo, fn); if (fr != FR_OK) { r->_errno = fatfs_to_errno(fr); return -1; } return 0; } \
[[maybe_unused]] static int      fatfs_mkdir_r_##D   (struct _reent *r, const char *path, int m) { (void)m; char fp[256]; build_fat_path(D, fp, sizeof(fp), path); FRESULT fr = f_mkdir(fp); if (fr != FR_OK) { r->_errno = fatfs_to_errno(fr); return -1; } return 0; } \
[[maybe_unused]] static DIR_ITER*fatfs_diropen_r_##D (struct _reent *r, DIR_ITER *ds, const char *path) { char fp[256]; build_fat_path(D, fp, sizeof(fp), path); DIR *dp = (DIR *)ds->dirStruct; memset(dp, 0, sizeof(DIR)); FRESULT fr = f_opendir(dp, fp); if (fr != FR_OK) { r->_errno = fatfs_to_errno(fr); return nullptr; } return ds; } \
[[maybe_unused]] static int      fatfs_rmdir_r_##D   (struct _reent *r, const char *name) { char fp[256]; build_fat_path(D, fp, sizeof(fp), name); FRESULT fr = f_unlink(fp); if (fr != FR_OK) { r->_errno = fatfs_to_errno(fr); return -1; } return 0; } \
[[maybe_unused]] static int      fatfs_lstat_r_##D   (struct _reent *r, const char *f, struct stat *st) { return fatfs_stat_r_##D(r, f, st); } \
[[maybe_unused]] static int      fatfs_statvfs_r_##D (struct _reent *r, const char *path, struct statvfs *buf) { (void)path; char dp[4]; snprintf(dp, sizeof(dp), "%d:", D); DWORD fc; FATFS *fsp = nullptr; FRESULT fr = f_getfree((const TCHAR *)dp, &fc, &fsp); if (fr != FR_OK) { r->_errno = fatfs_to_errno(fr); return -1; } memset(buf, 0, sizeof(struct statvfs)); buf->f_bsize = fsp->csize * 512; buf->f_frsize = fsp->csize * 512; buf->f_blocks = fsp->n_fatent - 2; buf->f_bfree = fc; buf->f_bavail = fc; buf->f_namemax = FF_MAX_LFN; return 0; }

DRIVE_FUNCS(0)
DRIVE_FUNCS(1)
DRIVE_FUNCS(2)

#define BUILD_DEVOPTAB(D) { \
    .name = nullptr, .structSize = sizeof(FIL), \
    .open_r = fatfs_open_r_##D, .close_r = fatfs_close_r, .write_r = fatfs_write_r, .read_r = fatfs_read_r, \
    .seek_r = fatfs_seek_r, .fstat_r = fatfs_fstat_r, .stat_r = fatfs_stat_r_##D, \
    .link_r = fatfs_stub_link_r, .unlink_r = fatfs_unlink_r_##D, .chdir_r = fatfs_stub_chdir_r, \
    .rename_r = fatfs_rename_r_##D, .mkdir_r = fatfs_mkdir_r_##D, .dirStateSize = sizeof(DIR), \
    .diropen_r = fatfs_diropen_r_##D, .dirreset_r = fatfs_dirreset_r, .dirnext_r = fatfs_dirnext_r, \
    .dirclose_r = fatfs_dirclose_r, .statvfs_r = fatfs_statvfs_r_##D, .ftruncate_r = fatfs_ftruncate_r, \
    .fsync_r = fatfs_fsync_r, .deviceData = nullptr, .chmod_r = fatfs_stub_chmod_r, \
    .fchmod_r = fatfs_stub_fchmod_r, .rmdir_r = fatfs_rmdir_r_##D, .lstat_r = fatfs_lstat_r_##D, \
    .utimes_r = fatfs_stub_utimes_r, .fpathconf_r = fatfs_fpathconf_r, .pathconf_r = fatfs_stub_pathconf_r, \
    .symlink_r = fatfs_stub_symlink_r, .readlink_r = fatfs_stub_readlink_r, \
}

static const devoptab_t devoptab_templates[MAX_FAT_DRIVES] = {BUILD_DEVOPTAB(0), BUILD_DEVOPTAB(1), BUILD_DEVOPTAB(2)};

static bool devoptab_register(const char *device_name, unsigned char drive_num) {
    if (drive_num >= MAX_FAT_DRIVES) return false;
    for (int i = 0; i < fat_dev_count; i++) {
        if (strcmp(fat_devs[i].name, device_name) == 0) return true;
        if (fat_devs[i].drive_num == (int)drive_num) return true;
    }
    if (fat_dev_count >= FF_VOLUMES) return false;
    FatDevEntry *dev = &fat_devs[fat_dev_count];
    dev->tab = devoptab_templates[drive_num];
    strncpy(dev->name, device_name, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';
    dev->tab.name = dev->name;
    dev->drive_num = (int)drive_num;
    dev->registered = false;
    int existing = FindDevice(device_name);
    if (existing >= 0) RemoveDevice(device_name);
    int result = AddDevice(&dev->tab);
    if (result < 0) return false;
    dev->registered = true;
    fat_dev_count++;
    return true;
}

static void devoptab_unregister_all() {
    for (int i = 0; i < fat_dev_count; i++) {
        if (FindDevice(fat_devs[i].name) >= 0) RemoveDevice(fat_devs[i].name);
        fat_devs[i].registered = false;
    }
    fat_dev_count = 0;
}

static bool mount_fat_drive(int drv, const char *dev_name) {
    if (drv < 0 || drv >= MAX_FAT_DRIVES) return false;
    if (drive_mounted[drv]) return true;
    log_message(LOG_DEBUG, "USB", "Mounting FAT32 drive %d as %s...", drv, dev_name);
    DSTATUS ds = disk_initialize((BYTE)drv);
    if (ds != 0) {
        log_message(LOG_ERROR, "USB", "disk_initialize failed for drive %d: %d", drv, ds);
        return false;
    }
    FATFS *fatfs = (FATFS *)aligned_alloc(0x40, sizeof(FATFS));
    if (!fatfs) {
        log_message(LOG_ERROR, "USB", "Failed to allocate FATFS for drive %d", drv);
        disk_shutdown((BYTE)drv);
        return false;
    }
    memset(fatfs, 0, sizeof(FATFS));
    if (drv == 1) {
        g_sectorOffset[drv] = FAT_SECTOR_OFFSET;
        log_message(LOG_DEBUG, "USB", "Applying sector offset %d for drive %d", FAT_SECTOR_OFFSET, drv);
    }
    char mount_path[16];
    snprintf(mount_path, sizeof(mount_path), "%d:", drv);
    FRESULT fr = f_mount(fatfs, mount_path, 1);
    if (fr != FR_OK) {
        log_message(LOG_ERROR, "USB", "f_mount failed for drive %d: %d", drv, fr);
        free(fatfs);
        disk_shutdown((BYTE)drv);
        return false;
    }
    f_chdrive(mount_path);
    if (!devoptab_register(dev_name, (BYTE)drv)) {
        log_message(LOG_ERROR, "USB", "Failed to register devoptab for %s", dev_name);
        f_unmount(mount_path);
        free(fatfs);
        disk_shutdown((BYTE)drv);
        return false;
    }
    fatfs_objs[drv] = fatfs;
    drive_mounted[drv] = true;
    log_message(LOG_OK, "USB", "FAT32 drive %d (%s) mounted successfully", drv, dev_name);
    return true;
}

static void unmount_fat_drive(int drv) {
    if (!drive_mounted[drv]) {
        log_message(LOG_DEBUG, "USB", "FAT32 drive %d not mounted, skipping", drv);
        return;
    }
    log_message(LOG_DEBUG, "USB", "Unmounting FAT32 drive %d...", drv);
    devoptab_unregister_all();
    char mount_path[16];
    snprintf(mount_path, sizeof(mount_path), "%d:", drv);
    FRESULT fr = f_unmount(mount_path);
    if (fr != FR_OK) log_message(LOG_ERROR, "USB", "f_unmount(%s) failed: %d", mount_path, fr);
    if (fatfs_objs[drv]) {
        free(fatfs_objs[drv]);
        fatfs_objs[drv] = nullptr;
    }
    disk_shutdown((BYTE)drv);
    drive_mounted[drv] = false;
    log_message(LOG_DEBUG, "USB", "FAT32 drive %d unmounted", drv);
}

void usb_init() {
    if (mocha_initialized) return;

    MochaUtilsStatus status = Mocha_InitLibrary();
    if (status == MOCHA_RESULT_SUCCESS) {
        mocha_initialized = true;
        log_message(LOG_DEBUG, "USB", "Mocha initialized successfully");
    } else {
        log_message(LOG_ERROR, "USB", "Failed to initialize Mocha: %d", status);
    }
}

void usb_mount() {
    if (!mocha_initialized) usb_init();

    if (!mocha_initialized) {
        log_message(LOG_ERROR, "USB", "Cannot mount USB - Mocha not initialized");
        return;
    }

    if (active_drive >= 0) {
        log_message(LOG_DEBUG, "USB", "USB already mounted on drive %d", active_drive);
        return;
    }

    log_message(LOG_DEBUG, "USB", "Attempting to mount FAT32 USB drive...");
    for (int drv = FAT_DRIVE_FIRST; drv <= FAT_DRIVE_LAST; drv++) {
        if (mount_fat_drive(drv, USB_DEVICE_NAME)) {
            active_drive = drv;
            log_message(LOG_OK, "USB", "USB mounted on drive %d", drv);
            return;
        }
    }

    log_message(LOG_ERROR, "USB", "No FAT32 USB drive found on drives %d-%d", FAT_DRIVE_FIRST, FAT_DRIVE_LAST);
}

void usb_unmount() {
    if (!mocha_initialized) return;

    if (active_drive < 0) {
        log_message(LOG_DEBUG, "USB", "No FAT32 drive to unmount");
        return;
    }

    log_message(LOG_DEBUG, "USB", "Unmounting FAT32 USB drive %d...", active_drive);
    unmount_fat_drive(active_drive);
    active_drive = -1;
}

void usb_shutdown() {
    if (!mocha_initialized) return;

    for (int i = 0; i < MAX_FAT_DRIVES; i++) {
        if (drive_mounted[i]) unmount_fat_drive(i);
    }

    active_drive = -1;
    log_message(LOG_DEBUG, "USB", "Deinitializing Mocha...");
    Mocha_DeInitLibrary();
    mocha_initialized = false;
    log_message(LOG_DEBUG, "USB", "Mocha deinitialized");
}
#endif
