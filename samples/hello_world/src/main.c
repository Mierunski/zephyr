/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <simple_led.h>

void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);
	struct device *led = device_get_binding(DT_NORDIC_SIMPLE_LED_0_LABEL);
	while(1) {
		simple_led_on(led);
		k_sleep(100);
		simple_led_off(led);
		k_sleep(100);
	}
}
