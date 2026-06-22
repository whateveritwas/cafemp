/*
 * Automatically redirect stdout to WHBLogWrite().
 * Copyright 2026  Daniel K. O. (dkosmari)
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * SPDX-License-Identifier: MIT
 *
 * Source: https://github.com/dkosmari/wiiu-stdout
 */

#ifdef __WIIU__

#include <cstdlib>              // malloc(), free()
#include <cstring>              // memcpy()

#include <sys/iosupport.h>      // devoptab_list, devoptab_t

#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>

#include "wiiu-stdout.hpp"


OSMutex* wiiu_whb_log_mutex; // initialized by wiiu-stderr.cpp


namespace {

    bool cafe_initialized   = false;
    bool module_initialized = false;
    bool udp_initialized    = false;

    unsigned whb_log_refs  = 0;
    unsigned devoptab_refs = 0;

} // namespace


__attribute__ (( __constructor__ (101) ))
void
wiiu_init_whb_log()
    noexcept
{
    if (whb_log_refs++)
        return;
    module_initialized = WHBLogModuleInit();
    if (!module_initialized) {
        cafe_initialized = WHBLogCafeInit();
        udp_initialized = WHBLogUdpInit();
    }
}


__attribute__ (( __destructor__ (101) ))
void
wiiu_fini_whb_log()
    noexcept
{
    if (!whb_log_refs)
        return;
    if (--whb_log_refs)
        return;
    if (module_initialized) {
        WHBLogModuleDeinit();
        module_initialized = false;
    }
    if (cafe_initialized) {
        WHBLogCafeDeinit();
        cafe_initialized = false;
    }
    if (udp_initialized) {
        WHBLogUdpDeinit();
        udp_initialized = false;
    }
}


ssize_t
wiiu_devoptab_to_whb_log(struct _reent*,
                         void*,
                         const char* buf,
                         std::size_t len)
    noexcept
{
    // Note: WHBLogWrite expects a null-terminated string.
    std::size_t msg_size = len + 1;
    if (msg_size == 0)
        return -1;
    char* msg = static_cast<char*>(std::malloc(msg_size));
    if (!msg)
        return -1;
    std::memcpy(msg, buf, len);
    msg[len] = 0;

    if (wiiu_whb_log_mutex) {
        OSLockMutex(wiiu_whb_log_mutex);
        WHBLogWrite(msg);
        OSUnlockMutex(wiiu_whb_log_mutex);
    } else {
        WHBLogWrite(msg);
    }

    std::free(msg);
    return len;
}


__attribute__(( __constructor__ (102) ))
void
wiiu_init_stdout()
    noexcept
{
    if (!whb_log_refs)
        wiiu_init_whb_log();
    if (devoptab_refs++)
        return;
    static devoptab_t stdout_dev;
    stdout_dev.name = "STDOUT";
    stdout_dev.structSize = sizeof stdout_dev;
    stdout_dev.write_r = wiiu_devoptab_to_whb_log;
    devoptab_list[STD_OUT] = &stdout_dev;
}

#endif // __WIIU__
