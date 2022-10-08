// SPDX-License-Identifier: GPL-2.0 
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>

#include "tasdevice-dsp.h"
#include "tasdevice-regbin.h"
#include "tasdevice.h"
#include "tasdevice-codec.h"
#include "tasdevice-rw.h"

const char *blocktype[5] = {
	"COEFF",
	"POST_POWER_UP",
	"PRE_SHUTDOWN",
	"PRE_POWER_UP",
	"POST_SHUTDOWN"
};

int tasdevice_process_block(void *pContext,
	unsigned char *data, unsigned char dev_idx, int sublocksize)
{
	struct tasdevice_priv *tas_dev =
		(struct tasdevice_priv *)pContext;
	unsigned char subblk_typ = data[1];
	int subblk_offset = 2;
	int chn = 0, chnend = 0;
	int rc = 0;
	int blktyp = dev_idx & 0xC0, idx = dev_idx & 0x3F;
	bool bError = false;

	if (idx) {
		chn = idx-1;
		chnend = idx;
	} else {
		chn = 0;
		chnend = tas_dev->ndev;
	}

	for (; chn < chnend; chn++) {
		if (tas_dev->tasdevice[chn].bLoading == false)
			continue;

		bError = false;
		subblk_offset = 2;
		switch (subblk_typ) {
		case TASDEVICE_CMD_SING_W: {
			int i = 0;
			unsigned short len = SMS_HTONS(data[2], data[3]);

			subblk_offset  += 2;
			if (subblk_offset + 4 * len > sublocksize) {
				dev_err(tas_dev->dev,
					"process_block: Out of memory\n");
				bError = true;
				break;
			}

			for (i = 0; i < len; i++) {
				rc = tasdevice_dev_write(tas_dev, chn,
					TASDEVICE_REG(data[subblk_offset],
						data[subblk_offset + 1],
						data[subblk_offset + 2]),
					data[subblk_offset + 3]);
				if (rc < 0) {
					bError = true;
					dev_err(tas_dev->dev,
						"process_block: single write error\n");
				}
				subblk_offset  += 4;
			}
		}
			break;
		case TASDEVICE_CMD_BURST: {
			unsigned short len = SMS_HTONS(data[2], data[3]);

			subblk_offset  += 2;
			if (subblk_offset + 4 + len > sublocksize) {
				dev_err(tas_dev->dev,
					"process_block: BURST Out of memory\n");
				bError = true;
				break;
			}
			if (len % 4) {
				dev_err(tas_dev->dev, "process_block: Burst "
					"len(%u) can be divided by 4\n", len);
				break;
			}

			rc = tasdevice_dev_bulk_write(tas_dev, chn,
				TASDEVICE_REG(data[subblk_offset],
					data[subblk_offset + 1],
					data[subblk_offset + 2]),
					&(data[subblk_offset + 4]), len);
			if (rc < 0) {
				bError = true;
				dev_err(tas_dev->dev,
					"process_block: bulk_write error = %d\n",
					rc);
			}
			subblk_offset  += (len + 4);
		}
			break;
		case TASDEVICE_CMD_DELAY: {
			unsigned short delay_time = 0;

			if (subblk_offset + 2 > sublocksize) {
				dev_err(tas_dev->dev,
					"process_block: deley Out of memory\n");
				bError = true;
				break;
			}
			delay_time = SMS_HTONS(data[2], data[3]);
			usleep_range(delay_time*1000, delay_time*1000);
			subblk_offset  += 2;
		}
			break;
		case TASDEVICE_CMD_FIELD_W:
		if (subblk_offset + 6 > sublocksize) {
			dev_err(tas_dev->dev,
				"process_block: bit write Out of memory\n");
			bError = true;
			break;
		}
		rc = tasdevice_dev_update_bits(tas_dev, chn,
			TASDEVICE_REG(data[subblk_offset + 2],
				data[subblk_offset + 3],
				data[subblk_offset + 4]),
				data[subblk_offset + 1],
				data[subblk_offset + 5]);
		if (rc < 0) {
			bError = true;
			dev_err(tas_dev->dev,
				"process_block: update_bits error = %d\n", rc);
		}
		subblk_offset  += 6;
			break;
		default:
			break;
		};
		if (bError == true && blktyp != 0) {
			if (blktyp == 0x80) {
				tas_dev->tasdevice[chn].mnCurrentProgram = -1;
				tas_dev->tasdevice[chn].mnCurrentConfiguration = -1;
			} else {
				tas_dev->tasdevice[chn].mnCurrentConfiguration = -1;
			}
		}
	}
	return subblk_offset;
}

int tasdevice_process_block_show(void *pContext,
	unsigned char *data, unsigned char dev_idx,
	int sublocksize, char *buf, ssize_t *length)
{
	struct tasdevice_priv *tas_dev =
		(struct tasdevice_priv *)pContext;
	unsigned char subblk_typ = data[1];
	int subblk_offset = 2;
	int chn = 0, chnend = 0;

	if (dev_idx) {
		chn = dev_idx-1;
		chnend = dev_idx;
	} else {
		chn = 0;
		chnend = tas_dev->ndev;
	}

	for (; chn < chnend; chn++) {
		subblk_offset = 2;
		switch (subblk_typ) {
		case TASDEVICE_CMD_SING_W: {
			int i = 0;
			unsigned short len = SMS_HTONS(data[2], data[3]);

			subblk_offset  += 2;
			if (*length + 16 < PAGE_SIZE) {
				*length  += scnprintf(buf + *length,
					PAGE_SIZE - *length,
					"\t\tSINGLE BYTE:\n");
			} else {
				*length  += scnprintf(buf + PAGE_SIZE - 16,
					16, "\nNo memory!\n");
				break;
			}
			if (subblk_offset + 4 * len > sublocksize) {
				if (*length + 32 < PAGE_SIZE) {
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"CMD_SING_W: Out of memory\n");
				} else
					*length  += scnprintf(buf + PAGE_SIZE - 16,
						16, "\nNo memory!\n");
				break;
			}

			for (i = 0; i < len; i++) {
				if (*length + 64 < PAGE_SIZE) {
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"\t\t\tBOOK0x%02x PAGE0x%02x REG0x%02x "
						"VALUE = 0x%02x\n",
						data[subblk_offset],
						data[subblk_offset + 1],
						data[subblk_offset + 2],
						data[subblk_offset + 3]);
					subblk_offset  += 4;
				} else {
					*length  += scnprintf(buf  +
					PAGE_SIZE - 16,
						16, "\nNo memory!\n");
					break;
				}
			}
		}
			break;
		case TASDEVICE_CMD_BURST: {
			int i = 0;
			unsigned short len = SMS_HTONS(data[2], data[3]);
			unsigned char reg = 0;

			subblk_offset  += 2;
			if (*length + 16 < PAGE_SIZE) {
				*length  += scnprintf(buf + *length,
				PAGE_SIZE - *length,
					"\t\tBURST:\n");
			} else {
				*length  += scnprintf(buf + PAGE_SIZE - 16,
						16, "\nNo memory!\n");
				break;
			}
			if (subblk_offset + 4 + len > sublocksize) {
				if (*length + 32 < PAGE_SIZE) {
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"CMD_BURST: Out of memory.\n");
				} else
					*length  += scnprintf(buf + PAGE_SIZE - 16,
						16, "\nNo memory!\n");
				break;
			}
			if (len % 4) {
				if (*length + 32 < PAGE_SIZE) {
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"CMD_BURST: Burst len is wrong\n");
				} else
					*length  += scnprintf(buf  +
					PAGE_SIZE - 16,
						16, "\nNo memory!\n");
				break;
			}
			reg = data[subblk_offset + 2];
			if (*length + 32 < PAGE_SIZE) {
				*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
					"\t\t\tBOOK0x%02x PAGE0x%02x\n",
					data[subblk_offset], data[subblk_offset + 1]);
			} else {
				*length  += scnprintf(buf + PAGE_SIZE - 16,
						16, "\nNo memory!\n");
				break;
			}
			subblk_offset  += 4;
			for (i = 0; i < len / 4; i++) {
				if (*length + 128 < PAGE_SIZE) {
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"\t\t\tREG0x%02x = 0x%02x REG0x%02x = 0x%02x "
						"REG0x%02x = 0x%02x REG0x%02x = 0x%02x\n",
						reg + i * 4, data[subblk_offset + 0],
						reg + i * 4 + 1, data[subblk_offset + 1],
						reg + i * 4 + 2, data[subblk_offset + 2],
						reg + i * 4 + 3, data[subblk_offset + 3]);
				} else {
					*length  += scnprintf(buf + PAGE_SIZE - 16,
							16, "\nNo memory!\n");
					break;
				}
				subblk_offset  += 4;
			}
		}
			break;
		case TASDEVICE_CMD_DELAY: {
			unsigned short delay_time = 0;

			if (subblk_offset + 2 > sublocksize) {
				if (*length + 32 < PAGE_SIZE) {
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"CMD_DELAY: Out of memory\n");
				} else
					*length  += scnprintf(buf + PAGE_SIZE - 16,
						16, "\nNo memory!\n");
				break;
			}
			delay_time = SMS_HTONS(data[2], data[3]);
			if (*length + 32 < PAGE_SIZE) {
				*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
					"\t\tDELAY = %ums\n", delay_time);
			} else {
				*length  += scnprintf(buf + PAGE_SIZE - 16,
					16, "\nNo memory!\n");
				break;
			}
			subblk_offset  += 2;
		}
			break;
		case TASDEVICE_CMD_FIELD_W:
		if (subblk_offset + 6 > sublocksize) {
			if (*length + 32 < PAGE_SIZE) {
				*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
					"FIELD_W: Out of memory\n");
			} else
				*length  += scnprintf(buf + PAGE_SIZE - 16,
					16, "\nNo memory!\n");
			break;
		}
		if (*length + 32 < PAGE_SIZE) {
			*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
				"\t\tFIELD:\n");
		} else {
			*length  += scnprintf(buf + PAGE_SIZE - 16,
				16, "\nNo memory!\n");
			break;
		}
		if (*length + 64 < PAGE_SIZE) {
			*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
				"\t\t\tBOOK0x%02x PAGE0x%02x REG0x%02x MASK0x%02x "
				"VALUE = 0x%02x\n",
				data[subblk_offset + 2], data[subblk_offset + 3],
				data[subblk_offset + 4], data[subblk_offset + 1],
				data[subblk_offset + 5]);
		} else {
			*length  += scnprintf(buf + PAGE_SIZE - 16,
				16, "\nNo memory!\n");
			break;
		}
		subblk_offset  += 6;
			break;
		default:
			break;
		};
	}
	return subblk_offset;
}

void tasdevice_powerup_regcfg_dev(void *pContext, unsigned char dev)
{
	struct tasdevice_priv *p_tasdevice = (struct tasdevice_priv *) pContext;
	struct tasdevice_regbin *regbin = &(p_tasdevice->mtRegbin);
	struct tasdevice_config_info **cfg_info = regbin->cfg_info;
	int j = 0, k = 0, conf_no = 0;

	if (dev >= p_tasdevice->ndev) {
		pr_err("%s:%u: dev:%d\n", __func__, __LINE__, dev);
		goto out;
	}

	if (0 == p_tasdevice->tasdevice[dev].PowerStatus) {
		pr_info("%s:%u:dev %d is power off\n",
			__func__, __LINE__, dev);
		goto out;
	}

	conf_no = (int)p_tasdevice->tasdevice[dev].mnCurrentRegConf;

	if (conf_no >= regbin->ncfgs || conf_no < 0 || NULL == cfg_info) {
		pr_err("conf_no should be in range from 0 to %d\n",
			regbin->ncfgs-1);
		goto out;
	} else {
		pr_info("%s:%u:profile_conf_id = %d\n", __func__, __LINE__,
			conf_no);
	}

	for (k = 0; k < p_tasdevice->ndev; k++) {
		p_tasdevice->tasdevice[k].bLoading = false;
		p_tasdevice->tasdevice[k].bLoaderr = false;
	}

	for (j = 0; j < (int)cfg_info[conf_no]->real_nblocks; j++) {
		unsigned int length = 0, rc = 0;

		if (TASDEVICE_BIN_BLK_PRE_POWER_UP !=
			cfg_info[conf_no]->blk_data[j]->block_type)
			continue;
		pr_info("%s:%u:conf %d\n", __func__, __LINE__, conf_no);
		pr_info("%s:%u:block type:%s\t device idx = 0x%02x\n",
			__func__, __LINE__,
			blocktype[cfg_info[conf_no]->blk_data[j]->block_type-1],
			cfg_info[conf_no]->blk_data[j]->dev_idx);
		if ((cfg_info[conf_no]->blk_data[j]->dev_idx != 0)
			&& (cfg_info[conf_no]->blk_data[j]->dev_idx-1 != dev)) {
			pr_info("%s:%u:No device %u in conf %d\n", __func__, __LINE__,
				dev, conf_no);
			goto out;
		}

		p_tasdevice->tasdevice[dev].bLoading = true;
		for (k = 0; k < (int)cfg_info[conf_no]->blk_data[j]->nSublocks; k++) {
			rc = tasdevice_process_block(p_tasdevice,
				cfg_info[conf_no]->blk_data[j]->regdata + length,
				dev + 1,
				cfg_info[conf_no]->blk_data[j]->block_size - length);
			length  += rc;
			if (cfg_info[conf_no]->blk_data[j]->block_size < length) {
				pr_err("%s:%u:ERROR:%u %u out of memory\n",
					__func__, __LINE__,
					length,
					cfg_info[conf_no]->blk_data[j]->block_size);
				break;
			}
		}
		if (length != cfg_info[conf_no]->blk_data[j]->block_size) {
			pr_err("%s:%u:ERROR: %u %u size is not same\n",
				__func__, __LINE__, length,
				cfg_info[conf_no]->blk_data[j]->block_size);
		}
	}
out:
	return;
}

void tasdevice_select_cfg_blk(void *pContext, int conf_no,
	unsigned char block_type)
{
	struct tasdevice_priv *tas_dev =
		(struct tasdevice_priv *) pContext;
	struct tasdevice_regbin *regbin = &(tas_dev->mtRegbin);
	struct tasdevice_config_info **cfg_info = regbin->cfg_info;
	int j = 0, k = 0, chn = 0, chnend = 0;

	dev_err(tas_dev->dev, "%s, enter\n", __func__);
	if (conf_no >= regbin->ncfgs || conf_no < 0 || NULL == cfg_info) {
		dev_err(tas_dev->dev,
			"conf_no should be not more than %u\n",
			regbin->ncfgs);
		goto out;
	} else {
		dev_info(tas_dev->dev,
			"select_cfg_blk: profile_conf_id = %d\n",
			conf_no);
	}
//TODO:INTR
	for (j = 0; j < (int)cfg_info[conf_no]->real_nblocks; j++) {
		unsigned int length = 0, rc = 0;

		if (block_type > 5 || block_type < 2) {
			dev_err(tas_dev->dev,
				"ERROR!!!block_type should be in range from 2 to 5\n");
			goto out;
		}
		if (block_type != cfg_info[conf_no]->blk_data[j]->block_type)
			continue;
		dev_info(tas_dev->dev, "select_cfg_blk: conf %d, "
			"block type:%s\t device idx = 0x%02x\n",
			conf_no, blocktype[cfg_info[conf_no]->blk_data[j]
			->block_type-1], cfg_info[conf_no]->blk_data[j]
			->dev_idx);

		for (k = 0; k < (int)cfg_info[conf_no]->blk_data[j]
			->nSublocks; k++) {
			if (cfg_info[conf_no]->blk_data[j]->dev_idx) {
				chn = cfg_info[conf_no]->blk_data[j]->dev_idx-1;
				chnend = cfg_info[conf_no]->blk_data[j]->dev_idx;
			} else {
				chn = 0;
				chnend = tas_dev->ndev;
			}
			for (; chn < chnend; chn++) {
				tas_dev->tasdevice[chn].bLoading = true;
			}
			rc = tasdevice_process_block(tas_dev,
				cfg_info[conf_no]->blk_data[j]->regdata + length,
				cfg_info[conf_no]->blk_data[j]->dev_idx,
				cfg_info[conf_no]->blk_data[j]->block_size - length);
			length  += rc;
			if (cfg_info[conf_no]->blk_data[j]->block_size < length) {
				dev_err(tas_dev->dev, "select_cfg_blk: "
					"ERROR:%u %u out of memory\n", length,
					cfg_info[conf_no]->blk_data[j]->block_size);
				break;
			}
		}
		if (length != cfg_info[conf_no]->blk_data[j]->block_size) {
			dev_err(tas_dev->dev,
				"select_cfg_blk: ERROR: %u %u size is not same\n",
				length, cfg_info[conf_no]->blk_data[j]->block_size);
		}
	}
//TODO: INTR
out:
	return;
}

static struct tasdevice_config_info *tasdevice_add_config(
	void *pContext, unsigned char *config_data,
	unsigned int config_size)
{
	struct tasdevice_priv *tas_dev =
		(struct tasdevice_priv *)pContext;
	struct tasdevice_config_info *cfg_info = NULL;
	int config_offset = 0, i = 0;

	cfg_info = kzalloc(
			sizeof(struct tasdevice_config_info), GFP_KERNEL);
	if (!cfg_info) {
		dev_err(tas_dev->dev,
			"add config: cfg_info alloc failed!\n");
		goto out;
	}

	if (tas_dev->mtRegbin.fw_hdr.binary_version_num >= 0x105) {
		if (config_offset + 64 > (int)config_size) {
			dev_err(tas_dev->dev,
				"add config: Out of memory\n");
			goto out;
		}
		memcpy(cfg_info->mpName, &config_data[config_offset], 64);
		config_offset  += 64;
	}

	if (config_offset + 4 > (int)config_size) {
		dev_err(tas_dev->dev,
			"add config: Out of memory\n");
		goto out;
	}
	cfg_info->nblocks =
		SMS_HTONL(config_data[config_offset],
		config_data[config_offset + 1],
	config_data[config_offset + 2], config_data[config_offset + 3]);
	config_offset  +=  4;

	cfg_info->blk_data = kcalloc(
		cfg_info->nblocks, sizeof(struct tasdevice_block_data *),
		GFP_KERNEL);
	if (!cfg_info->blk_data) {
		dev_err(tas_dev->dev,
			"add config: blk_data alloc failed!\n");
		goto out;
	}
	cfg_info->real_nblocks = 0;
	for (i = 0; i < (int)cfg_info->nblocks; i++) {
		if (config_offset + 12 > config_size) {
			dev_err(tas_dev->dev,
				"add config: Out of memory: i = %d nblocks = %u!\n",
				i, cfg_info->nblocks);
			break;
		}
		cfg_info->blk_data[i] = kzalloc(
			sizeof(struct tasdevice_block_data), GFP_KERNEL);
		if (!cfg_info->blk_data[i]) {
			dev_err(tas_dev->dev,
				"add config: blk_data[%d] alloc failed!\n", i);
			break;
		}
		cfg_info->blk_data[i]->dev_idx = config_data[config_offset];
		config_offset++;

		cfg_info->blk_data[i]->block_type = config_data[config_offset];
		config_offset++;

		if (cfg_info->blk_data[i]->block_type  ==
			TASDEVICE_BIN_BLK_PRE_POWER_UP) {
			if (0 == cfg_info->blk_data[i]->dev_idx) {
				cfg_info->active_dev = 1;
			} else {
				cfg_info->active_dev =
					(1 << (cfg_info->blk_data[i]->dev_idx-1));
			}
		}
		cfg_info->blk_data[i]->yram_checksum =
			SMS_HTONS(config_data[config_offset],
			config_data[config_offset + 1]);
		config_offset  += 2;
		cfg_info->blk_data[i]->block_size =
			SMS_HTONL(config_data[config_offset],
			config_data[config_offset + 1],
			config_data[config_offset + 2],
		config_data[config_offset + 3]);
		config_offset  += 4;

		cfg_info->blk_data[i]->nSublocks =
			SMS_HTONL(config_data[config_offset],
			config_data[config_offset + 1],
			config_data[config_offset + 2],
		config_data[config_offset + 3]);

		config_offset  += 4;
		cfg_info->blk_data[i]->regdata = kzalloc(
			cfg_info->blk_data[i]->block_size, GFP_KERNEL);
		if (!cfg_info->blk_data[i]->regdata) {
			dev_err(tas_dev->dev,
				"add config: regdata alloc failed!\n");
			goto out;
		}
		if (config_offset + cfg_info->blk_data[i]->block_size
			> config_size) {
			dev_err(tas_dev->dev,
				"add config: block_size Out of memory: "
				"i = %d nblocks = %u!\n", i, cfg_info->nblocks);
			break;
		}
		memcpy(cfg_info->blk_data[i]->regdata,
			&config_data[config_offset],
		cfg_info->blk_data[i]->block_size);
		config_offset  += cfg_info->blk_data[i]->block_size;
		cfg_info->real_nblocks  += 1;
	}
out:
	return cfg_info;
}

void tasdevice_regbin_ready(const struct firmware *pFW,
	void *pContext)
{
	struct tasdevice_priv *tas_dev =
		(struct tasdevice_priv *) pContext;
	struct tasdevice_regbin *regbin = NULL;
	struct tasdevice_regbin_hdr *fw_hdr = NULL;
	struct tasdevice_config_info **cfg_info = NULL;
	const struct firmware *fw_entry = NULL;
	unsigned char *buf = NULL;
	int offset = 0, i = 0;
	unsigned int total_config_sz = 0;
	int ret = 0;

	if (tas_dev == NULL) {
		dev_err(tas_dev->dev,
			"tasdev: regbin_ready: handle is NULL\n");
		return;
	}
	mutex_lock(&tas_dev->codec_lock);
	regbin = &(tas_dev->mtRegbin);
	fw_hdr = &(regbin->fw_hdr);
	if (unlikely(!pFW) || unlikely(!pFW->data)) {
		dev_err(tas_dev->dev,
			"Failed to read %s, no side - effect on driver running\n",
			tas_dev->regbin_binaryname);
		ret = -1;
		goto out;
	}
	buf = (unsigned char *)pFW->data;//����buf

	dev_info(tas_dev->dev, "tasdev: regbin_ready start\n");
	fw_hdr->img_sz = SMS_HTONL(buf[offset], buf[offset + 1],
		buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	if (fw_hdr->img_sz != pFW->size) {
		dev_err(tas_dev->dev,
			"File size not match, %d %u", (int)pFW->size,
			fw_hdr->img_sz);
		ret = -1;
		goto out;
	}

	fw_hdr->checksum = SMS_HTONL(buf[offset], buf[offset + 1],
					buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	fw_hdr->binary_version_num = SMS_HTONL(buf[offset],
		buf[offset + 1], buf[offset + 2], buf[offset + 3]);
	if (fw_hdr->binary_version_num < 0x103) {
		dev_err(tas_dev->dev,
			"File version 0x%04x is too low",
			fw_hdr->binary_version_num);
		ret = -1;
		goto out;
	}
	offset  += 4;
	fw_hdr->drv_fw_version = SMS_HTONL(buf[offset], buf[offset + 1],
					buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	fw_hdr->timestamp = SMS_HTONL(buf[offset], buf[offset + 1],
					buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	fw_hdr->plat_type = buf[offset];
	offset  += 1;
	fw_hdr->dev_family = buf[offset];
	offset  += 1;
	fw_hdr->reserve = buf[offset];
	offset  += 1;
	fw_hdr->ndev = buf[offset];
	offset  += 1;
	if (fw_hdr->ndev != tas_dev->ndev) {
		dev_err(tas_dev->dev, "ndev(%u) from Regbin and ndev(%u)"
			"from DTS does not match\n", fw_hdr->ndev,
			tas_dev->ndev);
		ret = -1;
		goto out;
	}
	if (offset + TASDEVICE_DEVICE_SUM > fw_hdr->img_sz) {
		dev_err(tas_dev->dev,
			"regbin_ready: Out of Memory!\n");
		ret = -1;
		goto out;
	}

	for (i = 0; i < TASDEVICE_DEVICE_SUM; i++, offset++) {
		fw_hdr->devs[i] = buf[offset];
	}
	fw_hdr->nconfig = SMS_HTONL(buf[offset], buf[offset + 1],
				buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	dev_info(tas_dev->dev, "nconfig = %u\n", fw_hdr->nconfig);
	for (i = 0; i < TASDEVICE_CONFIG_SUM; i++) {
		fw_hdr->config_size[i] = SMS_HTONL(buf[offset],
			buf[offset + 1], buf[offset + 2], buf[offset + 3]);
		offset  += 4;
		total_config_sz  += fw_hdr->config_size[i];
	}
	dev_info(tas_dev->dev,
		"img_sz = %u total_config_sz = %u offset = %d\n",
		fw_hdr->img_sz, total_config_sz, offset);
	if (fw_hdr->img_sz - total_config_sz != (unsigned int)offset) {
		dev_err(tas_dev->dev, "Bin file error!\n");
		ret = -1;
		goto out;
	}
	cfg_info = kcalloc(fw_hdr->nconfig,
		sizeof(struct tasdevice_config_info *),
		GFP_KERNEL);

	if (!cfg_info) {
		ret = -1;
		dev_err(tas_dev->dev, "nconfig Memory alloc failed!\n");
		goto out;
	}
	regbin->cfg_info = cfg_info;
	regbin->ncfgs = 0;
	for (i = 0; i < (int)fw_hdr->nconfig; i++) {
		cfg_info[i] = tasdevice_add_config(pContext, &buf[offset],
				fw_hdr->config_size[i]);
		if (!cfg_info[i]) {
			ret = -1;
			dev_err(tas_dev->dev,
				"add_config Memory alloc failed!\n");
			break;
		}
		offset  += (int)fw_hdr->config_size[i];
		regbin->ncfgs  += 1;
	}
	tasdevice_create_controls(tas_dev);
	
	tasdevice_dsp_remove(tas_dev);
	tasdevice_calbin_remove(tas_dev);
	tas_dev->fw_state = TASDEVICE_DSP_FW_PENDING;
	scnprintf(tas_dev->dsp_binaryname, 64, "%s_dsp.bin",
		tas_dev->dev_name);
	ret = request_firmware(&fw_entry, tas_dev->dsp_binaryname, tas_dev->dev);
	if (!ret) {
		ret = tasdevice_dspfw_ready(fw_entry, tas_dev);
		release_firmware(fw_entry);
		fw_entry = NULL;
	} else {
		tas_dev->fw_state = TASDEVICE_DSP_FW_FAIL;
		dev_err(tas_dev->dev, "%s: load %s error\n", __func__,
			tas_dev->dsp_binaryname);
		goto out;
	}
	tasdevice_dsp_create_control(tas_dev);

	tas_dev->fw_state = TASDEVICE_DSP_FW_ALL_OK;

	for (i = 0; i < tas_dev->ndev; i++) {
		scnprintf(tas_dev->cal_binaryname[i], 64, "%s_cal_%02X.bin",
			tas_dev->dev_name, tas_dev->tasdevice[i].mnDevAddr);
		ret = tas2781_load_calibration(tas_dev, tas_dev->cal_binaryname[i], i);
		if (ret != 0) {
			dev_err(tas_dev->dev, "%s: load %s error, no-side effect for playback\n",
				__func__, tas_dev->cal_binaryname[i]);
			ret = 0;
		}
	}
out:
	mutex_unlock(&tas_dev->codec_lock);
	if (pFW)
		release_firmware(pFW);
	dev_info(tas_dev->dev, "Firmware init complete\n");
}

void tasdevice_config_info_remove(void *pContext)
{
	struct tasdevice_priv *tas_dev =
		(struct tasdevice_priv *) pContext;
	struct tasdevice_regbin *regbin = &(tas_dev->mtRegbin);
	struct tasdevice_config_info **cfg_info = regbin->cfg_info;
	int i = 0, j = 0;

	mutex_lock(&tas_dev->dev_lock);
	if (cfg_info) {
		for (i = 0; i < regbin->ncfgs; i++) {
			if (cfg_info[i]) {
				for (j = 0; j < (int)cfg_info[i]->real_nblocks;
					j++) {
					kfree(cfg_info[i]->blk_data[j]->regdata);
					kfree(cfg_info[i]->blk_data[j]);
				}
				kfree(cfg_info[i]->blk_data);
				kfree(cfg_info[i]);
			}
		}
		kfree(cfg_info);
	}
	mutex_unlock(&tas_dev->dev_lock);
}