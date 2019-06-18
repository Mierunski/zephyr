/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sensors.h"

#include <zephyr.h>
#include <gpio.h>
#include <sensor.h>
#include <misc/printk.h>
#include <logging/log.h>
#include <power.h>
#include <device.h>

#include "display.h"
#include "led.h"
#include <hal/nrf_gpio.h>

LOG_MODULE_REGISTER(app_sensory, LOG_LEVEL_INF);

#define SENSORY_STACK_SIZE	(2048U)

extern void __spim_uninit(void);
extern void __spim_reinit(void);
extern void __twim_uninit(void);
extern void __twim_reinit(void);
extern void __uarte_unint(void);
extern void __uarte_reinit(void);
extern void __ssd1673_reinit(void);
extern void __hdc_1008_reinit(void);
static void sensors_thread_function(void *arg1, void *arg2, void *arg3);

struct device_info {
	struct device *dev;
	char *name;
};

enum periph_device {
	DEV_IDX_HDC1010,
	DEV_IDX_EPD,
	DEV_IDX_NUMOF,
};

static struct device_info dev_info[] = {
	{ NULL, DT_TI_HDC1010_0_LABEL },
	{ NULL, DT_SOLOMON_SSD1673FB_0_LABEL }
};

static K_THREAD_STACK_DEFINE(sensors_thread_stack, SENSORY_STACK_SIZE);
static struct k_delayed_work temperature_external_timeout;
static k_thread_stack_t *stack = sensors_thread_stack;
static const char *thread_name = "sensors";
struct k_thread sensors_thread;

#define TEMP_AVERAGE_SAMPLES 3
typedef struct {
	s32_t sample[TEMP_AVERAGE_SAMPLES];
	u8_t idx;
	bool valid;
} ext_temp_t;

static ext_temp_t ext_temperature;

static s32_t ext_temp_get(ext_temp_t *temp)
{
	s32_t average = 0;
	s32_t cmp;

	if ((temp->idx == 0) && (temp->valid == false)) {
		return INVALID_SENSOR_VALUE;
	} else if (temp->valid) {
		cmp = TEMP_AVERAGE_SAMPLES;
	} else {
		cmp = temp->idx;
	}

	for (u8_t i = 0; i < cmp; i++) {
		average += temp->sample[i];
	}

	return average / cmp;
}

static inline void ext_temp_reset(ext_temp_t *temp)
{
	temp->idx = 0;
	temp->valid = false;
}

static bool ext_temp_add(ext_temp_t *temp, s32_t val)
{
	if (!((val >= -500) && (val <= 1250))) {
		ext_temp_reset(temp);
		return false;
	}

	temp->sample[temp->idx++] = val;
	if (temp->idx >= TEMP_AVERAGE_SAMPLES) {
		temp->idx = 0;
		temp->valid = true;
	}

	return true;
}

static s32_t temperature = INVALID_SENSOR_VALUE;
static s32_t humidity = INVALID_SENSOR_VALUE;

static inline void power_sensors(bool state);

static void timeout_handle(struct k_work *work)
{
	ext_temp_reset(&ext_temperature);
}

/* humidity & temperature */
static int get_hdc1010_val(void)
{
	struct sensor_value sensor;

	if (sensor_sample_fetch(dev_info[DEV_IDX_HDC1010].dev)) {
		LOG_ERR("Failed to fetch sample for device %s",
			dev_info[DEV_IDX_HDC1010].name);

		temperature = INVALID_SENSOR_VALUE;
		humidity = INVALID_SENSOR_VALUE;
		return -1;
	}

	if (sensor_channel_get(dev_info[DEV_IDX_HDC1010].dev,
			       SENSOR_CHAN_AMBIENT_TEMP, &sensor)) {
		temperature = INVALID_SENSOR_VALUE;
		LOG_ERR("Invalid Temperature value");
		return -1;
	}
	temperature = sensor.val1 * 10 + sensor.val2 / 100000;

	if (sensor_channel_get(dev_info[DEV_IDX_HDC1010].dev,
			       SENSOR_CHAN_HUMIDITY, &sensor)) {
		humidity = INVALID_SENSOR_VALUE;
		LOG_ERR("Invalid Humidity value");
		return -1;
	}
	humidity = sensor.val1;

	return 0;
}

int sensory_init(void)
{
	k_delayed_work_init(&temperature_external_timeout, timeout_handle);

	for (u8_t i = 0; i < ARRAY_SIZE(dev_info); i++) {
		dev_info[i].dev = device_get_binding(dev_info[i].name);
		if (dev_info[i].dev == NULL) {
			LOG_ERR("Failed to get %s device", dev_info[i].name);
			return -EBUSY;
		}
	}

	k_tid_t tid = k_thread_create(&sensors_thread,
				      stack,
				      SENSORY_STACK_SIZE,
				      sensors_thread_function,
				      NULL,
				      NULL,
				      NULL,
				      K_LOWEST_APPLICATION_THREAD_PRIO,
				      0,
				      500);

	k_thread_name_set(tid, thread_name);
	return 0;
}
extern int ssd1673_resume(const struct device *dev);
extern int ssd1673_suspend(const struct device *dev);
static inline void power_spim(bool status)
{
	struct device * spi = device_get_binding(DT_SOLOMON_SSD1673FB_0_BUS_NAME);
	if (status) {
		device_set_power_state(spi, DEVICE_PM_ACTIVE_STATE, NULL, NULL);
	} else {
		device_set_power_state(spi, DEVICE_PM_LOW_POWER_STATE, NULL, NULL);
	}
}

static inline void power_uarte(bool status)
{
	struct device * uart = device_get_binding(DT_UART_0_NAME);
	if (status) {
		device_set_power_state(uart, DEVICE_PM_ACTIVE_STATE, NULL, NULL);
	} else {
		device_set_power_state(uart, DEVICE_PM_LOW_POWER_STATE, NULL, NULL);
	}
}

static inline void power_i2c(bool status)
{
	struct device * twi = device_get_binding(DT_TI_HDC1008_0_BUS_NAME);
	if (status) {
		device_set_power_state(twi, DEVICE_PM_ACTIVE_STATE, NULL, NULL);
		k_sleep(K_MSEC(2));
		__hdc_1008_reinit();
	} else {
		device_set_power_state(twi, DEVICE_PM_LOW_POWER_STATE, NULL, NULL);
		nrf_gpio_cfg_output(27);
		nrf_gpio_cfg_output(26);
		nrf_gpio_pin_write(22, 0);
		nrf_gpio_pin_write(23, 0);
		nrf_gpio_pin_write(24, 0);
		nrf_gpio_pin_write(25, 0);
		nrf_gpio_pin_write(26, 0);
		nrf_gpio_pin_write(27, 0);
	}
}

static inline void power_sensors(bool state)
{
	/* switch on/off power supply */
	if (state == false) {
		power_spim(0);
		power_i2c(0);
		power_uarte(0);
		nrf_gpio_pin_write(32, 0);
	} else {
		nrf_gpio_pin_write(32, 1);
		power_uarte(1);
		power_i2c(1);
		power_spim(1);
	}
}

int sensors_get_temperature(void)
{
	return temperature;
}

int sensors_get_temperature_external(void)
{
	return ext_temp_get(&ext_temperature);
}

void sensors_set_temperature_external(s16_t tmp)
{
	if (ext_temp_add(&ext_temperature, tmp)) {
		led_set_time(LED4, 25);
		k_delayed_work_submit(&temperature_external_timeout,
				      K_SECONDS(90));
	}
}

int sensors_get_humidity(void)
{
	return humidity;
}

static void sensors_thread_function(void *arg1, void *arg2, void *arg3)
{
	nrf_gpio_cfg_output(32);
	display_screen(SCREEN_BOOT);
	k_sleep(K_SECONDS(3));

	while (1) {
		/* power sensors and display */
		LOG_DBG("Sensors thread tick");
		if (get_hdc1010_val() != 0) {
			get_hdc1010_val();
		}
		display_screen(SCREEN_SENSORS);
		k_sleep(K_MSEC(100));
		power_sensors(false);
		/* switch off sensors and display */
		k_sleep(K_SECONDS(7));
		power_sensors(true);
	}
}
