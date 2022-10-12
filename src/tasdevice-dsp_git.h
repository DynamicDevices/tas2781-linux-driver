/* SPDX-License-Identifier: GPL-2.0 */
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#ifndef __TASDEVICE_DSP_GIT_H__
#define __TASDEVICE_DSP_GIT_H__
int fw_parse_variable_header_git(struct tasdevice_priv *tas_dev,
	const struct firmware *pFW, int offset);
int fw_parse_variable_header_cal(struct TFirmware *pCalFirmware,
	const struct firmware *pFW, int offset);
int tasdevice_load_block_git_show(struct tasdevice_priv *pTAS2781,
	struct TBlock *pBlock, char *buf, ssize_t *length);
#endif
