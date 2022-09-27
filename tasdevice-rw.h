/* SPDX-License-Identifier: GPL-2.0 */
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#ifndef __TASDEVICE_RW_H__
#define __TASDEVICE_RW_H__
int tasdevice_dev_read(struct tasdevice_priv *pPcmdev,
	enum channel chn, unsigned int reg, unsigned int *pValue);

int tasdevice_dev_write(struct tasdevice_priv *pPcmdev,
	enum channel chn, unsigned int reg, unsigned int value);

int tasdevice_dev_bulk_write(
	struct tasdevice_priv *pPcmdev, enum channel chn,
	unsigned int reg, unsigned char *p_data, unsigned int n_length);

int tasdevice_dev_bulk_read(struct tasdevice_priv *pPcmdev,
	enum channel chn, unsigned int reg, unsigned char *p_data,
	unsigned int n_length);

int tasdevice_dev_update_bits(
	struct tasdevice_priv *pPcmdev, enum channel chn,
	unsigned int reg, unsigned int mask, unsigned int value);
#endif
