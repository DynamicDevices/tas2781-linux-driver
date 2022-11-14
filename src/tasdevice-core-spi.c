/*
* Copyright (C) 2022, Texas Instruments Incorporated
*
* All rights reserved not granted herein.
* Limited License.
*
* Texas Instruments Incorporated grants a world-wide, royalty-free,
* non-exclusive license under copyrights and patents it now or hereafter
* owns or controls to make, have made, use, import, offer to sell and sell
* ("Utilize"), this software subject to the terms herein. With respect to the
* foregoing patent license, such license is granted solely to the extent that
* any such patent is necessary to Utilize the software alone.
* The patent license shall not apply to any combinations which include this
* software, other than combinations with devices manufactured by or for
* TI (TAS2781). No hardware patent is licensed hereunder.
* Redistributions must preserve existing copyright notices and reproduce this
* license (including the above copyright notice and the disclaimer and
* (if applicable) source code license limitations below)
* in the documentation and/or other materials provided with the distribution
*
* Redistribution and use in binary form, without modification, are permitted
* provided that the following conditions are met:
*
* * No reverse engineering, decompilation, or disassembly of this software is
* permitted with respect to any software provided in binary form.
* * any redistribution and use are licensed by TI for use only with TI Devices.
* * Nothing shall obligate TI to provide you with source code for the software
* licensed and provided to you in object code.
*
* If software source code is provided to you, modification and redistribution
* of the source code are permitted
* provided that the following conditions are met:
*
* * any redistribution and use of the source code, including any resulting
* derivative works, are licensed by TI for use only with TI Devices.
* * any redistribution and use of any object code compiled from the source
* code and any resulting derivative works, are licensed by TI for use only
* with TI Devices.
*
* Neither the name of Texas Instruments Incorporated nor the names of its
* suppliers may be used to endorse or promote products derived from this
* software without specific prior written permission.
*
* DISCLAIMER.
*
* THIS SOFTWARE IS PROVIDED BY TI AND TI?S LICENSORS "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL TI AND TI?S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

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

	dev_info(&spi->dev, "%s, spi_device_id:%s", __func__, id->name);
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
