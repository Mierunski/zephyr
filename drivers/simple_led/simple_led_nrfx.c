/*
 * Copyright (c) 2019, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <simple_led.h>
#include <device.h>
#include <nrf_gpio.h>

struct simple_led_config {
	u8_t pin_num;
};

static void nrfx_led_on(struct device *dev)
{
	struct simple_led_config *cfg =
		(struct simple_led_config *) dev->config->config_info;

	nrf_gpio_pin_set(cfg->pin_num);
}
static void nrfx_led_off(struct device *dev)
{
	struct simple_led_config *cfg =
		(struct simple_led_config *) dev->config->config_info;

	nrf_gpio_pin_clear(cfg->pin_num);
}

static struct simple_led_api api = {
		.turn_on = nrfx_led_on,
		.turn_off = nrfx_led_off
};

static int simple_led_init(struct device *dev)
{
	struct simple_led_config *cfg =
		(struct simple_led_config *) dev->config->config_info;

	nrf_gpio_cfg_output(cfg->pin_num);
	return 0;
}

struct simple_led_config simple_led0_config = {
	.pin_num = DT_NORDIC_SIMPLE_LED_0_PIN
};

DEVICE_AND_API_INIT(simple_led0,
		    DT_NORDIC_SIMPLE_LED_0_LABEL,
		    simple_led_init,
		    NULL,
		    &simple_led0_config,
		    POST_KERNEL,
		    CONFIG_SIMPLE_LED_INIT_PRIORITY,
		    &api);
