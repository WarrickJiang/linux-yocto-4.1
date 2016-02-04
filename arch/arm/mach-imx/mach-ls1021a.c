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
#include <linux/platform_data/dcfg-ls1021a.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>

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
	if (!is_ls1021a_rev1())
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

#ifdef CONFIG_FIXED_PHY
static int __init of_add_fixed_phys(void)
{
	int ret;
	struct device_node *np;
	u32 *fixed_link;
	struct fixed_phy_status status = {};

	for_each_node_by_name(np, "ethernet") {
		fixed_link  = (u32 *)of_get_property(np, "fixed-link", NULL);
		if (!fixed_link)
			continue;

		status.link = 1;
		status.duplex = be32_to_cpu(fixed_link[1]);
		status.speed = be32_to_cpu(fixed_link[2]);
		status.pause = be32_to_cpu(fixed_link[3]);
		status.asym_pause = be32_to_cpu(fixed_link[4]);

		ret = fixed_phy_add(PHY_POLL, be32_to_cpu(fixed_link[0]), &status);
		if (ret) {
			of_node_put(np);
			return ret;
		}
	}

	return 0;
}
arch_initcall(of_add_fixed_phys);
#endif /* CONFIG_FIXED_PHY */

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
