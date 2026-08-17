/*
 * Copyright (c) 2019-2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Runtime.
 *
 *   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The Moddable SDK Runtime is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "xsmc.h"

#include "xsHost.h"

#include "builtinCommon.h"

#if defined(__ets__) && !ESP32 && !defined(__ZEPHYR__)
	extern void system_deep_sleep_instant(uint32_t time_in_us);

static void deepSleepDeliver(void *notThe, void *refcon, uint8_t *message, uint16_t messageLength)
{
	system_deep_sleep_instant((uintptr_t)refcon);
}
#endif

#if defined(CYW43_LWIP)
	#include "lwip/dns.h"
#endif

void xs_system_deepSleep(xsMachine *the)
{
#if defined(__ets__) && !ESP32 && !defined(__ZEPHYR__)
	uint32_t us = 0;

	if (xsmcArgc) {
		us = xsmcToInteger(xsArg(0));
		if (xsmcArgc > 1) {
			int mode = xsmcToInteger(xsArg(1));
			system_deep_sleep_set_option(mode);
		}
	}
	modMessagePostToMachine(the, NULL, 0, deepSleepDeliver, (void *)us);
#endif
}

void xs_system_restart(xsMachine *the)
{
#if ESP32
	esp_restart();
#elif defined(__ets__) && !defined(__ZEPHYR__)
	system_restart();
#elif defined(PICO_BUILD)
	pico_reset();
#endif

	while (1)
		modDelayMilliseconds(1000);
}

