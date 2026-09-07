#pragma once
#include <stdlib.h>
/* LVGL's default assertion handler spins forever, hiding allocation faults
 * from the device panic/backtrace path and starving the USB heartbeat. */
#define LV_ASSERT_HANDLER abort();
