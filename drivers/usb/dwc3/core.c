/**
 * core.c - DesignWare USB3 DRD Controller Core file
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/acpi.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/of.h>
#include <linux/usb/otg.h>
#include <mach/hardware.h>
#include <mach/powergate.h>

#include "platform_data.h"
#include "core.h"
#include "gadget.h"
#include "io.h"

#include "usb3_regs.h"
#include "debug.h"

static char *maximum_speed = "high";
module_param(maximum_speed, charp, 0);
MODULE_PARM_DESC(maximum_speed, "Maximum supported speed.");
static int dwc3_slew_rate =-1;
module_param(dwc3_slew_rate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dwc3_slew_rate, "dwc3_slew_rate");
static int dwc3_tx_bias=-1;
module_param(dwc3_tx_bias, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dwc3_tx_bias, "dwc3_tx_bias");
/* -------------------------------------------------------------------------- */
#define DWC3_DEVS_POSSIBLE	32
struct dwc3_port_info {
	void __iomem *usbecs;
	void __iomem *devrst;
	void __iomem *usbpll;
};
struct dwc3_actions {
	struct platform_device	*dwc3;
	struct device		*dev;

	struct dwc3_port_info port_info;	
	void __iomem        *base;
       int 				ic_type;
};
enum{
   IC_ATM7039C = 0,
   IC_ATM7059A
};
static int g_ic_type = IC_ATM7039C;

static DECLARE_BITMAP(dwc3_devs, DWC3_DEVS_POSSIBLE);

#if SUPPORT_NOT_RMMOD_USBDRV
static struct platform_device *pdev_dwc;
extern  int __dwc3_set_plugstate(struct dwc3	*dwc,int s);
int dwc3_set_plugstate(int s)
{
	struct dwc3	*dwc;
    
    	if(pdev_dwc ==NULL){
		printk("------can't get dwc3 platform device (structure)!!---\n");
        	return -ENODEV;
	}            
            
    	dwc = platform_get_drvdata(pdev_dwc);
	 __dwc3_set_plugstate(dwc,s);
     	return 0;
}
EXPORT_SYMBOL_GPL(dwc3_set_plugstate);

void (*dwc3_clk_open)(void );
void (*dwc3_clk_close)(void );
 void dwc3_set_usb_clk_ops(void (*clk_open)(void ),void (*clk_close)(void ))
{
     dwc3_clk_open = clk_open;
     dwc3_clk_close = clk_close;
}
EXPORT_SYMBOL_GPL(dwc3_set_usb_clk_ops);
#endif

int dwc3_get_device_id(void)
{
	int		id;

again:
	id = find_first_zero_bit(dwc3_devs, DWC3_DEVS_POSSIBLE);
	if (id < DWC3_DEVS_POSSIBLE) {
		int old;

		old = test_and_set_bit(id, dwc3_devs);
		if (old)
			goto again;
	} else {
		pr_err("dwc3: no space for new device\n");
		id = -ENOMEM;
	}

	return id;
}
EXPORT_SYMBOL_GPL(dwc3_get_device_id);

void dwc3_put_device_id(int id)
{
	int			ret;

	if (id < 0)
		return;

	ret = test_bit(id, dwc3_devs);
	WARN(!ret, "dwc3: ID %d not in use\n", id);
	smp_mb__before_atomic();
	clear_bit(id, dwc3_devs);
}
EXPORT_SYMBOL_GPL(dwc3_put_device_id);

void dwc3_set_mode(struct dwc3 *dwc, u32 mode)
{
	u32 reg;

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~(DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG));
	reg |= DWC3_GCTL_PRTCAPDIR(mode);
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);
}

/**
 * dwc3_core_soft_reset - Issues core soft reset and PHY reset
 * @dwc: pointer to our context structure
 */
static int dwc3_core_soft_reset(struct dwc3 *dwc,int phy_reset_en)
{
	u32		reg;
	int		ret;

	/* Before Resetting PHY, put Core in Reset */
	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg |= DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	if(phy_reset_en){
		/* Assert USB3 PHY reset */
		reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
		reg |= DWC3_GUSB3PIPECTL_PHYSOFTRST;
		dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);

		/* Assert USB2 PHY reset */
		reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		reg |= DWC3_GUSB2PHYCFG_PHYSOFTRST;
		dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);

	       if(g_ic_type == IC_ATM7059A){
			/* port 1 */
			reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(1));
			reg |= DWC3_GUSB2PHYCFG_PHYSOFTRST;
			dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(1), reg);
		}

		usb_phy_init(dwc->usb2_phy);
		usb_phy_init(dwc->usb3_phy);
		ret = phy_init(dwc->usb2_generic_phy);
		if (ret < 0)
			return ret;

		ret = phy_init(dwc->usb3_generic_phy);
		if (ret < 0) {
			phy_exit(dwc->usb2_generic_phy);
			return ret;
		}
		mdelay(100);

		/* Clear USB3 PHY reset */
		reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
		reg &= ~DWC3_GUSB3PIPECTL_PHYSOFTRST;
		dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);

		/* Clear USB2 PHY reset */
		reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		reg &= ~DWC3_GUSB2PHYCFG_PHYSOFTRST;
		dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);

		 if(g_ic_type == IC_ATM7059A){
			/* port 1 */
			reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(1));
			reg &= ~DWC3_GUSB2PHYCFG_PHYSOFTRST;
			dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(1), reg);
		}
		mdelay(100);
	}

	/* After PHYs are stable we can take Core out of reset state */
	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	return 0;
}

/**
 * dwc3_soft_reset - Issue soft reset
 * @dwc: Pointer to our controller context structure
 */
static int dwc3_soft_reset(struct dwc3 *dwc)
{
	unsigned long timeout;
	u32 reg;

	timeout = jiffies + msecs_to_jiffies(500);
	dwc3_writel(dwc->regs, DWC3_DCTL, DWC3_DCTL_CSFTRST);
	do {
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		if (!(reg & DWC3_DCTL_CSFTRST))
			break;

		if (time_after(jiffies, timeout)) {
			dev_err(dwc->dev, "Reset Timed Out\n");
			return -ETIMEDOUT;
		}

		cpu_relax();
	} while (true);

	return 0;
}

/*
 * dwc3_frame_length_adjustment - Adjusts frame length if required
 * @dwc3: Pointer to our controller context structure
 * @fladj: Value of GFLADJ_30MHZ to adjust frame length
 */
static void dwc3_frame_length_adjustment(struct dwc3 *dwc, u32 fladj)
{
	u32 reg;
	u32 dft;

	if (dwc->revision < DWC3_REVISION_250A)
		return;

	if (fladj == 0)
		return;

	reg = dwc3_readl(dwc->regs, DWC3_GFLADJ);
	dft = reg & DWC3_GFLADJ_30MHZ_MASK;
	if (!dev_WARN_ONCE(dwc->dev, dft == fladj,
	    "request value same as default, ignoring\n")) {
		reg &= ~DWC3_GFLADJ_30MHZ_MASK;
		reg |= DWC3_GFLADJ_30MHZ_SDBND_SEL | fladj;
		dwc3_writel(dwc->regs, DWC3_GFLADJ, reg);
	}
}

/**
 * dwc3_free_one_event_buffer - Frees one event buffer
 * @dwc: Pointer to our controller context structure
 * @evt: Pointer to event buffer to be freed
 */
static void dwc3_free_one_event_buffer(struct dwc3 *dwc,
		struct dwc3_event_buffer *evt)
{
	dma_free_coherent(dwc->dev, evt->length, evt->buf, evt->dma);
}

/**
 * dwc3_alloc_one_event_buffer - Allocates one event buffer structure
 * @dwc: Pointer to our controller context structure
 * @length: size of the event buffer
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
static struct dwc3_event_buffer *dwc3_alloc_one_event_buffer(struct dwc3 *dwc,
		unsigned length)
{
	struct dwc3_event_buffer	*evt;

	evt = devm_kzalloc(dwc->dev, sizeof(*evt), GFP_KERNEL);
	if (!evt)
		return ERR_PTR(-ENOMEM);

	evt->dwc	= dwc;
	evt->length	= length;
	evt->buf	= dma_alloc_coherent(dwc->dev, length,
			&evt->dma, GFP_KERNEL);
	if (!evt->buf)
		return ERR_PTR(-ENOMEM);

	return evt;
}

/**
 * dwc3_free_event_buffers - frees all allocated event buffers
 * @dwc: Pointer to our controller context structure
 */
static void dwc3_free_event_buffers(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int i;

	for (i = 0; i < dwc->num_event_buffers; i++) {
		evt = dwc->ev_buffs[i];
		if (evt)
			dwc3_free_one_event_buffer(dwc, evt);
	}
}

/**
 * dwc3_alloc_event_buffers - Allocates @num event buffers of size @length
 * @dwc: pointer to our controller context structure
 * @length: size of event buffer
 *
 * Returns 0 on success otherwise negative errno. In the error case, dwc
 * may contain some buffers allocated but not all which were requested.
 */
static int dwc3_alloc_event_buffers(struct dwc3 *dwc, unsigned length)
{
	int			num;
	int			i;

	num = DWC3_NUM_INT(dwc->hwparams.hwparams1);
	dwc->num_event_buffers = num;

	dwc->ev_buffs = devm_kzalloc(dwc->dev, sizeof(*dwc->ev_buffs) * num,
			GFP_KERNEL);
	if (!dwc->ev_buffs)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		struct dwc3_event_buffer	*evt;

		evt = dwc3_alloc_one_event_buffer(dwc, length);
		if (IS_ERR(evt)) {
			dev_err(dwc->dev, "can't allocate event buffer\n");
			return PTR_ERR(evt);
		}
		dwc->ev_buffs[i] = evt;
	}

	return 0;
}

/**
 * dwc3_event_buffers_setup - setup our allocated event buffers
 * @dwc: pointer to our controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
int dwc3_event_buffers_setup(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int				n;

	for (n = 0; n < dwc->num_event_buffers; n++) {
		evt = dwc->ev_buffs[n];
		dev_dbg(dwc->dev, "Event buf %p dma %08llx length %d\n",
				evt->buf, (unsigned long long) evt->dma,
				evt->length);

		evt->lpos = 0;

		dwc3_writel(dwc->regs, DWC3_GEVNTADRLO(n),
				lower_32_bits(evt->dma));
		dwc3_writel(dwc->regs, DWC3_GEVNTADRHI(n),
				upper_32_bits(evt->dma));
		dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(n),
				DWC3_GEVNTSIZ_SIZE(evt->length));
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(n), 0);
	}

	return 0;
}

static void dwc3_event_buffers_cleanup(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int				n;

	for (n = 0; n < dwc->num_event_buffers; n++) {
		evt = dwc->ev_buffs[n];

		evt->lpos = 0;

		dwc3_writel(dwc->regs, DWC3_GEVNTADRLO(n), 0);
		dwc3_writel(dwc->regs, DWC3_GEVNTADRHI(n), 0);
		dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(n), DWC3_GEVNTSIZ_INTMASK
				| DWC3_GEVNTSIZ_SIZE(0));
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(n), 0);
	}
}

static int dwc3_alloc_scratch_buffers(struct dwc3 *dwc)
{
	if (!dwc->has_hibernation)
		return 0;

	if (!dwc->nr_scratch)
		return 0;

	dwc->scratchbuf = kmalloc_array(dwc->nr_scratch,
			DWC3_SCRATCHBUF_SIZE, GFP_KERNEL);
	if (!dwc->scratchbuf)
		return -ENOMEM;

	return 0;
}

static int dwc3_setup_scratch_buffers(struct dwc3 *dwc)
{
	dma_addr_t scratch_addr;
	u32 param;
	int ret;

	if (!dwc->has_hibernation)
		return 0;

	if (!dwc->nr_scratch)
		return 0;

	 /* should never fall here */
	if (!WARN_ON(dwc->scratchbuf))
		return 0;

	scratch_addr = dma_map_single(dwc->dev, dwc->scratchbuf,
			dwc->nr_scratch * DWC3_SCRATCHBUF_SIZE,
			DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dwc->dev, scratch_addr)) {
		dev_err(dwc->dev, "failed to map scratch buffer\n");
		ret = -EFAULT;
		goto err0;
	}

	dwc->scratch_addr = scratch_addr;

	param = lower_32_bits(scratch_addr);

	ret = dwc3_send_gadget_generic_command(dwc,
			DWC3_DGCMD_SET_SCRATCHPAD_ADDR_LO, param);
	if (ret < 0)
		goto err1;

	param = upper_32_bits(scratch_addr);

	ret = dwc3_send_gadget_generic_command(dwc,
			DWC3_DGCMD_SET_SCRATCHPAD_ADDR_HI, param);
	if (ret < 0)
		goto err1;

	return 0;

err1:
	dma_unmap_single(dwc->dev, dwc->scratch_addr, dwc->nr_scratch *
			DWC3_SCRATCHBUF_SIZE, DMA_BIDIRECTIONAL);

err0:
	return ret;
}

static void dwc3_free_scratch_buffers(struct dwc3 *dwc)
{
	if (!dwc->has_hibernation)
		return;

	if (!dwc->nr_scratch)
		return;

	 /* should never fall here */
	if (!WARN_ON(dwc->scratchbuf))
		return;

	dma_unmap_single(dwc->dev, dwc->scratch_addr, dwc->nr_scratch *
			DWC3_SCRATCHBUF_SIZE, DMA_BIDIRECTIONAL);
	kfree(dwc->scratchbuf);
}

static void dwc3_core_num_eps(struct dwc3 *dwc)
{
	struct dwc3_hwparams	*parms = &dwc->hwparams;

	dwc->num_in_eps = DWC3_NUM_IN_EPS(parms);
	dwc->num_out_eps = DWC3_NUM_EPS(parms) - dwc->num_in_eps;

	dwc3_trace(trace_dwc3_core, "found %d IN and %d OUT endpoints",
			dwc->num_in_eps, dwc->num_out_eps);
}

static void dwc3_cache_hwparams(struct dwc3 *dwc)
{
	struct dwc3_hwparams	*parms = &dwc->hwparams;

	parms->hwparams0 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS0);
	parms->hwparams1 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS1);
	parms->hwparams2 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS2);
	parms->hwparams3 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS3);
	parms->hwparams4 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS4);
	parms->hwparams5 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS5);
	parms->hwparams6 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS6);
	parms->hwparams7 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS7);
	parms->hwparams8 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS8);
}

static void setphy(unsigned char reg_add, unsigned char value, unsigned char port_num)
{
	void __iomem *usb3_usb_vcon=	NULL;
	volatile unsigned char addr_low;
	volatile unsigned char addr_high;
	volatile unsigned int vstate;

	if(g_ic_type == IC_ATM7059A){
		if (port_num == 0)
		usb3_usb_vcon = (void __iomem *)IO_ADDRESS((0xB0400000 + 0xCe00));
		else if (port_num == 1)
		usb3_usb_vcon = (void __iomem *)IO_ADDRESS((0xB0400000 + 0xCe10));
		else {
			printk("port_num must is 0 or 1!!\n");
			return;
		}
	}		
	else if(g_ic_type == IC_ATM7039C){
        	usb3_usb_vcon = (void __iomem *)IO_ADDRESS(USB3_USB2_P0_VDCTRL);
	}
	addr_low =  reg_add & 0x0f;
	addr_high =  (reg_add >> 4) & 0x0f;

	vstate = value;
	vstate = vstate << 8;

	addr_low |= 0x10;
	writel(vstate | addr_low, usb3_usb_vcon);
	mb();

	addr_low &= 0x0f; 
	writel(vstate | addr_low, usb3_usb_vcon);
	mb();

	addr_low |= 0x10;
	writel(vstate | addr_low, usb3_usb_vcon);
	mb();

	addr_high |= 0x10;
	writel(vstate | addr_high, usb3_usb_vcon);
	mb();

	addr_high &= 0x0f; 
	writel(vstate | addr_high, usb3_usb_vcon);
	mb();

	addr_high |= 0x10;
	writel(vstate | addr_high, usb3_usb_vcon);  
	mb();
	return;
}
#define USB3_UMON_FDT_COMPATIBLE_ATM7039C    "actions,atm7039c-usb"
#define USB3_UMON_FDT_COMPATIBLE_ATM7059TC  "actions,atm7059tc-usb"
#define USB3_UMON_FDT_COMPATIBLE_ATM7059A    "actions,atm7059a-usb"
static int dwc3_get_slewrate_config(void)
{
	struct device_node *fdt_node;
	const __be32 *property;
       int value;
       
	fdt_node = of_find_compatible_node(NULL, NULL, USB3_UMON_FDT_COMPATIBLE_ATM7039C);
	if (NULL == fdt_node) {
		fdt_node = of_find_compatible_node(NULL, NULL, USB3_UMON_FDT_COMPATIBLE_ATM7059TC);
		if (NULL == fdt_node) 
			fdt_node = of_find_compatible_node(NULL, NULL, USB3_UMON_FDT_COMPATIBLE_ATM7059A);
			if (NULL == fdt_node)
			{
				printk("<dwc3>err: no usb3-fdt-compatible\n");
				return -EINVAL;
			}
	}		
	property = of_get_property(fdt_node, "usb_hs_output_strength", NULL);
	value = be32_to_cpup(property);
	return value;
}
void dwc3_phy_init(u8 mode, unsigned char port_num)
{
	unsigned char val_u8,slew_rate;
    
       slew_rate= dwc3_get_slewrate_config();
       if((port_num!= 0)&&(port_num!= 1)){
		port_num = 0;
        }        
	if(g_ic_type == IC_ATM7039C){
		setphy(0xe7,0x0b, port_num);
		udelay(100);
		setphy(0xe7,0x0f, port_num);
		udelay(100);
		setphy(0xe2,0x74, port_num); 
		udelay(100);
	}
	else if(g_ic_type == IC_ATM7059A){

		if(mode == USB_DR_MODE_PERIPHERAL) {
			printk(" GS705A phy init for dwc3 gadget\n");
			val_u8 =(1<<7) |(1<<5)|(1<<4)|(2<<2)|(3<<0);//select page1
			setphy(0xf4, val_u8, port_num);
			val_u8 =(1<<5) |(1<<4)|(0<<3)|(1<<2)|(1<<0);//negative sample
			setphy(0xe0, val_u8, port_num);
            
			val_u8 =(1<<7) |(1<<5)|(1<<4)|(2<<2)|(3<<0);//select page1
			setphy(0xf4, val_u8, port_num);
			val_u8 =(slew_rate<<5) |(0<<4)|(1<<3)|(1<<2)|(3<<0);//slewRate
			setphy(0xe1, val_u8, port_num);
            
			val_u8 =(1<<7) |(0<<5)|(1<<4)|(2<<2)|(3<<0);//bit[6:5]=select page0
			setphy(0xf4, val_u8, port_num);
			val_u8 =(1<<7) |(4<<4)|(1<<3)|(0<<2)|(3<<0); //bit[3]= Enablepowerdown mode
			setphy(0xe6, val_u8, port_num);
            
			val_u8 = (1<<7) |(0<<5)|(1<<4)|(2<<2)|(3<<0);
			setphy(0xf4, val_u8, port_num);
			val_u8 = (7<<4)|(0<<1)|(1<<0);                 //sensitivity lower
			setphy(0xe7, val_u8, port_num);
            
			val_u8 = (1<<7) |(0<<5)|(1<<4)|(2<<2)|(3<<0);
			setphy(0xf4, val_u8, port_num);
			val_u8 = (9<<4)|(7<<0);                               //hstx current lower
			setphy(0xe4, val_u8, port_num);
		}
		else {
			printk(" GS705A phy init for xhci\n");
			val_u8 =(1<<7) |(1<<5)|(1<<4)|(2<<2)|(3<<0);//select page1
			setphy(0xf4, val_u8, port_num);
			val_u8 =(1<<5) |(1<<4)|(0<<3)|(1<<2)|(1<<0);//negative sample
			setphy(0xe0, val_u8, port_num);
            
			val_u8 =(1<<7) |(1<<5)|(1<<4)|(2<<2)|(3<<0);//select page1
			setphy(0xf4, val_u8, port_num);
			if((dwc3_slew_rate >=0) &&(dwc3_slew_rate <= 7))
				val_u8 =(dwc3_slew_rate<<5) |(0<<4)|(1<<3)|(1<<2)|(3<<0);//slewRate
			else
				val_u8 =(3<<5) |(0<<4)|(1<<3)|(1<<2)|(3<<0);//slewRate
			setphy(0xe1, val_u8, port_num);
                    
			val_u8 =(1<<7) |(0<<5)|(1<<4)|(2<<2)|(3<<0);//bit[6:5]=select page0
			setphy(0xf4, val_u8, port_num);
			val_u8 =(1<<7) |(4<<4)|(1<<3)|(0<<2)|(3<<0); //bit[3]= Enablepowerdown mode
			setphy(0xe6, val_u8, port_num);
            
			val_u8 = (1<<7) |(0<<5)|(1<<4)|(2<<2)|(3<<0);
			setphy(0xf4, val_u8, port_num);
			val_u8 = (9<<4)|(0<<1)|(1<<0);                 //sensitivity lower
			setphy(0xe7, val_u8, port_num);
            
			val_u8 =(1<<7) |(1<<5)|(1<<4)|(2<<2)|(3<<0);//select page1
			setphy(0xf4, val_u8, port_num);
			val_u8 =(1<<5) |(1<<4)|(0<<3)|(0<<2)|(1<<0);// REG_CAL=0
			setphy(0xe0, val_u8, port_num);
            
			val_u8 = (1<<7) |(0<<5)|(1<<4)|(2<<2)|(3<<0); // elect page0
			setphy(0xf4, val_u8, port_num);
			if((dwc3_tx_bias >=0) &&(dwc3_tx_bias <= 15))
				val_u8 = (0xf<<4)|(dwc3_tx_bias<<0);                                  //adjust hshd threshold and hstx current
			else
				val_u8 = (0xf<<4)|(4<<0);                                  //adjust hshd threshold and hstx current
			setphy(0xe4, val_u8, port_num);

			val_u8 = (1<<7) |(1<<6) |(1<<5)|(1<<4)|(1<<3)|(1<<2)|(0<<1)|(0<<0);
			setphy(0xf0, val_u8, port_num);   //disconnect enable
		}

	}

	return;
}
EXPORT_SYMBOL_GPL(dwc3_phy_init);

/*
 *fix bug:when usb3 bias enable ,reset machine will leak current by gpio 
 *if call this func will also disable super speed.
 */
void disable_bias(void)
{	
	struct dwc3 *dwc;
	u32 reg;
    
	dwc = platform_get_drvdata(pdev_dwc);
	reg = dwc3_readl(dwc->regs, 0xce04);
	reg |=(1<<27);
	dwc3_writel(dwc->regs, 0xce04, reg);
	reg = dwc3_readl(dwc->regs, ANA0F);
	reg &=~(1<<14);
	dwc3_writel(dwc->regs, ANA0F, reg);

}
EXPORT_SYMBOL_GPL(disable_bias);

void enable_bias(void)
{	
	struct dwc3 *dwc;
	u32 reg;
    
	dwc = platform_get_drvdata(pdev_dwc);
	
	reg = dwc3_readl(dwc->regs, ANA0F);
	reg |=(1<<14);
	dwc3_writel(dwc->regs, ANA0F, reg);

}
EXPORT_SYMBOL_GPL(enable_bias);

/**
 * dwc3_phy_setup - Configure USB PHY Interface of DWC3 Core
 * @dwc: Pointer to our controller context structure
 *
 * Returns 0 on success. The USB PHY interfaces are configured but not
 * initialized. The PHY interfaces and the PHYs get initialized together with
 * the core in dwc3_core_init.
 */
int dwc3_phy_setup(struct dwc3 *dwc)
{
	u32 reg;
	int ret;

	reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));

	/*
	 * Above 1.94a, it is recommended to set DWC3_GUSB3PIPECTL_SUSPHY
	 * to '0' during coreConsultant configuration. So default value
	 * will be '0' when the core is reset. Application needs to set it
	 * to '1' after the core initialization is completed.
	 */
	if (dwc->revision > DWC3_REVISION_194A)
		reg |= DWC3_GUSB3PIPECTL_SUSPHY;

	if (dwc->u2ss_inp3_quirk)
		reg |= DWC3_GUSB3PIPECTL_U2SSINP3OK;

	if (dwc->req_p1p2p3_quirk)
		reg |= DWC3_GUSB3PIPECTL_REQP1P2P3;

	if (dwc->del_p1p2p3_quirk)
		reg |= DWC3_GUSB3PIPECTL_DEP1P2P3_EN;

	if (dwc->del_phy_power_chg_quirk)
		reg |= DWC3_GUSB3PIPECTL_DEPOCHANGE;

	if (dwc->lfps_filter_quirk)
		reg |= DWC3_GUSB3PIPECTL_LFPSFILT;

	if (dwc->rx_detect_poll_quirk)
		reg |= DWC3_GUSB3PIPECTL_RX_DETOPOLL;

	if (dwc->tx_de_emphasis_quirk)
		reg |= DWC3_GUSB3PIPECTL_TX_DEEPH(dwc->tx_de_emphasis);

	if (dwc->dis_u3_susphy_quirk)
		reg &= ~DWC3_GUSB3PIPECTL_SUSPHY;

	dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);

	reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));

	/* Select the HS PHY interface */
	switch (DWC3_GHWPARAMS3_HSPHY_IFC(dwc->hwparams.hwparams3)) {
	case DWC3_GHWPARAMS3_HSPHY_IFC_UTMI_ULPI:
		if (dwc->hsphy_interface &&
				!strncmp(dwc->hsphy_interface, "utmi", 4)) {
			reg &= ~DWC3_GUSB2PHYCFG_ULPI_UTMI;
			break;
		} else if (dwc->hsphy_interface &&
				!strncmp(dwc->hsphy_interface, "ulpi", 4)) {
			reg |= DWC3_GUSB2PHYCFG_ULPI_UTMI;
			dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
		} else {
			dev_warn(dwc->dev, "HSPHY Interface not defined\n");

			/* Relying on default value. */
			if (!(reg & DWC3_GUSB2PHYCFG_ULPI_UTMI))
				break;
		}
		/* FALLTHROUGH */
	case DWC3_GHWPARAMS3_HSPHY_IFC_ULPI:
		/* Making sure the interface and PHY are operational */
		ret = dwc3_soft_reset(dwc);
		if (ret)
			return ret;

		udelay(1);

		ret = dwc3_ulpi_init(dwc);
		if (ret)
			return ret;
		/* FALLTHROUGH */
	default:
		break;
	}

	/*
	 * Above 1.94a, it is recommended to set DWC3_GUSB2PHYCFG_SUSPHY to
	 * '0' during coreConsultant configuration. So default value will
	 * be '0' when the core is reset. Application needs to set it to
	 * '1' after the core initialization is completed.
	 */
	if (dwc->revision > DWC3_REVISION_194A)
		reg |= DWC3_GUSB2PHYCFG_SUSPHY;

	if (dwc->dis_u2_susphy_quirk)
		reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;

	if (dwc->dis_enblslpm_quirk)
		reg &= ~DWC3_GUSB2PHYCFG_ENBLSLPM;

	dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);

	return 0;
}

/**
 * dwc3_core_init - Low-level initialization of DWC3 Core
 * @dwc: Pointer to our controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
int dwc3_core_init(struct dwc3 *dwc,int phy_reset_en)
{
	u32			hwparams4 = dwc->hwparams.hwparams4;
	u32			reg;
	int			ret;

	reg = dwc3_readl(dwc->regs, DWC3_GSNPSID);
	/* This should read as U3 followed by revision number */
	if ((reg & DWC3_GSNPSID_MASK) == 0x55330000) {
		/* Detected DWC_usb3 IP */
		dwc->revision = reg;
	} else if ((reg & DWC3_GSNPSID_MASK) == 0x33310000) {
		/* Detected DWC_usb31 IP */
		dwc->revision = dwc3_readl(dwc->regs, DWC3_VER_NUMBER);
		dwc->revision |= DWC3_REVISION_IS_DWC31;
	} else {
		dev_err(dwc->dev, "this is not a DesignWare USB3 DRD Core\n");
		ret = -ENODEV;
		goto err0;
	}

	/*
	 * Write Linux Version Code to our GUID register so it's easy to figure
	 * out which kernel version a bug was found.
	 */
	dwc3_writel(dwc->regs, DWC3_GUID, LINUX_VERSION_CODE);

	/* Handle USB2.0-only core configuration */
	if (DWC3_GHWPARAMS3_SSPHY_IFC(dwc->hwparams.hwparams3) ==
			DWC3_GHWPARAMS3_SSPHY_IFC_DIS) {
		if (dwc->maximum_speed == USB_SPEED_SUPER)
			dwc->maximum_speed = USB_SPEED_HIGH;
	}

	/* issue device SoftReset too */
	ret = dwc3_soft_reset(dwc);
	if (ret)
		goto err0;

	if(g_ic_type == IC_ATM7059A){
		dwc3_writel(dwc->regs, ANA02, 0x6046);
		dwc3_writel(dwc->regs, ANA0E, 0x2010);
		dwc3_writel(dwc->regs, ANA0F, 0x8000);
		dwc3_writel(dwc->regs, REV1, 0x0);
		dwc3_writel(dwc->regs, PAGE1_REG02, 0x0013);
		dwc3_writel(dwc->regs, PAGE1_REG06, 0x0004);
		dwc3_writel(dwc->regs, PAGE1_REG07, 0x22ed);
		dwc3_writel(dwc->regs, PAGE1_REG08, 0xf802);
		dwc3_writel(dwc->regs, PAGE1_REG09, 0x3080);
		dwc3_writel(dwc->regs, PAGE1_REG0B, 0x2030);
		dwc3_writel(dwc->regs, ANA0F, (1<<14));

		ret = dwc3_core_soft_reset(dwc,phy_reset_en);
		if (ret)
			goto err0;

		dwc3_cache_hwparams(dwc);
            	//====force to high speed====
		reg = dwc3_readl(dwc->regs, DWC3_DCFG);
		reg &= ~(DWC3_DCFG_SPEED_MASK);
		reg |= DWC3_DCFG_HIGHSPEED;
		dwc3_writel(dwc->regs, DWC3_DCFG, reg);
	}
	else if(g_ic_type == IC_ATM7039C){
		/*
		 * donot need set this DWC3_BACKDOOR reg,
		 * this may cause some masstorge device transfer error!!, 
		 * add by hwliu 2013.07.22 .
		 */
		ret = dwc3_core_soft_reset(dwc,phy_reset_en);
		if (ret)
			goto err0;

		reg = dwc3_readl(dwc->regs, DWC3_BACKDOOR);
		reg &= ~DWC3_FLADJ_30MHZ_MASK;	
		reg |= DWC3_FLADJ_30MHZ(0x20);
		dwc3_writel(dwc->regs, DWC3_BACKDOOR, reg);
	}
	else {
		ret = dwc3_core_soft_reset(dwc, 0);
			if (ret)
				goto err0;
	}

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~DWC3_GCTL_SCALEDOWN_MASK;

	switch (DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1)) {
	case DWC3_GHWPARAMS1_EN_PWROPT_CLK:
		/**
		 * WORKAROUND: DWC3 revisions between 2.10a and 2.50a have an
		 * issue which would cause xHCI compliance tests to fail.
		 *
		 * Because of that we cannot enable clock gating on such
		 * configurations.
		 *
		 * Refers to:
		 *
		 * STAR#9000588375: Clock Gating, SOF Issues when ref_clk-Based
		 * SOF/ITP Mode Used
		 */
		if ((dwc->dr_mode == USB_DR_MODE_HOST ||
				dwc->dr_mode == USB_DR_MODE_OTG) &&
				(dwc->revision >= DWC3_REVISION_210A &&
				dwc->revision <= DWC3_REVISION_250A))
			reg |= DWC3_GCTL_DSBLCLKGTNG | DWC3_GCTL_SOFITPSYNC;
		else
			reg &= ~DWC3_GCTL_DSBLCLKGTNG;
		break;
	case DWC3_GHWPARAMS1_EN_PWROPT_HIB:
		/* enable hibernation here */
		dwc->nr_scratch = DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(hwparams4);

		/*
		 * REVISIT Enabling this bit so that host-mode hibernation
		 * will work. Device-mode hibernation is not yet implemented.
		 */
		reg |= DWC3_GCTL_GBLHIBERNATIONEN;
		break;
	default:
		dev_dbg(dwc->dev, "No power optimization available\n");
	}

	/* check if current dwc3 is on simulation board */
	if (dwc->hwparams.hwparams6 & DWC3_GHWPARAMS6_EN_FPGA) {
		dev_dbg(dwc->dev, "it is on FPGA board\n");
		dwc->is_fpga = true;
	}

	WARN_ONCE(dwc->disable_scramble_quirk && !dwc->is_fpga,
			"disable_scramble cannot be used on non-FPGA builds\n");

	if (dwc->disable_scramble_quirk && dwc->is_fpga)
		reg |= DWC3_GCTL_DISSCRAMBLE;
	else
		reg &= ~DWC3_GCTL_DISSCRAMBLE;

	if (dwc->u2exit_lfps_quirk)
		reg |= DWC3_GCTL_U2EXIT_LFPS;

	/*
	 * WORKAROUND: DWC3 revisions <1.90a have a bug
	 * where the device can fail to connect at SuperSpeed
	 * and falls back to high-speed mode which causes
	 * the device to enter a Connect/Disconnect loop
	 */
	if (dwc->revision < DWC3_REVISION_190A)
		reg |= DWC3_GCTL_U2RSTECN;

	dwc3_core_num_eps(dwc);

	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	ret = dwc3_alloc_scratch_buffers(dwc);
	if (ret)
		goto err1;

	ret = dwc3_setup_scratch_buffers(dwc);
	if (ret)
		goto err2;

	return 0;

err2:
	dwc3_free_scratch_buffers(dwc);

err1:
	usb_phy_shutdown(dwc->usb2_phy);
	usb_phy_shutdown(dwc->usb3_phy);
	phy_exit(dwc->usb2_generic_phy);
	phy_exit(dwc->usb3_generic_phy);

err0:
	return ret;
}

static void dwc3_core_exit(struct dwc3 *dwc)
{
	dwc3_free_scratch_buffers(dwc);
	usb_phy_shutdown(dwc->usb2_phy);
	usb_phy_shutdown(dwc->usb3_phy);
	phy_exit(dwc->usb2_generic_phy);
	phy_exit(dwc->usb3_generic_phy);
}

static int dwc3_core_get_phy(struct dwc3 *dwc)
{
	struct device		*dev = dwc->dev;
	struct device_node	*node = dev->of_node;
	int ret;

	if (node) {
		dwc->usb2_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 0);
		dwc->usb3_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 1);
	} else {
		dwc->usb2_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
		dwc->usb3_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB3);
	}

	if (IS_ERR(dwc->usb2_phy)) {
		ret = PTR_ERR(dwc->usb2_phy);
		if (ret == -ENXIO || ret == -ENODEV) {
			dwc->usb2_phy = NULL;
		} else if (ret == -EPROBE_DEFER) {
			return ret;
		} else {
			dev_err(dev, "no usb2 phy configured\n");
			return ret;
		}
	}

	if (IS_ERR(dwc->usb3_phy)) {
		ret = PTR_ERR(dwc->usb3_phy);
		if (ret == -ENXIO || ret == -ENODEV) {
			dwc->usb3_phy = NULL;
		} else if (ret == -EPROBE_DEFER) {
			return ret;
		} else {
			dev_err(dev, "no usb3 phy configured\n");
			return ret;
		}
	}

	dwc->usb2_generic_phy = devm_phy_get(dev, "usb2-phy");
	if (IS_ERR(dwc->usb2_generic_phy)) {
		ret = PTR_ERR(dwc->usb2_generic_phy);
		if (ret == -ENOSYS || ret == -ENODEV) {
			dwc->usb2_generic_phy = NULL;
		} else if (ret == -EPROBE_DEFER) {
			return ret;
		} else {
			dev_err(dev, "no usb2 phy configured\n");
			return ret;
		}
	}

	dwc->usb3_generic_phy = devm_phy_get(dev, "usb3-phy");
	if (IS_ERR(dwc->usb3_generic_phy)) {
		ret = PTR_ERR(dwc->usb3_generic_phy);
		if (ret == -ENOSYS || ret == -ENODEV) {
			dwc->usb3_generic_phy = NULL;
		} else if (ret == -EPROBE_DEFER) {
			return ret;
		} else {
			dev_err(dev, "no usb3 phy configured\n");
			return ret;
		}
	}

	return 0;
}

static int dwc3_core_init_mode(struct dwc3 *dwc)
{
	struct device *dev = dwc->dev;
	int ret;

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_DEVICE);
		ret = dwc3_gadget_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			return ret;
		}
		break;
	case USB_DR_MODE_HOST:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_HOST);
		ret = dwc3_host_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			return ret;
		}
		break;
	case USB_DR_MODE_OTG:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_OTG);
		ret = dwc3_host_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			return ret;
		}

		ret = dwc3_gadget_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			return ret;
		}
		break;
	default:
		dev_err(dev, "Unsupported mode of operation %d\n", dwc->dr_mode);
		return -EINVAL;
	}

	return 0;
}

static void dwc3_core_exit_mode(struct dwc3 *dwc)
{
	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_gadget_exit(dwc);
		break;
	case USB_DR_MODE_HOST:
		dwc3_host_exit(dwc);
		break;
	case USB_DR_MODE_OTG:
		dwc3_host_exit(dwc);
		dwc3_gadget_exit(dwc);
		break;
	default:
		/* do nothing */
		break;
	}
}

#define DWC3_ALIGN_MASK		(16 - 1)

static int dwc3_probe(struct platform_device *pdev)
{
	struct device		*dev = &pdev->dev;
	struct dwc3_platform_data *pdata = dev_get_platdata(dev);
	struct resource		*res;
	struct dwc3		*dwc;
	u8			lpm_nyet_threshold;
	u8			tx_de_emphasis;
	u8			hird_threshold;
	u32			fladj = 0;

	int			ret;

	void __iomem		*regs;
	void			*mem;

#if SUPPORT_NOT_RMMOD_USBDRV
	pdev_dwc = pdev;
#endif

	mem = devm_kzalloc(dev, sizeof(*dwc) + DWC3_ALIGN_MASK, GFP_KERNEL);
	if (!mem)
		return -ENOMEM;

	dwc = PTR_ALIGN(mem, DWC3_ALIGN_MASK + 1);
	dwc->mem = mem;
	dwc->dev = dev;

       //to get ic type from dwc3-actions.ko,use resource method
       res = platform_get_resource(pdev, IORESOURCE_REG, 0);
	if (!res) {
		dev_err(dev, "missing IRQ\n");
		return -ENODEV;
	}
	g_ic_type = res->flags&0xff;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "missing IRQ\n");
		return -ENODEV;
	}
	dwc->xhci_resources[1].start = res->start;
	dwc->xhci_resources[1].end = res->end;
	dwc->xhci_resources[1].flags = res->flags;
	dwc->xhci_resources[1].name = res->name;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing memory resource\n");
		return -ENODEV;
	}

	dwc->xhci_resources[0].start = res->start;
	dwc->xhci_resources[0].end = dwc->xhci_resources[0].start +
					DWC3_XHCI_REGS_END;
	dwc->xhci_resources[0].flags = res->flags;
	dwc->xhci_resources[0].name = res->name;

	res->start += DWC3_GLOBALS_REGS_START;

	/*
	 * Request memory region but exclude xHCI regs,
	 * since it will be requested by the xhci-plat driver.
	 */
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		goto err0;
	}

	dwc->regs	= regs;
	dwc->regs_size	= resource_size(res);

	/* default to highest possible threshold */
	lpm_nyet_threshold = 0xff;

	/* default to -3.5dB de-emphasis */
	tx_de_emphasis = 1;

	/*
	 * default to assert utmi_sleep_n and use maximum allowed HIRD
	 * threshold value of 0b1100
	 */
	hird_threshold = 12;

	dwc->maximum_speed = usb_get_maximum_speed(dev);
	dwc->dr_mode = usb_get_dr_mode(dev);

	dwc->has_lpm_erratum = device_property_read_bool(dev,
				"snps,has-lpm-erratum");
	device_property_read_u8(dev, "snps,lpm-nyet-threshold",
				&lpm_nyet_threshold);
	dwc->is_utmi_l1_suspend = device_property_read_bool(dev,
				"snps,is-utmi-l1-suspend");
	device_property_read_u8(dev, "snps,hird-threshold",
				&hird_threshold);
	dwc->usb3_lpm_capable = device_property_read_bool(dev,
				"snps,usb3_lpm_capable");

	dwc->needs_fifo_resize = device_property_read_bool(dev,
				"tx-fifo-resize");

	dwc->disable_scramble_quirk = device_property_read_bool(dev,
				"snps,disable_scramble_quirk");
	dwc->u2exit_lfps_quirk = device_property_read_bool(dev,
				"snps,u2exit_lfps_quirk");
	dwc->u2ss_inp3_quirk = device_property_read_bool(dev,
				"snps,u2ss_inp3_quirk");
	dwc->req_p1p2p3_quirk = device_property_read_bool(dev,
				"snps,req_p1p2p3_quirk");
	dwc->del_p1p2p3_quirk = device_property_read_bool(dev,
				"snps,del_p1p2p3_quirk");
	dwc->del_phy_power_chg_quirk = device_property_read_bool(dev,
				"snps,del_phy_power_chg_quirk");
	dwc->lfps_filter_quirk = device_property_read_bool(dev,
				"snps,lfps_filter_quirk");
	dwc->rx_detect_poll_quirk = device_property_read_bool(dev,
				"snps,rx_detect_poll_quirk");
	dwc->dis_u3_susphy_quirk = device_property_read_bool(dev,
				"snps,dis_u3_susphy_quirk");
	dwc->dis_u2_susphy_quirk = device_property_read_bool(dev,
				"snps,dis_u2_susphy_quirk");
	dwc->dis_enblslpm_quirk = device_property_read_bool(dev,
				"snps,dis_enblslpm_quirk");

	dwc->tx_de_emphasis_quirk = device_property_read_bool(dev,
				"snps,tx_de_emphasis_quirk");
	device_property_read_u8(dev, "snps,tx_de_emphasis",
				&tx_de_emphasis);
	device_property_read_string(dev, "snps,hsphy_interface",
				    &dwc->hsphy_interface);
	device_property_read_u32(dev, "snps,quirk-frame-length-adjustment",
				 &fladj);

	if (pdata) {
		dwc->maximum_speed = pdata->maximum_speed;
		dwc->has_lpm_erratum = pdata->has_lpm_erratum;
		if (pdata->lpm_nyet_threshold)
			lpm_nyet_threshold = pdata->lpm_nyet_threshold;
		dwc->is_utmi_l1_suspend = pdata->is_utmi_l1_suspend;
		if (pdata->hird_threshold)
			hird_threshold = pdata->hird_threshold;

		dwc->needs_fifo_resize = pdata->tx_fifo_resize;
		dwc->usb3_lpm_capable = pdata->usb3_lpm_capable;
		dwc->dr_mode = pdata->dr_mode;

		dwc->disable_scramble_quirk = pdata->disable_scramble_quirk;
		dwc->u2exit_lfps_quirk = pdata->u2exit_lfps_quirk;
		dwc->u2ss_inp3_quirk = pdata->u2ss_inp3_quirk;
		dwc->req_p1p2p3_quirk = pdata->req_p1p2p3_quirk;
		dwc->del_p1p2p3_quirk = pdata->del_p1p2p3_quirk;
		dwc->del_phy_power_chg_quirk = pdata->del_phy_power_chg_quirk;
		dwc->lfps_filter_quirk = pdata->lfps_filter_quirk;
		dwc->rx_detect_poll_quirk = pdata->rx_detect_poll_quirk;
		dwc->dis_u3_susphy_quirk = pdata->dis_u3_susphy_quirk;
		dwc->dis_u2_susphy_quirk = pdata->dis_u2_susphy_quirk;
		dwc->dis_enblslpm_quirk = pdata->dis_enblslpm_quirk;

		dwc->tx_de_emphasis_quirk = pdata->tx_de_emphasis_quirk;
		if (pdata->tx_de_emphasis)
			tx_de_emphasis = pdata->tx_de_emphasis;

		dwc->hsphy_interface = pdata->hsphy_interface;
		fladj = pdata->fladj_value;
	}

	/* default to superspeed if no maximum_speed passed */
	if (dwc->maximum_speed == USB_SPEED_UNKNOWN)
		dwc->maximum_speed = USB_SPEED_SUPER;

	dwc->lpm_nyet_threshold = lpm_nyet_threshold;
	dwc->tx_de_emphasis = tx_de_emphasis;

	dwc->hird_threshold = hird_threshold
		| (dwc->is_utmi_l1_suspend << 4);

	platform_set_drvdata(pdev, dwc);
	dwc3_cache_hwparams(dwc);

	ret = dwc3_phy_setup(dwc);
	if (ret)
		goto err0;

	ret = dwc3_core_get_phy(dwc);
	if (ret)
		goto err0;

	owl_powergate_power_on(OWL_POWERGATE_USB3);

	spin_lock_init(&dwc->lock);

	if (!dev->dma_mask) {
		dev->dma_mask = dev->parent->dma_mask;
		dev->dma_parms = dev->parent->dma_parms;
		dma_set_coherent_mask(dev, dev->parent->coherent_dma_mask);
	}

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	pm_runtime_forbid(dev);

	ret = dwc3_alloc_event_buffers(dwc, DWC3_EVENT_BUFFERS_SIZE);
	if (ret) {
		dev_err(dwc->dev, "failed to allocate event buffers\n");
		ret = -ENOMEM;
		goto err1;
	}

	if (IS_ENABLED(CONFIG_USB_DWC3_HOST))
		dwc->dr_mode = USB_DR_MODE_HOST;
	else if (IS_ENABLED(CONFIG_USB_DWC3_GADGET))
		dwc->dr_mode = USB_DR_MODE_PERIPHERAL;

	if (dwc->dr_mode == USB_DR_MODE_UNKNOWN)
		dwc->dr_mode = USB_DR_MODE_OTG;

	dwc->dr_mode = USB_DR_MODE_PERIPHERAL;	
	if(g_ic_type == IC_ATM7039C){
		dwc3_phy_init(dwc->dr_mode, 0);
	}
	else if(g_ic_type == IC_ATM7059A){
		dwc3_phy_init(dwc->dr_mode, 0);
		dwc3_phy_init(dwc->dr_mode, 1);
	}

	ret = dwc3_core_init(dwc,0);
	if (ret) {
		dev_err(dev, "failed to initialize core\n");
		goto err1;
	}

	/* Adjust Frame Length */
	dwc3_frame_length_adjustment(dwc, fladj);

	usb_phy_set_suspend(dwc->usb2_phy, 0);
	usb_phy_set_suspend(dwc->usb3_phy, 0);
	ret = phy_power_on(dwc->usb2_generic_phy);
	if (ret < 0)
		goto err2;

	ret = phy_power_on(dwc->usb3_generic_phy);
	if (ret < 0)
		goto err3;

	ret = dwc3_event_buffers_setup(dwc);
	if (ret) {
		dev_err(dwc->dev, "failed to setup event buffers\n");
		goto err4;
	}

	ret = dwc3_core_init_mode(dwc);
	if (ret)
		goto err5;

	ret = dwc3_debugfs_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize debugfs\n");
		goto err6;
	}

	pm_runtime_allow(dev);

	return 0;

err6:
	dwc3_core_exit_mode(dwc);

err5:
	dwc3_event_buffers_cleanup(dwc);

err4:
	phy_power_off(dwc->usb3_generic_phy);

err3:
	phy_power_off(dwc->usb2_generic_phy);

err2:
	usb_phy_set_suspend(dwc->usb2_phy, 1);
	usb_phy_set_suspend(dwc->usb3_phy, 1);
	dwc3_core_exit(dwc);

err1:
	dwc3_free_event_buffers(dwc);
	dwc3_ulpi_exit(dwc);

err0:
	/*
	 * restore res->start back to its original value so that, in case the
	 * probe is deferred, we don't end up getting error in request the
	 * memory region the next time probe is called.
	 */
	res->start -= DWC3_GLOBALS_REGS_START;

	return ret;
}

static int dwc3_remove(struct platform_device *pdev)
{
	struct dwc3	*dwc = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	/*
	 * restore res->start back to its original value so that, in case the
	 * probe is deferred, we don't end up getting error in request the
	 * memory region the next time probe is called.
	 */
	res->start -= DWC3_GLOBALS_REGS_START;

	dwc3_debugfs_exit(dwc);
	dwc3_core_exit_mode(dwc);
	dwc3_event_buffers_cleanup(dwc);
	dwc3_free_event_buffers(dwc);

	usb_phy_set_suspend(dwc->usb2_phy, 1);
	usb_phy_set_suspend(dwc->usb3_phy, 1);
	phy_power_off(dwc->usb2_generic_phy);
	phy_power_off(dwc->usb3_generic_phy);

	dwc3_core_exit(dwc);
	dwc3_ulpi_exit(dwc);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwc3_suspend(struct device *dev)
{
	struct dwc3	*dwc = dev_get_drvdata(dev);
	unsigned long	flags;

	spin_lock_irqsave(&dwc->lock, flags);

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		dwc3_gadget_suspend(dwc);
		/* FALLTHROUGH */
	case USB_DR_MODE_HOST:
	default:
		dwc3_event_buffers_cleanup(dwc);
		break;
	}

	dwc->gctl = dwc3_readl(dwc->regs, DWC3_GCTL);
	spin_unlock_irqrestore(&dwc->lock, flags);

	usb_phy_shutdown(dwc->usb3_phy);
	usb_phy_shutdown(dwc->usb2_phy);
	phy_exit(dwc->usb2_generic_phy);
	phy_exit(dwc->usb3_generic_phy);

	return 0;
}

static int dwc3_resume(struct device *dev)
{
	struct dwc3	*dwc = dev_get_drvdata(dev);
	unsigned long	flags;
	int		ret;

	usb_phy_init(dwc->usb3_phy);
	usb_phy_init(dwc->usb2_phy);
	ret = phy_init(dwc->usb2_generic_phy);
	if (ret < 0)
		return ret;

	ret = phy_init(dwc->usb3_generic_phy);
	if (ret < 0)
		goto err_usb2phy_init;

	spin_lock_irqsave(&dwc->lock, flags);

	dwc3_event_buffers_setup(dwc);
	dwc3_writel(dwc->regs, DWC3_GCTL, dwc->gctl);

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		dwc3_gadget_resume(dwc);
		/* FALLTHROUGH */
	case USB_DR_MODE_HOST:
	default:
		/* do nothing */
		break;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);

	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;

err_usb2phy_init:
	phy_exit(dwc->usb2_generic_phy);

	return ret;
}

static const struct dev_pm_ops dwc3_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_suspend, dwc3_resume)
};

#define DWC3_PM_OPS	&(dwc3_dev_pm_ops)
#else
#define DWC3_PM_OPS	NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id of_dwc3_match[] = {
	{
		.compatible = "snps,dwc3"
	},
	{
		.compatible = "synopsys,dwc3"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_dwc3_match);
#endif

#ifdef CONFIG_ACPI

#define ACPI_ID_INTEL_BSW	"808622B7"

static const struct acpi_device_id dwc3_acpi_match[] = {
	{ ACPI_ID_INTEL_BSW, 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, dwc3_acpi_match);
#endif

static struct platform_driver dwc3_driver = {
	.probe		= dwc3_probe,
	.remove		= dwc3_remove,
	.driver		= {
		.name	= "dwc3",
		.of_match_table	= of_match_ptr(of_dwc3_match),
		.acpi_match_table = ACPI_PTR(dwc3_acpi_match),
		.pm	= DWC3_PM_OPS,
	},
};

module_platform_driver(dwc3_driver);

MODULE_ALIAS("platform:dwc3");
MODULE_AUTHOR("Felipe Balbi <balbi@ti.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 DRD Controller Driver");
