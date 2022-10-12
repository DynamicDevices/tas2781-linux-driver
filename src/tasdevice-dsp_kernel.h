/* SPDX-License-Identifier: GPL-2.0 */
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#ifndef __TASDEVICE_DSP_KERNEL_H__
#define __TASDEVICE_DSP_KERNEL_H__

int fw_parse_variable_header_kernel(struct tasdevice_priv *tas_dev,
	const struct firmware *pFW, int offset);
int fw_parse_program_data_kernel(struct TFirmware *pFirmware,
	const struct firmware *pFW, int offset);
int fw_parse_configuration_data_kernel(
	struct TFirmware *pFirmware, const struct firmware *pFW, int offset);
int tasdevice_load_block_kernel(struct tasdevice_priv *pTAS2781,
	struct TBlock *pBlock);
#endif
