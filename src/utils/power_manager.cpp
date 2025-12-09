#include <coreinit/energysaver.h>

#include "logger/logger.hpp"

#include "utils/power_manager.hpp"

int power_manager_sleep_enable(bool state) {
	IMError error;

	if (state) {
		error = IMDisableAPD();
		if (error != 0) goto error;

		error = IMDisableDim();

		log_message(LOG_OK, "Power Manager", "Enabled power managment");
	} else {
		error = IMDisableAPD();
		if (error != 0) goto error;

		error = IMDisableDim();

		log_message(LOG_OK, "Power Manager", "Disabled power managment");
	}

	if (error != 0) goto error;

	return 0;

error:
	log_message(LOG_WARNING, "Power Manager", "Faild power managment with code %i", error);
	return error;
}
