#include <coreinit/energysaver.h>

#include "logger/logger.hpp"

#include "utils/power_manager.hpp"

uint32_t power_manager_is_auto_power_down_enabled = 0;
uint32_t power_manager_is_auto_dim_enabled = 0;

int power_manager_sleep_enable(bool state) {
    IMError error = 0;

    if ((error = IMIsAPDEnabled(&power_manager_is_auto_power_down_enabled)) != 0)
        goto error;

    if ((error = IMIsDimEnabled(&power_manager_is_auto_dim_enabled)) != 0)
        goto error;

#ifdef DEBUG
    log_message(LOG_DEBUG, "Power Manager", "Restore state is apd: %s, dim: %s",
							power_manager_is_auto_power_down_enabled ? "on" : "off", power_manager_is_auto_dim_enabled ? "on" : "off");
#endif

    if (state) {
        if (power_manager_is_auto_power_down_enabled &&
            (error = IMEnableAPD()) != 0)
            goto error;

        if (power_manager_is_auto_dim_enabled &&
            (error = IMEnableDim()) != 0)
            goto error;

        log_message(LOG_OK, "Power Manager", "Enabled power management");
    } else {
        if (power_manager_is_auto_power_down_enabled &&
            (error = IMDisableAPD()) != 0)
            goto error;

        if (power_manager_is_auto_dim_enabled &&
            (error = IMDisableDim()) != 0)
            goto error;

        log_message(LOG_OK, "Power Manager", "Disabled power management");
    }

    return 0;

error:
    log_message(LOG_WARNING, "Power Manager", "Failed power management with code %i", error);
    return error;
}
