/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <misc/byteorder.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

static struct bt_conn *default_conn;

static struct bt_uuid_128 uuid = BT_UUID_INIT_128(
	0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static struct bt_uuid_128 led_uuid = BT_UUID_INIT_128(
	0xc0, 0xff, 0xee, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

static u8_t notify_func(struct bt_conn *conn,
			   struct bt_gatt_subscribe_params *params,
			   const void *data, u16_t length)
{
	if (!data) {
		LOG_INF("[UNSUBSCRIBED]");
		params->value_handle = 0;
		return BT_GATT_ITER_STOP;
	}

	LOG_INF("[NOTIFICATION] data %p length %u", data, length);

	return BT_GATT_ITER_CONTINUE;
}

static u8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		LOG_INF("Discover complete");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	LOG_INF("[ATTRIBUTE] handle %u", attr->handle);

	if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HRS)) {
		memcpy(&uuid, BT_UUID_HRS_MEASUREMENT, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_INF("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_HRS_MEASUREMENT)) {
		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params.value_handle = attr->handle + 1;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_INF("Discover failed (err %d)", err);
		}
	} else {
		subscribe_params.notify = notify_func;
		subscribe_params.value = BT_GATT_CCC_NOTIFY;
		subscribe_params.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			LOG_INF("Subscribe failed (err %d)", err);
		} else {
			LOG_INF("[SUBSCRIBED]");
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, u8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_INF("Failed to connect to %s (%u)", addr, conn_err);
		return;
	}

	LOG_INF("Connected: %s", addr);

	if (conn == default_conn) {
		discover_params.uuid = &uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = 0x0001;
		discover_params.end_handle = 0xffff;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed(err %d)", err);
			return;
		}
	}
}


static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;
	int i;

	LOG_INF("[AD]: %u data_len %u", data->type, data->data_len);

	switch (data->type) {
	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
#if 0
		if (data->data_len % sizeof(u16_t) != 0) {
			LOG_INF("AD malformed");
			return true;
		}

		for (i = 0; i < data->data_len; i += sizeof(u16_t)) {
			struct bt_uuid *uuid;
			u16_t u16;
			int err;

			memcpy(&u16, &data->data[i], sizeof(u16));
			uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
			LOG_INF("%02x", u16);
			if (bt_uuid_cmp(uuid, BT_UUID_HRS)) {
				continue;
			}

			err = bt_le_scan_stop();
			if (err) {
				LOG_INF("Stop LE scan failed (err %d)", err);
				continue;
			}

			default_conn = bt_conn_create_le(addr,
							 BT_LE_CONN_PARAM_DEFAULT);
			LOG_INF("Ret");
			return false;
		}
#endif
		break;
	case BT_DATA_UUID128_SOME:
	case BT_DATA_UUID128_ALL:
	{
		struct bt_uuid_128 t_uuid;
		int err;
		t_uuid.uuid.type = BT_UUID_TYPE_128;

		memcpy(t_uuid.val, data->data, 16);
		int res = bt_uuid_cmp((struct bt_uuid *)&uuid,
				      (struct bt_uuid *)&t_uuid);
		if (res != 0) {
			break;
		}
		LOG_INF("Found vendor uuid");
		err = bt_le_scan_stop();
		if (err) {
			LOG_INF("Stop LE scan failed (err %d)", err);
			break;
		}
		default_conn = bt_conn_create_le(addr,
						 BT_LE_CONN_PARAM_DEFAULT);
		LOG_INF("Connected");
		return false;
	}
	}

	return true;
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
			 struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];

	/* connect only to devices in close proximity */
	if (rssi < -40) {
		return;
	}

	bt_addr_le_to_str(addr, dev, sizeof(dev));
	LOG_INF("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i",
	       dev, type, ad->len, rssi);

	/* We're only interested in connectable events */
	if (type == BT_LE_ADV_IND || type == BT_LE_ADV_DIRECT_IND) {
		bt_data_parse(ad, eir_found, (void *)addr);
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		LOG_INF("Scanning failed to start (err %d)", err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

void main(void)
{
	int err;
	err = bt_enable(NULL);

	if (err) {
		LOG_INF("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized");

	bt_conn_cb_register(&conn_callbacks);

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);

	if (err) {
		LOG_INF("Scanning failed to start (err %d)", err);
		return;
	}

	LOG_INF("Scanning successfully started");
}
