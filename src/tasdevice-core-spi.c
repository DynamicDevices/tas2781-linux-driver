// SPDX-License-Identifier: GPL-2.0
// Sound driver
// Copyright (C) 2001-2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#ifdef CONFIG_TASDEV_CODEC_SPI
#include <linux/module.h>
#include <linux/regmap.h>
//#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/firmware.h>

#include "tasdevice.h"
#include "tasdevice-rw.h"
#include "tasdevice-node.h"

static int tasdevice_spi_probe(struct spi_device *spi)
{
	struct tasdevice_priv *tas_dev = NULL;
	int ret = 0, i = 0;
	const struct spi_device_id *id = spi_get_device_id(spi);

	dev_err(&spi->dev, "%s, spi_device_id:%s", __func__, id->name);//
	tas_dev = devm_kzalloc(&spi->dev, sizeof(*tas_dev), GFP_KERNEL);
	if (!tas_dev) {
		dev_err(&spi->dev,
			"probe devm_kzalloc failed %d\n", ret);
		ret = -ENOMEM;
		goto out;
	}

	tas_dev->dev = &spi->dev;
	tas_dev->client = (void *)spi;
	tas_dev->chip_id = id->driver_data;

	if (spi->dev.of_node)
		ret = tasdevice_parse_dt(tas_dev);
	else {
		dev_err(tas_dev->dev, "No DTS info\n");
		goto out;
	}
	spi->mode = SPI_MODE_1;
	tas_dev->regmap = devm_regmap_init_spi(spi,
		&tasdevice_regmap);
	if (IS_ERR(tas_dev->regmap)) {
		ret = PTR_ERR(tas_dev->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	ret = tasdevice_probe_next(tas_dev);

out:
	if (ret < 0 && tas_dev != NULL)
		tasdevice_remove(tas_dev);
	return ret;
}

static int tasdevice_spi_remove(struct spi_device *pClient)
{
	struct tasdevice_priv *tas_dev = spi_get_drvdata(pClient);

	if (tas_dev)
		tasdevice_remove(tas_dev);
	return 0;
}

static struct spi_driver tasdevice_spi_driver = {
	.driver = {
		.name	= "tasdevice-codec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tasdevice_of_match),
		.pm = &tasdevice_pm_ops,
	},
	.probe		= tasdevice_spi_probe,
	.remove = tasdevice_spi_remove,
	.id_table	= tasdevice_id,
};

module_spi_driver(tasdevice_spi_driver);

MODULE_AUTHOR("Kevin Lu <kevin-lu@ti.com>");
MODULE_DESCRIPTION("ASoC TASDEVICE Driver");
MODULE_LICENSE("GPL");
#endif