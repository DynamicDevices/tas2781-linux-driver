/*
 * TAS2563/TAS2871 Linux Driver
 *
 * Copyright (C) 2022 - 2024 Texas Instruments Incorporated
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TASDEVICE_MISC_H_
#define __TASDEVICE_MISC_H_

struct smartpa_params {
	int mProg;
	int config;
	int regscene;
};

struct smartpa_info {
	unsigned char dev_name[32];
	unsigned char spkvendorid;
	unsigned char ndev;
	unsigned char i2c_list[TASDEVICE_MAX_CHANNELS];
	unsigned char bSPIEnable;
};

struct BPR {
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nRegister;
};

#define TILOAD_IOC_MAGIC			(0xE0)
#define TILOAD_IOMAGICNUM_GET		_IOR(TILOAD_IOC_MAGIC, 1, int)
#define TILOAD_IOMAGICNUM_SET		_IOW(TILOAD_IOC_MAGIC, 2, int)
#define TILOAD_BPR_READ			_IOR(TILOAD_IOC_MAGIC, 3, struct BPR)
#define TILOAD_BPR_WRITE		_IOW(TILOAD_IOC_MAGIC, 4, struct BPR)
#define TILOAD_IOCTL_SET_CHL		_IOW(TILOAD_IOC_MAGIC, 5, int)
#define TILOAD_IOCTL_SET_CONFIG		_IOW(TILOAD_IOC_MAGIC, 6, int)
#define TILOAD_IOCTL_SET_CALIBRATION	_IOW(TILOAD_IOC_MAGIC, 7, int)

#define TILOAD_IOC_MAGIC_SET_DEFAULT_CALIB_PRI	_IOWR(TILOAD_IOC_MAGIC, 15, \
	void *)
#define TILOAD_IOC_MAGIC_PA_INFO_GET	_IOR(TILOAD_IOC_MAGIC, 34, \
	struct smartpa_info)
#define TILOAD_IOC_MAGIC_POWERON	_IOWR(TILOAD_IOC_MAGIC, 16, \
	struct smartpa_params)
#define TILOAD_IOC_MAGIC_POWER_OFF	_IOWR(TILOAD_IOC_MAGIC, 23, \
	struct smartpa_params)

#ifdef CONFIG_COMPAT
#define TILOAD_COMPAT_IOMAGICNUM_GET	_IOR(TILOAD_IOC_MAGIC, 1, compat_int_t)
#define TILOAD_COMPAT_IOMAGICNUM_SET	_IOW(TILOAD_IOC_MAGIC, 2, compat_int_t)
#define TILOAD_COMPAT_BPR_READ		_IOR(TILOAD_IOC_MAGIC, 3, struct BPR)
#define TILOAD_COMPAT_BPR_WRITE		_IOW(TILOAD_IOC_MAGIC, 4, struct BPR)
#define TILOAD_COMPAT_IOCTL_SET_CHL	_IOW(TILOAD_IOC_MAGIC, 5, compat_int_t)
#define TILOAD_COMPAT_IOCTL_SET_CONFIG	_IOW(TILOAD_IOC_MAGIC, 6, compat_int_t)
#define TILOAD_COMPAT_IOCTL_SET_CALIBRATION	_IOW(TILOAD_IOC_MAGIC, 7, \
	compat_int_t)
#define TILOAD_IOC_MAGIC_SET_DEFAULT_CALIB_PRI_COMPAT	\
 	_IOWR(TILOAD_IOC_MAGIC, 15, compat_uptr_t)
#define TILOAD_IOC_COMPAT_MAGIC_PA_INFO_GET	\
	_IOR(TILOAD_IOC_MAGIC, 34, struct smartpa_info)
#define TILOAD_IOC_MAGIC_POWERON_COMPAT	_IOWR(TILOAD_IOC_MAGIC, 16, \
	struct smartpa_params)
#define TILOAD_IOC_MAGIC_POWER_OFF_COMPAT	_IOWR(TILOAD_IOC_MAGIC, 23, \
	struct smartpa_params)
#endif

int tasdevice_misc_register(struct tasdevice_priv *tas_dev);

#endif
