/*
 * TAS2871 Linux Driver
 *
 * Copyright (C) 2022 - 2023 Texas Instruments Incorporated
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

#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#include "tasdevice-regbin.h"
#include "tasdevice-dsp.h"
#include "tasdevice.h"

#define TASDEVICE_MAXPROGRAM_NUM_KERNEL			(5)
#define TASDEVICE_MAXCONFIG_NUM_KERNEL_MULTIPLE_AMPS	(64)
#define TASDEVICE_MAXCONFIG_NUM_KERNEL			(10)
#define MAIN_ALL_DEVICES_1X				(0x01)
#define MAIN_DEVICE_A_1X				(0x02)
#define MAIN_DEVICE_B_1X				(0x03)
#define MAIN_DEVICE_C_1X				(0x04)
#define MAIN_DEVICE_D_1X				(0x05)
#define COEFF_DEVICE_A_1X				(0x12)
#define COEFF_DEVICE_B_1X				(0x13)
#define COEFF_DEVICE_C_1X				(0x14)
#define COEFF_DEVICE_D_1X				(0x15)
#define PRE_DEVICE_A_1X					(0x22)
#define PRE_DEVICE_B_1X					(0x23)
#define PRE_DEVICE_C_1X					(0x24)
#define PRE_DEVICE_D_1X					(0x25)

static int fw_parse_block_data_kernel(struct TFirmware *pFirmware,
	struct TBlock *pBlock, const struct firmware *pFW, int offset)
{
	const unsigned char *pData = pFW->data;

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->mnType = SMS_HTONL(pData[offset],
			pData[offset + 1], pData[offset + 2],
			pData[offset + 3]);
	offset  += 4;

	if (offset + 1 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->mbPChkSumPresent = pData[offset];
	offset++;

	if (offset + 1 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->mnPChkSum = pData[offset];
	offset++;

	if (offset + 1 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->mbYChkSumPresent = pData[offset];
	offset++;

	if (offset + 1 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->mnYChkSum = pData[offset];
	offset++;

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->blk_size = SMS_HTONL(pData[offset],
			pData[offset + 1], pData[offset + 2],
			pData[offset + 3]);
	offset  += 4;

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->nSublocks = SMS_HTONL(pData[offset],
		pData[offset + 1], pData[offset + 2], pData[offset + 3]);
	offset  += 4;

	pBlock->mpData = kzalloc(pBlock->blk_size, GFP_KERNEL);
	if (pBlock->mpData == NULL) {
		pr_err("%s: mpData memory error\n", __func__);
		offset = -1;
		goto out;
	}
	memcpy(pBlock->mpData, &pData[offset], pBlock->blk_size);
	offset  += pBlock->blk_size;
out:
	return offset;
}

static int fw_parse_data_kernel(struct TFirmware *pFirmware,
	struct TData *pImageData, const struct firmware *pFW, int offset)
{
	const unsigned char *pData = pFW->data;
	unsigned int nBlock;

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pImageData->mnBlocks = SMS_HTONL(pData[offset],
		pData[offset + 1], pData[offset + 2],pData[offset + 3]);
	offset  += 4;

	pImageData->mpBlocks =
		kcalloc(pImageData->mnBlocks, sizeof(struct TBlock),
			GFP_KERNEL);
	if (pImageData->mpBlocks == NULL) {
		pr_err("%s: FW memory failed!\n", __func__);
		goto out;
	}

	for (nBlock = 0; nBlock < pImageData->mnBlocks; nBlock++) {
		offset = fw_parse_block_data_kernel(pFirmware,
			&(pImageData->mpBlocks[nBlock]), pFW, offset);
		if (offset < 0) {
			offset = -1;
			goto out;
		}
	}
out:
	return offset;
}

int fw_parse_program_data_kernel(struct TFirmware *pFirmware,
	const struct firmware *pFW, int offset)
{
	struct TProgram *pProgram;
	const unsigned char *buf = pFW->data;
	unsigned int  nProgram = 0;

	for (nProgram = 0; nProgram < pFirmware->mnPrograms; nProgram++) {
		pProgram = &(pFirmware->mpPrograms[nProgram]);
		if (offset + 64 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		memcpy(pProgram->mpName, &buf[offset], 64);
		offset  += 64;

		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pProgram->mnAppMode = buf[offset];
		offset++;

		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pProgram->mnPDMI2SMode = buf[offset];
		offset++;
		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pProgram->mnISnsPD = buf[offset];
		offset++;
		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pProgram->mnVSnsPD = buf[offset];
		offset++;
		//skip 3-byte reserved
		offset  += 3;
		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pProgram->mnPowerLDG = buf[offset];
		offset++;

		offset = fw_parse_data_kernel(pFirmware, &(pProgram->mData),
			pFW, offset);
		if (offset < 0)
			goto out;
	}
out:
	return offset;
}

int fw_parse_configuration_data_kernel(
	struct TFirmware *pFirmware, const struct firmware *pFW, int offset)
{
	const unsigned char *pData = pFW->data;

	unsigned int nConfiguration;
	struct TConfiguration *pConfiguration;

	for (nConfiguration = 0; nConfiguration < pFirmware->mnConfigurations; nConfiguration++) {
		pConfiguration = &(pFirmware->mpConfigurations[nConfiguration]);
		if (offset + 64 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		memcpy(pConfiguration->mpName, &pData[offset], 64);
		offset  += 64;

		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnDevice_orientation = pData[offset];
		offset++;
		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnDevices = pData[offset + 1];
		offset  += 1;

		if (offset + 2 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mProgram = SMS_HTONS(pData[offset], pData[offset + 1]);
		offset  += 2;

		if (offset + 4 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnSamplingRate = SMS_HTONL(pData[offset],
			pData[offset + 1], pData[offset + 2], pData[offset + 3]);
		offset  += 4;

		if (offset + 2 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnPLLSrc = SMS_HTONS(pData[offset], pData[offset + 1]);
		offset  += 2;

		if (offset + 2 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnFsRate = SMS_HTONS(pData[offset], pData[offset + 1]);
		offset  += 2;

		if (offset + 4 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnPLLSrcRate = SMS_HTONL(pData[offset],
			pData[offset + 1], pData[offset + 2], pData[offset + 3]);
		offset  += 4;
		offset = fw_parse_data_kernel(pFirmware, &(pConfiguration->mData), pFW, offset);
		if (offset < 0)
			goto out;
	}
out:
	return offset;
}

int fw_parse_variable_header_kernel(struct tasdevice_priv *tas_dev,
	const struct firmware *pFW, int offset)
{
	struct TFirmware *pFirmware = tas_dev->mpFirmware;
	struct tasdevice_dspfw_hdr *pFw_hdr = &(pFirmware->fw_hdr);
	const unsigned char *buf = pFW->data;
	struct TProgram *pProgram;
	struct TConfiguration *pConfiguration;
	unsigned int  nProgram = 0, nConfiguration = 0;
	unsigned short maxConf = TASDEVICE_MAXCONFIG_NUM_KERNEL;

	if (offset + 2 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_hdr->mnDeviceFamily = SMS_HTONS(buf[offset], buf[offset + 1]);
	if (pFw_hdr->mnDeviceFamily != 0) {
		pr_err("ERROR:%s:not TAS device\n", __func__);
		offset = -1;
		goto out;
	}
	offset  += 2;
	if (offset + 2 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_hdr->mnDevice = SMS_HTONS(buf[offset], buf[offset + 1]);
	if (pFw_hdr->mnDevice >= TASDEVICE_DSP_TAS_MAX_DEVICE ||
		pFw_hdr->mnDevice == 6) {
		pr_err("ERROR:%s: not support device %d\n", __func__,
			pFw_hdr->mnDevice);
		offset = -1;
		goto out;
	}
	offset  += 2;
	pFw_hdr->ndev = deviceNumber[pFw_hdr->mnDevice];

	if (pFw_hdr->ndev != tas_dev->ndev) {
		pr_err("%s: ndev(%u) in dspbin dismatch ndev(%u) in DTS\n",
			__func__, pFw_hdr->ndev, tas_dev->ndev);
		offset = -1;
		goto out;
	}

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFirmware->mnPrograms = SMS_HTONL(buf[offset], buf[offset + 1],
		buf[offset + 2], buf[offset + 3]);
	offset  += 4;

	if (pFirmware->mnPrograms == 0 || pFirmware->mnPrograms >
		TASDEVICE_MAXPROGRAM_NUM_KERNEL) {
		pr_err("%s: mnPrograms is invalid\n", __func__);
		offset = -1;
		goto out;
	}

	if (offset + 4 * TASDEVICE_MAXPROGRAM_NUM_KERNEL > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}

	pFirmware->mpPrograms =
		kcalloc(pFirmware->mnPrograms,
		sizeof(struct TProgram), GFP_KERNEL);
	if (pFirmware->mpPrograms == NULL) {
		pr_err("%s: mpPrograms memory failed!\n", __func__);
		offset = -1;
		goto out;
	}

	for (nProgram = 0; nProgram < pFirmware->mnPrograms; nProgram++) {
		pProgram = &(pFirmware->mpPrograms[nProgram]);
		pProgram->prog_size = SMS_HTONL(buf[offset], buf[offset + 1],
			buf[offset + 2], buf[offset + 3]);
		pFirmware->cfg_start_offset  += pProgram->prog_size;
		offset  += 4;
	}
	offset  += (4 * (5 - nProgram));

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFirmware->mnConfigurations = SMS_HTONL(buf[offset], buf[offset + 1],
		buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	maxConf = (pFw_hdr->ndev >= 4) ?
		TASDEVICE_MAXCONFIG_NUM_KERNEL_MULTIPLE_AMPS :
		TASDEVICE_MAXCONFIG_NUM_KERNEL;
	if (pFirmware->mnConfigurations == 0 ||
		pFirmware->mnConfigurations > maxConf) {
		pr_err("%s: mnConfigurations is invalid\n", __func__);
		offset = -1;
		goto out;
	}

	if (offset + 4 * maxConf > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}

	pFirmware->mpConfigurations = kcalloc(pFirmware->mnConfigurations,
		sizeof(struct TConfiguration), GFP_KERNEL);
	if (pFirmware->mpConfigurations == NULL) {
		pr_err("%s: mpPrograms memory failed!\n", __func__);
		offset = -1;
		goto out;
	}

	for (nConfiguration = 0; nConfiguration < pFirmware->mnPrograms;
		nConfiguration++) {
		pConfiguration =
			&(pFirmware->mpConfigurations[nConfiguration]);
		pConfiguration->cfg_size = SMS_HTONL(buf[offset],
			buf[offset + 1], buf[offset + 2], buf[offset + 3]);
		offset  += 4;
	}

	offset  += (4 * (maxConf - nConfiguration));
	pFirmware->prog_start_offset = offset;
	pFirmware->cfg_start_offset  += offset;
out:
	return offset;
}

int tasdevice_load_block_kernel(struct tasdevice_priv *pTAS2781,
	struct TBlock *pBlock)
{
	int nResult = 0;

	unsigned char *pData = pBlock->mpData;
	unsigned int i = 0, length = 0;
	const unsigned int blk_size = pBlock->blk_size;
	unsigned char dev_idx = 0;
	struct tasdevice_dspfw_hdr *pFw_hdr = &(pTAS2781->mpFirmware->fw_hdr);
	struct tasdevice_fw_fixed_hdr *pFw_fixed_hdr = &(pFw_hdr->mnFixedHdr);

	if (pFw_fixed_hdr->mnPPCVersion >= PPC3_VERSION) {
		switch (pBlock->mnType) {
		case MAIN_ALL_DEVICES_1X:
			dev_idx = 0|0x80;
			break;
		case MAIN_DEVICE_A_1X:
			dev_idx = 1|0x80;
			break;
		case COEFF_DEVICE_A_1X:
		case PRE_DEVICE_A_1X:
			dev_idx = 1|0xC0;
			break;
		case MAIN_DEVICE_B_1X:
			dev_idx = 2|0x80;
			break;
		case COEFF_DEVICE_B_1X:
		case PRE_DEVICE_B_1X:
			dev_idx = 2|0xC0;
			break;
		case MAIN_DEVICE_C_1X:
			dev_idx = 3|0x80;
			break;
		case COEFF_DEVICE_C_1X:
		case PRE_DEVICE_C_1X:
			dev_idx = 3|0xC0;
			break;
		case MAIN_DEVICE_D_1X:
			dev_idx = 4|0x80;
			break;
		case COEFF_DEVICE_D_1X:
		case PRE_DEVICE_D_1X:
			dev_idx = 4|0xC0;
			break;
		default:
			dev_info(pTAS2781->dev, "%s: TAS2781 load block: "
				"Other Type = 0x%02x\n", __func__,
				pBlock->mnType);
			break;
		}
	} else {
		switch (pBlock->mnType) {
		case MAIN_ALL_DEVICES:
			dev_idx = 0|0x80;
			break;
		case MAIN_DEVICE_A:
			dev_idx = 1|0x80;
			break;
		case COEFF_DEVICE_A:
		case PRE_DEVICE_A:
			dev_idx = 1|0xC0;
			break;
		case MAIN_DEVICE_B:
			dev_idx = 2|0x80;
			break;
		case COEFF_DEVICE_B:
		case PRE_DEVICE_B:
			dev_idx = 2|0xC0;
			break;
		case MAIN_DEVICE_C:
			dev_idx = 3|0x80;
			break;
		case COEFF_DEVICE_C:
		case PRE_DEVICE_C:
			dev_idx = 3|0xC0;
			break;
		case MAIN_DEVICE_D:
			dev_idx = 4|0x80;
			break;
		case COEFF_DEVICE_D:
		case PRE_DEVICE_D:
			dev_idx = 4|0xC0;
			break;
		default:
			dev_info(pTAS2781->dev, "%s: TAS2781 load block: "
				"Other Type = 0x%02x\n", __func__,
				pBlock->mnType);
			break;
		}
	}

	for (i = 0; i < pBlock->nSublocks; i++) {
		int rc = tasdevice_process_block(pTAS2781, pData + length,
			dev_idx, blk_size - length);
		if (rc < 0) {
			dev_err(pTAS2781->dev, "%s: ERROR:%u %u sublock write "
				"error\n", __func__, length, blk_size);
			break;
		}
		length  += (unsigned int)rc;
		if (blk_size < length) {
			dev_err(pTAS2781->dev, "%s: ERROR:%u %u out of "
				"memory\n", __func__, length, blk_size);
			break;
		}
	}

	return nResult;
}
