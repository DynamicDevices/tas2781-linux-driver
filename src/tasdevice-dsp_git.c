/*
 * TAS2871 Linux Driver
 *
 * Copyright (C) 2022 Texas Instruments Incorporated 
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

#include <linux/miscdevice.h>
#include <linux/firmware.h>

#include "tasdevice-dsp.h"
#include "tasdevice-regbin.h"
#include "tasdevice.h"

const char deviceNumber[TASDEVICE_DSP_TAS_MAX_DEVICE] = {
	1, 2, 1, 2, 1, 1, 0, 2, 4, 3, 1, 2, 3, 4
};

int fw_parse_variable_header_git(struct tasdevice_priv *tas_dev,
	const struct firmware *pFW, int offset)
{
	const unsigned char *buf = pFW->data;
	struct TFirmware *pFirmware = tas_dev->mpFirmware;
	struct tasdevice_dspfw_hdr *pFw_hdr = &(pFirmware->fw_hdr);
	int i = strlen((char *)&buf[offset]);

	i++;

	if (offset + i > pFW->size) {
		dev_err(tas_dev->dev, "%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}

	pFw_hdr->mpDescription = kmemdup(&buf[offset], i, GFP_KERNEL);
	if (pFw_hdr->mpDescription == NULL) {
		dev_err(tas_dev->dev, "%s: mpDescription error!\n", __func__);
		goto out;
	}
	offset  += i;

	if (offset + 4 > pFW->size) {
		dev_err(tas_dev->dev, "%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_hdr->mnDeviceFamily = SMS_HTONL(buf[offset], buf[offset + 1],
		buf[offset + 2], buf[offset + 3]);
	if (pFw_hdr->mnDeviceFamily != 0) {
		dev_err(tas_dev->dev, "ERROR:%s: not TAS device\n", __func__);
		offset = -1;
		goto out;
	}
	offset  += 4;
	if (offset + 4 > pFW->size) {
		dev_err(tas_dev->dev, "%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_hdr->mnDevice = SMS_HTONL(buf[offset], buf[offset + 1],
		buf[offset + 2], buf[offset + 3]);
	if (pFw_hdr->mnDevice >= TASDEVICE_DSP_TAS_MAX_DEVICE ||
		pFw_hdr->mnDevice == 6) {
		dev_err(tas_dev->dev, "ERROR:%s: not support device %d\n",
			__func__, pFw_hdr->mnDevice);
		offset = -1;
		goto out;
	}
	offset  += 4;
	pFw_hdr->ndev = deviceNumber[pFw_hdr->mnDevice];
	if (pFw_hdr->ndev != tas_dev->ndev) {
		dev_err(tas_dev->dev, "%s: ndev(%u) in dspbin dismatch "
			"ndev(%u) in DTS\n", __func__, pFw_hdr->ndev,
			tas_dev->ndev);
		offset = -1;
	}

out:
	return offset;
}

int fw_parse_variable_header_cal(struct TFirmware *pCalFirmware,
	const struct firmware *pFW, int offset)
{
	const unsigned char *buf = pFW->data;
	struct tasdevice_dspfw_hdr *pFw_hdr = &(pCalFirmware->fw_hdr);
	int i = strlen((char *)&buf[offset]);

	i++;

	if (offset + i > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}

	pFw_hdr->mpDescription = kmemdup(&buf[offset], i, GFP_KERNEL);
	if (pFw_hdr->mpDescription == NULL) {
		pr_err("%s: mpDescription error!\n", __func__);
		goto out;
	}
	offset  += i;

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_hdr->mnDeviceFamily = SMS_HTONL(buf[offset], buf[offset + 1],
		buf[offset + 2], buf[offset + 3]);
	if (pFw_hdr->mnDeviceFamily != 0) {
		pr_err("ERROR:%s: not TAS device\n", __func__);
		offset = -1;
		goto out;
	}
	offset  += 4;
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_hdr->mnDevice = SMS_HTONL(buf[offset], buf[offset + 1],
		buf[offset + 2], buf[offset + 3]);
	if (pFw_hdr->mnDevice >= TASDEVICE_DSP_TAS_MAX_DEVICE ||
		pFw_hdr->mnDevice == 6) {
		pr_err("ERROR:%s: not support device %d\n",
			__func__, pFw_hdr->mnDevice);
		offset = -1;
		goto out;
	}
	offset  += 4;
	pFw_hdr->ndev = deviceNumber[pFw_hdr->mnDevice];
	if (pFw_hdr->ndev != 1) {
		pr_err("%s: calbin must be 1, but currently ndev(%u)\n",
			__func__, pFw_hdr->ndev);
		offset = -1;
	}

out:
	return offset;
}

int tasdevice_load_block_git_show(struct tasdevice_priv *pTAS2781,
	struct TBlock *pBlock, char *buf, int *length)
{
	int nResult = 0;
	unsigned int nCommand = 0;
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nOffset;
	unsigned char nData;
	unsigned int nLength;
	unsigned int nSleep;
	unsigned char *pData = pBlock->mpData;
	unsigned int i = 0, n = 0, subblk_offset = 0;
	int chn = 0, chnend = 0;

	*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
		"\t\t\t\tblock = 0x%08x\n", pBlock->mnType);
	*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
		"\t\t\t\t\tmbPChkSumPresent = 0x%02x\n",
		pBlock->mbPChkSumPresent);
	*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
		"\t\t\t\t\tmbPChkSumPresent = 0x%02x\n",
		pBlock->mbPChkSumPresent);
	*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
		"\t\t\t\t\tmnPChkSum = 0x%02x\n", pBlock->mnPChkSum);
	*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
		"\t\t\t\t\tmbYChkSumPresent = 0x%02x\n",
		pBlock->mbYChkSumPresent);
	*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
		"\t\t\t\t\tmnYChkSum = 0x%02x\n",
		pBlock->mnYChkSum);
	*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
		"\t\t\t\t\tmnCommands = 0x%08x\n",
		pBlock->mnCommands);

	switch (pBlock->mnType) {
	case MAIN_ALL_DEVICES:
		chn = 0;
		chnend = pTAS2781->mpFirmware->fw_hdr.ndev;
		break;
	case MAIN_DEVICE_A:
	case COEFF_DEVICE_A:
	case PRE_DEVICE_A:
		chn = 0;
		chnend = 1;
		break;
	case MAIN_DEVICE_B:
	case COEFF_DEVICE_B:
	case PRE_DEVICE_B:
		chn = 1;
		chnend = 2;
		break;
	case MAIN_DEVICE_C:
	case COEFF_DEVICE_C:
	case PRE_DEVICE_C:
		chn = 2;
		chnend = 3;
		break;
	case MAIN_DEVICE_D:
	case COEFF_DEVICE_D:
	case PRE_DEVICE_D:
		chn = 3;
		chnend = 4;
		break;
	default:
		*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
			"%s: TAS2781 load block: Other Type = 0x%02x\n",
			__func__, pBlock->mnType);
		break;
	}

	for (; chn < chnend; chn++) {
		nCommand = 0;

		while (nCommand < pBlock->mnCommands) {
			pData = pBlock->mpData + nCommand * 4;
			if (nCommand + 1 > pBlock->mnCommands) {
				*length  += scnprintf(buf + *length,
					PAGE_SIZE - *length,
					"%s: Out of memory\n", __func__);
				nResult = -1;
				break;
			}
			nBook = pData[0];
			nPage = pData[1];
			nOffset = pData[2];
			nData = pData[3];

			nCommand++;

			if (nOffset <= 0x7F) {
				*length  += scnprintf(buf + *length,
					PAGE_SIZE - *length,
					"\t\t\t\t\t\tDEV%d BOOK0x%02x "
					"PAGE0x%02x REG0x%02x VALUE = "
					"0x%02x\n", chn, nBook, nPage, nOffset,
					nData);
			} else if (nOffset == 0x81) {
				nSleep = SMS_HTONS(nBook, nPage);
				*length  += scnprintf(buf + *length,
					PAGE_SIZE - *length,
					"\t\t\t\t\t\tDEV%d DELAY = %ums\n",
					chn, nSleep);
			} else if (nOffset == 0x85) {
				if (nCommand + 1 > pBlock->mnCommands) {
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"%s: Out of memory\n",
						__func__);
					nResult = -1;
					break;
				}
				pData  += 4;
				nLength = SMS_HTONS(nBook, nPage);
				nBook = pData[0];
				nPage = pData[1];
				nOffset = pData[2];
				if (nLength > 1) {
					n = ((nLength - 2) / 4) + 1;
					if (nCommand + n >
						pBlock->mnCommands) {
						*length  += scnprintf(buf +
							*length,
							PAGE_SIZE - *length,
							"%s: Out of memory\n",
							__func__);
						nResult = -1;
						break;
					}
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"\t\t\t\t\t\tDEV%d BOOK0x%02x "
						"PAGE0x%02x\n",
						chn, nBook, nPage);
					for (i = 0, subblk_offset = 3; i <
						nLength / 4; i++) {
						*length  += scnprintf(buf +
							*length,
							PAGE_SIZE - *length,
							"\t\t\t\t\t\t\t "
							"REG0x%02x = 0x%02x "
							"REG0x%02x "
							"= 0x%02x REG0x%02x = "
							"0x%02x REG0x%02x = "
							"0x%02x\n",
							nOffset + i * 4,
							pData[subblk_offset
								+ 0],
							nOffset + i * 4 + 1,
							pData[subblk_offset
								+ 1],
							nOffset + i * 4 + 2,
							pData[subblk_offset
								+ 2],
							nOffset + i * 4 + 3,
							pData[subblk_offset
								+ 3]);
						subblk_offset  += 4;
					}
					nCommand  += n;
				} else {
					*length  += scnprintf(buf + *length,
						PAGE_SIZE - *length,
						"\t\t\t\t\t\tDEV%d BOOK0x%02x "
						"PAGE0x%02x REG0x%02x VALUE = "
						"0x%02x\n", chn, nBook, nPage,
						nOffset, pData[3]);
				}
				nCommand++;
			}
		}
	}
	if (nResult < 0) {
		*length  += scnprintf(buf + *length, PAGE_SIZE - *length,
			"%s: Block (%d) load error\n",
			__func__, pBlock->mnType);
	}
	return nResult;
}
