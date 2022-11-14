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

#include <linux/version.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/miscdevice.h>
#include <linux/firmware.h>

#include "tasdevice-dsp.h"
#include "tasdevice-ctl.h"
#include "tasdevice-dsp.h"
#include "tasdevice-regbin.h"
#include "tasdevice.h"
#include "tasdevice-rw.h"

/* max. length of a alsa mixer control name */
#define MAX_CONTROL_NAME		(48)
#define TASDEVICE_CLK_DIR_IN		(0)
#define TASDEVICE_CLK_DIR_OUT		(1)

static int tasdevice_program_get(struct snd_kcontrol *pKcontrol,
		struct snd_ctl_elem_value *pValue)
{
	struct snd_soc_component *codec
		= snd_soc_kcontrol_component(pKcontrol);
	struct tasdevice_priv *pTAS2781 = snd_soc_component_get_drvdata(codec);

	mutex_lock(&pTAS2781->codec_lock);
	pValue->value.integer.value[0] = pTAS2781->mnCurrentProgram;
	mutex_unlock(&pTAS2781->codec_lock);
	return 0;
}

static int tasdevice_program_put(struct snd_kcontrol *pKcontrol,
		struct snd_ctl_elem_value *pValue)
{
	struct snd_soc_component *codec
		= snd_soc_kcontrol_component(pKcontrol);
	struct tasdevice_priv *pTAS2781 = snd_soc_component_get_drvdata(codec);
	unsigned int nProgram = pValue->value.integer.value[0];

	mutex_lock(&pTAS2781->codec_lock);
	pTAS2781->mnCurrentProgram = nProgram;
	mutex_unlock(&pTAS2781->codec_lock);
	return 0;
}

static int tasdevice_configuration_get(
	struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{

	struct snd_soc_component *codec
		= snd_soc_kcontrol_component(pKcontrol);
	struct tasdevice_priv *pTAS2781 = snd_soc_component_get_drvdata(codec);

	mutex_lock(&pTAS2781->codec_lock);
	pValue->value.integer.value[0] = pTAS2781->mnCurrentConfiguration;
	mutex_unlock(&pTAS2781->codec_lock);
	return 0;
}

static int tasdevice_configuration_put(
	struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
	struct snd_soc_component *codec
					= snd_soc_kcontrol_component(pKcontrol);
	struct tasdevice_priv *pTAS2781 = snd_soc_component_get_drvdata(codec);
	unsigned int nConfiguration = pValue->value.integer.value[0];

	mutex_lock(&pTAS2781->codec_lock);
	pTAS2781->mnCurrentConfiguration = nConfiguration;
	mutex_unlock(&pTAS2781->codec_lock);
	return 0;
}

static int tasdevice_dac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct tasdevice_priv *tas_dev = snd_soc_component_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_info(tas_dev->dev, "SND_SOC_DAPM_POST_PMU\n");
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_info(tas_dev->dev, "SND_SOC_DAPM_PRE_PMD\n");
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget tasdevice_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("ASI", "ASI Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ASI OUT", "ASI Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0,
		tasdevice_dac_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT"),
	SND_SOC_DAPM_INPUT("DMIC"),
	SND_SOC_DAPM_SIGGEN("VMON"),
	SND_SOC_DAPM_SIGGEN("IMON")
};

static const struct snd_soc_dapm_route tasdevice_audio_map[] = {
	{"DAC", NULL, "ASI"},
	{"OUT", NULL, "DAC"},
	{"ASI OUT", NULL, "DMIC"}
};


static int tasdevice_startup(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct tasdevice_priv *tas_dev = snd_soc_component_get_drvdata(codec);
	int ret = 0;

	if (tas_dev->fw_state != TASDEVICE_DSP_FW_ALL_OK) {
		dev_err(tas_dev->dev, "DSP bin file not loaded\n");
		ret = -EINVAL;
	}
	return ret;
}

static int tasdevice_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct tasdevice_priv *tas_dev = snd_soc_dai_get_drvdata(dai);
	unsigned int fsrate;
	unsigned int slot_width;
	int bclk_rate;
	int rc = 0;

	dev_info(tas_dev->dev, "%s: %s\n",
		__func__, substream->stream == SNDRV_PCM_STREAM_PLAYBACK?
		"Playback":"Capture");

	fsrate = params_rate(params);
	switch (fsrate) {
	case 48000:
		break;
	case 44100:
		break;
	default:
		dev_err(tas_dev->dev,
			"%s: incorrect sample rate = %u\n",
			__func__, fsrate);
		rc = -EINVAL;
		goto out;
	}

	slot_width = params_width(params);
	switch (slot_width) {
	case 16:
		break;
	case 20:
		break;
	case 24:
		break;
	case 32:
		break;
	default:
		dev_err(tas_dev->dev,
			"%s: incorrect slot width = %u\n",
			__func__, slot_width);
		rc = -EINVAL;
		goto out;
	}

	bclk_rate = snd_soc_params_to_bclk(params);
	if (bclk_rate < 0) {
		dev_err(tas_dev->dev,
			"%s: incorrect bclk rate = %d\n",
			__func__, bclk_rate);
		rc = bclk_rate;
		goto out;
	}
	dev_info(tas_dev->dev, "%s: BCLK rate = %d Channel = %d"
		"Sample rate = %u slot width = %u\n",
		__func__, bclk_rate, params_channels(params),
		fsrate, slot_width);
out:
	return rc;
}

static int tasdevice_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct tasdevice_priv *tas_dev = snd_soc_dai_get_drvdata(codec_dai);

	dev_info(tas_dev->dev,
		"%s: clk_id = %d, freq = %u, CLK direction %s\n",
		__func__, clk_id, freq,
		dir == TASDEVICE_CLK_DIR_OUT ? "OUT":"IN");

	return 0;
}

void powercontrol_routine(struct work_struct *work)
{
	struct tasdevice_priv *tas_dev =
		container_of(work, struct tasdevice_priv,
		powercontrol_work.work);
	struct TFirmware *pFw = NULL;
	int profile_cfg_id = 0;

	dev_info(tas_dev->dev, "%s: enter\n", __func__);

	mutex_lock(&tas_dev->codec_lock);
	/*mnCurrentProgram != 0 is dsp mode or tuning mode*/
	if (tas_dev->mnCurrentProgram) {
		/*bypass all in regbin is profile id 0*/
		profile_cfg_id = REGBIN_CONFIGID_BYPASS_ALL;
	} else {
		profile_cfg_id = tas_dev->mtRegbin.profile_cfg_id;
		pFw = tas_dev->mpFirmware;
		dev_info(tas_dev->dev, "%s: %s\n", __func__,
			pFw->mpConfigurations[tas_dev->mnCurrentConfiguration]
			.mpName);
		tasdevice_select_tuningprm_cfg(tas_dev,
			tas_dev->mnCurrentProgram,
			tas_dev->mnCurrentConfiguration,
			profile_cfg_id);

	}
	tasdevice_select_cfg_blk(tas_dev, profile_cfg_id,
		TASDEVICE_BIN_BLK_PRE_POWER_UP);

	if (tas_dev->chip_id != GENERAL_AUDEV)
		tasdevice_enable_irq(tas_dev, true);
	mutex_unlock(&tas_dev->codec_lock);
	dev_info(tas_dev->dev, "%s: leave\n", __func__);
}

static void tasdevice_set_power_state(
	struct tasdevice_priv *tas_dev, int state)
{
	switch (state) {
	case 0:
		schedule_delayed_work(&tas_dev->powercontrol_work,
			msecs_to_jiffies(20));
		break;
	default:
		if (!(tas_dev->pstream || tas_dev->cstream)) {
			if (tas_dev->chip_id != GENERAL_AUDEV)
				tasdevice_enable_irq(tas_dev, false);
			tasdevice_select_cfg_blk(tas_dev,
				tas_dev->mnCurrentConfiguration,
				TASDEVICE_BIN_BLK_PRE_SHUTDOWN);
		}
		break;
	}
}

static int tasdevice_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_component *codec = dai->component;
	struct tasdevice_priv *tas_dev = snd_soc_component_get_drvdata(codec);

	/* Codec Lock Hold */
	mutex_lock(&tas_dev->codec_lock);
	/* misc driver file lock hold */
	mutex_lock(&tas_dev->file_lock);

	if (mute) {
		/* stop DSP only when both playback and capture streams
		* are deactivated
		*/
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			tas_dev->pstream = 0;
		else
			tas_dev->cstream = 0;
		if (tas_dev->pstream != 0 || tas_dev->cstream != 0)
			goto out;
	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			tas_dev->pstream = 1;
		else
			tas_dev->cstream = 1;

	}
	tasdevice_set_power_state(tas_dev, mute);
out:
	/* Misc driver file lock release */
	mutex_unlock(&tas_dev->file_lock);
	/* Codec Lock Release*/
	mutex_unlock(&tas_dev->codec_lock);

	return 0;
}

static struct snd_soc_dai_ops tasdevice_dai_ops = {
	.startup = tasdevice_startup,
	.hw_params	= tasdevice_hw_params,
	.set_sysclk	= tasdevice_set_dai_sysclk,
	.mute_stream = tasdevice_mute,
};


static struct snd_soc_dai_driver tasdevice_dai_driver[] = {
	{
		.name = "tasdevice_codec",
		.id = 0,
		.playback = {
			.stream_name	= "Playback",
			.channels_min   = 1,
			.channels_max   = 4,
			.rates	 = TASDEVICE_RATES,
			.formats	= TASDEVICE_FORMATS,
		},
		.capture = {
			.stream_name	= "Capture",
			.channels_min   = 1,
			.channels_max   = 4,
			.rates	 = TASDEVICE_RATES,
			.formats	= TASDEVICE_FORMATS,
		},
		.ops = &tasdevice_dai_ops,
		.symmetric_rates = 1,
	},
};

static int tasdevice_codec_probe(
	struct snd_soc_component *codec)
{
	struct tasdevice_priv *tas_dev =
		snd_soc_component_get_drvdata(codec);
	int ret = 0;
	int i = 0;

	dev_info(tas_dev->dev, "%s, enter\n", __func__);
	/* Codec Lock Hold */
	mutex_lock(&tas_dev->codec_lock);
	tas_dev->codec = codec;
	scnprintf(tas_dev->regbin_binaryname, 64, "%s_regbin.bin",
		tas_dev->dev_name);
	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
		tas_dev->regbin_binaryname, tas_dev->dev, GFP_KERNEL, tas_dev,
		tasdevice_regbin_ready);
	if (ret)
		dev_err(tas_dev->dev,
			"%s: request_firmware_nowait error:0x%08x\n",
			__func__, ret);

	for (i = 0; i < tas_dev->ndev; i++) {
		ret = tasdevice_dev_write(tas_dev, i,
			TASDEVICE_REG_SWRESET,
			TASDEVICE_REG_SWRESET_RESET);
		if (ret < 0) {
			dev_err(tas_dev->dev, "%s: chn %d I2c fail, %d\n",
				__func__, i, ret);
			goto out;
		}
	}

	//usleep_range(1000, 1050);

out:
	/* Codec Lock Release*/
	mutex_unlock(&tas_dev->codec_lock);
	dev_info(tas_dev->dev, "%s, codec probe success\n", __func__);

	return ret;
}

static void tasdevice_codec_remove(
	struct snd_soc_component *codec)
{
	struct tasdevice_priv *tas_dev =
		snd_soc_component_get_drvdata(codec);
	/* Codec Lock Hold */
	mutex_lock(&tas_dev->codec_lock);
	tasdevice_deinit(tas_dev);
	/* Codec Lock Release*/
	mutex_unlock(&tas_dev->codec_lock);

	return;

}

static const struct snd_soc_component_driver
	soc_codec_driver_tasdevice = {
	.probe			= tasdevice_codec_probe,
	.remove			= tasdevice_codec_remove,
	.dapm_widgets		= tasdevice_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(tasdevice_dapm_widgets),
	.dapm_routes		= tasdevice_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(tasdevice_audio_map),
	.idle_bias_on		= 1,
	.endianness		= 1,
};

static int tasdevice_info_profile(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *codec
		= snd_soc_kcontrol_component(kcontrol);
	struct tasdevice_priv *p_tasdevice =
		snd_soc_component_get_drvdata(codec);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	/* Codec Lock Hold*/
	mutex_lock(&p_tasdevice->codec_lock);
	uinfo->count = 1;
	/* Codec Lock Release*/
	mutex_unlock(&p_tasdevice->codec_lock);

	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max(0, p_tasdevice->mtRegbin.ncfgs);

	return 0;
}

static int tasdevice_get_profile_id(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec
		= snd_soc_kcontrol_component(kcontrol);
	struct tasdevice_priv *p_tasdevice
		= snd_soc_component_get_drvdata(codec);

	/* Codec Lock Hold*/
	mutex_lock(&p_tasdevice->codec_lock);
	ucontrol->value.integer.value[0] =
		p_tasdevice->mtRegbin.profile_cfg_id;
	/* Codec Lock Release*/
	mutex_unlock(&p_tasdevice->codec_lock);

	return 0;
}

static int tasdevice_set_profile_id(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec
		= snd_soc_kcontrol_component(kcontrol);
	struct tasdevice_priv *p_tasdevice =
		snd_soc_component_get_drvdata(codec);

	/* Codec Lock Hold*/
	mutex_lock(&p_tasdevice->codec_lock);
	p_tasdevice->mtRegbin.profile_cfg_id =
		ucontrol->value.integer.value[0];
	/* Codec Lock Release*/
	mutex_unlock(&p_tasdevice->codec_lock);

	return 0;
}

int tasdevice_create_controls(struct tasdevice_priv *p_tasdevice)
{
	int  nr_controls = 1, ret = 0, mix_index = 0;
	char *name = NULL;
	struct snd_kcontrol_new *tasdevice_profile_controls = NULL;

	tasdevice_profile_controls = devm_kzalloc(p_tasdevice->dev,
			nr_controls * sizeof(tasdevice_profile_controls[0]),
			GFP_KERNEL);
	if (tasdevice_profile_controls == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/* Create a mixer item for selecting the active profile */
	name = devm_kzalloc(p_tasdevice->dev,
		MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name) {
		ret = -ENOMEM;
		goto out;
	}
	scnprintf(name, MAX_CONTROL_NAME, "TASDEVICE Profile id");
	tasdevice_profile_controls[mix_index].name = name;
	tasdevice_profile_controls[mix_index].iface =
		SNDRV_CTL_ELEM_IFACE_MIXER;
	tasdevice_profile_controls[mix_index].info =
		tasdevice_info_profile;
	tasdevice_profile_controls[mix_index].get =
		tasdevice_get_profile_id;
	tasdevice_profile_controls[mix_index].put =
		tasdevice_set_profile_id;
	mix_index++;

	ret = snd_soc_add_component_controls(p_tasdevice->codec,
		tasdevice_profile_controls,
		nr_controls < mix_index ? nr_controls : mix_index);

	p_tasdevice->tas_ctrl.nr_controls =
		nr_controls < mix_index ? nr_controls : mix_index;
out:
	return ret;
}

static int tasdevice_info_programs(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *codec
		= snd_soc_kcontrol_component(kcontrol);
	struct tasdevice_priv *p_tasdevice =
		snd_soc_component_get_drvdata(codec);
	struct TFirmware *Tfw = p_tasdevice->mpFirmware;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	/* Codec Lock Hold*/
	mutex_lock(&p_tasdevice->codec_lock);
	uinfo->count = 1;
	/* Codec Lock Release*/
	mutex_unlock(&p_tasdevice->codec_lock);

	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = (int)Tfw->mnPrograms;

	return 0;
}

static int tasdevice_info_configurations(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *codec
		= snd_soc_kcontrol_component(kcontrol);
	struct tasdevice_priv *p_tasdevice =
		snd_soc_component_get_drvdata(codec);
	struct TFirmware *Tfw = p_tasdevice->mpFirmware;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	/* Codec Lock Hold*/
	mutex_lock(&p_tasdevice->codec_lock);
	uinfo->count = 1;

	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = (int)Tfw->mnConfigurations - 1;

	/* Codec Lock Release*/
	mutex_unlock(&p_tasdevice->codec_lock);

	return 0;
}

int tasdevice_dsp_create_control(struct tasdevice_priv *p_tasdevice)
{
	int  nr_controls = 2, ret = 0, mix_index = 0;
	char *program_name = NULL;
	char *configuraton_name = NULL;
	struct snd_kcontrol_new *tasdevice_dsp_controls = NULL;

	tasdevice_dsp_controls = devm_kzalloc(p_tasdevice->dev,
			nr_controls * sizeof(tasdevice_dsp_controls[0]),
			GFP_KERNEL);
	if (tasdevice_dsp_controls == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/* Create a mixer item for selecting the active profile */
	program_name = devm_kzalloc(p_tasdevice->dev,
		MAX_CONTROL_NAME, GFP_KERNEL);
	configuraton_name = devm_kzalloc(p_tasdevice->dev,
		MAX_CONTROL_NAME, GFP_KERNEL);
	if (!program_name || !configuraton_name) {
		ret = -ENOMEM;
		goto out;
	}

	scnprintf(program_name, MAX_CONTROL_NAME, "Program");
	tasdevice_dsp_controls[mix_index].name = program_name;
	tasdevice_dsp_controls[mix_index].iface =
		SNDRV_CTL_ELEM_IFACE_MIXER;
	tasdevice_dsp_controls[mix_index].info =
		tasdevice_info_programs;
	tasdevice_dsp_controls[mix_index].get =
		tasdevice_program_get;
	tasdevice_dsp_controls[mix_index].put =
		tasdevice_program_put;
	mix_index++;

	scnprintf(configuraton_name, MAX_CONTROL_NAME, "Configuration");
	tasdevice_dsp_controls[mix_index].name = configuraton_name;
	tasdevice_dsp_controls[mix_index].iface =
		SNDRV_CTL_ELEM_IFACE_MIXER;
	tasdevice_dsp_controls[mix_index].info =
		tasdevice_info_configurations;
	tasdevice_dsp_controls[mix_index].get =
		tasdevice_configuration_get;
	tasdevice_dsp_controls[mix_index].put =
		tasdevice_configuration_put;
	mix_index++;

	ret = snd_soc_add_component_controls(p_tasdevice->codec,
		tasdevice_dsp_controls,
		nr_controls < mix_index ? nr_controls : mix_index);

	p_tasdevice->tas_ctrl.nr_controls += nr_controls;
out:
	return ret;
}

int tasdevice_register_codec(struct tasdevice_priv *pTAS2781)
{
	int nResult = 0;

	dev_err(pTAS2781->dev, "%s, enter\n", __func__);
	nResult = devm_snd_soc_register_component(pTAS2781->dev,
		&soc_codec_driver_tasdevice,
		tasdevice_dai_driver, ARRAY_SIZE(tasdevice_dai_driver));

	return nResult;
}

void tasdevice_deregister_codec(struct tasdevice_priv *pTAS2781)
{
	snd_soc_unregister_component(pTAS2781->dev);
}
