/*
 * Intel Broxton-P I2S Machine Driver for IVI - HU
 *
 * Copyright (C) 2014-2015, Intel Corporation. All rights reserved.
 *
 * Modified from:
 *   Intel BXT P RT298 Machine driver
 *   Intel Skylake I2S Machine driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>

#define DEF_BT_RATE_INBDEX 0x0

struct bxtp_ivi_hu_prv {
	int srate;
};

static unsigned int ivi_hu_bt_rates[] = {
	8000,
	16000,
};

/* sound card controls */
static const char * const bt_rate[] = {"8K", "16K"};

static const struct soc_enum btrate_enum =
	SOC_ENUM_SINGLE_EXT(2, bt_rate);

static int bt_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct bxtp_ivi_hu_prv *drv = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = drv->srate;
	return 0;
}

static int bt_sample_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct bxtp_ivi_hu_prv *drv = snd_soc_card_get_drvdata(card);

	if (ucontrol->value.integer.value[0] == drv->srate)
		return 0;

	drv->srate = ucontrol->value.integer.value[0];
	return 0;

}

static const struct snd_kcontrol_new hu_snd_controls[] = {

	SOC_ENUM_EXT("BT Rate", btrate_enum,
			bt_sample_rate_get, bt_sample_rate_put),
};

static const struct snd_soc_dapm_widget broxton_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("DMIC2", NULL),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
};

static const struct snd_soc_dapm_route broxton_rt298_map[] = {
	{"Speaker", NULL, "Dummy Playback"},
	{"Dummy Capture", NULL, "DMIC2"},

	{ "Dummy Playback", NULL, "ssp0 Tx"},
	{ "ssp0 Tx", NULL, "codec0_out"},

	{ "bt_ssp0_in", NULL, "ssp0 Rx" },
	{ "ssp0 Rx", NULL, "Dummy Capture" },
};

static int bxtp_ssp0_gpio_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	char *gpio_addr;
	u32 gpio_value1 = 0x40900500;
	u32 gpio_value2 = 0x44000600;

	gpio_addr = (void *)ioremap_nocache(0xd0c40610, 0x30);
	if (gpio_addr == NULL)
		return(-EIO);

	memcpy_toio(gpio_addr + 0x8, &gpio_value1, sizeof(gpio_value1));
	memcpy_toio(gpio_addr + 0x10, &gpio_value2, sizeof(gpio_value2));
	memcpy_toio(gpio_addr + 0x18, &gpio_value2, sizeof(gpio_value2));
	memcpy_toio(gpio_addr + 0x20, &gpio_value2, sizeof(gpio_value2));

	iounmap(gpio_addr);
	return 0;
}

static int broxton_ssp0_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_soc_card *card =  rtd->card;
	struct bxtp_ivi_hu_prv *drv = snd_soc_card_get_drvdata(card);

	/* SSP0 operates with a BT Transceiver */
	rate->min = rate->max = ivi_hu_bt_rates[drv->srate];
	return 0;
}

/* broxton digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link broxton_rt298_dais[] = {
	/* Front End DAI links */
	{
		.name = "Bxt Audio Port 1",
		.stream_name = "Audio",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
	},
	{
		.name = "Bxt Audio Reference cap",
		.stream_name = "BT Capture",
		.cpu_dai_name = "System Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.init = NULL,
		.dpcm_capture = 1,
		.nonatomic = 1,
		.dynamic = 1,
	},
	/* Trace Buffer DAI links */
	{
		.name = "Bxt Trace Buffer0",
		.stream_name = "Core 0 Trace Buffer",
		.cpu_dai_name = "TraceBuffer0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.capture_only = true,
		.ignore_suspend = 1,
	},
	{
		.name = "Bxt Trace Buffer1",
		.stream_name = "Core 1 Trace Buffer",
		.cpu_dai_name = "TraceBuffer1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.capture_only = true,
		.ignore_suspend = 1,
	},
	{
		.name = "Bxt Compress Probe playback",
		.stream_name = "Probe Playback",
		.cpu_dai_name = "Compress Probe0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.nonatomic = 1,
	},
	{
		.name = "Bxt Compress Probe capture",
		.stream_name = "Probe Capture",
		.cpu_dai_name = "Compress Probe1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.nonatomic = 1,
	},


	/* Back End DAI links */
	{
		/* SSP0 - Codec - for HDMI MCH */
		.name = "SSP0-Codec",
		.be_id = 0,
		.cpu_dai_name = "SSP0 Pin",
		.platform_name = "0000:00:0e.0",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.init = bxtp_ssp0_gpio_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = broxton_ssp0_fixup,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
	},
};

/* broxton audio machine driver for SPT + RT298S */
static struct snd_soc_card broxton_rt298 = {
	.name = "broxton-ivi-hu",
	.owner = THIS_MODULE,
	.dai_link = broxton_rt298_dais,
	.num_links = ARRAY_SIZE(broxton_rt298_dais),
	.controls = hu_snd_controls,
	.num_controls = ARRAY_SIZE(hu_snd_controls),
	.dapm_widgets = broxton_widgets,
	.num_dapm_widgets = ARRAY_SIZE(broxton_widgets),
	.dapm_routes = broxton_rt298_map,
	.num_dapm_routes = ARRAY_SIZE(broxton_rt298_map),
	.fully_routed = true,
};

static int broxton_audio_probe(struct platform_device *pdev)
{
	broxton_rt298.dev = &pdev->dev;
	int ret_val;
	struct bxtp_ivi_hu_prv *drv;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->srate = DEF_BT_RATE_INBDEX;
	snd_soc_card_set_drvdata(&broxton_rt298, drv);

	ret_val = snd_soc_register_card(&broxton_rt298);
	if (ret_val) {
		dev_dbg(&pdev->dev, "snd_soc_register_card failed %d\n",
								 ret_val);
		return ret_val;
	}

	platform_set_drvdata(pdev, &broxton_rt298);

	return ret_val;
}

static int broxton_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&broxton_rt298);
	return 0;
}

static struct platform_driver broxton_audio = {
	.probe = broxton_audio_probe,
	.remove = broxton_audio_remove,
	.driver = {
		.name = "bxt_ivi_hu_i2s",
	},
};

module_platform_driver(broxton_audio)

/* Module information */
MODULE_AUTHOR("Pardha Saradhi K <pardha.saradhi.kesapragada@intel.com>");
MODULE_AUTHOR("Ramesh Babu <Ramesh.Babu@intel.com>");
MODULE_AUTHOR("Senthilnathan Veppur <senthilnathanx.veppur@intel.com>");
MODULE_DESCRIPTION("Intel SST Audio for Broxton-P IVI HU");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bxt_ivi_hu_i2s");
