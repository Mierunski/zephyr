/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <hal/nrf_gpio.h>
#include <device.h>


void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

	struct device *uart = device_get_binding(DT_UART_0_NAME);
	printk("%p\n", uart);
	device_set_power_state(uart, DEVICE_PM_SUSPEND_STATE, NULL, NULL);
	nrf_gpio_cfg_output(32);
	nrf_gpio_pin_write(32, 0);
	while (1) {
	k_sleep(2000);
	device_set_power_state(uart, DEVICE_PM_ACTIVE_STATE, NULL, NULL);
	printk("wow\n");
	device_set_power_state(uart, DEVICE_PM_SUSPEND_STATE, NULL, NULL);
	}
	return;
}
