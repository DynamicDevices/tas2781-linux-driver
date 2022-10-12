// SPDX-License-Identifier: GPL-2.0
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#include <linux/module.h>
#ifdef CONFIG_TASDEV_CODEC_SPI
	#include <linux/spi/spi.h>
#else
	#include <linux/i2c.h>
#endif
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_COMPAT
	#include <linux/compat.h>
#endif

#include "tasdevice-regbin.h"
#include "tasdevice.h"
#include "tasdevice-rw.h"
#include "tasdevice-ctl.h"
#include "tasdevice-node.h"
#include "tasdevice-codec.h"
#include "tasdevice-dsp.h"
#include "tasdevice-misc.h"

#define TASDEVICE_IRQ_DET_TIMEOUT		(30000)
#define TASDEVICE_IRQ_DET_CNT_LIMIT	(500)

static const char * dts_tag[] = {
	"ti,topleft-channel",
	"ti,topright-channel",
	"ti,bottomleft-channel",
	"ti,bottomright-channel",
	"ti,i2c-spi-gpio"
};

static const char *dts_xxx_tag = "ti,%s-gpio%d";

#ifdef CONFIG_TASDEV_CODEC_SPI
const struct spi_device_id tasdevice_id[] = {
	{ "audev", GENERAL_AUDEV },
	{ "tas2781", TAS2781	 },
	{}
};
MODULE_DEVICE_TABLE(spi, tasdevice_id);

#else
const struct i2c_device_id tasdevice_id[] = {
	{ "audev", GENERAL_AUDEV },
	{ "tas2781", TAS2781	 },
	{}
};

MODULE_DEVICE_TABLE(i2c, tasdevice_id);
#endif

static ssize_t devinfo_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);
	int n = 0, i = 0;

	if (tas_dev != NULL) {
		n  += scnprintf(buf + n, 32, "No.\tDevTyp\tAddr\n");
		for (i = 0; i < tas_dev->ndev; i++) {
			n  += scnprintf(buf + n, 16, "%d\t", i);
			n  += scnprintf(buf + n, 32, "%s\t",
				tasdevice_id[tas_dev->chip_id].name);
		}
	} else
		n  += scnprintf(buf + n, 16, "Invalid data\n");

	return n;
}

static bool tasdevice_volatile(struct device *dev, unsigned int reg)
{
	return true;
}

static bool tasdevice_writeable(struct device *dev, unsigned int reg)
{
	return true;
}

const struct regmap_config tasdevice_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tasdevice_writeable,
	.volatile_reg = tasdevice_volatile,
	.cache_type = REGCACHE_FLAT,
	.max_register = 1 * 128,
};

const struct of_device_id tasdevice_of_match[] = {
	{ .compatible = "ti,audev" },
	{ .compatible = "ti,tas2781" },
	{},
};

MODULE_DEVICE_TABLE(of, tasdevice_of_match);

static DEVICE_ATTR(reg, 0664, reg_show, reg_store);
static DEVICE_ATTR(regdump, 0664, regdump_show, regdump_store);
static DEVICE_ATTR(act_addr, 0664, active_address_show, NULL);
static DEVICE_ATTR(dspfwinfo_list, 0664, dspfwinfo_list_show,
	NULL);
static DEVICE_ATTR(regbininfo_list, 0664, regbininfo_list_show,
	NULL);
static DEVICE_ATTR(regcfg_list, 0664, regcfg_list_show,
	regcfg_list_store);
static DEVICE_ATTR(devinfo, 0664, devinfo_show, NULL);
static DEVICE_ATTR(dspfw_config, 0664, dspfw_config_show, NULL);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_reg.attr,
	&dev_attr_regdump.attr,
	&dev_attr_act_addr.attr,
	&dev_attr_dspfwinfo_list.attr,
	&dev_attr_regbininfo_list.attr,
	&dev_attr_regcfg_list.attr,
	&dev_attr_devinfo.attr,
	&dev_attr_dspfw_config.attr,
	NULL
};
//nodes are in /sys/devices/platform/XXXXXXXX.i2cX/i2c-X/
// Or /sys/bus/i2c/devices/7-004c/
const struct attribute_group tasdevice_attribute_group = {
	.attrs = sysfs_attrs
};

//IRQ
void tasdevice_enable_irq(
	struct tasdevice_priv *tas_dev, bool enable)
{
	struct irq_desc *desc = NULL;

	if (enable) {
		if (tas_dev->mIrqInfo.mb_irq_enable)
			return;
		if (gpio_is_valid(tas_dev->mIrqInfo.mn_irq_gpio)) {
			desc = irq_to_desc(tas_dev->mIrqInfo.mn_irq);
			if (desc && desc->depth > 0)
				enable_irq(tas_dev->mIrqInfo.mn_irq);
			else
				dev_info(tas_dev->dev,
					"### irq already enabled\n");
		}
		tas_dev->mIrqInfo.mb_irq_enable = true;
	} else {
		if (gpio_is_valid(tas_dev->mIrqInfo.mn_irq_gpio))
			disable_irq_nosync(tas_dev->mIrqInfo.mn_irq);
		tas_dev->mIrqInfo.mb_irq_enable = false;
	}
}

static void irq_work_routine(struct work_struct *pWork)
{
	struct tasdevice_priv *tas_dev =
		container_of(pWork, struct tasdevice_priv,
		mIrqInfo.irq_work.work);

	dev_info(tas_dev->dev, "%s enter\n", __func__);

	mutex_lock(&tas_dev->codec_lock);
	if (tas_dev->mb_runtime_suspend) {
		dev_info(tas_dev->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}
	/*Logical Layer IRQ function, return is ignored*/
	if (tas_dev->irq_work_func)
		tas_dev->irq_work_func(tas_dev);
	else
		dev_info(tas_dev->dev,
			"%s, irq_work_func is NULL\n", __func__);
end:
	mutex_unlock(&tas_dev->codec_lock);
	dev_info(tas_dev->dev, "%s leave\n", __func__);
}

static irqreturn_t tasdevice_irq_handler(int irq,
	void *dev_id)
{
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *)dev_id;
;
	/* get IRQ status after 100 ms */
	schedule_delayed_work(&tas_dev->mIrqInfo.irq_work,
		msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

int tasdevice_parse_dt(struct tasdevice_priv *tas_dev)
{
	struct device_node *np = tas_dev->dev->of_node;
	int rc = 0, i = 0, ndev = 0;
	char buf[32];


	for (i = 0, ndev = 0; i < MaxChn; i++) {
		rc = of_property_read_u32(np, dts_tag[i],
			&(tas_dev->tasdevice[ndev].mnDevAddr));
		if (rc) {
			dev_err(tas_dev->dev,
				"Looking up %s property in node %s failed "
				"%d\n", dts_tag[i], np->full_name, rc);
			continue;
		} else {
			dev_dbg(tas_dev->dev, "%s=0x%02x", dts_tag[i],
				tas_dev->tasdevice[ndev].mnDevAddr);
			ndev++;
		}
	}

	tas_dev->ndev = (ndev == 0) ? 1 : ndev;

	for (i = 0, ndev = 0; i < tas_dev->ndev; i++) {
		scnprintf(buf, sizeof(buf), dts_xxx_tag, "reset", i);
		tas_dev->tasdevice[ndev].mnResetGpio = of_get_named_gpio(np,
			buf, 0);
		if (gpio_is_valid(tas_dev->tasdevice[ndev].mnResetGpio)) {
			rc = gpio_request(tas_dev->tasdevice[ndev].mnResetGpio,
				buf);
			if (!rc) {
				gpio_direction_output(
					tas_dev->tasdevice[ndev].mnResetGpio,
					1);
				dev_info(tas_dev->dev, "%s = %d", buf,
					tas_dev->tasdevice[ndev].mnResetGpio);
				ndev++;
			} else
				dev_err(tas_dev->dev,
					"%s: Failed to request dev[%d] "
					"gpio %d\n", __func__, ndev,
					tas_dev->tasdevice[ndev].mnResetGpio);
		} else
			dev_err(tas_dev->dev,
				"Looking up %s property in node %s "
				"failed %d\n", buf, np->full_name,
				tas_dev->tasdevice[ndev].mnResetGpio);
	}
	dev_info(tas_dev->dev, "%s, chip_id:%d\n", __func__, tas_dev->chip_id);
	strcpy(tas_dev->dev_name, tasdevice_id[tas_dev->chip_id].name);
	if (tas_dev->chip_id != GENERAL_AUDEV) {
		tas_dev->mIrqInfo.mn_irq_gpio = of_get_named_gpio(np,
			"ti,irq-gpio", 0);
		if (gpio_is_valid(tas_dev->mIrqInfo.mn_irq_gpio)) {
			dev_dbg(tas_dev->dev, "irq-gpio = %d",
				tas_dev->mIrqInfo.mn_irq_gpio);
			INIT_DELAYED_WORK(&tas_dev->mIrqInfo.irq_work,
				irq_work_routine);

			rc = gpio_request(tas_dev->mIrqInfo.mn_irq_gpio,
						"AUDEV-IRQ");
			if (!rc) {
				gpio_direction_input(tas_dev->mIrqInfo.
					mn_irq_gpio);

				tas_dev->mIrqInfo.mn_irq =
					gpio_to_irq(tas_dev->mIrqInfo.
					mn_irq_gpio);
				dev_info(tas_dev->dev,
					"irq = %d\n",
					tas_dev->mIrqInfo.mn_irq);

				rc = request_threaded_irq(
					tas_dev->mIrqInfo.mn_irq,
					tasdevice_irq_handler,
					NULL, IRQF_TRIGGER_FALLING|
					IRQF_ONESHOT,
					SMARTAMP_MODULE_NAME, tas_dev);
				if (!rc)
					disable_irq_nosync(
						tas_dev->mIrqInfo.mn_irq);
				else
					dev_err(tas_dev->dev,
						"request_irq failed, %d\n",
						rc);
			} else
				dev_err(tas_dev->dev,
					"%s: GPIO %d request error\n",
					__func__,
					tas_dev->mIrqInfo.mn_irq_gpio);
		} else
			dev_err(tas_dev->dev, "Looking up irq-gpio property "
				"in node %s failed %d\n", np->full_name,
				tas_dev->mIrqInfo.mn_irq_gpio);
	}

	if (tas_dev->chip_id != GENERAL_AUDEV && rc == 0) {
		if (TAS2781 == tas_dev->chip_id)
			tas_dev->irq_work_func = tas2781_irq_work_func;
		else
			dev_info(tas_dev->dev, "%s: No match irq_work_func\n",
				__func__);
	}

	return 0;
}

int tasdevice_probe_next(struct tasdevice_priv *tas_dev)
{
	int nResult = 0, i = 0;

	tas_dev->mnCurrentProgram = -1;
	tas_dev->mnCurrentConfiguration = -1;

	for (i = 0; i < tas_dev->ndev; i++) {
		tas_dev->tasdevice[i].mnBkPg.mnBook = -1;
		tas_dev->tasdevice[i].mnBkPg.mnPage = -1;
		tas_dev->tasdevice[i].mnCurrentProgram = -1;
		tas_dev->tasdevice[i].mnCurrentConfiguration = -1;
	}
	mutex_init(&tas_dev->dev_lock);
	mutex_init(&tas_dev->file_lock);
	tas_dev->read = tasdevice_dev_read;
	tas_dev->write = tasdevice_dev_write;
	tas_dev->bulk_read = tasdevice_dev_bulk_read;
	tas_dev->bulk_write = tasdevice_dev_bulk_write;
	tas_dev->update_bits = tasdevice_dev_update_bits;
	tas_dev->set_calibration = tas2781_set_calibration;

	nResult = tasdevice_misc_register(tas_dev);
	if (nResult < 0) {
		dev_err(tas_dev->dev, "misc dev registration failed\n");
		goto out;
	}

	dev_set_drvdata(tas_dev->dev, tas_dev);
	nResult = sysfs_create_group(&tas_dev->dev->kobj,
				&tasdevice_attribute_group);
	if (nResult < 0) {
		dev_err(tas_dev->dev, "Sysfs registration failed\n");
		goto out;
	}

	INIT_DELAYED_WORK(&tas_dev->powercontrol_work,
		powercontrol_routine);

	mutex_init(&tas_dev->codec_lock);
	nResult = tasdevice_register_codec(tas_dev);
	if (nResult)
		dev_err(tas_dev->dev, "%s: codec register error:0x%08x\n",
			__func__, nResult);

	INIT_DELAYED_WORK(&tas_dev->mIrqInfo.irq_work, irq_work_routine);
	tas_dev->mIrqInfo.mb_irq_enable = false;

	dev_info(tas_dev->dev, "i2c register success\n");
out:
	return nResult;
}

void tasdevice_remove(struct tasdevice_priv *tas_dev)
{
	int i = 0;

	for (i = 0; i < tas_dev->mtRstGPIOs.ndev; i++) {
		if (gpio_is_valid(tas_dev->mtRstGPIOs.mnResetGpio[i]))
			gpio_free(tas_dev->mtRstGPIOs.mnResetGpio[i]);
	}
	if (gpio_is_valid(tas_dev->mnI2CSPIGpio))
		gpio_free(tas_dev->mnI2CSPIGpio);
	for (i = 0; i < tas_dev->ndev; i++) {
		if (gpio_is_valid(tas_dev->tasdevice[i].mnIRQGPIO))
			gpio_free(tas_dev->tasdevice[i].mnIRQGPIO);
	}

	if (delayed_work_pending(&tas_dev->mIrqInfo.irq_work)) {
		dev_info(tas_dev->dev, "cancel IRQ work\n");
		cancel_delayed_work(&tas_dev->mIrqInfo.irq_work);
	}
	cancel_delayed_work_sync(&tas_dev->mIrqInfo.irq_work);

	mutex_destroy(&tas_dev->dev_lock);
	mutex_destroy(&tas_dev->file_lock);
	mutex_destroy(&tas_dev->codec_lock);
	misc_deregister(&tas_dev->misc_dev);
	tasdevice_deregister_codec(tas_dev);
	sysfs_remove_group(&tas_dev->dev->kobj, &tasdevice_attribute_group);
}

static int tasdevice_pm_suspend(struct device *dev)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);

	if (!tas_dev) {
		dev_err(tas_dev->dev, "%s: drvdata is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&tas_dev->codec_lock);

	tas_dev->mb_runtime_suspend = true;

	if (tas_dev->chip_id != GENERAL_AUDEV) {
		if (delayed_work_pending(&tas_dev->mIrqInfo.irq_work)) {
			dev_dbg(tas_dev->dev, "cancel IRQ work\n");
			cancel_delayed_work_sync(&tas_dev->mIrqInfo.irq_work);
		}
	}
	mutex_unlock(&tas_dev->codec_lock);
	return 0;
}

static int tasdevice_pm_resume(struct device *dev)
{
	struct tasdevice_priv *tas_dev = dev_get_drvdata(dev);

	if (!tas_dev) {
		dev_err(tas_dev->dev, "%s: drvdata is NULL\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&tas_dev->codec_lock);
	tas_dev->mb_runtime_suspend = false;
	mutex_unlock(&tas_dev->codec_lock);
	return 0;
}

const struct dev_pm_ops tasdevice_pm_ops = {
	.suspend = tasdevice_pm_suspend,
	.resume = tasdevice_pm_resume
};
