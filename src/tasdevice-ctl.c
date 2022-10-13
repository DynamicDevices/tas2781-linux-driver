// SPDX-License-Identifier: GPL-2.0
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#include <linux/miscdevice.h>
#include <linux/firmware.h>

#include "tasdevice-dsp.h"
#include "tasdevice-regbin.h"
#include "tasdevice.h"

void tasdevice_deinit(void *pContext)
{
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *) pContext;

	tasdevice_config_info_remove(tas_dev);
	tasdevice_dsp_remove(tas_dev);
	tasdevice_calbin_remove(tas_dev);
	tas_dev->fw_state = TASDEVICE_DSP_FW_PENDING;
}
