/*
 * Automatically glue stdout to WHBLogWrite().
 */

#include <string>
#include <thread>
#include <mutex>

#include <sys/iosupport.h>      // devoptab_list, devoptab_t

#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>


namespace {

    bool cafe_initialized   = false;
    bool module_initialized = false;
    bool udp_initialized    = false;


    void
    init_logs()
        noexcept
    {
        module_initialized = WHBLogModuleInit();
        if (!module_initialized) {
            cafe_initialized = WHBLogCafeInit();
            udp_initialized = WHBLogUdpInit();
        }
    }


    void
    fini_logs()
        noexcept
    {
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
    write_msg_to_whb_log(struct _reent*,
                         void*,
                         const char* buf,
                         size_t len)
        noexcept
    {
        try {
            static std::mutex mutex;
            std::lock_guard guard{mutex};
            std::string msg(buf, len);
            WHBLogWrite(msg.data());
            return len;
        }
        catch (...) {
            return -1;
        }
    }

} // namespace


__attribute__(( __constructor__ ))
void
init_stdout()
{
    init_logs();

    static devoptab_t dev;
    dev.name = "STDOUT";
    dev.structSize = sizeof dev;
    dev.write_r = write_msg_to_whb_log;
    devoptab_list[STD_OUT] = &dev;
}


__attribute__(( __destructor__ ))
void
fini_stdout()
{
    fini_logs();
}