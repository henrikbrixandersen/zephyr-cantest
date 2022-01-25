/*
 * Copyright (c) 2021 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/can.h>
#include <drivers/gpio.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static const struct zcan_frame frame = {
	.id = 16,
	.fd = 0,
	.rtr = CAN_DATAFRAME,
	.id_type = CAN_STANDARD_IDENTIFIER,
	.dlc = 0,
	.brs = 0,
	.data = { },
};

struct button_callback_context {
	struct gpio_callback callback;
	struct k_sem sem;
};

static void can_tx_callback(int error, void *user_data)
{
	struct k_sem *tx_sem = user_data;

	k_sem_give(tx_sem);
}

static void button_callback(const struct device *port,
			    struct gpio_callback *cb,
			    gpio_port_pins_t pins)
{
	struct button_callback_context *ctx =
		CONTAINER_OF(cb, struct button_callback_context, callback);

	k_sem_give(&ctx->sem);
}

void main(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
	const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
	struct button_callback_context btn_cb_ctx;
	struct k_sem tx_sem;
	int err;

	k_sem_init(&tx_sem, 0, INT_MAX);
	k_sem_init(&btn_cb_ctx.sem, 0, 1);

	if (!device_is_ready(dev)) {
		LOG_ERR("CAN device not ready");
		return;
	}

	if (!device_is_ready(btn.port)) {
		LOG_ERR("button device not ready");
		return;
	}

	LOG_INF("starting CAN bus hog on device '%s' using CAN ID %d ...",
		dev->name, frame.id);

	err = gpio_pin_configure_dt(&btn, GPIO_INPUT);
	if (err != 0) {
		LOG_ERR("failed to configure button GPIO (err %d)", err);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_TO_ACTIVE);
	if (err != 0) {
		LOG_ERR("failed to configure button interrupt (err %d)", err);
		return;
	}

	gpio_init_callback(&btn_cb_ctx.callback, button_callback, BIT(btn.pin));
	gpio_add_callback(btn.port, &btn_cb_ctx.callback);

	LOG_INF("queuing 1st frame");
	err = can_send(dev, &frame, K_NO_WAIT, can_tx_callback, &tx_sem);
	if (err != 0) {
		LOG_ERR("failed to queue 1st CAN frame (err %d)", err);
		return;
	}

	LOG_INF("queuing 2nd frame");
	err = can_send(dev, &frame, K_NO_WAIT, can_tx_callback, &tx_sem);
	if (err != 0) {
		LOG_ERR("failed to queue 2nd CAN frame (err %d)", err);
		return;
	}

	LOG_INF("bus hog running, abort by pressing sw0 button");

	while (true) {
		if (k_sem_take(&btn_cb_ctx.sem, K_NO_WAIT) == 0) {
			LOG_INF("button press detected, aborting bus hog");
			return;
		}

		err = can_send(dev, &frame, K_NO_WAIT, can_tx_callback, &tx_sem);
		if (err != 0) {
			LOG_ERR("failed to queue CAN frame (err %d)", err);
			return;
		}

		k_sem_take(&tx_sem, K_FOREVER);
	}
}
