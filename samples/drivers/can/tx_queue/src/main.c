/*
 * Copyright (c) 2021 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <drivers/can.h>
#include <logging/log.h>
#include <shell/shell.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static void can_tx_callback(int error, void *user_data)
{
	ARG_UNUSED(user_data);

	if (error == 0) {
		LOG_INF("Queued TX frame transmitted");
	} else {
		LOG_ERR("Queued TX frame not transmitted (err %d)", error);
	}
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
	struct zcan_frame frame = {
		.id = 16,
		.fd = 0,
		.rtr = CAN_DATAFRAME,
		.id_type = CAN_STANDARD_IDENTIFIER,
		.dlc = 0,
		.brs = 0,
		.data = { },
	};
	int err;

	if (!device_is_ready(dev)) {
		shell_error(shell, "CAN device not ready");
		return -EINVAL;
	}

	frame.id = strtoul(argv[1], NULL, 0);
	shell_print(shell, "Queuing TX frame with CAN ID 0x%03x", frame.id);

	err = can_send(dev, &frame, K_NO_WAIT, can_tx_callback, NULL);
	if (err != 0) {
		shell_error(shell, "Failed to queue TX frame (err %d)", err);
		return err;
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(can_cmds,
	SHELL_CMD_ARG(send, NULL, "<ID>", cmd_send, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(can, &can_cmds, "CAN test shell commands", NULL);
