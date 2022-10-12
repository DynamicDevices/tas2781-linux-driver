// SPDX-License-Identifier: GPL-2.0
// PCMDEVICE Sound driver
// Copyright (C) 2022 Texas Instruments Incorporated  -
// https://www.ti.com/

#include <linux/miscdevice.h>
#include <linux/firmware.h>

#include "tasdevice-dsp.h"
#include "tasdevice-regbin.h"
#include "tasdevice.h"

static const char *spk_vendor_name[] = {
	"goertek",
	"sw"
};

long tasdevice_init(void *pContext, int spkvendorid)
{
	int ret = 0, i = 0;
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *) pContext;
	const struct firmware *fw_entry = NULL;
	char pHint[256];

	tasdevice_dsp_remove(tas_dev);

	tasdevice_calbin_remove(tas_dev);
	tas_dev->fw_state = TASDEVICE_DSP_FW_PENDING;

	spkvendorid = (spkvendorid < sizeof(spk_vendor_name)/
		sizeof(spk_vendor_name[0])) ? spkvendorid : 0;
	scnprintf(pHint, 256, "%s_%s", spk_vendor_name[spkvendorid],
		tas_dev->dsp_binaryname);

	ret = request_firmware(&fw_entry, pHint, tas_dev->dev);
	if (!ret) {
		ret = tasdevice_dspfw_ready(fw_entry, tas_dev);
		release_firmware(fw_entry);
		fw_entry = NULL;
	} else {
		tas_dev->fw_state = TASDEVICE_DSP_FW_FAIL;
		dev_err(tas_dev->dev, "%s: load %s error\n", __func__, pHint);
		goto out;
	}
	tas_dev->fw_state = TASDEVICE_DSP_FW_ALL_OK;

	for (i = 0; i < tas_dev->ndev; i++) {
		ret = tas2781_load_calibration(tas_dev,
			tas_dev->cal_binaryname[i], i);
		if (ret != 0) {
			dev_err(tas_dev->dev, "%s: load %s error, no-side "
				"effect for playback\n", __func__,
				tas_dev->cal_binaryname[i]);
			ret = 0;
		}
	}
out:
	return ret;
}

void tasdevice_deinit(void *pContext)
{
	struct tasdevice_priv *tas_dev = (struct tasdevice_priv *) pContext;

	tasdevice_config_info_remove(tas_dev);
	tasdevice_dsp_remove(tas_dev);
	tasdevice_calbin_remove(tas_dev);
	tas_dev->fw_state = TASDEVICE_DSP_FW_PENDING;
}
