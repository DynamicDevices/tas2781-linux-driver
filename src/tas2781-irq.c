// SPDX-License-Identifier: GPL-2.0
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#include <linux/firmware.h>
#include <sound/soc.h>

#include "tasdevice.h"
#include "tas2781-reg.h"
#include "tasdevice-rw.h"

void tas2781_irq_work_func(struct tasdevice_priv *tas_dev)
{
	int rc = 0;
	unsigned int reg_val = 0, array_size = 0, i = 0, ndev = 0;
	unsigned int int_reg_array[] = {
		TAS2781_REG_INT_LTCH0,
		TAS2781_REG_INT_LTCH1,
		TAS2781_REG_INT_LTCH1_0,
		TAS2781_REG_INT_LTCH2,
		TAS2781_REG_INT_LTCH3,
		TAS2781_REG_INT_LTCH4};

	tasdevice_enable_irq(tas_dev, false);

	array_size = ARRAY_SIZE(int_reg_array);

	for (ndev = 0; ndev < tas_dev->ndev; ndev++) {
		for (i = 0; i < array_size; i++) {
			rc = tasdevice_dev_read(tas_dev,
				ndev, int_reg_array[i], &reg_val);
			if (!rc) {
				dev_info(tas_dev->dev,
					"INT STATUS REG 0x%04x=0x%02x\n",
					int_reg_array[i], reg_val);
			} else {
				dev_err(tas_dev->dev,
					"Read Reg 0x%04x error(rc=%d)\n",
					int_reg_array[i], rc);
			}
		}
	}

}
