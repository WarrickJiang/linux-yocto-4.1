/*
 * Copyright 2013-2014 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk-provider.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <asm/mach/arch.h>

#include "common.h"

static int ls1021a_platform_notifier(struct notifier_block *nb,
				  unsigned long event, void *__dev)
{
	struct device *dev = __dev;

	if (event != BUS_NOTIFY_ADD_DEVICE)
		return NOTIFY_DONE;

	if (of_device_is_compatible(dev->of_node, "fsl,etsec2"))
		set_dma_ops(dev, &arm_coherent_dma_ops);
	else if (of_property_read_bool(dev->of_node, "dma-coherent"))
		set_dma_ops(dev, &arm_coherent_dma_ops);

	return NOTIFY_OK;
}

static struct notifier_block ls1021a_platform_nb = {
	.notifier_call = ls1021a_platform_notifier,
};

static void __init ls1021a_init_machine(void)
{
	bus_register_notifier(&platform_bus_type, &ls1021a_platform_nb);

	mxc_arch_reset_init_dt();
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static void __init ls1021a_init_time(void)
{
	of_clk_init(NULL);
	clocksource_of_init();
	tick_setup_hrtimer_broadcast();
}

static const char *ls1021a_dt_compat[] __initdata = {
	"fsl,ls1021a",
	NULL,
};

DT_MACHINE_START(LS1021A, "Freescale LS1021A")
	.smp		= smp_ops(ls1021a_smp_ops),
	.init_time	= ls1021a_init_time,
	.init_machine   = ls1021a_init_machine,
	.dt_compat	= ls1021a_dt_compat,
	.restart	= mxc_restart,
MACHINE_END
