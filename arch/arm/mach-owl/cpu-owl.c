/*
 * arch/arm/mach-owl/cpu-owl.c
 *
 * cpu peripheral init for Actions SOC
 *
 * Copyright 2012 Actions Semi Inc.
 * Author: Actions Semi, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/of_irq.h>
#include <linux/irqchip.h>
#include <linux/module.h>

#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/smp_twd.h>

#include <mach/hardware.h>
#include <mach/irqs.h>

extern void __init owl_gp_timer_init(void);
extern void __init owl_init_clocks(void);

static struct map_desc owl_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(OWL_PA_REG_BASE),
		.pfn		= __phys_to_pfn(OWL_PA_REG_BASE),
		.length		= OWL_PA_REG_SIZE,
		.type		= MT_DEVICE,
	},

   /*for suspend , to load ddr ops code. by jlingzhang*/
#if 1
	{
		.virtual	= OWL_VA_BOOT_RAM,
		.pfn		= __phys_to_pfn(OWL_PA_BOOT_RAM),
		.length		= SZ_4K,
		.type		= MT_MEMORY_RWX,
	},
#endif
};

/*
 return ATM7033: 0xf, ATM7039: 0xe
 */
int cpu_package(void)
{
	return (act_readl(0xb01b00e0) & 0xf);
}
EXPORT_SYMBOL(cpu_package);

void __init owl_map_io(void)
{
	/* dma coherent allocate buffer: 2 ~ 14MB */
	init_dma_coherent_pool_size(14 << 20);
	iotable_init(owl_io_desc, ARRAY_SIZE(owl_io_desc));
}

void __init owl_init_irq(void)
{
#ifdef CONFIG_OF
	irqchip_init();
#else
	gic_init(0, 29, IO_ADDRESS(OWL_PA_GIC_DIST),
		IO_ADDRESS(OWL_PA_GIC_CPU));
#endif
}

#ifdef CONFIG_CACHE_L2X0
static int __init owl_l2x0_init(void)
{
	void __iomem *l2x0_base;
	u32 val;

	printk(KERN_INFO "%s()\n", __func__);

	l2x0_base = (void *)IO_ADDRESS(OWL_PA_L2CC);

	/* config l2c310 */
	act_writel(0x78800002, (unsigned int)OWL_PA_L2CC + L310_PREFETCH_CTRL);
	act_writel(L310_DYNAMIC_CLK_GATING_EN | L310_STNDBY_MODE_EN,
		(unsigned int)OWL_PA_L2CC + L310_POWER_CTRL);

	/* Instruction prefetch enable
	   Data prefetch enable
	   Round-robin replacement
	   Use AWCACHE attributes for WA
	   32kB way size, 16 way associativity
	   disable exclusive cache
	*/
	val = L310_AUX_CTRL_INSTR_PREFETCH
	| L310_AUX_CTRL_DATA_PREFETCH
	| L310_AUX_CTRL_NS_INT_CTRL
	| L310_AUX_CTRL_NS_LOCKDOWN
	| (1 << 25) /* round robin*/
	| L2C_AUX_CTRL_WAY_SIZE(2)
	| L310_AUX_CTRL_ASSOCIATIVITY_16;

#ifdef CONFIG_OF
	l2x0_of_init(val, 0xc0000fff);
#else
	l2x0_init(l2x0_base, val, 0xc0000fff);
#endif

	return 0;
}
early_initcall(owl_l2x0_init);
#endif
