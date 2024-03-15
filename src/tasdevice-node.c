/*
 * TAS2563/TAS2871 Linux Driver
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

#include <linux/crc8.h>
#include <linux/firmware.h>
#include <linux/module.h>
#ifdef CONFIG_TASDEV_CODEC_SPI
	#include <linux/spi/spi.h>
#else
	#include <linux/i2c.h>
#endif
#include <linux/uaccess.h>
#include <sound/soc.h>

#include "tasdevice.h"
#include "tasdevice-rw.h"
#include "tasdevice-node.h"

static char gSysCmdLog[MaxCmd][256];

ssize_t dspfwinfo_list_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	struct tasdevice_fw *pFirmware = NULL;
	struct TProgram *pProgram = NULL;
	struct TConfiguration *pConfiguration = NULL;
	struct tasdevice_dspfw_hdr *pFw_hdr = NULL;
	const int size = PAGE_SIZE;
	int n = 0, i = 0, j = 0;

	mutex_lock(&tas_dev->file_lock);
	if (tas_dev == NULL) {
		if (n + 42 < size)
			n  += scnprintf(buf + n, size  - n,
				"ERROR: Can't find tasdevice_priv "
				"handle!\n\r");
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100,
				"\n[SmartPA]:%s Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
		}
		goto out;
	}
	pFirmware = tas_dev->fmw;
	if (pFirmware == NULL) {
		if (n + 42 < size)
			n  += scnprintf(buf + n, size  - n, "ERROR: No data "
				"in pTAS256x->mpFirmware!\n\r");
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100,
				"\n[SmartPA]:%s Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
		}
		goto out;
	}
	pFw_hdr = &(pFirmware->fw_hdr);
	if (pFw_hdr->ndev != tas_dev->ndev) {
		if (n + 65 < size)
			n  += scnprintf(buf + n, size  - n,
				"ERROR: device is NOT "
				"same between DTS(%u) and DSP(%u) "
				"firmware!\n\r",
				tas_dev->ndev, pFw_hdr->ndev);
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:%s "
				"Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
		}
		goto out;
	}
	if (n + 320 < size)
		n  += scnprintf(buf + n, size  - n,
			"%s\n%s format\nGenerated by PPC3V0x%X\n",
			tas_dev->dsp_binaryname,
			pFirmware->bKernelFormat == true ?
			"Kernel" : "Git",
			pFw_hdr->mnFixedHdr.ppcver);
	else {
		scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:%s "
			"Out of memory!\n\r", __func__);
		n = PAGE_SIZE;
		goto out;
	}
	if (n + 12 < size)
		n  += scnprintf(buf + n, size  - n, "\nndev: %u\n\r",
			pFw_hdr->ndev);
	else {
		scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:%s "
			"Out of memory!\n\r", __func__);
		n = PAGE_SIZE;
		goto out;
	}

	if (n + 16 < size)
		n  += scnprintf(buf + n, size - n, "mnPrograms: %d\n\r",
			pFirmware->nr_programs);
	else {
		scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:%s "
		"Out of memory!\n\r", __func__);
		n = PAGE_SIZE;
		goto out;
	}

	for (i = 0; i < pFirmware->nr_programs; i++) {
		pProgram = &(pFirmware->mpPrograms[i]);
		if (pProgram == NULL) {
			if (n + 33 < size)
				n  += scnprintf(buf + n, size  - n, "ERROR: "
					"No data in pProgram[%d]!\n\r", i);
			else {
				scnprintf(buf + PAGE_SIZE - 100, 100,
					"\n[SmartPA]:%s Out of memory!\n\r",
					__func__);
				n = PAGE_SIZE;
			}
			goto out;
		}
		if (n + 21 < size)
			n  += scnprintf(buf + n, size - n, "\tProgramName:\t");
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:"
				"%s Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
			goto out;
		}
		for (j = 0; j < 64; j++) {
			if (n + 2 < size)
				n  += scnprintf(buf + n, size - n, "%c",
					pProgram->mpName[j]);
			else {
				scnprintf(buf + PAGE_SIZE - 100, 100,
					"\n[SmartPA]:%s Out of memory!\n\r",
					__func__);
				n = PAGE_SIZE;
				goto out;
			}
		}
		if (n + 3 < size)
			n  += scnprintf(buf + n, size - n, "\n\r");
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:%s "
				"Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
			goto out;
		}
	}

	if (n + 22 < size)
		n  += scnprintf(buf + n, size - n, "mnConfigurations: %d\n\r",
			pFirmware->nr_configurations);
	else {
		scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:%s "
			"Out of memory!\n\r", __func__);
		n = PAGE_SIZE;
		goto out;
	}
	for (i = 0; i < pFirmware->nr_configurations; i++) {
		if (n + 16 < size)
			n  += scnprintf(buf + n, size - n,
				"\tnConfig:%d\n\r", i);
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100,
				"\n[SmartPA]:%s Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
			goto out;
		}
		pConfiguration = &(pFirmware->mpConfigurations[i]);
		if (pConfiguration == NULL) {
			if (n + 37 < size)
				n  += scnprintf(buf + n, size  - n, "ERROR: "
					"No data pConfiguration[%d]!\n\r", i);
			else {
				scnprintf(buf + PAGE_SIZE - 100, 100,
					"\n[SmartPA]: %s Out of memory!\n\r",
					__func__);
				n = PAGE_SIZE;
			}
			goto out;
		}
		if (n + 16 < size)
			n  += scnprintf(buf + n, size  - n, "\tConfigName:");
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:%s "
				"Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
			goto out;
		}
		for (j = 0; j < 64; j++)
			if (n + 2 < size)
				n  += scnprintf(buf + n, size  - n, "%c",
					pConfiguration->mpName[j]);
			else {
				scnprintf(buf + PAGE_SIZE - 100, 100,
					"\n[SmartPA]: %s Out of memory!\n\r",
					__func__);
				n = PAGE_SIZE;
				goto out;
			}

		if (n + 19 < size)
			n  += scnprintf(buf + n, size  - n,
				"\tProgram:0x%02x\n\r",
				pConfiguration->mProgram);
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:"
				"%s Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
			goto out;
		}
		if (n + 3 < size)
			n  += scnprintf(buf + n, size - n, "\n\r");
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100, "\n[SmartPA]:"
				"%s Out of memory!\n\r", __func__);
			n = PAGE_SIZE;
			goto out;
		}
	}
out:
	mutex_unlock(&tas_dev->file_lock);
	return n;

}

ssize_t active_address_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	const int size = 32;
	int n = 0;

	if (tas_dev != NULL) {
#ifdef CONFIG_TASDEV_CODEC_SPI
		struct spi_device *client =
			(struct spi_device *)tas_dev->client;
		n  += scnprintf(buf, size,
			"Active SmartPA - chn0x%02x\n", client->chip_select);
#else
		struct i2c_client *client =
			(struct i2c_client *)tas_dev->client;
		n  += scnprintf(buf, size,
			"Active SmartPA - addr0x%02x\n", client->addr);
#endif
	}
	return n;
}

ssize_t reg_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	ssize_t len = 0;
	const int size = PAGE_SIZE;
	int data = 0;
	int n_result = 0;

	if (tas_dev != NULL) {
		struct Tsyscmd *pSysCmd = &tas_dev->nSysCmd[RegSettingCmd];

		if (pSysCmd->bCmdErr == true) {
			len  += scnprintf(buf, pSysCmd->bufLen,
				gSysCmdLog[RegSettingCmd]);
			goto out;
		}

		if (pSysCmd->mnCurrentChannel >= tas_dev->ndev) {
			len = scnprintf(buf, PAGE_SIZE,
				"channel no is invalid\n");
			goto out;
		}
		//15 bytes
#ifdef CONFIG_TASDEV_CODEC_SPI
		len  += scnprintf(buf + len, size - len, "spi - chn: 0x%02x\n",
			client->chip_select);
#else
		len  += scnprintf(buf + len, size - len,
			"i2c - addr: 0x%02x\n",
			tas_dev->tasdevice[pSysCmd->mnCurrentChannel].mnDevAddr);
#endif
		//2560 bytes

		n_result = tasdevice_dev_read(tas_dev,
			pSysCmd->mnCurrentChannel,
			TASDEVICE_REG(pSysCmd->mnBook, pSysCmd->mnPage,
			pSysCmd->mnReg), &data);
		if (n_result < 0) {
			len  += scnprintf(buf, size - len,
				"[tasdevice]reg_show: read register failed\n");
			goto out;
		}
		//20 bytes
		if (len + 25 <= size)
			len  += scnprintf(buf + len, size - len,
				"Chn%dB0x%02xP0x%02xR0x%02x:0x%02x\n",
				pSysCmd->mnCurrentChannel, pSysCmd->mnBook,
				pSysCmd->mnPage, pSysCmd->mnReg, data);
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100,
				"[tasdevice]reg_show: mem is not enough: "
				"PAGE_SIZE = %lu\n", PAGE_SIZE);
			len = PAGE_SIZE;
		}
	}

out:
	return len;
}

ssize_t reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	int ret = 0;
	unsigned char kbuf[5];
	char *temp = NULL;
	int n_result = 0;
	struct Tsyscmd *pSysCmd = NULL;

	dev_dbg(tas_dev->dev, "reg: count = %d\n", (int)count);

	if (tas_dev == NULL)
		return count;
	pSysCmd = &tas_dev->nSysCmd[RegSettingCmd];
	pSysCmd->bufLen = snprintf(gSysCmdLog[RegSettingCmd], 256,
		"command: echo chn 0xBK 0xPG 0xRG 0xXX > NODE\n"
		"chn is channel no, should be 1 - digital;"
		"BK, PG, RG & XX must be 2 - digital HEX\n"
		"eg: echo 0 0x00 0x00 0x2 0xE1 > NODE\n\r");

	if (count >= 20) {
		temp = kmalloc(count, GFP_KERNEL);
		if (!temp) {
			pSysCmd->bCmdErr = true;
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegSettingCmd],
				15, "No memory!\n");
			goto out;
		}
		memcpy(temp, buf, count);
		ret = sscanf(temp, "%hd 0x%hhx 0x%hhx 0x%hhx 0x%hhx",
			(unsigned short *)&kbuf[0], &kbuf[1], &kbuf[2],
			&kbuf[3], &kbuf[4]);
		if (!ret) {
			pSysCmd->bCmdErr = true;
			goto out;
		}
		dev_dbg(tas_dev->dev,
			"[tasdevice]reg: chn=%d, book=0x%02x page=0x%02x "
			"reg=0x%02x val=0x%02x, cnt=%d\n", kbuf[0], kbuf[1],
			kbuf[2], kbuf[3], kbuf[4], (int)count);

		if (kbuf[0]  >= tas_dev->ndev) {
			pSysCmd->bCmdErr = true;
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegSettingCmd],
				20, "Channel no err!\n\r");
			goto out;
		}

		if (kbuf[3]&0x80) {
			pSysCmd->bCmdErr = true;
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegSettingCmd],
				46, "Register NO is larger than 0x7F!\n\r");
			goto out;
		}

		n_result = tasdevice_dev_write(tas_dev, kbuf[0],
			TASDEVICE_REG(kbuf[1], kbuf[2], kbuf[3]), kbuf[4]);
		if (n_result < 0)
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegSettingCmd],
				256 - pSysCmd->bufLen,
				"[tasdevice]reg: write "
				"chn%dB0x%02xP0x%02xR0x%02x "
				"failed.\n\r", kbuf[0], kbuf[1], kbuf[2],
				kbuf[3]);
		else {
			pSysCmd->bCmdErr = false;
			pSysCmd->mnCurrentChannel = kbuf[0];
			pSysCmd->mnBook = kbuf[1];
			pSysCmd->mnPage = kbuf[2];
			pSysCmd->mnReg = kbuf[3];
			gSysCmdLog[RegSettingCmd][0] = '\0';
			pSysCmd->bufLen = 0;
		}
	} else {
		pSysCmd->bCmdErr = true;
		ret = -1;
		dev_err(tas_dev->dev, "[tasdevice]reg: count error.\n");
	}
out:
	kfree(temp);
	return count;
}

ssize_t regdump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (tas_dev != NULL) {
		int i;
		const int size = PAGE_SIZE;

		int data = 0;
		int n_result = 0;
		struct Tsyscmd *pSysCmd = &tas_dev->nSysCmd[RegDumpCmd];

		if (pSysCmd->bCmdErr == true) {
			len = scnprintf(buf, PAGE_SIZE,
				gSysCmdLog[RegDumpCmd]);
			goto out;
		}

		if (pSysCmd->mnCurrentChannel >= tas_dev->ndev) {
			len = scnprintf(buf, PAGE_SIZE,
				"channel no is invalid\n");
			goto out;
		}

		//20 bytes
		if (len + 20 <= size)
#ifdef CONFIG_TASDEV_CODEC_SPI
			len  += scnprintf(buf + len, size - len,
				"addr: 0x%02x\n\r", client->chip_select);
#else
			len  += scnprintf(buf + len, size - len,
				"addr: 0x%02x\n\r",
				tas_dev->tasdevice[pSysCmd->mnCurrentChannel].mnDevAddr);
#endif
		else {
			scnprintf(buf + PAGE_SIZE - 64, 64,
				"[tasdevice]regdump: mem is not enough: "
				"PAGE_SIZE = %lu\n", PAGE_SIZE);
			len = PAGE_SIZE;
			goto out;
		}

		//2560 bytes
		for (i = 0; i < 128; i++) {
			n_result = tasdevice_dev_read(tas_dev,
				pSysCmd->mnCurrentChannel,
				TASDEVICE_REG(pSysCmd->mnBook,
				pSysCmd->mnPage, i), &data);
			if (n_result < 0) {
				len  += scnprintf(buf + len, size - len,
					"[tasdevice]regdump: read register "
					"failed!\n\r");
				break;
			}
			//20 bytes
			if (len + 20 <= size)
				len  += scnprintf(buf + len, size - len,
					"Chn%dB0x%02xP0x%02xR0x%02x:0x%02x\n",
					pSysCmd->mnCurrentChannel,
					pSysCmd->mnBook,
					pSysCmd->mnPage, i, data);
			else {
				scnprintf(buf + PAGE_SIZE - 64, 64,
					"[tasdevice]regdump: mem is not "
					"enough: PAGE_SIZE = %lu\n",
					PAGE_SIZE);
				len = PAGE_SIZE;
				break;
			}
		}
		if (len + 40 <= size)
			len  += scnprintf(buf + len, size - len,
				" == == == caught smartpa reg end "
				"== ==  == \n\r");
	}
out:
	return len;
}

ssize_t regdump_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	struct Tsyscmd *pSysCmd = NULL;
	int ret = 0;
	unsigned char kbuf[3];
	char *temp = NULL;

	dev_info(tas_dev->dev, "regdump: count = %d\n", (int)count);
	if (tas_dev == NULL)
		return count;
	pSysCmd = &tas_dev->nSysCmd[RegDumpCmd];
	pSysCmd->bufLen = snprintf(gSysCmdLog[RegDumpCmd],
		256, "command: echo chn 0xBK 0xPG > NODE\n"
		"chn is channel no, 1 - digital;"
		"BK & PG must be 2 - digital HEX\n"
		"PG must be 2 - digital HEX and less than or equeal to 4.\n"
		"eg: echo 2 0x00 0x00\n\r");

	if (count > 9) {
		temp = kmalloc(count, GFP_KERNEL);
		if (!temp) {
			pSysCmd->bCmdErr = true;
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegDumpCmd],
				20, "No Memory!\n\r");
			goto out;
		}
		memcpy(temp, buf, count);
		ret = sscanf(temp, "%hd 0x%hhx 0x%hhx",
			(unsigned short *)&kbuf[0], &kbuf[1], &kbuf[2]);
		if (!ret) {
			pSysCmd->bCmdErr = true;
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegDumpCmd],
				20, "Command err!\n\r");
			goto out;
		}

		if (kbuf[0] >= tas_dev->ndev) {
			pSysCmd->bCmdErr = true;
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegDumpCmd],
				20, "Channel no err!\n\r");
			goto out;
		}

		pSysCmd->bCmdErr = false;
		pSysCmd->mnCurrentChannel = kbuf[0];
		pSysCmd->mnBook = kbuf[1];
		pSysCmd->mnPage = kbuf[2];
		gSysCmdLog[RegDumpCmd][0] = '\0';
		pSysCmd->bufLen = 0;
	} else {
		pSysCmd->bCmdErr = true;
		pSysCmd->bufLen  += snprintf(&gSysCmdLog[RegDumpCmd][pSysCmd->bufLen],
			40, "Input params less than 9 bytes!\n\r");
		dev_err(tas_dev->dev, "[regdump] count error.\n");
	}
out:
	kfree(temp);
	return count;
}

ssize_t regbininfo_list_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	struct tasdevice_regbin *regbin = &(tas_dev->mtRegbin);
	struct tasdevice_config_info **cfg_info = regbin->cfg_info;
	int n = 0, i = 0;

	if (tas_dev == NULL) {
		if (n + 42 < PAGE_SIZE)
			n  += scnprintf(buf + n, PAGE_SIZE  - n,
				"ERROR: Can't find tasdevice_priv "
				"handle!\n\r");
		else {
			scnprintf(buf + PAGE_SIZE - 64, 64,
				"\n[regbininfo] Out of memory!\n\r");
			n = PAGE_SIZE;
		}
		return n;
	}
	mutex_lock(&tas_dev->codec_lock);
	if (n + 128 < PAGE_SIZE) {
		n  += scnprintf(buf + n, PAGE_SIZE  - n,
			"Regbin File Version: 0x%04X ",
			regbin->fw_hdr.binary_version_num);
		if (regbin->fw_hdr.binary_version_num < 0x105)
			n  += scnprintf(buf + n, PAGE_SIZE  - n,
				"No confname in this version");
		n  += scnprintf(buf + n, PAGE_SIZE  - n, "\n\r");
	} else {
		scnprintf(buf + PAGE_SIZE - 64, 64,
			"\n[regbininfo] Out of memory!\n\r");
		n = PAGE_SIZE;
		goto out;
	}

	for (i = 0; i < regbin->ncfgs; i++) {
		if (n + 16 < PAGE_SIZE)
			n  += scnprintf(buf + n, PAGE_SIZE  - n,
				"conf %02d", i);
		else {
			scnprintf(buf + PAGE_SIZE - 64, 64,
				"\n[regbininfo] Out of memory!\n\r");
			n = PAGE_SIZE;
			break;
		}
		if (regbin->fw_hdr.binary_version_num >= 0x105) {
			if (n + 100 < PAGE_SIZE)
				n  += scnprintf(buf + n, PAGE_SIZE - n,
					": %s\n\r", cfg_info[i]->mpName);
			else {
				scnprintf(buf + PAGE_SIZE - 64, 64,
					"\n[regbininfo] Out of memory!\n\r");
				n = PAGE_SIZE;
				break;
			}
		} else
			n  += scnprintf(buf + n, PAGE_SIZE - n, "\n\r");
	}
out:
	mutex_unlock(&tas_dev->codec_lock);
	return n;
}

ssize_t regcfg_list_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	struct tasdevice_regbin *regbin = NULL;
	int ret = 0;
	char *temp = NULL;
	struct Tsyscmd *pSysCmd = NULL;

	dev_info(tas_dev->dev, "regcfg: count = %d\n", (int)count);
	if (tas_dev == NULL)
		return count;
	mutex_lock(&tas_dev->codec_lock);
	regbin = &(tas_dev->mtRegbin);
	pSysCmd = &tas_dev->nSysCmd[RegCfgListCmd];
	pSysCmd->bufLen = snprintf(gSysCmdLog[RegCfgListCmd],
		256, "command: echo CG > NODE\n"
		"CG is conf NO, it should be 2 - digital decimal\n"
		"eg: echo 01 > NODE\n\r");

	if (count >= 1) {
		temp = kmalloc(count, GFP_KERNEL);
		if (!temp) {
			pSysCmd->bCmdErr = true;
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegCfgListCmd],
				15, "No memory!\n");
			goto out;
		}
		memcpy(temp, buf, count);
		ret = sscanf(temp, "%hhd", &(pSysCmd->mnPage));
		if (!ret) {
			pSysCmd->bCmdErr = true;
			goto out;
		}
		dev_info(tas_dev->dev, "[regcfg_list]cfg=%2d, cnt=%d\n",
			pSysCmd->mnPage, (int)count);
		if (pSysCmd->mnPage >= (unsigned char)regbin->ncfgs) {
			pSysCmd->bCmdErr = true;
			pSysCmd->bufLen  += snprintf(gSysCmdLog[RegCfgListCmd],
				30, "Wrong conf NO!\n\r");
		} else {
			pSysCmd->bCmdErr = false;
			gSysCmdLog[RegCfgListCmd][0] = '\0';
			pSysCmd->bufLen = 0;
		}
	} else {
		pSysCmd->bCmdErr = true;
		dev_err(tas_dev->dev, "[regcfg_list]: count error.\n");
	}
out:
	mutex_unlock(&tas_dev->codec_lock);
	kfree(temp);
	return count;
}

ssize_t regcfg_list_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	ssize_t len = 0;
	int j = 0, k = 0;

	if (tas_dev != NULL) {
		struct Tsyscmd *pSysCmd = &tas_dev->nSysCmd[RegCfgListCmd];
		struct tasdevice_regbin *regbin = &(tas_dev->mtRegbin);
		struct tasdevice_config_info **cfg_info = regbin->cfg_info;

		mutex_lock(&tas_dev->codec_lock);
		if (pSysCmd->bCmdErr == true ||
			pSysCmd->mnPage >= regbin->ncfgs) {
			len  += scnprintf(buf, pSysCmd->bufLen,
				gSysCmdLog[RegCfgListCmd]);
			goto out;
		}

		len  += scnprintf(buf + len, PAGE_SIZE - len,
			"Conf %02d", pSysCmd->mnPage);
		if (regbin->fw_hdr.binary_version_num >= 0x105) {
			if (len + 100 < PAGE_SIZE)
				len  += scnprintf(buf + len, PAGE_SIZE - len,
					": %s\n\r",
					cfg_info[pSysCmd->mnPage]->mpName);
			else {
				scnprintf(buf + PAGE_SIZE - 64, 64,
					"\n[tasdevice-regcfg_list] Out of "
					"memory!\n\r");
				len = PAGE_SIZE;
				goto out;
			}
		} else
			len  += scnprintf(buf + len, PAGE_SIZE - len, "\n\r");

		for (j = 0; j < (int)cfg_info[pSysCmd->mnPage]->real_nblocks;
			j++) {
			unsigned int length = 0, rc = 0;

			len  += scnprintf(buf + len, PAGE_SIZE - len,
				"block type:%s\t device idx = 0x%02x\n",
				blocktype[cfg_info[pSysCmd->mnPage]->
					blk_data[j]->block_type - 1],
				cfg_info[pSysCmd->mnPage]->blk_data[j]
					->dev_idx);
			for (k = 0; k <
				(int)cfg_info[pSysCmd->mnPage]->
					blk_data[j]->nSublocks;
				k++) {
				rc = tasdevice_process_block_show(tas_dev,
					cfg_info[pSysCmd->mnPage]->
						blk_data[j]->regdata
						 +  length,
					cfg_info[pSysCmd->mnBook]->
						blk_data[j]->dev_idx,
					cfg_info[pSysCmd->mnPage]->
						blk_data[j]->block_size
					- length, buf, &len);
				length  += rc;
				if (cfg_info[pSysCmd->mnPage]->
					blk_data[j]->block_size
					< length) {
					len  += scnprintf(buf + len,
						PAGE_SIZE - len,
						"tasdevice-regcfg_list: "
						"ERROR:%u %u "
						"out of memory\n", length,
						cfg_info[pSysCmd->mnPage]->
						blk_data[j]->block_size);
					break;
				}
			}
			if (length != cfg_info[pSysCmd->mnPage]->
				blk_data[j]->block_size) {
				len  += scnprintf(buf + len, PAGE_SIZE - len,
					"tasdevice-regcfg_list: ERROR: %u %u "
					"size is not same\n", length,
					cfg_info[pSysCmd->mnPage]->
					blk_data[j]->block_size);
			}
		}
out:
		mutex_unlock(&tas_dev->codec_lock);
	} else {
		if (len + 42 < PAGE_SIZE)
			len  += scnprintf(buf + len, PAGE_SIZE - len,
				"ERROR: Can't find tasdevice_priv "
				"handle!\n\r");
		else {
			scnprintf(buf + PAGE_SIZE - 100, 100,
				"\n[regbininfo] Out of memory!\n\r");
			len = PAGE_SIZE;
		}
	}
	return len;
}

ssize_t dspfw_config_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	struct tasdevice_fw *pFirmware = NULL;
	int n = 0;

	mutex_lock(&tas_dev->file_lock);
	pFirmware = tas_dev->fmw;
	if (pFirmware == NULL) {
		n = scnprintf(buf, 64, "No dsp fw is the system.\n");
		goto out;
	}
	if (!tas_dev->cur_prog)
		n = scnprintf(buf, 64, "%s\n",
			pFirmware->mpConfigurations[tas_dev->cur_conf].mpName);
	else
		n = scnprintf(buf, 64, "Current mode is bypass\n");

out:
	mutex_unlock(&tas_dev->file_lock);
	return n;
}

ssize_t force_fw_load_chip_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);

	mutex_unlock(&tas_dev->codec_lock);

	if (tas_dev == NULL)
		goto out;

	tasdevice_force_dsp_download(tas_dev);

out:
	mutex_unlock(&tas_dev->codec_lock);

	return count;
}

ssize_t force_fw_load_chip_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	struct tasdevice_t *tasdevice;
	int n = 0, i = 0;

	if (tas_dev != NULL) {
		n  += scnprintf(buf + n, 32, "Dev\tPrg No\tTimes\n");
		for (i = 0; i < tas_dev->ndev; i++) {
			tasdevice = &(tas_dev->tasdevice[i]);
			n += scnprintf(buf + n, 16, "%d\t", i);
			n += scnprintf(buf + n, 32, "%d\t",
				tasdevice->mnCurrentProgram);
			n += scnprintf(buf + n, 16, "%d\n",
				tasdevice->prg_download_cnt);
		}
	} else
		n += scnprintf(buf + n, 16, "Invalid data\n");

	return n;
}
