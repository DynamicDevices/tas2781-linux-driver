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

#ifndef CONFIG_TASDEV_CODEC_SPI
#include <linux/module.h>
#include <linux/regmap.h>
//#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/firmware.h>

#include "tasdevice.h"
#include "tasdevice-rw.h"
#include "tasdevice-node.h"

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
		&tasdevice_regmap);
	if (IS_ERR(tas_dev->regmap)) {
		ret = PTR_ERR(tas_dev->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
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
