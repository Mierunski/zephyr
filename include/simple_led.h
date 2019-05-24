/*
 * Copyright (c) 2019, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef ZEPHYR_INCLUDE_SIMPLE_LED_H_
#define ZEPHYR_INCLUDE_SIMPLE_LED_H_

#include <device.h>

struct simple_led_api {
	void (*turn_on)(struct device *dev);
	void (*turn_off)(struct device *dev);
};

static inline void simple_led_on(struct device *dev)
{
	const struct simple_led_api *api =
			(const struct simple_led_api *)dev->driver_api;

	return api->turn_on(dev);
}

static inline void simple_led_off(struct device *dev)
{
	const struct simple_led_api *api =
			(const struct simple_led_api *)dev->driver_api;

	return api->turn_off(dev);
}

#endif /* ZEPHYR_INCLUDE_SIMPLE_LED_H_ */
