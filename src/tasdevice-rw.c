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

#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/firmware.h>

#include "tasdevice.h"

static int tasdevice_regmap_write(
	struct tasdevice_priv *tas_dev,
	unsigned int reg, unsigned int value)
{
	int nResult = 0;
	int retry_count = TASDEVICE_RETRY_COUNT;

	while (retry_count--) {
		nResult = regmap_write(tas_dev->regmap, reg,
			value);
		if (nResult >= 0)
			break;
		usleep_range(5000, 5050);
	}
	if (retry_count == -1)
		return TASDEVICE_ERROR_FAILED;
	else
		return 0;
}

static int tasdevice_regmap_bulk_write(
	struct tasdevice_priv *tas_dev, unsigned int reg,
	unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;
	int retry_count = TASDEVICE_RETRY_COUNT;

	while (retry_count--) {
		nResult = regmap_bulk_write(tas_dev->regmap, reg,
				pData, nLength);
		if (nResult >= 0)
			break;
		usleep_range(5000, 5050);
	}
	if (retry_count == -1)
		return TASDEVICE_ERROR_FAILED;
	else
		return 0;
}

static int tasdevice_regmap_read(
	struct tasdevice_priv *tas_dev,
	unsigned int reg, unsigned int *value)
{
	int nResult = 0;
	int retry_count = TASDEVICE_RETRY_COUNT;

	while (retry_count--) {
		nResult = regmap_read(tas_dev->regmap, reg,
			value);
		if (nResult >= 0)
			break;
		usleep_range(5000, 5050);
	}
	if (retry_count == -1)
		return TASDEVICE_ERROR_FAILED;
	else
		return 0;
}

static int tasdevice_regmap_bulk_read(
	struct tasdevice_priv *tas_dev, unsigned int reg,
	unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;
	int retry_count = TASDEVICE_RETRY_COUNT;

	while (retry_count--) {
		nResult = regmap_bulk_read(tas_dev->regmap, reg,
			pData, nLength);
		if (nResult >= 0)
			break;
		usleep_range(5000, 5050);
	}
	if (retry_count == -1)
		return TASDEVICE_ERROR_FAILED;
	else
		return 0;
}

static int tasdevice_regmap_update_bits(
	struct tasdevice_priv *tas_dev, unsigned int reg,
	unsigned int mask, unsigned int value)
{
	int nResult = 0;
	int retry_count = TASDEVICE_RETRY_COUNT;

	while (retry_count--) {
		nResult = regmap_update_bits(tas_dev->regmap, reg,
			mask, value);
		if (nResult >= 0)
			break;
		usleep_range(5000, 5050);
	}
	if (retry_count == -1)
		return TASDEVICE_ERROR_FAILED;
	else
		return 0;
}

static int tasdevice_change_chn_book_page(
	struct tasdevice_priv *tas_dev, enum channel chn,
	int book, int page)
{
	int n_result = 0;

#ifdef CONFIG_TASDEV_CODEC_SPI
	struct spi_device *pClient =
		(struct spi_device *)tas_dev->client;
#else
	struct i2c_client *pClient =
		(struct i2c_client *)tas_dev->client;
#endif

	if (chn < tas_dev->ndev) {
#ifdef CONFIG_TASDEV_CODEC_SPI
		pClient->chip_select = tas_dev->tasdevice[chn].mnDevAddr;
#else
		pClient->addr = tas_dev->tasdevice[chn].mnDevAddr;
#endif

		if (tas_dev->tasdevice[chn].mnBkPg.mnBook != book) {
			n_result = tasdevice_regmap_write(tas_dev,
				TASDEVICE_BOOKCTL_PAGE, 0);
			if (n_result < 0) {
				dev_err(tas_dev->dev, "%s, ERROR, E=%d\n",
					__func__, n_result);
				goto out;
			}
			tas_dev->tasdevice[chn].mnBkPg.mnPage = 0;
			n_result = tasdevice_regmap_write(tas_dev,
				TASDEVICE_BOOKCTL_REG, book);
			if (n_result < 0) {
				dev_err(tas_dev->dev, "%s, ERROR, E=%d\n",
					__func__, n_result);
				goto out;
			}
			tas_dev->tasdevice[chn].mnBkPg.mnBook = book;
		}

		if (tas_dev->tasdevice[chn].mnBkPg.mnPage != page) {
			n_result = tasdevice_regmap_write(tas_dev,
				TASDEVICE_BOOKCTL_PAGE, page);
			if (n_result < 0) {
				dev_err(tas_dev->dev, "%s, ERROR, E=%d\n",
					__func__, n_result);
				goto out;
			}
			tas_dev->tasdevice[chn].mnBkPg.mnPage = page;
		}
	} else
		dev_err(tas_dev->dev, "%s, ERROR, no such channel(%d)\n",
			__func__, chn);

out:
	return n_result;
}

int tasdevice_dev_read(struct tasdevice_priv *tas_dev,
	enum channel chn, unsigned int reg, unsigned int *pValue)
{
	int n_result = 0;
#ifdef CONFIG_TASDEV_CODEC_SPI
	struct spi_device *pClient =
		(struct spi_device *)tas_dev->client;
#else
	struct i2c_client *pClient =
		(struct i2c_client *)tas_dev->client;
#endif

	mutex_lock(&tas_dev->dev_lock);
	if (chn < tas_dev->ndev) {
		n_result = tasdevice_change_chn_book_page(tas_dev, chn,
			TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg));
		if (n_result < 0)
			goto out;

#ifdef CONFIG_TASDEV_CODEC_SPI
		pClient->chip_select = tas_dev->tasdevice[chn].mnDevAddr;
#else
		pClient->addr = tas_dev->tasdevice[chn].mnDevAddr;
#endif
		n_result = tasdevice_regmap_read(tas_dev,
			TASDEVICE_PAGE_REG(reg), pValue);
		if (n_result < 0)
			dev_err(tas_dev->dev, "%s, ERROR,E=%d\n",
				__func__, n_result);
		else
			dev_dbg(tas_dev->dev,
				"%s: chn:0x%02x:BOOK:PAGE:REG 0x%02x:0x%02x:"
				"0x%02x, 0x%02x\n", __func__,
				tas_dev->tasdevice[chn].mnDevAddr,
				TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg),
				TASDEVICE_PAGE_REG(reg), *pValue);
	} else
		dev_err(tas_dev->dev, "%s, ERROR, no such channel(%d)\n",
			__func__, chn);

out:
	mutex_unlock(&tas_dev->dev_lock);
	return n_result;
}


int tasdevice_dev_write(struct tasdevice_priv *tas_dev,
	enum channel chn, unsigned int reg, unsigned int value)
{
	int n_result = 0;

#ifdef CONFIG_TASDEV_CODEC_SPI
	struct spi_device *pClient =
		(struct spi_device *)tas_dev->client;
#else
	struct i2c_client *pClient =
		(struct i2c_client *)tas_dev->client;
#endif

	mutex_lock(&tas_dev->dev_lock);
	if (chn < tas_dev->ndev) {
		n_result = tasdevice_change_chn_book_page(tas_dev, chn,
			TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg));
		if (n_result < 0)
			goto out;

#ifdef CONFIG_TASDEV_CODEC_SPI
		pClient->chip_select = tas_dev->tasdevice[chn].mnDevAddr;
#else
		pClient->addr = tas_dev->tasdevice[chn].mnDevAddr;
#endif
		n_result = tasdevice_regmap_write(tas_dev,
			TASDEVICE_PAGE_REG(reg), value);
		if (n_result < 0)
			dev_err(tas_dev->dev, "%s, ERROR, E=%d\n",
				__func__, n_result);
		else
			dev_dbg(tas_dev->dev, "%s: chn0x%02x:BOOK:PAGE:REG "
				"0x%02x:0x%02x:0x%02x, VAL: 0x%02x\n",
				__func__, tas_dev->tasdevice[chn].mnDevAddr,
				TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg),
				TASDEVICE_PAGE_REG(reg), value);
	} else
		dev_err(tas_dev->dev, "%s, ERROR, no such channel(%d)\n",
			__func__, chn);
out:
	mutex_unlock(&tas_dev->dev_lock);
	return n_result;
}


int tasdevice_dev_bulk_write(
	struct tasdevice_priv *tas_dev, enum channel chn,
	unsigned int reg, unsigned char *p_data,
	unsigned int n_length)
{
	int n_result = 0;
#ifdef CONFIG_TASDEV_CODEC_SPI
	struct spi_device *pClient =
		(struct spi_device *)tas_dev->client;
#else
	struct i2c_client *pClient =
		(struct i2c_client *)tas_dev->client;
#endif
	mutex_lock(&tas_dev->dev_lock);
	if (chn < tas_dev->ndev) {
		n_result = tasdevice_change_chn_book_page(tas_dev, chn,
			TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg));
		if (n_result < 0)
			goto out;

#ifdef CONFIG_TASDEV_CODEC_SPI
		pClient->chip_select = tas_dev->tasdevice[chn].mnDevAddr;
#else
		pClient->addr = tas_dev->tasdevice[chn].mnDevAddr;
#endif
		n_result = tasdevice_regmap_bulk_write(tas_dev,
			TASDEVICE_PAGE_REG(reg), p_data, n_length);
		if (n_result < 0)
			dev_err(tas_dev->dev, "%s, ERROR, E=%d\n",
				__func__, n_result);
		else
			dev_dbg(tas_dev->dev,
				"%s: chn0x%02x:BOOK:PAGE:REG 0x%02x:0x%02x:"
				"0x%02x, len: 0x%02x\n", __func__,
				tas_dev->tasdevice[chn].mnDevAddr,
				TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg),
				TASDEVICE_PAGE_REG(reg), n_length);
	} else
		dev_err(tas_dev->dev, "%s, ERROR, no such channel(%d)\n",
			__func__, chn);
out:
	mutex_unlock(&tas_dev->dev_lock);
	return n_result;
}


int tasdevice_dev_bulk_read(struct tasdevice_priv *tas_dev,
	enum channel chn, unsigned int reg, unsigned char *p_data,
	unsigned int n_length)
{
	int n_result = 0;
#ifdef CONFIG_TASDEV_CODEC_SPI
	struct spi_device *pClient =
		(struct spi_device *)tas_dev->client;
#else
	struct i2c_client *pClient =
		(struct i2c_client *)tas_dev->client;
#endif
	mutex_lock(&tas_dev->dev_lock);
	if (chn < tas_dev->ndev) {
		n_result = tasdevice_change_chn_book_page(tas_dev, chn,
			TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg));
		if (n_result < 0)
			goto out;
#ifdef CONFIG_TASDEV_CODEC_SPI
		pClient->chip_select = tas_dev->tasdevice[chn].mnDevAddr;
#else
		pClient->addr = tas_dev->tasdevice[chn].mnDevAddr;
#endif
		n_result = tasdevice_regmap_bulk_read(tas_dev,
			TASDEVICE_PAGE_REG(reg), p_data, n_length);
		if (n_result < 0)
			dev_err(tas_dev->dev, "%s, ERROR, E=%d\n",
				__func__, n_result);
		else
			dev_dbg(tas_dev->dev,
				"%s: chn0x%02x:BOOK:PAGE:REG 0x%02x:0x%02x:"
				"0x%02x, len: 0x%02x\n", __func__,
				tas_dev->tasdevice[chn].mnDevAddr,
				TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg),
				TASDEVICE_PAGE_REG(reg), n_length);
	} else
		dev_err(tas_dev->dev, "%s, ERROR, no such channel(%d)\n",
			__func__, chn);

out:
	mutex_unlock(&tas_dev->dev_lock);
	return n_result;
}


int tasdevice_dev_update_bits(
	struct tasdevice_priv *tas_dev, enum channel chn,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	int n_result = 0;
#ifdef CONFIG_TASDEV_CODEC_SPI
	struct spi_device *pClient =
		(struct spi_device *)tas_dev->client;
#else
	struct i2c_client *pClient =
		(struct i2c_client *)tas_dev->client;
#endif
	mutex_lock(&tas_dev->dev_lock);
	if (chn < tas_dev->ndev) {
		n_result = tasdevice_change_chn_book_page(tas_dev, chn,
			TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg));
		if (n_result < 0)
			goto out;
#ifdef CONFIG_TASDEV_CODEC_SPI
		pClient->chip_select = tas_dev->tasdevice[chn].mnDevAddr;
#else
		pClient->addr = tas_dev->tasdevice[chn].mnDevAddr;
#endif
		n_result = tasdevice_regmap_update_bits(tas_dev,
			TASDEVICE_PAGE_REG(reg), mask, value);
		if (n_result < 0)
			dev_err(tas_dev->dev, "%s, ERROR, E=%d\n",
				__func__, n_result);
		else
			dev_dbg(tas_dev->dev,
			"%s: chn0x%02x:BOOK:PAGE:REG 0x%02x:0x%02x:0x%02x, "
				"mask: 0x%02x, val: 0x%02x\n",
				__func__, tas_dev->tasdevice[chn].mnDevAddr,
				TASDEVICE_BOOK_ID(reg), TASDEVICE_PAGE_ID(reg),
				TASDEVICE_PAGE_REG(reg), mask, value);
	} else
		dev_err(tas_dev->dev, "%s, ERROR, no such channel(%d)\n",
			__func__, chn);

out:
	mutex_unlock(&tas_dev->dev_lock);
	return n_result;
}

