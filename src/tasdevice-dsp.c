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

#include <linux/regmap.h>
#ifdef CONFIG_TASDEV_CODEC_SPI
	#include <linux/spi/spi.h>
#else
	#include <linux/i2c.h>
#endif
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/crc8.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
// #include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/time.h>

#include "tasdevice-dsp.h"
#include "tasdevice-regbin.h"
#include "tasdevice.h"
#include "tasdevice-dsp_git.h"
#include "tasdevice-dsp_kernel.h"

#define TAS2781_CAL_BIN_PATH	("/lib/firmware/")

#define ERROR_PRAM_CRCCHK			(0x0000000)
#define ERROR_YRAM_CRCCHK			(0x0000001)
#define BINFILEDOCVER				(0)
#define DRVFWVER					(1)
#define	PPC_DRIVER_CRCCHK			(0x00000200)

#define TAS2781_SA_COEFF_SWAP_REG	TASDEVICE_REG(0, 0x35, 0x2c)
#define TAS2781_YRAM_BOOK1			(140)
#define TAS2781_YRAM1_PAGE			(42)
#define TAS2781_YRAM1_START_REG		(88)

#define TAS2781_YRAM2_START_PAGE	(43)
#define TAS2781_YRAM2_END_PAGE		(49)
#define TAS2781_YRAM2_START_REG		(8)
#define TAS2781_YRAM2_END_REG		(127)

#define TAS2781_YRAM3_PAGE			(50)
#define TAS2781_YRAM3_START_REG		(8)
#define TAS2781_YRAM3_END_REG		(27)

/*should not include B0_P53_R44-R47 */
#define TAS2781_YRAM_BOOK2			(0)
#define TAS2781_YRAM4_START_PAGE	(50)
#define TAS2781_YRAM4_END_PAGE		(60)

#define TAS2781_YRAM5_PAGE			(61)
#define TAS2781_YRAM5_START_REG		(8)
#define TAS2781_YRAM5_END_REG		(27)

struct TYCRC {
	unsigned char mnOffset;
	unsigned char mnLen;
};

const unsigned int BinFileformatVerInfo[][2] = {
	{0x100, 0x100},
	{0x110, 0x200},
	{0x200, 0x300},
	{0x210, 0x310},
	{0x230, 0x320},
	{0x300, 0x400}
};

const char *devicefamily[1] = {
	"TAS Devices" };

const char *devicelist[TASDEVICE_DSP_TAS_MAX_DEVICE] = {
	"TAS2555",
	"TAS2555 Stereo",
	"TAS2557 Mono",
	"TAS2557 Dual Mono",
	"TAS2559",
	"TAS2563",
	NULL,
	"TAS2563 Dual Mono",
	"TAS2563 Quad",
	"TAS2563 2.1",
	"TAS2781",
	"TAS2781 Stereo",
	"TAS2781 2.1",
	"TAS2781 Quad"
};

static inline void tas2781_clear_Calfirmware(struct TFirmware
	*mpCalFirmware)
{
	int i = 0;
	unsigned int nBlock = 0;

	if (mpCalFirmware->mpCalibrations) {
		struct TCalibration *pCalibration;

		for (i = 0; i < mpCalFirmware->mnCalibrations; i++) {
			pCalibration = &(mpCalFirmware->mpCalibrations[i]);
			if (pCalibration) {
				struct TData *pImageData =
					&(pCalibration->mData);

				if (pImageData->mpBlocks) {
					struct TBlock *pBlock;

					for (nBlock = 0; nBlock <
						pImageData->mnBlocks;
						nBlock++) {
						pBlock = &(pImageData->
							mpBlocks[nBlock]);
						kfree(pBlock->mpData);
					}
				kfree(pImageData->mpBlocks);
				}
				kfree(pCalibration->mpDescription);
			}
		}
		kfree(mpCalFirmware->mpCalibrations);
	}
	kfree(mpCalFirmware);
}

static int fw_parse_block_data(struct TFirmware *pFirmware,
	struct TBlock *pBlock, const struct firmware *pFW, int offset)
{
	unsigned char *pData = (unsigned char *)pFW->data;
	int n;

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->mnType = SMS_HTONL(pData[offset],
		pData[offset + 1], pData[offset + 2], pData[offset + 3]);
	offset  += 4;

	if (pFirmware->fw_hdr.mnFixedHdr.mnDriverVersion >=
		PPC_DRIVER_CRCCHK) {
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
	} else {
		pBlock->mbPChkSumPresent = 0;
		pBlock->mbYChkSumPresent = 0;
	}
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pBlock->mnCommands = SMS_HTONL(pData[offset],
		pData[offset + 1], pData[offset + 2], pData[offset + 3]);
	offset  += 4;

	n = pBlock->mnCommands * 4;
	if (offset + n > pFW->size) {
		pr_err("%s: File Size(%u) error offset = %d n = %d\n",
			__func__, pFW->size, offset, n);
		offset = -1;
		goto out;
	}
	pBlock->mpData = kmemdup(&pData[offset], n, GFP_KERNEL);
	if (pBlock->mpData == NULL) {
		pr_err("%s: mpData memory error\n", __func__);
		offset = -1;
		goto out;
	}
	offset  += n;
out:
	return offset;
}

static int fw_parse_data(struct TFirmware *pFirmware,
	struct TData *pImageData, const struct firmware *pFW, int offset)
{
	const unsigned char *pData = (unsigned char *)pFW->data;
	int n = 0;
	unsigned int nBlock;

	if (offset + 64 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		n = -1;
		goto out;
	}
	memcpy(pImageData->mpName, &pData[offset], 64);
	offset  += 64;

	n = strlen((char *)&pData[offset]);
	n++;
	if (offset + n > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pImageData->mpDescription = kmemdup(pData, n, GFP_KERNEL);
	if (pImageData->mpDescription == NULL) {
		pr_err("%s: FW memory failed!\n", __func__);
		goto out;
	}
	offset  += n;

	if (offset + 2 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pImageData->mnBlocks = SMS_HTONS(pData[offset], pData[offset + 1]);
	offset  += 2;

	pImageData->mpBlocks =
		kcalloc(pImageData->mnBlocks, sizeof(struct TBlock),
			GFP_KERNEL);
	if (pImageData->mpBlocks == NULL) {
		pr_err("%s: FW memory failed!\n", __func__);
		goto out;
	}
	for (nBlock = 0; nBlock < pImageData->mnBlocks; nBlock++) {
		offset = fw_parse_block_data(pFirmware,
			&(pImageData->mpBlocks[nBlock]), pFW, offset);
		if (offset < 0) {
			offset = -1;
			goto out;
		}
	}
out:
	return offset;
}

static int fw_parse_calibration_data(struct TFirmware *pFirmware,
	const struct firmware *pFW, int offset)
{
	unsigned char *pData = (unsigned char *)pFW->data;
	unsigned int n = 0;
	unsigned int nCalibration = 0;
	struct TCalibration *pCalibration = NULL;

	if (offset + 2 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFirmware->mnCalibrations = SMS_HTONS(pData[offset],
		pData[offset + 1]);
	offset  += 2;

	if (pFirmware->mnCalibrations != 1) {
		pr_err("%s: only support one calibraiton(%d)!\n",
			__func__, pFirmware->mnCalibrations);
		goto out;
	}

	pFirmware->mpCalibrations =
		kcalloc(pFirmware->mnCalibrations, sizeof(struct TCalibration),
			GFP_KERNEL);
	if (pFirmware->mpCalibrations == NULL) {
		pr_err("%s: mpCalibrations memory failed!\n", __func__);
		offset = -1;
		goto out;
	}
	for (nCalibration = 0; nCalibration < pFirmware->mnCalibrations;
		nCalibration++) {
		if (offset + 64 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pCalibration = &(pFirmware->mpCalibrations[nCalibration]);
		memcpy(pCalibration->mpName, &pData[offset], 64);
		offset  += 64;

		n = strlen((char *)&pData[offset]);
		n++;
		if (offset + n > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pCalibration->mpDescription = kmemdup(&pData[offset], n,
			GFP_KERNEL);
		if (pCalibration->mpDescription == NULL) {
			pr_err("%s: mpPrograms memory failed!\n", __func__);
			offset = -1;
			goto out;
		}
		offset  += n;

		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error, offset = %d\n", __func__,
				offset);
			offset = -1;
			goto out;
		}
		pCalibration->mnProgram = pData[offset];
		offset++;

		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error, offset = %d\n", __func__,
				offset);
			offset = -1;
			goto out;
		}
		pCalibration->mnConfiguration = pData[offset];
		offset++;

		offset = fw_parse_data(pFirmware, &(pCalibration->mData), pFW,
			offset);
		if (offset < 0)
			goto out;
	}

out:

	return offset;
}

static int fw_parse_program_data(struct TFirmware *pFirmware,
	const struct firmware *pFW, int offset)
{
	struct TProgram *pProgram;
	unsigned char *buf = (unsigned char *)pFW->data;
	int nProgram = 0;

	if (offset + 2 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFirmware->mnPrograms = SMS_HTONS(buf[offset], buf[offset + 1]);
	offset  += 2;

	if (pFirmware->mnPrograms == 0) {
		pr_info("%s: mnPrograms is null, maybe calbin\n", __func__);
		//Do not "offset = -1;", because of calbin
		goto out;
	}

	pFirmware->mpPrograms =
		kcalloc(pFirmware->mnPrograms, sizeof(struct TProgram),
			GFP_KERNEL);
	if (pFirmware->mpPrograms == NULL) {
		pr_err("%s: mpPrograms memory failed!\n", __func__);
		offset = -1;
		goto out;
	}
	for (nProgram = 0; nProgram < pFirmware->mnPrograms; nProgram++) {
		int n = 0;

		pProgram = &(pFirmware->mpPrograms[nProgram]);
		if (offset + 64 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		memcpy(pProgram->mpName, &buf[offset], 64);
		offset  += 64;

		n = strlen((char *)&buf[offset]);
		n++;
		if (offset + n > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pProgram->mpDescription = kmemdup(&buf[offset], n, GFP_KERNEL);
		if (pProgram->mpDescription == NULL) {
			pr_err("%s: mpPrograms memory failed!\n", __func__);
			offset = -1;
			goto out;
		}

		offset  += n;

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
		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pProgram->mnPowerLDG = buf[offset];
		offset++;

		offset = fw_parse_data(pFirmware, &(pProgram->mData), pFW,
			offset);
		if (offset < 0)
			goto out;
	}
out:
	return offset;
}

static int fw_parse_configuration_data(struct TFirmware *pFirmware,
	const struct firmware *pFW, int offset)
{
	unsigned char *pData = (unsigned char *)pFW->data;
	int n;
	unsigned int nConfiguration;
	struct TConfiguration *pConfiguration;

	if (offset + 2 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFirmware->mnConfigurations = SMS_HTONS(pData[offset],
		pData[offset + 1]);
	offset  += 2;

	if (pFirmware->mnConfigurations == 0) {
		pr_err("%s: mnConfigurations is zero\n", __func__);
		//Do not "offset = -1;", because of calbin
		goto out;
	}
	pFirmware->mpConfigurations =
		kcalloc(pFirmware->mnConfigurations,
				sizeof(struct TConfiguration), GFP_KERNEL);

	for (nConfiguration = 0; nConfiguration < pFirmware->mnConfigurations;
		nConfiguration++) {
		pConfiguration =
			&(pFirmware->mpConfigurations[nConfiguration]);
		if (offset + 64 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		memcpy(pConfiguration->mpName, &pData[offset], 64);
		offset  += 64;

		n = strlen((char *)&pData[offset]);
		n++;
		if (offset + n > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mpDescription = kmemdup(&pData[offset], n,
			GFP_KERNEL);

		if (pConfiguration->mpDescription == NULL) {
			pr_err("%s: FW memory failed!\n", __func__);
			goto out;
		}
		offset  += n;
		if (offset + 2 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnDevice_orientation = pData[offset];

		pConfiguration->mnDevices = pData[offset + 1];
		offset  += 2;

		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mProgram = pData[offset];
		offset++;

		if (offset + 4 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnSamplingRate = SMS_HTONL(pData[offset],
			pData[offset + 1], pData[offset + 2],
			pData[offset + 3]);
		offset  += 4;

		if (offset + 1 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnPLLSrc = pData[offset];
		offset++;

		if (offset + 4 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnPLLSrcRate = SMS_HTONL(pData[offset],
			pData[offset + 1], pData[offset + 2],
			pData[offset + 3]);
		offset  += 4;

		if (offset + 2 > pFW->size) {
			pr_err("%s: File Size error\n", __func__);
			offset = -1;
			goto out;
		}
		pConfiguration->mnFsRate = SMS_HTONS(pData[offset],
			pData[offset + 1]);
		offset  += 2;

		offset = fw_parse_data(pFirmware, &(pConfiguration->mData),
			pFW, offset);
		if (offset < 0)
			goto out;
	}
out:
	return offset;
}

static int fw_parse_header(struct TFirmware *pFirmware,
	const struct firmware *pFW, int offset)
{
	struct tasdevice_dspfw_hdr *pFw_hdr = &(pFirmware->fw_hdr);
	struct tasdevice_fw_fixed_hdr *pFw_fixed_hdr = &(pFw_hdr->mnFixedHdr);
	const unsigned char *buf = (unsigned char *)pFW->data;
	int i = 0;
	unsigned char pMagicNumber[] = { 0x35, 0x35, 0x35, 0x32 };

	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -EINVAL;
		goto out;
	}
	if (memcmp(&buf[offset], pMagicNumber, 4)) {
		pr_err("Firmware: Magic number doesn't match");
		offset = -EINVAL;
		goto out;
	}
	pFw_fixed_hdr->mnMagicNumber = SMS_HTONL(buf[offset],
		buf[offset + 1], buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_fixed_hdr->mnFWSize = SMS_HTONL(buf[offset],
		buf[offset + 1], buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	if (pFw_fixed_hdr->mnFWSize != pFW->size) {
		pr_err("File size not match, %d %d", pFW->size,
			pFw_fixed_hdr->mnFWSize);
		offset = -1;
		goto out;
	}
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_fixed_hdr->mnChecksum = SMS_HTONL(buf[offset],
		buf[offset + 1], buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_fixed_hdr->mnPPCVersion = SMS_HTONL(buf[offset],
		buf[offset + 1], buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_fixed_hdr->mnFWVersion = SMS_HTONL(buf[offset],
		buf[offset + 1], buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	pFw_fixed_hdr->mnDriverVersion = SMS_HTONL(buf[offset],
		buf[offset + 1], buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	if (offset + 4 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	for (i = 0; i < sizeof(BinFileformatVerInfo) /
		sizeof(BinFileformatVerInfo[0]); i++) {
		if (BinFileformatVerInfo[i][DRVFWVER] ==
			pFw_fixed_hdr->mnDriverVersion) {
			pFw_hdr->mnBinFileDocVer =
				BinFileformatVerInfo[i][BINFILEDOCVER];
			break;
		}
	}
	pFw_fixed_hdr->mnTimeStamp = SMS_HTONL(buf[offset],
		buf[offset + 1], buf[offset + 2], buf[offset + 3]);
	offset  += 4;
	if (offset + 64 > pFW->size) {
		pr_err("%s: File Size error\n", __func__);
		offset = -1;
		goto out;
	}
	memcpy(pFw_fixed_hdr->mpDDCName, &buf[offset], 64);
	offset  += 64;

 out:
	return offset;
}

static const unsigned char crc8_lookup_table[CRC8_TABLE_SIZE] = {
	0x00, 0x4D, 0x9A, 0xD7, 0x79, 0x34, 0xE3, 0xAE,
	0xF2, 0xBF, 0x68, 0x25, 0x8B, 0xC6, 0x11, 0x5C,
	0xA9, 0xE4, 0x33, 0x7E, 0xD0, 0x9D, 0x4A, 0x07,
	0x5B, 0x16, 0xC1, 0x8C, 0x22, 0x6F, 0xB8, 0xF5,
	0x1F, 0x52, 0x85, 0xC8, 0x66, 0x2B, 0xFC, 0xB1,
	0xED, 0xA0, 0x77, 0x3A, 0x94, 0xD9, 0x0E, 0x43,
	0xB6, 0xFB, 0x2C, 0x61, 0xCF, 0x82, 0x55, 0x18,
	0x44, 0x09, 0xDE, 0x93, 0x3D, 0x70, 0xA7, 0xEA,
	0x3E, 0x73, 0xA4, 0xE9, 0x47, 0x0A, 0xDD, 0x90,
	0xCC, 0x81, 0x56, 0x1B, 0xB5, 0xF8, 0x2F, 0x62,
	0x97, 0xDA, 0x0D, 0x40, 0xEE, 0xA3, 0x74, 0x39,
	0x65, 0x28, 0xFF, 0xB2, 0x1C, 0x51, 0x86, 0xCB,
	0x21, 0x6C, 0xBB, 0xF6, 0x58, 0x15, 0xC2, 0x8F,
	0xD3, 0x9E, 0x49, 0x04, 0xAA, 0xE7, 0x30, 0x7D,
	0x88, 0xC5, 0x12, 0x5F, 0xF1, 0xBC, 0x6B, 0x26,
	0x7A, 0x37, 0xE0, 0xAD, 0x03, 0x4E, 0x99, 0xD4,
	0x7C, 0x31, 0xE6, 0xAB, 0x05, 0x48, 0x9F, 0xD2,
	0x8E, 0xC3, 0x14, 0x59, 0xF7, 0xBA, 0x6D, 0x20,
	0xD5, 0x98, 0x4F, 0x02, 0xAC, 0xE1, 0x36, 0x7B,
	0x27, 0x6A, 0xBD, 0xF0, 0x5E, 0x13, 0xC4, 0x89,
	0x63, 0x2E, 0xF9, 0xB4, 0x1A, 0x57, 0x80, 0xCD,
	0x91, 0xDC, 0x0B, 0x46, 0xE8, 0xA5, 0x72, 0x3F,
	0xCA, 0x87, 0x50, 0x1D, 0xB3, 0xFE, 0x29, 0x64,
	0x38, 0x75, 0xA2, 0xEF, 0x41, 0x0C, 0xDB, 0x96,
	0x42, 0x0F, 0xD8, 0x95, 0x3B, 0x76, 0xA1, 0xEC,
	0xB0, 0xFD, 0x2A, 0x67, 0xC9, 0x84, 0x53, 0x1E,
	0xEB, 0xA6, 0x71, 0x3C, 0x92, 0xDF, 0x08, 0x45,
	0x19, 0x54, 0x83, 0xCE, 0x60, 0x2D, 0xFA, 0xB7,
	0x5D, 0x10, 0xC7, 0x8A, 0x24, 0x69, 0xBE, 0xF3,
	0xAF, 0xE2, 0x35, 0x78, 0xD6, 0x9B, 0x4C, 0x01,
	0xF4, 0xB9, 0x6E, 0x23, 0x8D, 0xC0, 0x17, 0x5A,
	0x06, 0x4B, 0x9C, 0xD1, 0x7F, 0x32, 0xE5, 0xA8
};

static int isInPageYRAM(struct tasdevice_priv *pTAS2781,
	struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage,
	unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	if (nBook == TAS2781_YRAM_BOOK1) {
		if (nPage == TAS2781_YRAM1_PAGE) {
			if (nReg >= TAS2781_YRAM1_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else if ((nReg + len) > TAS2781_YRAM1_START_REG) {
				pCRCData->mnOffset = TAS2781_YRAM1_START_REG;
				pCRCData->mnLen =
				len - (TAS2781_YRAM1_START_REG - nReg);
				nResult = 1;
			} else
				nResult = 0;
		} else if (nPage == TAS2781_YRAM3_PAGE) {
			if (nReg > TAS2781_YRAM3_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2781_YRAM3_START_REG) {
				if ((nReg + len) > TAS2781_YRAM3_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen =
					TAS2781_YRAM3_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len-1)) <
					TAS2781_YRAM3_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset =
					TAS2781_YRAM3_START_REG;
					pCRCData->mnLen =
					len - (TAS2781_YRAM3_START_REG - nReg);
					nResult = 1;
				}
			}
		}
	} else if (nBook ==
		TAS2781_YRAM_BOOK2) {
		if (nPage == TAS2781_YRAM5_PAGE) {
			if (nReg > TAS2781_YRAM5_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2781_YRAM5_START_REG) {
				if ((nReg + len) > TAS2781_YRAM5_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen =
					TAS2781_YRAM5_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len-1)) <
					TAS2781_YRAM5_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset =
					TAS2781_YRAM5_START_REG;
					pCRCData->mnLen =
					len - (TAS2781_YRAM5_START_REG - nReg);
					nResult = 1;
				}
			}
		}
	} else
		nResult = 0;

	return nResult;
}

static int isInBlockYRAM(struct tasdevice_priv *pTAS2781,
	struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage,
	unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	if (nBook == TAS2781_YRAM_BOOK1) {
		if (nPage < TAS2781_YRAM2_START_PAGE)
			nResult = 0;
		else if (nPage <= TAS2781_YRAM2_END_PAGE) {
			if (nReg > TAS2781_YRAM2_END_REG)
				nResult = 0;
			else if (nReg >= TAS2781_YRAM2_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len-1)) <
					TAS2781_YRAM2_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset =
					TAS2781_YRAM2_START_REG;
					pCRCData->mnLen =
					nReg + len - TAS2781_YRAM2_START_REG;
					nResult = 1;
				}
			}
		} else
			nResult = 0;
	} else if (nBook ==
		TAS2781_YRAM_BOOK2) {
		if (nPage < TAS2781_YRAM4_START_PAGE)
			nResult = 0;
		else if (nPage <= TAS2781_YRAM4_END_PAGE) {
			if (nReg > TAS2781_YRAM2_END_REG)
				nResult = 0;
			else if (nReg >= TAS2781_YRAM2_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len-1))
					< TAS2781_YRAM2_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset =
					TAS2781_YRAM2_START_REG;
					pCRCData->mnLen =
					nReg + len - TAS2781_YRAM2_START_REG;
					nResult = 1;
				}
			}
		} else
			nResult = 0;
	} else
		nResult = 0;

	return nResult;
}

static int isYRAM(struct tasdevice_priv *pTAS2781, struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage,
	unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	nResult = isInPageYRAM(pTAS2781, pCRCData, nBook, nPage, nReg, len);
	if (nResult == 0)
		nResult = isInBlockYRAM(pTAS2781, pCRCData, nBook,
				nPage, nReg, len);

	return nResult;
}

/*
 * crc8-calculate a crc8 over the given input data.
 *
 * table: crc table used for calculation.
 * pdata: pointer to data buffer.
 * nbytes: number of bytes in data buffer.
 * crc: previous returned crc8 value.
 */
static u8 ti_crc8(const u8 table[CRC8_TABLE_SIZE], u8 *pdata,
			size_t nbytes, u8 crc)
{
	/*loop over the buffer data */
	while (nbytes-- > 0)
		crc = table[(crc ^ *(pdata  += 1)) & 0xff];

	return crc;
}

static int doSingleRegCheckSum(struct tasdevice_priv *pTAS2781,
	enum channel chl,
		unsigned char nBook, unsigned char nPage,
		unsigned char nReg, unsigned char nValue)
{
	int nResult = 0;
	struct TYCRC sCRCData;
	unsigned int nData1 = 0;

	if ((nBook == TASDEVICE_BOOK_ID(TAS2781_SA_COEFF_SWAP_REG))
		&& (nPage == TASDEVICE_PAGE_ID(TAS2781_SA_COEFF_SWAP_REG))
		&& (nReg >= TASDEVICE_PAGE_REG(TAS2781_SA_COEFF_SWAP_REG))
		&& (nReg <= (TASDEVICE_PAGE_REG(
		TAS2781_SA_COEFF_SWAP_REG) + 4))) {
		/*DSP swap command, pass */
		nResult = 0;
		goto end;
	}

	nResult = isYRAM(pTAS2781, &sCRCData, nBook, nPage, nReg, 1);
	if (nResult == 1) {
		nResult = pTAS2781->read(pTAS2781, chl,
				TASDEVICE_REG(nBook, nPage, nReg), &nData1);
		if (nResult < 0)
			goto end;

		if (nData1 != nValue) {
			dev_err(pTAS2781->dev, "error2, B[0x%x]P[0x%x]R[0x%x] "
				"W[0x%x], R[0x%x]\n", nBook, nPage, nReg,
				nValue, nData1);
			nResult = -EAGAIN;
			pTAS2781->tasdevice[chl].mnErrCode |=
				ERROR_YRAM_CRCCHK;
			goto end;
		}

		if (nData1 != nValue) {
			dev_err(pTAS2781->dev, "error2, B[0x%x]P[0x%x]R[0x%x] "
				"W[0x%x], R[0x%x]\n", nBook, nPage, nReg,
				nValue, nData1);
			nResult = -EAGAIN;
			goto end;
		}

		nResult = ti_crc8(crc8_lookup_table, &nValue, 1, 0);
	}

end:
	return nResult;
}

static int doMultiRegCheckSum(struct tasdevice_priv *pTAS2781,
	enum channel chn, unsigned char nBook, unsigned char nPage,
	unsigned char nReg, unsigned int len)
{
	int nResult = 0, i = 0;
	unsigned char nCRCChkSum = 0;
	unsigned char nBuf1[128] = {0};
	struct TYCRC TCRCData;

	if ((nReg + len-1) > 127) {
		nResult = -EINVAL;
		dev_err(pTAS2781->dev, "firmware error\n");
		goto end;
	}

	if ((nBook == TASDEVICE_BOOK_ID(TAS2781_SA_COEFF_SWAP_REG))
		&& (nPage == TASDEVICE_PAGE_ID(TAS2781_SA_COEFF_SWAP_REG))
		&& (nReg == TASDEVICE_PAGE_REG(TAS2781_SA_COEFF_SWAP_REG))
		&& (len == 4)) {
		/*DSP swap command, pass */
		nResult = 0;
		goto end;
	}

	nResult = isYRAM(pTAS2781, &TCRCData, nBook, nPage, nReg, len);
	dev_info(pTAS2781->dev,
		"isYRAM: nBook 0x%x, nPage 0x%x, nReg 0x%x\n",
		nBook, nPage, nReg);
	dev_info(pTAS2781->dev,
		"isYRAM: TCRCData.mnLen 0x%x, len 0x%x, nResult %d\n",
		TCRCData.mnLen, len, nResult);
	dev_info(pTAS2781->dev, "TCRCData.mnOffset %x\n", TCRCData.mnOffset);
	if (nResult == 1) {
		if (len == 1) {
			dev_err(pTAS2781->dev, "firmware error\n");
			nResult = -EINVAL;
			goto end;
		} else {
			nResult = pTAS2781->bulk_read(pTAS2781, chn,
				TASDEVICE_REG(nBook, nPage, TCRCData.mnOffset),
				nBuf1, TCRCData.mnLen);
			if (nResult < 0)
				goto end;

			for (i = 0; i < TCRCData.mnLen; i++) {
				if ((nBook == TASDEVICE_BOOK_ID(
					TAS2781_SA_COEFF_SWAP_REG))
					&& (nPage == TASDEVICE_PAGE_ID(
						TAS2781_SA_COEFF_SWAP_REG))
					&& ((i + TCRCData.mnOffset)
					>= TASDEVICE_PAGE_REG(
						TAS2781_SA_COEFF_SWAP_REG))
					&& ((i + TCRCData.mnOffset)
					<= (TASDEVICE_PAGE_REG(
						TAS2781_SA_COEFF_SWAP_REG)
						+ 4))) {
					/*DSP swap command, bypass */
					continue;
				} else
					nCRCChkSum  +=
					ti_crc8(crc8_lookup_table, &nBuf1[i],
						1, 0);
			}

			nResult = nCRCChkSum;
		}
	}

end:
	return nResult;
}

static int tasdevice_load_block(struct tasdevice_priv *tas_dev,
				struct TBlock *pBlock)
{
	int nResult = 0;
	unsigned int nCommand = 0;
	unsigned char nBook = 0;
	unsigned char nPage = 0;
	unsigned char nOffset = 0;
	unsigned char nData = 0;
	unsigned int nLength = 0;
	unsigned int nSleep = 0;
	unsigned char nCRCChkSum = 0;
	unsigned int nValue = 0;
	int nRetry = 6;
	unsigned char *pData = pBlock->mpData;
	int chn = 0, chnend = 0;

	dev_info(tas_dev->dev,
		"TAS2781 load block: Type = %d, commands = %d\n",
		pBlock->mnType, pBlock->mnCommands);
	switch (pBlock->mnType) {
	case MAIN_ALL_DEVICES:
		chn = 0;
		chnend = tas_dev->ndev;
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
		dev_info(tas_dev->dev,
			"TAS2781 load block: Other Type = 0x%02x\n",
			pBlock->mnType);
		break;
	}

	for (; chn < chnend; chn++) {
		if (tas_dev->tasdevice[chn].bLoading == false)
			continue;
start:
		if (pBlock->mbPChkSumPresent) {
			nResult = tas_dev->write(tas_dev, chn,
				TASDEVICE_I2CChecksum, 0);
			if (nResult < 0)
				goto end;
		}

		if (pBlock->mbYChkSumPresent)
			nCRCChkSum = 0;

		nCommand = 0;

		while (nCommand < pBlock->mnCommands) {
			pData = pBlock->mpData + nCommand * 4;

			nBook = pData[0];
			nPage = pData[1];
			nOffset = pData[2];
			nData = pData[3];

			nCommand++;

			if (nOffset <= 0x7F) {
				nResult = tas_dev->write(tas_dev, chn,
					TASDEVICE_REG(nBook, nPage, nOffset),
					nData);
				if (nResult < 0)
					goto end;
				if (pBlock->mbYChkSumPresent) {
					nResult = doSingleRegCheckSum(tas_dev,
						chn, nBook, nPage, nOffset,
						nData);
					if (nResult < 0)
						goto check;
					nCRCChkSum  += (unsigned char)nResult;
				}
			} else if (nOffset == 0x81) {
				nSleep = (nBook << 8) + nPage;
				msleep(nSleep);
			} else if (nOffset == 0x85) {
				pData  += 4;
				nLength = (nBook << 8) + nPage;
				nBook = pData[0];
				nPage = pData[1];
				nOffset = pData[2];
				if (nLength > 1) {
					nResult = tas_dev->bulk_write(tas_dev,
						chn, TASDEVICE_REG(nBook,
						nPage, nOffset), pData + 3,
						nLength);
					if (nResult < 0)
						goto end;
					if (pBlock->mbYChkSumPresent) {
						nResult = doMultiRegCheckSum(
							tas_dev, chn, nBook,
							nPage, nOffset,
							nLength);
						if (nResult < 0)
							goto check;
						nCRCChkSum  +=
							(unsigned char)nResult;
					}
				} else {
					nResult = tas_dev->write(tas_dev, chn,
						TASDEVICE_REG(nBook, nPage,
						nOffset),
						pData[3]);
					if (nResult < 0)
						goto end;
					if (pBlock->mbYChkSumPresent) {
						nResult = doSingleRegCheckSum(
							tas_dev, chn, nBook,
							nPage, nOffset,
							pData[3]);
						if (nResult < 0)
							goto check;
						nCRCChkSum  +=
							(unsigned char)nResult;
					}
				}

				nCommand++;

				if (nLength >= 2)
					nCommand  += ((nLength - 2) / 4) + 1;
			}
		}
		if (pBlock->mbPChkSumPresent) {
			nResult = tas_dev->read(tas_dev, chn,
				TASDEVICE_I2CChecksum, &nValue);
			if (nResult < 0) {
				dev_err(tas_dev->dev, "%s: Channel %d\n",
					__func__, chn);
				goto check;
			}
			if ((nValue&0xff) != pBlock->mnPChkSum) {
				dev_err(tas_dev->dev,
					"Block PChkSum Channel %d Error: "
					"FW = 0x%x, Reg = 0x%x\n", chn,
					pBlock->mnPChkSum, (nValue&0xff));
				nResult = -EAGAIN;
				tas_dev->tasdevice[chn].mnErrCode |=
					ERROR_PRAM_CRCCHK;
				goto check;
			}
			nResult = 0;
			tas_dev->tasdevice[chn].mnErrCode &=
				~ERROR_PRAM_CRCCHK;
			dev_info(tas_dev->dev, "Block[0x%02x] PChkSum match\n",
				pBlock->mnType);
		}

		if (pBlock->mbYChkSumPresent) {
			//TBD, open it when FW ready
			dev_err(tas_dev->dev, "Block YChkSum: FW = 0x%x, "
				"YCRC = 0x%x\n", pBlock->mnYChkSum,
				nCRCChkSum);

			tas_dev->tasdevice[chn].mnErrCode &=
				~ERROR_YRAM_CRCCHK;
			nResult = 0;
			dev_info(tas_dev->dev,
				"Block[0x%x] YChkSum match\n", pBlock->mnType);
		}
check:
		if (nResult == -EAGAIN) {
			nRetry--;
			if (nRetry > 0)
				goto start;
			else {
				//tas_dev->tasdevice[chn].bLoading = false;
				if ((MAIN_ALL_DEVICES == pBlock->mnType)
					|| (MAIN_DEVICE_A == pBlock->mnType)
					|| (MAIN_DEVICE_B == pBlock->mnType)
					|| (MAIN_DEVICE_C == pBlock->mnType)
					|| (MAIN_DEVICE_D == pBlock->mnType)) {
					tas_dev->tasdevice[chn].
						mnCurrentProgram = -1;
				} else {
					tas_dev->tasdevice[chn].
						mnCurrentConfiguration = -1;
				}
				nRetry = 6;
			}
		}
	}
end:
	if (nResult < 0) {
		dev_err(tas_dev->dev, "Block (%d) load error\n",
				pBlock->mnType);
	}
	return nResult;
}


static int tasdevice_load_data(struct tasdevice_priv *tas_dev,
	struct TData *pData)
{
	int nResult = 0;
	unsigned int nBlock = 0;
	struct TBlock *pBlock = NULL;

	dev_info(tas_dev->dev, "%s: TAS2781 load data: %s, Blocks = %d\n",
		__func__,
		pData->mpName, pData->mnBlocks);

	for (nBlock = 0; nBlock < pData->mnBlocks; nBlock++) {
		pBlock = &(pData->mpBlocks[nBlock]);
		nResult = tas_dev->tasdevice_load_block(tas_dev, pBlock);
		if (nResult < 0)
			break;
	}

	return nResult;
}

static int tasdevice_load_calibrated_data(
	struct tasdevice_priv *tas_dev, struct TData *pData)
{
	int nResult = 0;
	unsigned int nBlock = 0;
	struct TBlock *pBlock = NULL;

	dev_info(tas_dev->dev, "%s: TAS2781 load data: %s, Blocks = %d\n",
		__func__,
		pData->mpName, pData->mnBlocks);

	for (nBlock = 0; nBlock < pData->mnBlocks; nBlock++) {
		pBlock = &(pData->mpBlocks[nBlock]);
		nResult = tasdevice_load_block(tas_dev, pBlock);
		if (nResult < 0)
			break;
	}

	return nResult;
}

int tas2781_load_calibration(void *pContext,
			char *pFileName, enum channel i)
{
	int ret = 0, nSize = 0, offset = 0;
	loff_t pos = 0;
	struct file *filp = NULL;
	mm_segment_t fs;
	struct firmware FW;
	const struct firmware *fw_entry = NULL;
	char *data = NULL;
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *)pContext;
	struct Ttasdevice *pTasdev = &(tas_dev->tasdevice[i]);
	struct TFirmware *mpCalFirmware = NULL;
	char pHint[256];

	dev_info(tas_dev->dev, "%s: enter\n", __func__);

	ret = request_firmware(&fw_entry, pFileName, tas_dev->dev);
	if (!ret) {
		if (!fw_entry->size) {
			dev_err(tas_dev->dev,
				"%s: file read error: size = %d\n",
				__func__, fw_entry->size);
			goto out;
		}
		FW.size = fw_entry->size;
		FW.data = fw_entry->data;
		dev_info(tas_dev->dev,
			"%s: file = %s, file size %zd\n",
			__func__, pFileName, fw_entry->size);
	} else {
		dev_info(tas_dev->dev,
			"%s: Request firmware failed, try flip_open()\n",
			__func__);
		fs = get_fs();
		set_fs(KERNEL_DS);

		scnprintf(pHint, sizeof(pHint), "%s%s\n",
			TAS2781_CAL_BIN_PATH, pFileName);
		filp = filp_open(pHint, O_RDONLY, 664);
		if (!IS_ERR_OR_NULL(filp)) {
			FW.size = i_size_read(file_inode(filp));
			dev_info(tas_dev->dev,
				"%s: file = %s, file size %ld\n",
				__func__, pHint, (long)FW.size);
			data = kmalloc(FW.size, GFP_KERNEL);
			if (data == NULL) {
				dev_err(tas_dev->dev, "%s: malloc error\n",
					__func__);
				goto out;
			}
			nSize = (int)kernel_read(filp, data, FW.size, &pos);
			if (!nSize) {
				dev_err(tas_dev->dev,
					"%s: file read error: size = %d\n",
					__func__, nSize);
				goto out;
			}
			dev_info(tas_dev->dev, "read filed nSize = %d\n",
				nSize);
			FW.data = data;
		} else {
			dev_err(tas_dev->dev,
				"%s: cannot open calibration file: %s\n",
				__func__, pHint);
			goto out;
		}
	}

	mpCalFirmware = pTasdev->mpCalFirmware = kcalloc(1,
		sizeof(struct TFirmware), GFP_KERNEL);
	if (pTasdev->mpCalFirmware == NULL) {
		dev_err(tas_dev->dev, "%s: FW memory failed!\n", __func__);
		ret = -1;
		goto out;
	}
	offset = fw_parse_header(mpCalFirmware, &FW, offset);
	if (offset == -1) {
		dev_err(tas_dev->dev, "%s: EXIT!\n", __func__);
		goto out;
	}
	offset = fw_parse_variable_header_cal(mpCalFirmware, &FW, offset);
	if (offset == -1) {
		dev_err(tas_dev->dev, "%s: EXIT!\n", __func__);
		goto out;
	}
	offset = fw_parse_program_data(mpCalFirmware, &FW, offset);
	if (offset == -1) {
		dev_err(tas_dev->dev, "%s: EXIT!\n", __func__);
		goto out;
	}
	offset = fw_parse_configuration_data(mpCalFirmware, &FW, offset);
	if (offset == -1) {
		dev_err(tas_dev->dev, "%s: EXIT!\n", __func__);
		goto out;
	}
	offset = fw_parse_calibration_data(mpCalFirmware, &FW, offset);
	if (offset == -1) {
		dev_err(tas_dev->dev, "%s: EXIT!\n", __func__);
		goto out;
	}
	pTasdev->mbCalibrationLoaded = true;
out:
	if (!IS_ERR_OR_NULL(filp)) {
		set_fs(fs);
		filp_close(filp, NULL);
		kfree(data);
	}
	if (fw_entry) {
		release_firmware(fw_entry);
		fw_entry = NULL;
	}
	return ret;
}

int tasdevice_dspfw_ready(const void *pVoid, void *pContext)
{
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *) pContext;
	const struct firmware *pFW = (const struct firmware *)pVoid;
	struct TFirmware *pFirmware = NULL;
	struct tasdevice_fw_fixed_hdr *pFw_fixed_hdr = NULL;
	int offset = 0, ret = 0;

	if (!pFW || !pFW->data) {
		dev_err(tas_dev->dev, "%s: Failed to read firmware %s\n",
			__func__, tas_dev->dsp_binaryname);
		ret = -1;
		goto out;
	}

	tas_dev->mpFirmware = kcalloc(1,
		sizeof(struct TFirmware), GFP_KERNEL);
	if (tas_dev->mpFirmware == NULL) {
		dev_err(tas_dev->dev, "%s: FW memory failed!\n", __func__);
		ret = -1;
		goto out;
	}
	pFirmware = tas_dev->mpFirmware;

	offset = fw_parse_header(pFirmware, pFW, offset);

	if (offset == -1)
		goto out;
	pFw_fixed_hdr = &(pFirmware->fw_hdr.mnFixedHdr);
	switch (pFw_fixed_hdr->mnDriverVersion) {
	case 0x301:
	case 0x302:
	case 0x502:
		tas_dev->fw_parse_variable_header =
			fw_parse_variable_header_kernel;
		tas_dev->fw_parse_program_data =
			fw_parse_program_data_kernel;
		tas_dev->fw_parse_configuration_data =
			fw_parse_configuration_data_kernel;
		tas_dev->tasdevice_load_block =
			tasdevice_load_block_kernel;
		pFirmware->bKernelFormat = true;
		break;
	case 0x202:
	case 0x400:
		tas_dev->fw_parse_variable_header =
			fw_parse_variable_header_git;
		tas_dev->fw_parse_program_data =
			fw_parse_program_data;
		tas_dev->fw_parse_configuration_data =
			fw_parse_configuration_data;
		tas_dev->tasdevice_load_block =
			tasdevice_load_block;
		pFirmware->bKernelFormat = false;
		break;
	default:
	if (pFw_fixed_hdr->mnDriverVersion == 0x100) {
		if (pFw_fixed_hdr->mnPPCVersion >= PPC3_VERSION) {
			tas_dev->fw_parse_variable_header =
				fw_parse_variable_header_kernel;
			tas_dev->fw_parse_program_data =
				fw_parse_program_data_kernel;
			tas_dev->fw_parse_configuration_data =
				fw_parse_configuration_data_kernel;
			tas_dev->tasdevice_load_block =
				tasdevice_load_block_kernel;
			tas_dev->fw_parse_calibration_data = NULL;
			break;
		} else {
			switch (pFw_fixed_hdr->mnPPCVersion) {
			case 0x00:
				tas_dev->fw_parse_variable_header =
					fw_parse_variable_header_git;
				tas_dev->fw_parse_program_data =
					fw_parse_program_data;
				tas_dev->fw_parse_configuration_data =
					fw_parse_configuration_data;
				tas_dev->fw_parse_calibration_data =
					fw_parse_calibration_data;
				tas_dev->tasdevice_load_block =
					tasdevice_load_block;
				break;
			default:
				dev_err(tas_dev->dev, "%s: PPCVersion must be "
					"0x0 or 0x%02x Current:0x%02x\n",
					__func__, PPC3_VERSION,
					pFw_fixed_hdr->mnPPCVersion);
				offset = -1;
				break;
			}
		}
	} else {
		dev_err(tas_dev->dev, "%s: DriverVersion must be 0x0, 0x230 "
			"or above 0x230:0x%02x\n", __func__,
			pFw_fixed_hdr->mnDriverVersion);
		offset = -1;
	}
		break;
	}

	offset = tas_dev->fw_parse_variable_header(tas_dev, pFW, offset);
	if (offset == -1)
		goto out;

	offset = tas_dev->fw_parse_program_data(pFirmware, pFW, offset);
	if (offset < 0) {
		ret = -1;
		goto out;
	}
	offset = tas_dev->fw_parse_configuration_data(pFirmware, pFW, offset);
	if (offset < 0)
		ret = -1;

out:
	return ret;
}

void tasdevice_calbin_remove(void *pContext)
{
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *) pContext;
	struct Ttasdevice *pTasdev = NULL;
	int i = 0;

	if (tas_dev) {
		for (i = 0; i < tas_dev->ndev; i++) {
			pTasdev = &(tas_dev->tasdevice[i]);
			if (pTasdev->mpCalFirmware) {
				tas2781_clear_Calfirmware(
					pTasdev->mpCalFirmware);
				pTasdev->mpCalFirmware = NULL;
			}
		}
	}
}

void tasdevice_dsp_remove(void *pContext)
{
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *) pContext;
	int i = 0;

	if (tas_dev) {
		if (tas_dev->mpFirmware) {
			struct TFirmware *pFirmware = tas_dev->mpFirmware;

			if (pFirmware->mpPrograms) {
				struct TProgram *pProgram;

				for (i = 0; i < pFirmware->mnPrograms; i++) {
					pProgram = &(pFirmware->mpPrograms[i]);
					if (pProgram) {
						struct TData *pImageData =
							&(pProgram->mData);

						if (pImageData->mpBlocks) {
							struct TBlock *pBlock;
							unsigned int nBlock;

							for (nBlock = 0;
								nBlock <
									pImageData->mnBlocks;
								nBlock++) {
								pBlock =
									&(pImageData->mpBlocks[nBlock]);
								if (pBlock) {
									kfree(pBlock->mpData);
								}
							}
						kfree(pImageData->mpBlocks);
						}
						kfree(pProgram->mpDescription);
					}
				}
				kfree(pFirmware->mpPrograms);
			}

			if (pFirmware->mpConfigurations) {
				struct TConfiguration *pConfig;

				for (i = 0; i < pFirmware->mnConfigurations;
					i++) {
					pConfig = &(pFirmware->
						mpConfigurations[i]);
					if (pConfig) {
						struct TData *pImageData =
							&(pConfig->mData);

						if (pImageData->mpBlocks) {
							struct TBlock *pBlock;
							unsigned int nBlock;

							for (nBlock = 0;
								nBlock <
									pImageData->mnBlocks;
								nBlock++) {
								pBlock =
									&(pImageData->mpBlocks[nBlock]);
								if (pBlock) {
									kfree(pBlock->mpData);
								}
							}
							kfree(pImageData->mpBlocks);
						}
						kfree(pConfig->mpDescription);
					}
				}
				kfree(pFirmware->mpConfigurations);
			}
			kfree(pFirmware->fw_hdr.mpDescription);
			kfree(pFirmware);
			tas_dev->mpFirmware = NULL;
		}
	}
}

void tasdevice_select_tuningprm_cfg(void *pContext, int prm_no,
	int cfg_no, int regbin_conf_no)
{
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *) pContext;
	struct tasdevice_regbin *regbin = &(tas_dev->mtRegbin);
	struct tasdevice_config_info **cfg_info = regbin->cfg_info;
	struct TFirmware *pFirmware = tas_dev->mpFirmware;
	struct TConfiguration *pConfigurations = NULL;
	struct TProgram *pProgram = NULL;
	int i = 0;
	int status = 0;

	if (pFirmware == NULL) {
		dev_err(tas_dev->dev, "%s: Firmware is NULL\n", __func__);
		goto out;
	}

	if (cfg_no >= pFirmware->mnConfigurations) {
		dev_err(tas_dev->dev,
			"%s: cfg(%d) is not in range of conf %u\n",
			__func__, cfg_no, pFirmware->mnConfigurations);
		goto out;
	}

	if (prm_no >= pFirmware->mnPrograms || prm_no == 1) {
		dev_err(tas_dev->dev,
			"%s: prm(%d) is not in range of Programs %u\n",
			__func__,  prm_no, pFirmware->mnPrograms);
		goto out;
	}

	if (regbin_conf_no > regbin->ncfgs || regbin_conf_no < 0 ||
		cfg_info == NULL) {
		dev_err(tas_dev->dev,
			"conf_no:%d should be in range from 0 to %u\n",
			regbin_conf_no, regbin->ncfgs-1);
		goto out;
	} else {
		dev_info(tas_dev->dev, "%s: regbin_profile_conf_id = %d\n",
			__func__, regbin_conf_no);
	}

	tas_dev->mnCurrentConfiguration = cfg_no;
	tas_dev->mnCurrentProgram = prm_no;

	pConfigurations = &(pFirmware->mpConfigurations[cfg_no]);
	for (i = 0; i < tas_dev->ndev; i++) {
		if (cfg_info[regbin_conf_no]->active_dev & (1 << i)) {
			if (tas_dev->tasdevice[i].mnCurrentProgram != prm_no) {
				tas_dev->tasdevice[i].mnCurrentConfiguration
					= -1;
				tas_dev->tasdevice[i].bLoading = true;
				status++;
			}
		} else
			tas_dev->tasdevice[i].bLoading = false;
		tas_dev->tasdevice[i].bLoaderr = false;
	}

	if (status) {
		pProgram = &(pFirmware->mpPrograms[prm_no]);
		tasdevice_load_data(tas_dev, &(pProgram->mData));
		for (i = 0; i < tas_dev->ndev; i++) {
			if (tas_dev->tasdevice[i].bLoaderr == true)
				continue;
			else if (tas_dev->tasdevice[i].bLoaderr == false
				&& tas_dev->tasdevice[i].bLoading == true) {
				struct TFirmware *pCalFirmware =
					tas_dev->tasdevice[i].mpCalFirmware;

				if (pCalFirmware) {
					struct TCalibration *pCalibration =
						pCalFirmware->mpCalibrations;

					if (pCalibration)
						tasdevice_load_calibrated_data(
							tas_dev,
							&(pCalibration->
							mData));
				}
				tas_dev->tasdevice[i].mnCurrentProgram
					= prm_no;
			}
		}
	}

	if(tas_dev->mbCalibrationLoaded == false) {
		for (i = 0; i < tas_dev->ndev; i++)
			tas_dev->set_calibration(tas_dev, i, 0x100);
		tas_dev->mbCalibrationLoaded = true;
		/* We don't want to reload calibrationdata everytime,
		this part will work once detected
		tas_dev->mbCalibrationLoaded == false at first time */
	}

	status = 0;
	for (i = 0; i < tas_dev->ndev; i++) {
		dev_info(tas_dev->dev, "%s,fun %d,%d,%d\n", __func__,
			tas_dev->tasdevice[i].mnCurrentConfiguration,
			cfg_info[regbin_conf_no]->active_dev,
			tas_dev->tasdevice[i].bLoaderr);
		if (tas_dev->tasdevice[i].mnCurrentConfiguration != cfg_no
			&& (cfg_info[regbin_conf_no]->active_dev & (1 << i))
			&& (tas_dev->tasdevice[i].bLoaderr == false)) {
			status++;
			tas_dev->tasdevice[i].bLoading = true;
		} else
			tas_dev->tasdevice[i].bLoading = false;
	}

	if (status) {
		status = 0;
		tasdevice_load_data(tas_dev, &(pConfigurations->mData));
		for (i = 0; i < tas_dev->ndev; i++) {
			if (tas_dev->tasdevice[i].bLoaderr == true) {
				status |= 1 << (i + 4);
				continue;
			} else if (tas_dev->tasdevice[i].bLoaderr == false
				&& tas_dev->tasdevice[i].bLoading == true) {
				tas_dev->tasdevice[i].mnCurrentConfiguration
					= cfg_no;
				tas_dev->tasdevice[i].bDSPBypass = false;
			}
		}
	} else {
		dev_err(tas_dev->dev,
			"%s: No device is in active in conf %d\n",
			__func__, regbin_conf_no);
	}

	status |= cfg_info[regbin_conf_no]->active_dev;
	dev_info(tas_dev->dev, "%s: DSP mode: load status is %08x\n",
		__func__, status);
out:
	return;
}

int tas2781_set_calibration(void *pContext, enum channel i,
	int nCalibration)
{
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *) pContext;
	int nResult = 0;
	struct Ttasdevice *pTasdev = &(tas_dev->tasdevice[i]);
	struct TFirmware *pCalFirmware = pTasdev->mpCalFirmware;

	dev_info(tas_dev->dev, "%s start\n", __func__);
	if ((!tas_dev->mpFirmware->mpPrograms)
		|| (!tas_dev->mpFirmware->mpConfigurations)) {
		dev_err(tas_dev->dev, "%s, Firmware not loaded\n\r", __func__);
		nResult = 0;
		goto out;
	}

	if (nCalibration == 0xFF || (nCalibration == 0x100
		&& pTasdev->mbCalibrationLoaded == false)) {
		if (pCalFirmware) {
			pTasdev->mbCalibrationLoaded = false;
			tas2781_clear_Calfirmware(pCalFirmware);
			pCalFirmware = NULL;
		}

		scnprintf(tas_dev->cal_binaryname[i], 64, "%s_cal_0x%02x.bin",
			tas_dev->dev_name, tas_dev->tasdevice[i].mnDevAddr);
		nResult = tas2781_load_calibration(tas_dev,
			tas_dev->cal_binaryname[i], i);
		if (nResult != 0) {
			dev_err(tas_dev->dev, "%s: load %s error, "
				"no-side effect for playback\n",
				__func__, tas_dev->cal_binaryname[i]);
			nResult = 0;
		}
	}
	pTasdev->bLoading = true;
	pTasdev->bLoaderr = false;

	if (pCalFirmware) {
		struct TCalibration *pCalibration =
			pCalFirmware->mpCalibrations;

		if (pCalibration)
			tasdevice_load_calibrated_data(tas_dev,
				&(pCalibration->mData));
	} else
		dev_err(tas_dev->dev,
			"%s: No calibrated data for device %d\n", __func__, i);

out:
	return nResult;
}
