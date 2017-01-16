/*********************************************************************************
*                            Module: usb monitor driver
*                (c) Copyright 2003 - 2008, Actions Co,Ld. 
*                        All Right Reserved 
*
* History:        
*      <author>      <time>       <version >    <desc>
*       houjingkun   2011/07/08   1.0         build this file 
********************************************************************************/
/*!
 * \file   umonitor_core.c
 * \brief  
 *      usb monitor detect opration code.
 * \author houjingkun
 * \par GENERAL DESCRIPTION:
 * \par EXTERNALIZED FUNCTIONS:
 *       null
 *
 *  Copyright(c) 2008-2012 Actions Semiconductor, All Rights Reserved.
 *
 * \version 1.0
 * \date  2011/07/08
 *******************************************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>
#include <mach/hardware.h>
#include <linux/io.h>

#include "aotg_regs.h"
#include "umonitor_config.h"
#include "umonitor_core.h"


enum {
	USB_DET_NONE = 0,	/* nothing detected, maybe B plus is out. */
	USB_DET_DEVICE_DEBUOUNCING,	/* detected device, debouncing and confirming. */
	USB_DET_DEVICE_PC,	/* detected device confirmed. pc connected. */
	USB_DET_DEVICE_CHARGER	/* detected device confirmed. charger connected. */
};

enum {
	USB_DET_HOST_NONE = 0,	/* nothing detected. maybe udisk is plug out. */
	USB_DET_HOST_DEBOUNCING,	/* detecting host, debouncing and confirming. */
	USB_DET_HOST_UDISK	/* detected udisk confirmed. udisk connected. */
};

#define USB_DEVICE_DETECT_STEPS    4
#define USB_HOST_DETECT_STEPS      4
#define USB_MONITOR_DEF_INTERVAL   500	/* interval to check usb port state, unit: ms. */

umonitor_dev_status_t *umonitor_status;
static int usb_monitor_debug_status_inf( void )
{
#if 0
	umonitor_dev_status_t *pStatus = umonitor_status;

	printk(KERN_INFO ".det_phase %d %d %d %d %d \n",
	       (unsigned int) pStatus->det_phase,
	       (unsigned int) pStatus->vbus_status,
	       (unsigned int) pStatus->timer_steps,
	       (unsigned int) pStatus->host_confirm,
	       (unsigned int) pStatus->message_status);
	printk(KERN_INFO "-----------------------------\n");
	printk(KERN_INFO ".vbus_status %d %x \n", (unsigned int) pStatus->vbus_status,
	       (unsigned int) pStatus->vbus_status);
	printk(KERN_INFO ".vbus_enable_power %d\n", (unsigned int) pStatus->vbus_enable_power);
	printk(KERN_INFO ".det_phase %d \n", (unsigned int) pStatus->det_phase);
	printk(KERN_INFO ".device_confirm %d\n", (unsigned int) pStatus->device_confirm);
	printk(KERN_INFO ".host_confirm %d \n", (unsigned int) pStatus->host_confirm);
	printk(KERN_INFO ".usb_pll_on %d \n", (unsigned int) pStatus->usb_pll_on);
	printk(KERN_INFO ".dp_dm_status %d 0x%x \n", (unsigned int) pStatus->dp_dm_status,
	       (unsigned int) pStatus->dp_dm_status);
	printk(KERN_INFO ".timer_steps %d \n", (unsigned int) pStatus->timer_steps);
	printk(KERN_INFO ".timer_interval %d \n", (unsigned int) pStatus->timer_interval);
	printk(KERN_INFO ".check_cnt %d \n", (unsigned int) pStatus->check_cnt);
	printk(KERN_INFO ".sof_check_times %d\n", (unsigned int) pStatus->sof_check_times);
	printk(KERN_INFO "\n \n ");
#endif
	return 0;
}

static int usb_init_monitor_status(umonitor_dev_status_t * pStatus)
{
  
	pStatus->detect_valid = 0;
	pStatus->detect_running = 0;
	pStatus->vbus_status = 0;
	pStatus->dc5v_status = 0;
	pStatus->det_phase = 0;
	pStatus->device_confirm = 0;
	pStatus->sof_check_times = 0;
	pStatus->host_confirm = 0;
	pStatus->usb_pll_on = 0;
	pStatus->dp_dm_status = 0;
	pStatus->timer_steps = 0;
	pStatus->timer_interval = USB_MONITOR_DEF_INTERVAL;
	pStatus->check_cnt = 0;
	pStatus->message_status = 0;
	pStatus->core_ops = NULL;
		
	pStatus->vbus_enable_power = 0;
	
	return 0;
}

/* \BB\F1ȡ\D4\CB\D0е\BD\CF\C2һ\B4μ\EC\B2\E2\B5\C4ʱ\BC\E4\BC\E4\B8\F4\A3\AC\B7\B5\BB\D8ֵ\D2Ժ\C1\C3\EBΪ\B5\A5λ\A1\A3 */
unsigned int umonitor_get_timer_step_interval(void)
{
	umonitor_dev_status_t *pStatus;

	pStatus = umonitor_status;

	if ((pStatus->port_config->detect_type == UMONITOR_DISABLE)
		|| (pStatus->detect_valid == 0)) {
		return 0x70000000;	/* be longer enough that it would not run again. */
	}

	if (pStatus->timer_steps == 0) {
		//pStatus->timer_interval = USB_MONITOR_DEF_INTERVAL;
		pStatus->timer_interval = 30;	/*\D6\D8\D0½\F8\C8\EBstep 0 \BC\EC\B2\E9 */
		goto out;
	}

	if (pStatus->det_phase == 0) {
		switch (pStatus->timer_steps) {
			/* 
			 * 1\A3\AD3\B2\BD\A3\AC\BC\EC\B2\E2ʱ\BC\E4\BC\E4\B8\F4500 ms\A3\A8\BFɵ\F7\D5\FB\A3\A9\A3\ACһ\B5\A9\B6˿\DA\D3б仯\C2\ED\C9ϵ\F7\B5\BD\B5\DA\CBĲ\BD\A3\AC
			 * \BD\F8\C8\EBdebounce\BA\CDconfirm״̬\A1\A3
			 */
		case 1:
		case 2:
		case 3:
			pStatus->timer_interval = USB_MONITOR_DEF_INTERVAL;
			break;

		case 4:
			switch (pStatus->device_confirm) {
			case 0:	/* \BD\F8\C8\EB\B5\DA4\B2\BD\A3\AC\CF\C2һ\B2\BD\CAǼ\EC\B2\E2\B6˿\DAvbus\D3\D0\CEޱ仯\A1\A3 */
				pStatus->timer_interval =
				    USB_MONITOR_DEF_INTERVAL;
				break;

			case 1:	/* \D2Ѿ\AD\BC\EC\B2⵽\B6˿\DAvbus\D3е磬\D0\E8Ҫ\CF\C2һ\B2\BD\D4\D9ȷ\C8\CFһ\B4Ρ\A3 */
				pStatus->timer_interval = 10;	/* 10 ms, 1 tick. */
				break;

				/*
				 * device_confirm == 2, \D2Ѿ\ADȷ\C8\CFvbus\D3е磬\B2\A2enable\C9\CF\C0\AD500k\A3\AC
				 * disable\CF\C2\C0\AD15k\A3\AC\CF\C2һ\B2\BD\BC\EC\B2\E2dp\A1\A2dm״̬\A1\A3 
				 */
			case 2:
				pStatus->timer_interval = 300;
				break;

				/* \B5\DAһ\B4λ\F1ȡdp\A1\A2dm״̬\A3\AC\D0\E8Ҫ\CF\C2һ\B4\CE\D4\D9ȷ\C8\CFһ\B4Ρ\A3 */
			case 3:
				pStatus->timer_interval = 30;
				break;

				/* 
				 * \BD\F8\C8\EB\D5\E2һ\B2\BD\A3\AC\D0\E8Ҫ\B6\E0\B4\CE\C5ж\CFpc\CAǷ\F1\D3\D0sof\BB\F2reset\D0źŸ\F8С\BB\FA\A3\AC\D5\E2\C0\EFÿ20\BA\C1\C3\EB\B2\E9ѯsof\D6ж\CFһ\B4Σ\AC
				 * \C0ۻ\FD\BB\E1\B2\E9ѯ MAX_DETECT_SOF_CNT \B4Σ\AC\B5ȴ\FDʱ\BC\E4\BF\C9\C4ܻ\E1\D3\D08\C3\EB\D2\D4\C9ϡ\A3 
				 * \D2\F2Ϊ\B4\D3С\BB\FAdp\C9\CF\C0\AD\B5\BDpc\B7\A2sof\A3\AC\D6м\E4\BF\C9\C4\DC\D1\D3ʱ\B3\A4\B4\EF8\C3\EB\D2\D4\C9ϡ\A3
				 */
				/* wait sof again time interval, the whole detect sof time is (20 * sof_check_times) msecond. */
			case 4:
				pStatus->timer_interval = 20;
				break;

			default:
				USB_ERR_PLACE;
				pStatus->timer_interval =
				    USB_MONITOR_DEF_INTERVAL;
				break;
			}
			break;

		default:
			USB_ERR_PLACE;
			pStatus->timer_interval = USB_MONITOR_DEF_INTERVAL;
			break;
		}
	} else {
		switch (pStatus->timer_steps) {
		case 1:	/* \B4\D3step 0\B5\C4idle״̬\C7л\BB\B5\BDvbus\B6\D4\CD⹩\B5\E7\B5\C4ʱ\BC\E4\BC\E4\B8\F4\A1\A3 */
			pStatus->timer_interval = 30;
			break;

		case 2:	/* vbus\B6\D4\CD⹩\B5\E7\BA󣬵\BD\BF\AAʼ\B5\DA2\B4μ\EC\B2⣬\C5ж\CFdp\CAǷ\F1\C9\CF\C0\AD\B5\C4ʱ\BC䡣 */
			pStatus->timer_interval = 600;
			break;

		case 3:	/* \B5\DA2\B4μ\EC\B2⵽\B5\DA3\B4μ\EC\B2\E2֮\BC䣬\C5ж\CFdp\CAǷ\F1\C9\CF\C0\AD\B5\C4ʱ\BC䡣 */
			pStatus->timer_interval = 600;
			break;

		case 4:
			switch (pStatus->host_confirm) {
			case 0:
				pStatus->timer_interval =
				    USB_MONITOR_DEF_INTERVAL;
				break;

			case 1:	/* debounce time. */
				pStatus->timer_interval = 10;	/* 10 ms, 1 tick. */
				break;

			default:
				USB_ERR_PLACE;
				pStatus->timer_interval =
				    USB_MONITOR_DEF_INTERVAL;
				break;
			}
			break;

		default:
			USB_ERR_PLACE;
			pStatus->timer_interval = USB_MONITOR_DEF_INTERVAL;
			break;
		}
	}

out:
	return pStatus->timer_interval;
}

/* 
 * retval:
 * refer to below macro:
 *    USB_DET_NONE,
 *    USB_DET_DEVICE_DEBUOUNCING,
 *    USB_DET_DEVICE_PC,
 *    USB_DET_DEVICE_CHARGER,  
 */
static int usb_timer_det_pc_charger(umonitor_dev_status_t * pStatus)
{
	int ret = 0;
	unsigned int val = 0;
	usb_hal_monitor_t *p_hal = &pStatus->umonitor_hal;
	
	MONITOR_PRINTK("entring usb_timer_det_pc_charger\n");

	if (pStatus->device_confirm == 0) {
		/* make sure power off. */
		if (pStatus->vbus_enable_power != 0) {
			p_hal->vbus_power_onoff(p_hal, 0);
			pStatus->vbus_enable_power = 0;
			p_hal->set_soft_id(p_hal, 1, 1);
		}
	}

	pStatus->vbus_status = (unsigned char) p_hal->get_vbus_state(p_hal);

	if (pStatus->vbus_status == USB_VBUS_HIGH) {
      MONITOR_PRINTK("vbus is high!!!!!!!\n");
		/* 
		 * if B_IN is send out, needn't check device at all. 
		 * \CE\D2\C3\C7ֻ\B4\A6\C0\ED\CFȲ\E5\C8\EB\B3\E4\B5\E7\C6\F7\B5\C4\C7\E9\BF\F6\CF£\AC\BC\EC\B2\E2pc\B5\C4\C1\AC\BDӺͶϿ\AA\A1\A3\C8\E7\B9\FB\CF\C8\C1\AC\BD\D3pc\A3\AC\D4\F2\B6\D4\D3ڳ\E4\B5\E7\C6\F7\B5Ĳ\E5\B0μ\EC\B2ⲻ\C1ˡ\A3
		 */
		if ((pStatus->message_status & (0x1 << MONITOR_B_IN)) != 0) {
#if 0
			/*
			 * \B4˶δ\FA\C2뱾Ϊ\BC\EC\B2\E2Բ\BFڳ\E4\B5\E7\C6\F7\BA\CDpcͬʱ\B2\E5\C8\EB\B5\C4\C7\E9\BF\F6\CF¼\EC\B2\E2pc\B5İ\CE\CFߣ\AC\B5\ABʵ\BC\CA\C9ϴ\E6\D4\DA\CE\F3\C5ж\CFpc\B0\CE\CFߵ\C4\C7\E9\BF\F6\A3\AC
			 * \D2\F2Ϊpc\B5\C4sof\BF\C9\C4\DC\D4ڶ\E0\B4η\A2\CB\CDδ\B9\FB\BA\F3һ\B6\CEʱ\BC\E4\C4ڲ\BB\D4ٷ\A2\CB͡\A3
			 */
			/* if pc is connected, and charger is new plug in, we ignore it. */
			if ((pStatus->message_status &
			     (0x1 << MONITOR_CHARGER_IN)) == 0)
#endif
			pStatus->device_confirm = 0;
			pStatus->timer_steps = 0;
			ret = USB_DET_DEVICE_PC;
			goto out2;
		}
		if ((pStatus->message_status & (0x1 << MONITOR_CHARGER_IN)) != 0) {
			pStatus->device_confirm = 0;
			pStatus->timer_steps = 0;
			ret = USB_DET_DEVICE_CHARGER;
			goto out2;
		}

		switch (pStatus->device_confirm) {
			/* \BD\F8\C8\EB\B5\DA4\B2\BD\A3\AC\BC\EC\B2⵽\B6˿\DAvbus\D3е硣\D6\C1\C9\D9deboundceһ\B4Σ\ACȷ\B1\A3\BC\EC\B2\E2\D5\FDȷ\A1\A3 */
		case 0:
			pStatus->timer_steps = USB_DEVICE_DETECT_STEPS;	/* the last timer_steps is to confirm. */
			pStatus->device_confirm = 1;
			ret = USB_DET_DEVICE_DEBUOUNCING;
			goto out2;

			/* \D2Ѿ\ADȷ\C8\CFvbus\D3е磬\B2\A2enable\C9\CF\C0\AD500k\A3\ACdisable\CF\C2\C0\AD15k\A3\AC\CF\C2һ\B2\BD\BC\EC\B2\E2dp\A1\A2dm״̬\A1\A3 */
		case 1:
			p_hal->set_dp_500k_15k(p_hal, 1, 0);	/* 500k up enable, 15k down disable; */
			pStatus->device_confirm = 2;
			ret = USB_DET_DEVICE_DEBUOUNCING;
			goto out2;

			/* \B5\DAһ\B4λ\F1ȡdp\A1\A2dm״̬\A3\AC\D0\E8Ҫ\D4\D9ȷ\C8\CFһ\B4Ρ\A3 */
		case 2:
			pStatus->dp_dm_status = p_hal->get_linestates(p_hal);	// get dp dm status.
			pStatus->device_confirm = 3;
			//pStatus->device_confirm = 2;  /* always in get dp dm states, just for test. */
			ret = USB_DET_DEVICE_DEBUOUNCING;
			goto out2;

			/* 
			 * \B5ڶ\FE\B4λ\F1ȡdp\A1\A2dm״̬\A3\AC\C8\E7\B9\FB\C1\BD\B4β\BB\B1䣬\D4\F2ȷ\C8\CFok\A3\AC\B7\F1\D4\F2\BD\F8һ\B2\BDdebounce\A1\A3
			 * dp\BA\CDdm\B7\C70״̬Ϊ\B3\E4\B5\E7\C6\F7\A3\AC\B7\B4֮\BD\F8һ\B2\BD\C5ж\CFsof\D6ж\CFλ\BF\B4\BF\B4\CAǷ\F1pc\A1\A3
			 */
		case 3:
			val = p_hal->get_linestates(p_hal);	// get dp dm status.
			pStatus->sof_check_times = 0;
			if (val == pStatus->dp_dm_status) {
				if (val == 0x00) {
					pStatus->timer_steps = 0;
					pStatus->device_confirm = 0;
					ret = USB_DET_DEVICE_PC;
					
					goto out2;
				} else {
					pStatus->device_confirm = 0;
					/* if enable monitor again, it should begin from step 0.  */
					pStatus->timer_steps = 0;
					ret = USB_DET_DEVICE_PC;
					goto out2;
				}
			} else {
				pStatus->device_confirm = 1;
				ret = USB_DET_DEVICE_DEBUOUNCING;
				goto out2;
			}

			/* 
			 * \BD\F8\C8\EB\D5\E2һ\B2\BD\A3\AC\D0\E8Ҫ\B6\E0\B4\CE\C5ж\CFpc\CAǷ\F1\D3\D0sof\BB\F2reset\D0źŸ\F8С\BB\FA\A3\AC
			 * \B5ȴ\FDʱ\BC\E4\BF\C9\C4ܻ\E1\D3\D08\C3\EB\D2\D4\C9ϡ\A3 \D2\F2Ϊ\B4\D3С\BB\FAdp\C9\CF\C0\AD\B5\BDpc\B7\A2sof\A3\AC\D6м\E4\BF\C9\C4\DC\D1\D3ʱ\B3\A4\B4\EF8\C3\EB\D2\D4\C9ϡ\A3
			 */
			/* for detect sof or reset irq. */
		case 4:
			val = p_hal->is_sof(p_hal);
			if (val != 0) {
				/* if enable monitor again, it should begin from step 0. */
				pStatus->timer_steps = 0;
				pStatus->device_confirm = 0;
				pStatus->sof_check_times = 0;
				p_hal->dp_down(p_hal);
				ret = USB_DET_DEVICE_PC;
				goto out2;
			}
			if (pStatus->sof_check_times < MAX_DETECT_SOF_CNT) {	/* 10 * MAX_DETECT_SOF_CNT ms. */
				pStatus->device_confirm = 4;	/* next step still check again. */
				pStatus->sof_check_times++;
				ret = USB_DET_DEVICE_DEBUOUNCING;
				goto out2;
			}

			/* if enable monitor again, it should begin from step 0. */
			pStatus->timer_steps = 0;
			pStatus->device_confirm = 0;
			pStatus->sof_check_times = 0;
			p_hal->dp_down(p_hal);
			/* treated as charger in. */
			ret = USB_DET_DEVICE_CHARGER;
			goto out2;

		default:
			MONITOR_ERR("into device confirm default, err!\n");
			pStatus->device_confirm = 0;
			ret = USB_DET_NONE;
			goto out;
		}
	} else {	  
		pStatus->device_confirm = 0;
		pStatus->timer_steps =USB_DEVICE_DETECT_STEPS;
		ret = USB_DET_NONE;
		goto out;
	}

      out:
	pStatus->timer_steps++;
	if (pStatus->timer_steps > USB_DEVICE_DETECT_STEPS) {
		pStatus->timer_steps = 0;
	}
      out2:
	return ret;
}

/* 
 * retval:
 * refer to below macro:
 *    USB_DET_HOST_NONE,
 *    USB_DET_HOST_DEBOUNCING,
 *    USB_DET_HOST_UDISK,
 */
static int usb_timer_det_udisk(umonitor_dev_status_t * pStatus)
{
	unsigned int val;
	usb_hal_monitor_t *p_hal = &pStatus->umonitor_hal;

	if (pStatus->timer_steps == 1) {

		p_hal->set_dp_500k_15k(p_hal, 0, 1);	/* disable 500k, enable 15k. */

		if (pStatus->vbus_enable_power == 0) {
			p_hal->vbus_power_onoff(p_hal, 1);
			pStatus->vbus_enable_power = 1;
			p_hal->set_soft_id(p_hal, 1, 0);
		}
		goto out;
	} else {
		if (pStatus->vbus_enable_power != 1) {
			USB_ERR_PLACE;
		}
		   
		val = p_hal->get_linestates(p_hal);	// get dp dm status.
		MONITOR_PRINTK("host debounce!!!, linestate %04x\n", val);
		
    pStatus->timer_steps = 0;
    pStatus->host_confirm = 0;
    return USB_DET_HOST_UDISK;
		    
		if ((val == 0x1) || (val == 0x2)) {
			switch (pStatus->host_confirm) {
			case 0:
				pStatus->host_confirm = 1;
				/* the last step is always debounce and confirm step. */
				pStatus->timer_steps = USB_HOST_DETECT_STEPS;
				pStatus->dp_dm_status = val;
				return USB_DET_HOST_DEBOUNCING;

			case 1:
				if (val == pStatus->dp_dm_status) {
					/* if enable monitor again, it should begin from step 0.  */
					pStatus->timer_steps = 0;
					pStatus->host_confirm = 0;
					return USB_DET_HOST_UDISK;
				} else {
					pStatus->dp_dm_status = val;
					pStatus->host_confirm = 0;
					return USB_DET_HOST_DEBOUNCING;
				}

			default:
				break;
			}
		} else {
			pStatus->host_confirm = 0;
			goto out;
		}
	}

out:
	pStatus->timer_steps++;
	if (pStatus->timer_steps > USB_HOST_DETECT_STEPS) {
		pStatus->timer_steps = 0;
		return USB_DET_HOST_NONE;	/* nothing detect, maybe udisk is plug out. */
	}
	return USB_DET_HOST_DEBOUNCING;
}

/* 
 * \B4\A6\C0\ED\BBָ\B4\B5\BDstep 0\A3\A8\BC\B4\B5\DA0\B2\BD\A3\A9\B5\C4\C7\E9\BF\F6\A1\A3step 0\BD׶β\BB\C7\F8\B7\D6device\BC\EC\B2⻹\CA\C7host\BC\EC\B2⣬
 * \CB\FCֻ\CAǻָ\B4\B5\BDһ\B8\F6Ĭ\C8\CF״̬\A3\ACΪ\CF\C2һ\B4\CEdevice\BB\F2host\BC\EC\B2\E2\D7\F6׼\B1\B8\A1\A3 
 */
static int usb_timer_process_step0(umonitor_dev_status_t * pStatus)
{
	int ret = 0;
	unsigned int status = 0;
	usb_hal_monitor_t *p_hal = &pStatus->umonitor_hal;
	
	MONITOR_PRINTK("entring usb_timer_process_step0\n");

	if ((pStatus->message_status & (0x1 << MONITOR_B_IN)) != 0) {
		MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_B_OUT\n", __FILE__, __LINE__);
		printk("\n%s--%d, SYS_MSG_USB_B_OUT\n", __FILE__, __LINE__);
		pStatus->core_ops->putt_msg(MON_MSG_USB_B_OUT);
		pStatus->message_status =pStatus->message_status & (~(0x1 << MONITOR_B_IN));
	}

	if ((pStatus->message_status & (0x1 << MONITOR_A_IN)) != 0) {
		MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_A_OUT\n", __FILE__, __LINE__);
		printk("\n%s--%d, SYS_MSG_USB_A_OUT\n", __FILE__, __LINE__);
		pStatus->core_ops->putt_msg(MON_MSG_USB_A_OUT);
		pStatus->message_status = pStatus->message_status & (~(0x1 << MONITOR_A_IN));
	}

	/*
	 * \B6\D4\D3\DA\D3\D0id pin, \BB\F2\D3\C3gpio\BC\EC\B2\E2idpin\B5\C4\C7\E9\BF\F6, \B5\B1idpinΪ0, \D4\F2\C8\C3\CB\FCһֱ\B4\A6\D3\DAhost\BC\EC\B2\E2״̬,
	 * \B2\BBҪ\C8\C3vbus\B5\F4\B5\E7. һֱvbus\B9\A9\B5\E7,\BF\C9\D2Լ\E6\C8ݲ\E5\C8\EBmp3,mp4\B5\C4\C7\E9\BF\F6. (\D2\F2Ϊ\D5\E2Щ\C9豸\D3п\C9\C4\DC\D4ڹ\A9\B5\E7
	 * \BC\B8ʮ\C3\EB\BA\F3dp\B2\C5\C9\CF\C0\AD\A1\A3
	 */
	if (p_hal->config->detect_type == UMONITOR_DEVICE_ONLY) {
		ret = USB_ID_STATE_DEVICE;
	} else if (p_hal->config->detect_type == UMONITOR_HOST_ONLY) {
		ret = USB_ID_STATE_HOST;
	} else {
		ret = p_hal->get_idpin_state(p_hal);
	}
  MONITOR_PRINTK("idpin is %d\n", ret);
  

	if (ret != USB_ID_STATE_INVALID) {
		if (ret == USB_ID_STATE_HOST) {
host_detect:		  
		  MONITOR_PRINTK("host detecting!!!!\n");
			if ((pStatus->message_status & (0x1 << MONITOR_B_IN)) != 0) {
				MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_B_OUT\n", __FILE__, __LINE__);
				printk("\n%s--%d, SYS_MSG_USB_B_OUT\n", __FILE__, __LINE__);
				pStatus->core_ops->putt_msg(MON_MSG_USB_B_OUT);
				pStatus->message_status =pStatus->message_status & (~(0x1 << MONITOR_B_IN));
			}
			if ((pStatus->message_status & (0x1 << MONITOR_CHARGER_IN)) != 0) {
				MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_CHARGER_OUT\n", __FILE__, __LINE__);
				pStatus->core_ops->putt_msg(MON_MSG_USB_CHARGER_OUT);
				pStatus->message_status =pStatus->message_status & (~(0x1 << MONITOR_CHARGER_IN));
			}
      
			p_hal->set_dp_500k_15k(p_hal, 0, 1);	/* disable 500k, enable 15k. */

			if (pStatus->vbus_enable_power == 0) {
				p_hal->vbus_power_onoff(p_hal, 1);
				pStatus->vbus_enable_power = 1;
				p_hal->set_soft_id(p_hal, 1, 0);
			}
			pStatus->det_phase = 1;
		} else {
		  MONITOR_PRINTK("device detect prepare!!!!\n");
			if ((pStatus->message_status & (0x1 << MONITOR_A_IN)) != 0) {
				MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_A_OUT\n", __FILE__, __LINE__);
				printk("\n%s--%d, SYS_MSG_USB_A_OUT\n", __FILE__, __LINE__);
				pStatus->core_ops->putt_msg(MON_MSG_USB_A_OUT);
				pStatus->message_status = pStatus->message_status & (~(0x1 << MONITOR_A_IN));
			}			
			if (pStatus->vbus_enable_power) {
			    p_hal->vbus_power_onoff(p_hal, 0);
			    pStatus->vbus_enable_power = 0;
			}
			p_hal->set_dp_500k_15k(p_hal, 0, 0);	/* disable 500k, disable 15k. */
			p_hal->set_soft_id(p_hal, 1, 1);

			pStatus->det_phase = 0;
		}
		pStatus->device_confirm = 0;
		pStatus->host_confirm = 0;
		pStatus->timer_steps = 1;
		goto out;
	}

	/* the last time check host state before change to device detect phase. */
	if ((pStatus->vbus_enable_power != 0) && (pStatus->det_phase != 0)) {
		pStatus->dp_dm_status = p_hal->get_linestates(p_hal);	// get dp dm status.
		if ((pStatus->dp_dm_status == 0x1) || (pStatus->dp_dm_status == 0x2)) {
			pStatus->timer_steps = USB_HOST_DETECT_STEPS;
			pStatus->host_confirm = 0;
			goto out;
		}
	}

	p_hal->vbus_power_onoff(p_hal, 0);
	pStatus->vbus_enable_power = 0;
	p_hal->set_dp_500k_15k(p_hal, 0, 0);	/* disable 500k, disable 15k. */
	p_hal->set_soft_id(p_hal, 1, 1);
	
	pStatus->check_cnt++;

	/* if it's the first time to check, must in checking device phase. */
	if ((pStatus->check_cnt == 1) ||
	    (pStatus->port_config->detect_type == UMONITOR_DEVICE_ONLY)) {
		pStatus->det_phase = 0;
	} else {
		/* reverse detect phase. */
		pStatus->det_phase = !pStatus->det_phase;

		/* if it's B_IN status, it needn't to check host in, because there is just one usb port. 
		   ͬʱ\A3\AC\C8\E7\B9\FBֻ\C1\AC\BD\D3usb\B3\E4\B5\E7\C6\F7\A3\AC\B4\CBʱ\D4\D9ʹ\D3\C3GPIO\B6\D4\CD⹩\B5\E7\C0\B4\BC\EC\B2\E9host\CAǷ\F1\B2\E5\C8룬\D4\F2\BBᵼ\D6»\FA\C6\F7\B5\F4\B5硣
		   һ\D1\F9Ҳ\CA\C7\D0\E8Ҫ\B4\CBʱ\BD\FBֹ\BC\EC\B2\E2host */
		status = pStatus->message_status & ((0x1 << MONITOR_B_IN) | (0x1 << MONITOR_CHARGER_IN));
		if ((pStatus->det_phase == 1) && (status != 0)) {
			pStatus->det_phase = 0;
			goto out1;
		}
		pStatus->check_cnt = 0;
		goto host_detect;
		
	}
out1:
	pStatus->device_confirm = 0;
	pStatus->host_confirm = 0;
	pStatus->timer_steps = 1;

out:
	return 0;
}

/******************************************************************************/
/*!
* \brief  check whether usb plug in/out
*
* \par    Description
*         this function is a timer func, interval is 500ms.
*
* \param[in]  null
* \return     null
* \ingroup   usbmonitor
*
* \par
******************************************************************************/
void umonitor_timer_func(void)
{
	int ret = 0;
	unsigned int status = 0;
	umonitor_dev_status_t *pStatus;
	usb_hal_monitor_t * p_hal;
	u32 reg;
    
	pStatus = umonitor_status;
	p_hal = &pStatus->umonitor_hal;
	
	MONITOR_PRINTK("entring umonitor_timer_func\n");

	if ((pStatus->port_config->detect_type == UMONITOR_DISABLE)
		|| (pStatus->detect_valid == 0)) {
		goto out;
	}
	pStatus->detect_running = 1;

	/* err check! */
	if ((pStatus->timer_steps > USB_DEVICE_DETECT_STEPS)
	    && (pStatus->timer_steps > USB_HOST_DETECT_STEPS)) {
		MONITOR_ERR("timer_steps err:%d \n", pStatus->timer_steps);
		pStatus->timer_steps = 0;
		goto out;
	}
	//usb_monitor_debug_status_inf(usb_ctrl_no);

	if (pStatus->timer_steps == 0) {	/* power on/off phase. */
		usb_timer_process_step0(pStatus);
		goto out;
	}

	if (pStatus->det_phase == 0) {	/* power off, device detect phase. */
		ret = usb_timer_det_pc_charger(pStatus);
		switch (ret) {
		case USB_DET_NONE:
			if ((pStatus->message_status & (0x1 << MONITOR_B_IN)) != 0) {
				//MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_B_OUT\n", __FILE__, __LINE__);
				printk("\n%s--%d, SYS_MSG_USB_B_OUT\n", __FILE__, __LINE__);
				pStatus->core_ops->putt_msg(MON_MSG_USB_B_OUT);
				pStatus->message_status =pStatus->message_status & (~(0x1 << MONITOR_B_IN));
			}
			if ((pStatus->message_status & (0x1 << MONITOR_CHARGER_IN)) != 0) {
				printk("\n%s--%d, SYS_MSG_USB_CHARGER_OUT\n", __FILE__, __LINE__);
				//MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_CHARGER_OUT\n", __FILE__, __LINE__);
				pStatus->core_ops->putt_msg(MON_MSG_USB_CHARGER_OUT);
				pStatus->message_status = pStatus->message_status & (~(0x1 << MONITOR_CHARGER_IN));
			}
			break;

		case USB_DET_DEVICE_DEBUOUNCING:	/* debounce. */
			break;

		case USB_DET_DEVICE_PC:
			if(p_hal->get_idpin_state(p_hal) != USB_ID_STATE_DEVICE){
				pStatus->device_confirm = 0;
				pStatus->timer_steps =0;
                		goto out;
			}
			status = pStatus->message_status & (0x1 << MONITOR_B_IN);
			if (status != 0) {
				goto out;
			}
			p_hal->set_mode(p_hal, USB_IN_DEVICE_MOD);
			//need to reset dp/dm before dwc3 loading
			reg = readl(p_hal->usbecs);
			reg &=  ~((0x1 << USB3_P0_CTL_DPPUEN_P0)|(0x1 << USB3_P0_CTL_DMPUEN_P0)); 
			writel(reg, p_hal->usbecs );
			//MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_B_IN\n", __FILE__, __LINE__);
			printk("\n%s--%d, SYS_MSG_USB_B_IN\n", __FILE__, __LINE__);
			pStatus->core_ops->putt_msg(MON_MSG_USB_B_IN);
			pStatus->message_status |= 0x1 << MONITOR_B_IN;
			pStatus->detect_valid = 0;	//disable detection
			goto out;	/* todo stop timer. */

		case USB_DET_DEVICE_CHARGER:
			/* if B_IN message not clear, clear it. B_OUT when adaptor is in. */
			status = pStatus->message_status & (0x1 << MONITOR_B_IN);
			if (status != 0) {
			  printk("\n%s--%d, SYS_MSG_USB_B_OUT\n", __FILE__, __LINE__);
				//MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_B_OUT\n", __FILE__, __LINE__);
				pStatus->core_ops->putt_msg(MON_MSG_USB_B_OUT);
				pStatus->message_status =pStatus->message_status & (~(0x1 << MONITOR_B_IN));
			}
			/* if adaptor in is send, it needn't sent again. */
			status = pStatus->message_status & (0x1 << MONITOR_CHARGER_IN);
			if (status != 0) {
				goto out;
			}
			p_hal->set_mode(p_hal, USB_IN_DEVICE_MOD);
			MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_CHARGER_IN\n", __FILE__, __LINE__);
			pStatus->core_ops->putt_msg(MON_MSG_USB_CHARGER_IN);
			pStatus->message_status |= 0x1 << MONITOR_CHARGER_IN;
			pStatus->detect_valid = 0;	//disable detection
			goto out;	/* todo stop timer. */

		default:
			USB_ERR_PLACE;
			break;
		}
		goto out;
	} else {		/* power on, host detect phase. */

		ret = usb_timer_det_udisk(pStatus);
		status = pStatus->message_status & (0x1 << MONITOR_A_IN);
		if ((status != 0) && (ret == USB_DET_HOST_NONE)) {
			MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_A_OUT\n", __FILE__, __LINE__);
			pStatus->core_ops->putt_msg(MON_MSG_USB_A_OUT);
			pStatus->message_status = pStatus->message_status & (~(0x1 << MONITOR_A_IN));
			goto out;
		}
		if (ret == USB_DET_HOST_UDISK) {
			p_hal->set_mode(p_hal, USB_IN_HOST_MOD);
			MONITOR_PRINTK("\n%s--%d, SYS_MSG_USB_A_IN\n", __FILE__, __LINE__);
			pStatus->core_ops->putt_msg(MON_MSG_USB_A_IN);
			pStatus->message_status |= 0x1 << MONITOR_A_IN;

			/*\C8\E7\B9\FB\CA\C7A\CF߲\E5\C8룬\D4\F2\B9رն\A8ʱ\C6\F7\A3\BBB\CF߲\E5\C8룬\B6\A8ʱ\C6\F7\B2\BB\D3ùر\D5 */
			pStatus->detect_valid = 0;	//disable detection
			goto out;	/* todo stop timer. */
		}
		goto out;
	}
	
out:
	pStatus->detect_running = 0;
	return;
}

/******************************************************************************/
/*!
* \brief  set monitor detect flag
*
* \par    Description
*         set monitor detect flag
*
* \param[in]  status
*             1---   set detection flag to detect
*             0---   reverse
* \return     0------\C9\E8\D6óɹ\A6
* \ingroup   usbmonitor
*
* \par
******************************************************************************/
static int set_monitor_detect_flag(umonitor_dev_status_t *pStatus, unsigned int status)
{
	int i;
	unsigned int ms_status = 0;	/* record is a in ? */
	usb_hal_monitor_t *p_hal = &pStatus->umonitor_hal;

	pStatus->check_cnt = 0;
	pStatus->det_phase = 0;
	pStatus->timer_steps = 0;

	if (status != 0) {	/*enable detect flag */
		p_hal->vbus_power_onoff(p_hal, 0);
		pStatus->vbus_enable_power = 0;

		if (pStatus->detect_valid == 0) {
			MONITOR_PRINTK("%s,%d\n", __FUNCTION__, __LINE__);
			pStatus->detect_valid = 1;
			goto out;
		} else {
			MONITOR_PRINTK("usb detection flag is already setted, %s,%d\n", __FUNCTION__, __LINE__);
		}
	} 
	else {		/*disable detection flag */
		i = 0;
		do {
			if (pStatus->detect_running == 0) {
				pStatus->detect_valid = 0;
				break;
			}
			msleep(1);
			++i;
		} while (i < 1000);
		MONITOR_PRINTK("enable detection flag\n");
		
		if (ms_status == 0) {
			/* make sure power is off. */
			p_hal->vbus_power_onoff(p_hal, 0);
			pStatus->vbus_enable_power = 0;
			p_hal->set_soft_id(p_hal, 1, 1);
		}
	}

out:
	if (pStatus->core_ops->wakeup_func != NULL) {
		pStatus->core_ops->wakeup_func();
	}
	return 0;
}

/*! \cond USBMONITOR_INTERNAL_API*/
/******************************************************************************/
/*!
* \brief  enable or disable usb plug_in/out check
*
* \par    Description
*         enable or disable the func of checking usb plug_in/out
*
*
* \param[in]  status
*             1---   enable check func;
*             0---   disable check func;
* \return     0------ʹ\C4\DC/\BD\FBֹ\B3ɹ\A6
                \B8\BAֵ---\C7\FD\B6\AF\B5\B1ǰæ\A3\AC\C7\EB\C9Ժ\F2\D6\D8\D0½\F8\D0д˲\D9\D7\F7
* \ingroup   usbmonitor
*
* \par
******************************************************************************/
int umonitor_detection(unsigned int status)
{
	umonitor_dev_status_t *pStatus;
	usb_hal_monitor_t * p_hal;


	pStatus = umonitor_status;
	p_hal = &pStatus->umonitor_hal;
	MONITOR_PRINTK("umonitor_detection:%d\n", status);

	if (status != 0) {
		p_hal->dwc3_otg_mode_cfg(p_hal);
		p_hal->aotg_enable(p_hal, 1);
		p_hal->set_mode(p_hal, USB_IN_DEVICE_MOD);
		set_monitor_detect_flag(pStatus, 1);
	} else {
		//\D5ⲿ\B7\D6\CF\D6\D4\DA\D2\D1\CE\DEЧ,\D3\C9\D4\DA\D2Ѿ\AD\BC\EC\B2⵽B\BB\F2\D5\DFA\B2\E5\C8\EBʱ,\B7\A2\CB\CD\C1\CB\CF\FBϢ\BA\F3,\BD\AB\B6\A8ʱ\C6\F7port_timerֹͣ;
		//checktimer\C8Ծ\C9\D4\CB\D0м\E0\B2\E2A(id\CAǷ\F1\B1仯)\BB\F2\D5\DFB(vbus\CAǷ\F1\B8ı\E4)\B2\E5\C8\EB\B5\C4״̬\CAǷ\F1\B7\A2\C9\FA\B8ı\E4.
		//\B4˴\A6Ϊ\B1\A3\C1\F4ԭʼ\B4\FA\C2\EB,/*disable detection,\BC\D3\D4\D8UDC\C7\FD\B6\AFʱ\A3\AC\CFȹرն\A8ʱ\C6\F7\BC\EC\B2\E2 */
		p_hal->aotg_enable(p_hal, 0);
		set_monitor_detect_flag(pStatus, 0);
	}
	return 0;
}

/*! \cond NOT_INCLUDE*/
/******************************************************************************/
/*!
* \brief  parse the runtime args of monitor driver.
* \par    Description
*         \B3\F5ʼ\BB\AF\A3\AC\BF\AAʼ׼\B1\B8\BD\F8\D0м\EC\B2⡣
*
* \retval      0---args parse successed
* \ingroup     UsbMonitor
******************************************************************************/
int umonitor_core_init(umonitor_api_ops_t * core_ops,
		   umon_port_config_t * port_config , unsigned int base)
{
	umonitor_dev_status_t *pStatus;

	pStatus = kmalloc(sizeof (umonitor_dev_status_t), GFP_KERNEL);
	if (pStatus == NULL) {
		return -1;
	}
	umonitor_status = pStatus;

	usb_hal_init_monitor_hw_ops(&pStatus->umonitor_hal, port_config, base);
	usb_init_monitor_status(pStatus);
	pStatus->core_ops = core_ops;
	pStatus->port_config = port_config;

	return 0;
}

int umonitor_core_exit(void)
{
	umonitor_dev_status_t *pStatus;
	usb_hal_monitor_t *p_hal;

	pStatus = umonitor_status;
	p_hal = &pStatus->umonitor_hal;

	p_hal->enable_irq(p_hal, 0);
	if (pStatus != NULL)
		kfree(pStatus);
		
	umonitor_status = NULL;
	return 0;
}

unsigned int umonitor_get_run_status(void)
{
	umonitor_dev_status_t *pStatus;

	pStatus = umonitor_status;
	
	return (unsigned int)pStatus->detect_valid;
}

unsigned int umonitor_get_message_status(void)
{
	umonitor_dev_status_t *pStatus;

	pStatus = umonitor_status;
	
	return (unsigned int)pStatus->message_status;
}

void umonitor_printf_debuginfo(void)
{
	umonitor_dev_status_t *pStatus;
	usb_hal_monitor_t *p_hal;

	pStatus = umonitor_status;
	p_hal = &pStatus->umonitor_hal;

	usb_monitor_debug_status_inf();
	printk("in printf_debuginfo\n");
	p_hal->debug(p_hal);

	return;
}

int umonitor_vbus_power_onoff(int value)
{
	umonitor_dev_status_t *pStatus;
	usb_hal_monitor_t *p_hal;

	pStatus = umonitor_status;
	p_hal = &pStatus->umonitor_hal;
	
	return p_hal->vbus_power_onoff(p_hal, value);
}

int umonitor_core_suspend(void)
{
	umonitor_dev_status_t *pStatus;
	usb_hal_monitor_t *p_hal;

	pStatus = umonitor_status;
	p_hal = &pStatus->umonitor_hal;

  pStatus->detect_valid = 0;
  
  printk("SUSPEND pStatus->message_status is %d!!!!!!!!!!!!!!\n", pStatus->message_status);
  
	if(pStatus->vbus_enable_power && p_hal->vbus_power_onoff)
		p_hal->vbus_power_onoff(p_hal,  0);
	
  p_hal->suspend_or_resume(p_hal, 1);
	
	return 0;
}

int umonitor_core_resume(void)
{
	umonitor_dev_status_t *pStatus;
	usb_hal_monitor_t *p_hal;
	pStatus = umonitor_status;
	p_hal = &pStatus->umonitor_hal;	
	
	printk(KERN_DEBUG"RESUME pStatus->message_status is %d!!!!!!!!!!!!!!\n", pStatus->message_status);

	if((pStatus->message_status &(0x1 << MONITOR_B_IN)) != 0){
		printk(KERN_DEBUG"RESUME SNED B_OUT\n");
		pStatus->core_ops->putt_msg(MON_MSG_USB_B_OUT);
			pStatus->message_status &= ~(0x1 << MONITOR_B_IN);
	}
	if((pStatus->message_status &(0x1 << MONITOR_A_IN)) != 0){
		printk(KERN_DEBUG"RESUME SNED A_OUT\n");
		//p_hal->vbus_power_onoff(p_hal,  1);
		//pStatus->vbus_enable_power = 1;
		pStatus->core_ops->putt_msg(MON_MSG_USB_A_OUT);
		pStatus->message_status &= ~(0x1 << MONITOR_A_IN);
	}
	p_hal->suspend_or_resume(p_hal, 0);
	umonitor_detection(1);
	return 0;
}

/*\B1\A3֤\B9ػ\FA\C1\F7\B3\CC\D6вŻ\E1\B5\F7\D3\C3,\C6\E4\CB\FC\B5ط\BD\B2\BB\BB\E1\B5\F7\D3\C3*/
int umonitor_dwc_otg_init(void)
{
	umonitor_dev_status_t *pStatus;
	usb_hal_monitor_t *p_hal;

	pStatus = umonitor_status;
	p_hal = &pStatus->umonitor_hal;

  p_hal->dwc3_otg_init(p_hal);
	
	return 0;
}
