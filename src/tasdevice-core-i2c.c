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
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include "tasdevice.h"
#include "tasdevice-rw.h"
#include "tasdevice-node.h"
#ifndef CONFIG_TASDEV_CODEC_SPI

static const struct regmap_range_cfg tasdevice_ranges[] = {
	{
		.range_min = 0,
		.range_max = 256 * 128,
		.selector_reg = TASDEVICE_PAGE_SELECT,
		.selector_mask = 0xff,
		.selector_shift = 0,
		.window_start = 0,
		.window_len = 128,
	},
};

static const struct regmap_config tasdevice_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.ranges = tasdevice_ranges,
	.num_ranges = ARRAY_SIZE(tasdevice_ranges),
	.max_register = 256 * 128,
};

static void tas2781_set_global_mode(struct tasdevice_priv *tas_dev)
{
	int i = 0, ret = 0;

	for (; i < tas_dev->ndev; i++) {
		ret = tasdevice_dev_update_bits(tas_dev, i,
			TAS2871_MISC_CFG2, TASDEVICE_GLOBAL_ADDR_MASK,
			TASDEVICE_GLOBAL_ADDR_ENABLE);
		if (ret < 0) {
			dev_err(tas_dev->dev, "chn %d set global fail, %d\n",
				i, ret);
		}
	}
}

static void tas2563_set_global_mode(struct tasdevice_priv *tas_dev)
{
	int i = 0, ret = 0;

	for (; i < tas_dev->ndev; i++) {
		ret = tasdevice_dev_update_bits(tas_dev, i,
			TAS2563_MISC_CFG2, TASDEVICE_GLOBAL_ADDR_MASK,
			TASDEVICE_GLOBAL_ADDR_ENABLE);
		if (ret < 0) {
			dev_err(tas_dev->dev, "chn %d set global fail, %d\n",
				i, ret);
		}
	}
}

static int tasdevice_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct tasdevice_priv *tas_dev = NULL;
	int ret = 0;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev,
			"%s: I2C check failed\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	tas_dev = devm_kzalloc(&i2c->dev, sizeof(*tas_dev), GFP_KERNEL);
	if (!tas_dev) {
		dev_err(&i2c->dev,
			"probe devm_kzalloc failed %d\n", ret);
		ret = -ENOMEM;
		goto out;
	}
	tas_dev->dev = &i2c->dev;
	tas_dev->client = (void *)i2c;
	tas_dev->chip_id = id->driver_data;

	if (i2c->dev.of_node)
		ret = tasdevice_parse_dt(tas_dev);
	else {
		dev_err(tas_dev->dev, "No DTS info\n");
		goto out;
	}

	tas_dev->regmap = devm_regmap_init_i2c(i2c,
		&tasdevice_i2c_regmap);
	if (IS_ERR(tas_dev->regmap)) {
		ret = PTR_ERR(tas_dev->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	if (tas_dev->glb_addr.dev_addr != 0
		&& tas_dev->glb_addr.dev_addr < 0x7F) {
		if (tas_dev->chip_id == TAS2781)
			tas_dev->set_global_mode = tas2781_set_global_mode;
		else
			tas_dev->set_global_mode = tas2563_set_global_mode;
	}

	ret = tasdevice_probe_next(tas_dev);

out:
	if (ret < 0 && tas_dev != NULL)
		tasdevice_remove(tas_dev);
	return ret;

}

static int tasdevice_i2c_remove(struct i2c_client *pClient)
{
	struct tasdevice_priv *tas_dev = i2c_get_clientdata(pClient);

	if (tas_dev)
		tasdevice_remove(tas_dev);

	return 0;
}

static struct i2c_driver tasdevice_i2c_driver = {
	.driver = {
		.name	= "tasdevice-codec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tasdevice_of_match),
		.pm = &tasdevice_pm_ops,
	},
	.probe	= tasdevice_i2c_probe,
	.remove = tasdevice_i2c_remove,
	.id_table	= tasdevice_id,
};

module_i2c_driver(tasdevice_i2c_driver);

MODULE_AUTHOR("Kevin Lu <kevin-lu@ti.com>");
MODULE_DESCRIPTION("ASoC TASDEVICE Driver");
MODULE_LICENSE("GPL");
#endif
