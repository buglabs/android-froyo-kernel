/*
 * omap3bug.c  -- ALSA SoC support for OMAP3 BugBase
 *
 * Author: Anuj Aggarwal <anuj.aggarwal@ti.com>
 *
 * Based on sound/soc/omap/beagle.c by Steve Sakoman
 *
 * Copyright (C) 2008 Texas Instruments, Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/gpio.h>
#include <plat/mcbsp.h>

#include "omap-mcbsp.h"
#include "omap-pcm.h"
#include "../codecs/twl4030.h"

static int omap3bug_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
				  SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		printk(KERN_ERR "Can't set codec DAI configuration\n");
		return ret;
	}

	/* Set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai,
				  SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		printk(KERN_ERR "Can't set cpu DAI configuration\n");
		return ret;
	}

	/* Set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, 26000000,
				     SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk(KERN_ERR "Can't set codec system clock\n");
		return ret;
	}

	return 0;
}

static struct snd_soc_ops omap3bug_ops = {
	.hw_params = omap3bug_hw_params,
};

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link omap3bug_dai = {
	.name 		= "TWL4030",
	.stream_name 	= "TWL4030",
	.cpu_dai 	= &omap_mcbsp_dai[0],
	.codec_dai 	= &twl4030_dai[TWL4030_DAI_HIFI],
	.ops 		= &omap3bug_ops,
};

/* Audio machine driver */
static struct snd_soc_card snd_soc_omap3bug = {
	.name = "omap3bug",
	.platform = &omap_soc_platform,
	.dai_link = &omap3bug_dai,
	.num_links = 1,
};

/* twl4030 setup */
static struct twl4030_setup_data twl4030_setup = {
	.ramp_delay_value = 4,
	.sysclk = 26000,
};

/* Audio subsystem */
static struct snd_soc_device omap3bug_snd_devdata = {
	.card = &snd_soc_omap3bug,
	.codec_dev = &soc_codec_dev_twl4030,
	.codec_data = &twl4030_setup,
};

static struct platform_device *omap3bug_snd_device;

static int __init omap3bug_soc_init(void)
{
	int ret;

	pr_info("OMAP3 BugBase Audio init\n");

	omap3bug_snd_device = platform_device_alloc("soc-audio", -1);
	if (!omap3bug_snd_device) {
		printk(KERN_ERR "Platform device allocation failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(omap3bug_snd_device, &omap3bug_snd_devdata);
	omap3bug_snd_devdata.dev = &omap3bug_snd_device->dev;
	*(unsigned int *)omap3bug_dai.cpu_dai->private_data = 1;

	ret = platform_device_add(omap3bug_snd_device);
	if (ret)
		goto err1;

	return 0;

err1:
	printk(KERN_ERR "Unable to add platform device\n");
	platform_device_put(omap3bug_snd_device);

	return ret;
}

static void __exit omap3bug_soc_exit(void)
{
	platform_device_unregister(omap3bug_snd_device);
}

module_init(omap3bug_soc_init);
module_exit(omap3bug_soc_exit);

MODULE_AUTHOR("Anuj Aggarwal <anuj.aggarwal@ti.com>");
MODULE_DESCRIPTION("ALSA SoC OMAP3 BUG");
MODULE_LICENSE("GPLv2");
