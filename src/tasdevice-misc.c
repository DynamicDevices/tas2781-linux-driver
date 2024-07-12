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

#ifdef CONFIG_COMPAT
	#include <linux/compat.h>
#endif
#include <linux/crc8.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_TASDEV_CODEC_SPI
	#include <linux/spi/spi.h>
#else
	#include <linux/i2c.h>
#endif

#include "tasdevice.h"
#include "tasdevice-misc.h"

#define	TIAUDIO_CMD_REG_WITE			1
#define	TIAUDIO_CMD_REG_READ			2
#define	TIAUDIO_CMD_REG_FCT			0xff

static int tasdev_fct_read(struct tasdevice_priv *tas_dev,
	char *buf, size_t count)
{
	unsigned int nCompositeRegister, Value;
	char rd_data[MAX_LENGTH];
	char reg_addr;
	size_t size;
	int ret;

	/* copy register address from user space */
	size = copy_from_user(&reg_addr, buf, 1);
	if (size != 0) {
		dev_err(tas_dev->dev, "read: copy_from_user failure\n");
		size = -EINVAL;
		goto out;
	}
	size = count;

	nCompositeRegister = TASDEVICE_REG(tas_dev->rwinfo.mBook,
				tas_dev->rwinfo.mPage, reg_addr);
	if (count == 1) {
		ret = tas_dev->read(tas_dev, tas_dev->rwinfo.mnCurrentChannel,
				nCompositeRegister, &Value);
		rd_data[0] = Value;
	} else if (count > 1) {
		ret = tas_dev->bulk_read(tas_dev,
			tas_dev->rwinfo.mnCurrentChannel, nCompositeRegister,
			rd_data, size);
	}
	if (ret < 0) {
		dev_err(tas_dev->dev,
			"%s, ret=%d, count=%zu error happen!\n",
			__func__, ret, count);
		size = ret;
		goto out;
	}

	if (copy_to_user(buf, rd_data, size) != 0) {
		dev_err(tas_dev->dev, "%s: copy_to_user failed\n", __func__);
		size = -EINVAL;
	}
out:
	return size;
}

static int tasdev_rccd2_read(struct tasdevice_priv *tas_dev,
	char *buf, size_t count)
{
	unsigned int nCompositeRegister, Value;
	char rd_data[MAX_LENGTH];
	size_t size = count;
	int ret;

	if (count > MAX_LENGTH) {
		dev_err(tas_dev->dev, "%s: read: size too large\n",
			__func__);
		size = -EINVAL;
		goto out;
	}
	nCompositeRegister = TASDEVICE_REG(tas_dev->rwinfo.mBook,
		tas_dev->rwinfo.mPage, tas_dev->rwinfo.mnCurrentReg);

	if (count == 1) {
		ret = tas_dev->read(tas_dev, tas_dev->rwinfo.mnCurrentChannel,
			nCompositeRegister, &Value);
		rd_data[0] = Value;
	} else if (count > 1) {
		ret = tas_dev->bulk_read(tas_dev,
			tas_dev->rwinfo.mnCurrentChannel, nCompositeRegister,
			rd_data, count);
	}

	if (ret < 0) {
		dev_err(tas_dev->dev,
			"%s, ret=%d, count=%zu error happen!\n",
			__func__, ret, count);
		size = ret;
		goto out;
	}

	if (copy_to_user(buf, rd_data, count) != 0) {
		/* Failed to copy all the data, exit */
		dev_err(tas_dev->dev, "%s: copy to user fail %d\n",
			__func__, ret);
		size = -EINVAL;
	}

out:
	return size;
}

static ssize_t tasdevice_read(struct file *file, char *buf,
			size_t count, loff_t *ppos)
{
	struct miscdevice *dev = file->private_data;
	struct tasdevice_priv *tas_dev = container_of(dev,
		struct tasdevice_priv, misc_dev);
	size_t size;

	mutex_lock(&tas_dev->file_lock);

	if (count > MAX_LENGTH) {
		dev_err(tas_dev->dev,
			"Max %d bytes can be read\n", MAX_LENGTH);
		size = -EINVAL;
		goto out;
	}


	switch (tas_dev->rwinfo.mnDBGCmd) {
	case TIAUDIO_CMD_REG_FCT:
	size = tasdev_fct_read(tas_dev, buf, count);
		break;
	case TIAUDIO_CMD_REG_READ:
	size = tasdev_rccd2_read(tas_dev, buf, count);
		break;
	default:
	dev_err(tas_dev->dev, "%s: No such cmd 0x%02x\n", __func__,
		tas_dev->rwinfo.mnDBGCmd);
	size = 0;
		break;
	}
out:
	tas_dev->rwinfo.mnDBGCmd = 0;
	mutex_unlock(&tas_dev->file_lock);
	return size;
}

#define TASDEV_DSP_BOOK				0x8C

static int tasdev_rccd2_dsp_write(struct tasdevice_priv *tas_dev,
	char *wr_data, int count)
{
	unsigned int nCompositeRegister;
	unsigned char *pData = wr_data;
	unsigned char i2c_addr;
	int size, ret, idx;

	if (pData[1] != TASDEV_DSP_BOOK) {
		dev_err(tas_dev->dev, "%s: 0x%02xnot dsp book!\n", __func__,
			pData[1]);
		return -EINVAL;
	}
	size = count;
	i2c_addr = pData[0] >> 1;

	for (idx = 0; idx < tas_dev->ndev; idx++) {
		if (i2c_addr == tas_dev->tasdevice[idx].mnDevAddr)
			break;
	}

	if (idx == tas_dev->ndev) {
		dev_err(tas_dev->dev, "%s: can't find device!\n", __func__);
		return -EINVAL;
	}
	nCompositeRegister = TASDEVICE_REG(pData[1], pData[2], pData[3]);

	ret = tas_dev->bulk_write(tas_dev, idx, nCompositeRegister, &pData[4],
		count - 4);

	if (ret < 0) {
		size = ret;
		dev_err(tas_dev->dev, "%s, ret=%d, count=%d, ERROR Happen\n",
			__func__, ret, (int)count);
	}

	return size;
}

static int tasdev_fct_write(struct tasdevice_priv *tas_dev,
	char *wr_data, int count)
{
	unsigned int nCompositeRegister, nRegister;
	char *pData = wr_data;
	int size, ret;


	nRegister = wr_data[0];
	size = count;
	if ((nRegister == 127) && (tas_dev->rwinfo.mPage == 0)) {
		tas_dev->rwinfo.mBook = wr_data[1];
		return size;
	}

	if (nRegister == 0) {
		tas_dev->rwinfo.mPage = wr_data[1];
		pData++;
		count--;
	}

	nCompositeRegister = TASDEVICE_REG(tas_dev->rwinfo.mBook,
		tas_dev->rwinfo.mPage, nRegister);
	if (count == 2)
		ret = tas_dev->write(tas_dev, tas_dev->rwinfo.mnCurrentChannel,
					nCompositeRegister, pData[1]);
	else if (count > 2)
		ret = tas_dev->bulk_write(tas_dev,
			tas_dev->rwinfo.mnCurrentChannel,
			nCompositeRegister, &pData[1], count - 1);

	if (!ret) {
		tas_dev->rwinfo.mnDBGCmd = TIAUDIO_CMD_REG_FCT;
	} else {
		size = ret;
		dev_err(tas_dev->dev, "%s, ret=%d, count=%d, ERROR Happen\n",
			__func__, ret, (int)count);
	}

	return size;
}

static int tasdev_rccd2_tas_write(struct tasdevice_priv *tas_dev,
	char *wr_data, int count, int offset)
{
	struct Trwinfo *p = &tas_dev->rwinfo;
	int ret;

	switch (p->mnDBGCmd) {
	case TIAUDIO_CMD_REG_WITE:
	if (count > 6) {
		unsigned int len = count - offset;
		unsigned int nCompositeRegister = TASDEVICE_REG(p->mBook,
			p->mPage, p->mnCurrentReg);

		if (len == 1)
			ret = tas_dev->write(tas_dev,
				tas_dev->rwinfo.mnCurrentChannel,
				nCompositeRegister, wr_data[offset]);
		else
			ret = tas_dev->bulk_write(tas_dev,
				tas_dev->rwinfo.mnCurrentChannel,
				nCompositeRegister, &wr_data[1], len);
		if (!ret)
			ret = count;
	} else {
		dev_err(tas_dev->dev, "%s, write len fail, count=%d.\n",
			__func__, (int)count);
		ret = -EINVAL;
	}
	p->mnDBGCmd = 0;
		break;
	case TIAUDIO_CMD_REG_READ:
		if (count != 6) {
			ret = -EINVAL;
			dev_err(tas_dev->dev, "read len fail.\n");
			memset(p, 0, sizeof(tas_dev->rwinfo));
		} else {
			ret = count;
		}
		break;
	}

	return ret;
}

#define CALIBRATION_BIN_SIZE	460
static int tasdev_load_calbin_from_user_space(
	struct tasdevice_priv *tas_dev, const char *buf)
{
	struct tasdev_buf *p = &tas_dev->calbin_buf;
	int size;

	if (CALIBRATION_BIN_SIZE > p->real_size) {
		if (p->real_size)
			kfree(p->buf);
		p->buf = kcalloc(CALIBRATION_BIN_SIZE, sizeof(char),
			GFP_KERNEL);
		if (!p->buf) {
			size = -ENOMEM;
			return size;
		}
		p->real_size = CALIBRATION_BIN_SIZE;
	}
	size = copy_from_user(p->buf, buf, CALIBRATION_BIN_SIZE);
	if (size != 0) {
		dev_err(tas_dev->dev,
			"copy_from_user failure %d bytes\n", size);
		size = -EINVAL;
		return size;
	}

	p->used_size = CALIBRATION_BIN_SIZE;
	return tasdevice_calbin_load(tas_dev);
}

static ssize_t tasdevice_write(struct file *file, const char *buf,
				size_t count, loff_t *ppos)
{
	struct miscdevice *dev = file->private_data;
	struct tasdevice_priv *tas_dev = container_of(dev, struct
		tasdevice_priv, misc_dev);
	char wr_data[MAX_LENGTH];
	int size;

	mutex_lock(&tas_dev->file_lock);

	dev_info(tas_dev->dev, "%s: count = %d\n", __func__, (int)count);

	if (count == CALIBRATION_BIN_SIZE) {
		size = tasdev_load_calbin_from_user_space(tas_dev, buf);
		goto out;
	}

	if (count > MAX_LENGTH) {
		dev_err(tas_dev->dev, "%s: write len is too large\n",
			__func__);
		size =  -EINVAL;
		goto out;
	}

	/* copy buffer from user space	*/
	size = copy_from_user(wr_data, buf, count);
	if (size != 0) {
		dev_err(tas_dev->dev, "copy_from_user failure %d bytes\n",
			size);
		size = -EINVAL;
		goto out;
	}

	if (count <= 5) {
		size = tasdev_fct_write(tas_dev, wr_data, count);
		goto out;
	}

	if (wr_data[0] == TIAUDIO_CMD_REG_WITE ||
		wr_data[0] == TIAUDIO_CMD_REG_READ) {
		tas_dev->rwinfo.mnDBGCmd = wr_data[0];
		tas_dev->rwinfo.mnCurrentChannel = wr_data[1];
		tas_dev->rwinfo.mBook = wr_data[3];
		if (wr_data[3] >= TASDEV_DSP_BOOK) {
			dev_info(tas_dev->dev,
				"%s: Pls check the package type 0x%02x\n",
				__func__, wr_data[3]);
		}
		tas_dev->rwinfo.mPage = wr_data[4];
		tas_dev->rwinfo.mnCurrentReg = wr_data[5];
		size = tasdev_rccd2_tas_write(tas_dev, wr_data, count, 6);
		goto out;
	}

	size = tasdev_rccd2_dsp_write(tas_dev, wr_data, count);

out:
	mutex_unlock(&tas_dev->file_lock);

	return size;
}

static void tiload_route_IO(struct tasdevice_priv *tas_dev,
	unsigned int bLock)
{
	if (bLock)
		tas_dev->write(tas_dev, tas_dev->rwinfo.mnCurrentChannel,
				0xAFFEAFFE, 0xBABEBABE);
	else
		tas_dev->write(tas_dev, tas_dev->rwinfo.mnCurrentChannel,
				0xBABEBABE, 0xAFFEAFFE);
}

static long tasdevice_ioctl(struct file *f,
	unsigned int cmd, void __user *arg)
{
	struct miscdevice *dev = f->private_data;
	struct tasdevice_priv *tas_dev = container_of(dev,
		struct tasdevice_priv, misc_dev);
	int ret = 0, i = 0;
	unsigned long val = 0;
	/* Misc Lock Hold */
	dev_info(tas_dev->dev, "%s: cmd = 0x%08x\n", __func__, cmd);
	mutex_lock(&tas_dev->file_lock);
	switch (cmd) {
	case TILOAD_IOMAGICNUM_GET:
	dev_info(tas_dev->dev,
		"%s, cmd=TILOAD_IOMAGICNUM_GET\n", __func__);
	ret = copy_to_user(arg, &tas_dev->magic_num, sizeof(int));
	break;
	case TILOAD_IOMAGICNUM_SET:
	dev_info(tas_dev->dev,
		"%s, cmd=TILOAD_IOMAGICNUM_SET\n", __func__);
	ret = copy_from_user(&tas_dev->magic_num, arg, sizeof(int));
	tiload_route_IO(tas_dev, tas_dev->magic_num);
	break;
	case TILOAD_BPR_READ:
	dev_info(tas_dev->dev,
		"%s, cmd=TILOAD_BPR_READ\n", __func__);
	break;
	case TILOAD_BPR_WRITE: {
		struct BPR bpr;

		ret = copy_from_user(&bpr, arg, sizeof(struct BPR));
		dev_info(tas_dev->dev,
			"TILOAD_BPR_WRITE: 0x%02X, 0x%02X, 0x%02X\n\r",
			bpr.nBook, bpr.nPage, bpr.nRegister);
	}
		break;
	case TILOAD_IOCTL_SET_CHL:
	mutex_lock(&tas_dev->dev_lock);
	{
		unsigned char addr;

		ret = copy_from_user(&val, arg, sizeof(int));
		addr = (unsigned char)(val>>1);
		dev_info(tas_dev->dev,
			"%s, cmd=TILOAD_IOCTL_SET_CHL\n", __func__);

		for (i = 0; i < tas_dev->ndev; i++)
			if (tas_dev->tasdevice[i].mnDevAddr == addr) {
				tas_dev->rwinfo.mnCurrentChannel = i;
				dev_info(tas_dev->dev,
					"TILOAD_IOCTL_SET_CHL(%d) = 0x%x\n",
					i, addr);
				break;
			}

		if (i == tas_dev->ndev)
			dev_err(tas_dev->dev, "TILOAD_IOCTL_SET_CHL: "
				"err addr = 0x%02x\n\r", addr);
	}
	mutex_unlock(&tas_dev->dev_lock);
		break;
	case TILOAD_IOCTL_SET_CALIBRATION:
	dev_info(tas_dev->dev,
		"%s, cmd=TILOAD_IOCTL_SET_CALIBRATION\n", __func__);
	ret = copy_from_user(&val, arg, sizeof(val));
	if (ret  == 0) {
		for (i = 0; i < tas_dev->ndev; i++)
			tas_dev->set_calibration(tas_dev, i, val);
	} else {
		dev_err(tas_dev->dev,
			"%s: error copy from user "
			"cmd=TILOAD_IOCTL_SET_CALIBRATION=0x%08x\n",
			__func__,
			(unsigned int)TILOAD_IOCTL_SET_CALIBRATION);
	}
		break;
	case TILOAD_IOC_MAGIC_PA_INFO_GET:
	{
		struct smartpa_info a;

		strcpy(a.dev_name, tas_dev->dev_name);
		a.ndev = tas_dev->ndev;
		for (i = 0; i < a.ndev; i++)
			a.i2c_list[i] =
				tas_dev->tasdevice[i].mnDevAddr;
		a.bSPIEnable = tas_dev->mnSPIEnable?1:0;
		ret = copy_to_user(arg, &a,
			sizeof(struct smartpa_info));
	}
		break;
	case TILOAD_IOC_MAGIC_SET_DEFAULT_CALIB_PRI:
		break;
	case TILOAD_IOC_MAGIC_POWER_OFF:
	if (tas_dev->pstream != 0 && tas_dev->cstream == 0) {
		tas_dev->pstream = 0;
		mutex_lock(&tas_dev->codec_lock);
		if (gpio_is_valid(tas_dev->irq_info.irq_gpio))
			tasdevice_enable_irq(tas_dev, false);
		tasdevice_select_cfg_blk(tas_dev, tas_dev->cur_conf,
			TASDEVICE_BIN_BLK_PRE_SHUTDOWN);
		dev_info(tas_dev->dev, "%s: cmd=TILOAD_IOC_MAGIC_POWER_OFF"
			"=0x%08x: regscene = %d\n", __func__,
			(unsigned int)TILOAD_IOC_MAGIC_POWER_OFF,
			tas_dev->mtRegbin.profile_cfg_id);
		if (tas_dev->mtRegbin.profile_cfg_id ==
				TASDEVICE_CALIBRATION_PROFILE)
			tasdevice_force_dsp_download(tas_dev);
		mutex_unlock(&tas_dev->codec_lock);
	} else {
		dev_info(tas_dev->dev, "%s:%u: AMP is already power off\n",
			__func__, __LINE__);
	}
		break;
	case TILOAD_IOC_MAGIC_POWERON: {
		struct smartpa_params param;

		dev_info(tas_dev->dev, "%s:%u: pstream is %d\n",
			__func__, __LINE__, tas_dev->pstream);

		tas_dev->pstream = 1;
		ret = copy_from_user(&param, arg,
			sizeof(struct smartpa_params));
		if(ret == 0) {
			dev_info(tas_dev->dev,
				"%s, TILOAD_IOC_MAGIC_POWERON=0x%08x:"
				"mProg = %d, config = %d, regscene = "
				"%d\n", __func__,
				(unsigned int)TILOAD_IOC_MAGIC_POWERON,
				param.mProg,
				param.config, param.regscene);
			tas_dev->cur_prog = param.mProg;
			tas_dev->cur_conf = param.config;
			tas_dev->mtRegbin.profile_cfg_id =
				param.regscene;
			schedule_delayed_work(
				&tas_dev->powercontrol_work,
				msecs_to_jiffies(20));
		} else {
			dev_err(tas_dev->dev,
				"%s: error copy from user cmd="
				"TILOAD_IOC_MAGIC_POWERON=0x%08x\n",
				__func__,
				(unsigned int)TILOAD_IOC_MAGIC_POWERON);
		}
	}
		break;
	case TILOAD_IOCTL_SET_CONFIG: {
		int config;

		ret = copy_from_user(&config, arg, sizeof(config));
		if(ret == 0) {
			tas_dev->cur_conf = config;
			tas_dev->mtRegbin.profile_cfg_id =
				config;
			schedule_delayed_work(
				&tas_dev->powercontrol_work,
				msecs_to_jiffies(20));
		} else
			dev_err(tas_dev->dev,
				"%s: error copy from user cmd="
				"TILOAD_IOCTL_SET_CONFIG=0x%08x\n",
				__func__,
				(unsigned int)TILOAD_IOCTL_SET_CONFIG);
	}
		break;
	default:
	dev_info(tas_dev->dev, "%s:COMMAND = 0x%08x Not Imple\n",
		__func__, cmd);
		break;
	}
		/* Misc Lock Release*/
	mutex_unlock(&tas_dev->file_lock);
	return ret;
}

static long tasdevice_unlocked_ioctl(struct file *f,
	unsigned int cmd, unsigned long arg)
{
	return tasdevice_ioctl(f, cmd, (void __user *)arg);
}


#ifdef CONFIG_COMPAT
static long tasdevice_compat_ioctl(struct file *f,
	unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = f->private_data;
	struct tasdevice_priv *tas_dev = container_of(dev,
		struct tasdevice_priv, misc_dev);
	unsigned int cmd64;

	switch (cmd) {
	case TILOAD_COMPAT_IOMAGICNUM_GET:
	dev_info(tas_dev->dev,
		"%s, TILOAD_COMPAT_IOMAGICNUM_GET=0x%08x\n",
		__func__, cmd);
	cmd64 = TILOAD_IOMAGICNUM_GET;
		break;
	case TILOAD_COMPAT_IOMAGICNUM_SET:
	dev_info(tas_dev->dev,
		"%s, TILOAD_COMPAT_IOMAGICNUM_SET=0x%x\n",
		__func__, cmd);
	cmd64 = TILOAD_IOMAGICNUM_SET;
		break;
	case TILOAD_COMPAT_BPR_READ:
	dev_info(tas_dev->dev, "%s, TILOAD_COMPAT_BPR_READ=0x%x\n",
		__func__, cmd);
	cmd64 = TILOAD_BPR_READ;
		break;
	case TILOAD_COMPAT_BPR_WRITE:
	dev_info(tas_dev->dev, "%s, TILOAD_COMPAT_BPR_WRITE=0x%x\n",
		__func__, cmd);
	cmd64 = TILOAD_BPR_WRITE;
		break;
	case TILOAD_IOC_COMPAT_MAGIC_PA_INFO_GET:
	cmd64 = TILOAD_IOC_MAGIC_PA_INFO_GET;
		break;
	case TILOAD_IOC_MAGIC_SET_DEFAULT_CALIB_PRI_COMPAT:
	cmd64 = TILOAD_IOC_MAGIC_SET_DEFAULT_CALIB_PRI;
		break;
	case TILOAD_COMPAT_IOCTL_SET_CHL:
	cmd64 = TILOAD_IOCTL_SET_CHL;
		break;
	case TILOAD_COMPAT_IOCTL_SET_CALIBRATION:
	cmd64 = TILOAD_IOCTL_SET_CALIBRATION;
		break;
	case TILOAD_IOC_MAGIC_POWER_OFF_COMPAT:
	cmd64 = TILOAD_IOC_MAGIC_POWER_OFF;
		break;
	case TILOAD_IOC_MAGIC_POWERON_COMPAT:
	cmd64 = TILOAD_IOC_MAGIC_POWERON;
		break;
	case TILOAD_COMPAT_IOCTL_SET_CONFIG:
	cmd64 = TILOAD_IOCTL_SET_CONFIG;
		break;
	default:
	dev_info(tas_dev->dev, "%s:COMMAND = 0x%08x Not Imple\n",
		__func__, cmd);
		return 0;
	}
	return tasdevice_ioctl(f, cmd64, (void __user *)arg);
}
#endif

const struct file_operations tasdevice_fops = {
	.owner = THIS_MODULE,
	.read = tasdevice_read,
	.write = tasdevice_write,
	.unlocked_ioctl = tasdevice_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tasdevice_compat_ioctl,
#endif
};

int tasdevice_misc_register(struct tasdevice_priv *tas_dev)
{
	int nResult;
	tas_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	tas_dev->misc_dev.name = "tiload_node";
	tas_dev->misc_dev.fops = &tasdevice_fops;

	nResult = misc_register(&tas_dev->misc_dev);
	if (nResult) {
		dev_err(tas_dev->dev, "error creating misc device\n");
		goto out;
	}

out:
    return nResult;
}
