/*
 * This file was part of the flashrom project.
 *
 * Copyright (C) 2014 Alexandre Boeglin <alex@boeglin.org>
 * Copyright (C) 2021 Daniel Palmer <daniel@thingy.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <stdbool.h>

#include "spi_controller.h"

//#define MSTARDDC_DEBUG

#define msg_perr printf
#define msg_pinfo printf

struct mstarddc_spi_data {
	int fd;
	int addr;
	int doreset;
};

// MSTAR DDC Commands
#define MSTARDDC_SPI_WRITE	0x10
#define MSTARDDC_SPI_READ	0x11
#define MSTARDDC_SPI_END	0x12
#define MSTARDDC_SPI_RESET	0x24

static struct mstarddc_spi_data *mstarddc_data;

/* Returns 0 upon success, a negative number upon errors. */
static int mstarddc_spi_shutdown(void)
{
	if (close(mstarddc_data->fd) < 0) {
		msg_perr("Error closing device: errno %d.\n", errno);
		return -1;
	}

	free(mstarddc_data);
	return 0;
}

/* Returns 0 upon success, a negative number upon errors. */
static int mstarddc_spi_send_command(unsigned int writecnt,
				     unsigned int readcnt,
				     const unsigned char *writearr,
				     unsigned char *readarr)
{
	int ret = 0, i,j;

#ifdef MSTARDDC_DEBUG
	printf("%s %d %d\n", __func__, writecnt, readcnt);
#endif

	if (!ret && writecnt) {
		size_t cmdsz = (writecnt + 1) * sizeof(uint8_t);
		uint8_t *cmd = malloc(cmdsz);
		struct i2c_rdwr_ioctl_data i2c_data;
		struct i2c_msg msg[1];
		int tries = 10;

		if (cmd == NULL) {
			msg_perr("Error allocating memory: errno %d.\n", errno);
			ret = -1;
		}

		cmd[0] = MSTARDDC_SPI_WRITE;
		memcpy(cmd + 1, writearr, writecnt);

		memset(&i2c_data, 0, sizeof(i2c_data));
		memset(&msg, 0, sizeof(msg));

		i2c_data.nmsgs = 1;
		i2c_data.msgs = msg;
		i2c_data.msgs[0].addr = mstarddc_data->addr;
		i2c_data.msgs[0].len = cmdsz;
		i2c_data.msgs[0].flags = 0;
		i2c_data.msgs[0].buf = cmd;

		for(; tries; tries--) {
			if (ioctl(mstarddc_data->fd, I2C_RDWR, &i2c_data) < 0) {
				msg_perr("Error sending write command: errno %d, tries left %d\n", errno, tries);
				ret = -1;
			}
			else {
				ret = 0;
				break;
			}
		}

		if (cmd)
			free(cmd);
	}

	if (!ret && readcnt) {
		struct i2c_rdwr_ioctl_data i2c_data;
		struct i2c_msg msg[2];
		int tries = 10;

		memset(&i2c_data, 0, sizeof(i2c_data));
		memset(&msg, 0, sizeof(msg));

		uint8_t cmd = MSTARDDC_SPI_READ;
		i2c_data.nmsgs = 2;
		i2c_data.msgs = msg;
		i2c_data.msgs[0].addr = mstarddc_data->addr;
		i2c_data.msgs[0].len = 1;
		i2c_data.msgs[0].flags = 0;
		i2c_data.msgs[0].buf = &cmd;
		i2c_data.msgs[1].addr = mstarddc_data->addr;
		i2c_data.msgs[1].len = readcnt;
		i2c_data.msgs[1].flags = I2C_M_RD;
		i2c_data.msgs[1].buf = readarr;

		for(; tries; tries--) {
			if (ioctl(mstarddc_data->fd, I2C_RDWR, &i2c_data) < 0) {
				msg_perr("Error sending read command: errno %d, tries left %d\n", errno, tries);
				ret = -1;
			}
			else {
				ret = 0;
				break;
			}
		}
	}

#ifdef MSTARDDC_DEBUG
	for (i = 0; i < writecnt; i++){
		if(i % 16 == 0)
			printf("w: ");
		printf("0x%02x ", writearr[i]);
		if((i + 1) % 16 == 0)
			printf("\n");
	}
	printf("\n");
	for (i = 0; i < readcnt; i++){
		if(i % 16 == 0)
			printf("r: ");
		printf("0x%02x ", readarr[i]);
		if((i + 1) % 16 == 0)
			printf("\n");
	}
	printf("\n");
#endif

//	if (!ret && (writecnt || readcnt)) {
//	}

	/* Do not reset if something went wrong, as it might prevent from
	 * retrying flashing. */
	if (ret != 0)
		mstarddc_data->doreset = 0;

	return ret;
}

static int mstarddc_spi_end_command(void)
{
	uint8_t cmd[1];
	int tries = 10, ret;
	bool success = false;
	struct i2c_rdwr_ioctl_data i2c_data;
	struct i2c_msg msg[1];

#ifdef MSTARDDC_DEBUG
	printf("%s\n", __func__);
#endif

	cmd[0] = MSTARDDC_SPI_END;

	memset(&i2c_data, 0, sizeof(i2c_data));
	memset(&msg, 0, sizeof(msg));

	i2c_data.nmsgs = 1;
	i2c_data.msgs = msg;
	i2c_data.msgs[0].addr = mstarddc_data->addr;
	i2c_data.msgs[0].len = 1;
	i2c_data.msgs[0].buf = cmd;

	for(; tries; tries--) {
		if (ioctl(mstarddc_data->fd, I2C_RDWR, &i2c_data) < 0) {
			msg_perr("Error sending end command: errno %d, tries left %d\n", errno, tries);
			ret = -1;
		}
		else {
			ret = 0;
			break;
		}
	}

	return ret;
}


/* Returns 0 upon success, a negative number upon errors. */
static int mstarddc_spi_init(const char *connection)
{
	int ret = 0;
	int mstarddc_fd = -1;
	int mstarddc_addr;
	int mstarddc_doreset = 1;

	// Get device, address from command-line
	char *i2c_device = connection;
	if (i2c_device != NULL && strlen(i2c_device) > 0) {
		char *i2c_address = strchr(i2c_device, ':');
		if (i2c_address != NULL) {
			*i2c_address = '\0';
			i2c_address++;
		}
		if (i2c_address == NULL || strlen(i2c_address) == 0) {
			msg_perr("Error: no address specified.\n"
				 "Use flashrom -p mstarddc_spi:dev=/dev/device:address.\n");
			ret = -1;
			goto out;
		}
		mstarddc_addr = strtol(i2c_address, NULL, 16); // FIXME: error handling
	} else {
		msg_perr("Error: no device specified.\n"
			 "Use flashrom -p mstarddc_spi:dev=/dev/device:address.\n");
		ret = -1;
		goto out;
	}
	msg_pinfo("Info: Will try to use device %s and address 0x%02x.\n", i2c_device, mstarddc_addr);

	// Get noreset=1 option from command-line
	//char *noreset = extract_programmer_param("noreset");
	//if (noreset != NULL && noreset[0] == '1')
		mstarddc_doreset = 0;
	//free(noreset);
	msg_pinfo("Info: Will %sreset the device at the end.\n", mstarddc_doreset ? "" : "NOT ");
	// Open device
	if ((mstarddc_fd = open(i2c_device, O_RDWR)) < 0) {
		switch (errno) {
		case EACCES:
			msg_perr("Error opening %s: Permission denied.\n"
				 "Please use sudo or run as root.\n",
				 i2c_device);
			break;
		case ENOENT:
			msg_perr("Error opening %s: No such file.\n"
				 "Please check you specified the correct device.\n",
				 i2c_device);
			break;
		default:
			msg_perr("Error opening %s: %s.\n", i2c_device, strerror(errno));
		}
		ret = -1;
		goto out;
	}

	// Set slave address
	if (ioctl(mstarddc_fd, I2C_SLAVE, mstarddc_addr) < 0) {
		msg_perr("Error setting slave address 0x%02x: errno %d.\n",
			 mstarddc_addr, errno);
		ret = -1;
		goto out;
	}
	// Enable ISP mode
	uint8_t cmd[5] = { 'M', 'S', 'T', 'A', 'R' };
	if (write(mstarddc_fd, cmd, 5) < 0) {
		int enable_err = errno;
		uint8_t end_cmd = MSTARDDC_SPI_END;

		// Assume device is already in ISP mode, try to send END command
		if (write(mstarddc_fd, &end_cmd, 1) < 0) {
			msg_perr("Error enabling ISP mode: errno %d & %d.\n"
				 "Please check that device (%s) and address (0x%02x) are correct.\n",
				 enable_err, errno, i2c_device, mstarddc_addr);
			ret = -1;
			goto out;
		}
	}

	mstarddc_data = calloc(1, sizeof(*mstarddc_data));
	if (!mstarddc_data) {
		msg_perr("Unable to allocate space for SPI master data\n");
		ret = -1;
		goto out;
	}

	mstarddc_data->fd = mstarddc_fd;
	mstarddc_data->addr = mstarddc_addr;
	mstarddc_data->doreset = mstarddc_doreset;

out:
	free(i2c_device);
	if (ret && (mstarddc_fd >= 0))
		close(mstarddc_fd);
	return ret;
}

const struct spi_controller mstarddc_spictrl = {
	.name = "mstarddc",
	.init = mstarddc_spi_init,
	.shutdown = mstarddc_spi_shutdown,
	.send_command = mstarddc_spi_send_command,
	.cs_release = mstarddc_spi_end_command,
	.max_transfer = 64,
};
