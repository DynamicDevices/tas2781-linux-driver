/* SPDX-License-Identifier: GPL-2.0 */
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#ifndef __TASDEVICE_REGBIN_H__
#define __TASDEVICE_REGBIN_H__

#define TASDEVICE_CONFIG_SUM  (64)
#define TASDEVICE_DEVICE_SUM  (8)

#define TASDEVICE_CMD_SING_W  (0x1)
#define TASDEVICE_CMD_BURST  (0x2)
#define TASDEVICE_CMD_DELAY  (0x3)
#define TASDEVICE_CMD_FIELD_W  (0x4)

enum tasdevice_dsp_fw_state {
	TASDEVICE_DSP_FW_NONE = 0,
	TASDEVICE_DSP_FW_PENDING,
	TASDEVICE_DSP_FW_FAIL,
	TASDEVICE_DSP_FW_ALL_OK,
};

enum tasdevice_bin_blk_type {
	TASDEVICE_BIN_BLK_COEFF = 1,
	TASDEVICE_BIN_BLK_POST_POWER_UP,
	TASDEVICE_BIN_BLK_PRE_SHUTDOWN,
	TASDEVICE_BIN_BLK_PRE_POWER_UP,
	TASDEVICE_BIN_BLK_POST_SHUTDOWN
};

struct tasdevice_regbin_hdr {
	unsigned int img_sz;
	unsigned int checksum;
	unsigned int binary_version_num;
	unsigned int drv_fw_version;
	unsigned int timestamp;
	unsigned char plat_type;
	unsigned char dev_family;
	unsigned char reserve;
	unsigned char ndev;
	unsigned char devs[TASDEVICE_DEVICE_SUM];
	unsigned int nconfig;
	unsigned int config_size[TASDEVICE_CONFIG_SUM];
};

struct tasdevice_block_data {
	unsigned char dev_idx;
	unsigned char block_type;
	unsigned short yram_checksum;
	unsigned int block_size;
	unsigned int nSublocks;
	unsigned char *regdata;
};

struct tasdevice_config_info {
	char mpName[64];
	unsigned int nblocks;
	unsigned int real_nblocks;
	unsigned char active_dev;
	struct tasdevice_block_data **blk_data;
};

struct tasdevice_regbin {
	struct tasdevice_regbin_hdr fw_hdr;
	int ncfgs;
	struct tasdevice_config_info **cfg_info;
	int profile_cfg_id;
};

void tasdevice_regbin_ready(const struct firmware *pFW,
	void *pContext);
void tasdevice_config_info_remove(void *pContext);
void tasdevice_powerup_regcfg_dev(void *pContext,
	unsigned char dev);
void tasdevice_select_cfg_blk(void *pContext, int conf_no,
	unsigned char block_type);
int tasdevice_process_block(void *pContext,
	unsigned char *data, unsigned char dev_idx, int sublocksize);
int tasdevice_process_block_show(void *pContext,
	unsigned char *data, unsigned char dev_idx, int sublocksize,
	char *buf, ssize_t *len);
#endif
