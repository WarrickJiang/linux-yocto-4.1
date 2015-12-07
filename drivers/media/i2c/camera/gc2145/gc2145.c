/*
 * gc2145 Camera Driver
 *
 * Copyright (C) 2011 Actions Semiconductor Co.,LTD
 * Wang Xin <wangxin@actions-semi.com>
 *
 * Based on ov227x driver
 *
 * Copyright (C) 2008 Renesas Solutions Corp.
 * Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <media/v4l2-chip-ident.h>
#include <linux/platform_device.h>
#include <mach/isp-owl.h>
#include "module_diff.h"
#include "../module_comm/module_comm.c"
#ifdef  SELf_DETECT
#include "../module_comm/module_detect.c"
#endif

static int camera_i2c_read(struct i2c_adapter *i2c_adap, unsigned int reg, unsigned int *dest)
{
	unsigned char regs_array[4] = {0, 0, 0, 0};
    unsigned char data_array[4] = {0, 0, 0, 0};
	struct i2c_msg msg;
	int ret = 0;
	regs_array[0] = reg & 0xff;
	
	msg.addr = MODULE_I2C_REAL_ADDRESS;
	msg.flags = 0;
	msg.len   = I2C_REGS_WIDTH;
	msg.buf   = regs_array;
	ret = i2c_transfer(i2c_adap, &msg, 1);
	if (ret < 0) {
		printk("write register %s error %d",CAMERA_MODULE_NAME, ret);
		return ret;
	}


	
	msg.flags = I2C_M_RD;
	msg.len   = I2C_DATA_WIDTH;
	msg.buf   = data_array;	
	ret = i2c_transfer(i2c_adap, &msg, 1);
	if (ret >= 0) {
        ret = 0;
		*dest = data_array[0];
	}
	else {
	    printk("read register %s error %d",CAMERA_MODULE_NAME, ret);
	}
	
	return ret;
}

static int camera_i2c_write(struct i2c_adapter *i2c_adap, unsigned int reg, unsigned int src)
{
	unsigned char regs_array[4] = {0, 0, 0, 0};
    unsigned char data_array[4] = {0, 0, 0, 0};
    unsigned char tran_array[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	struct i2c_msg msg;
	unsigned int data = src;
	int ret,i;
	
	
	regs_array[0] = reg & 0xff;
	data_array[0] = data & 0xff;
	for (i = 0; i < I2C_REGS_WIDTH; i++) {
        tran_array[i] = regs_array[i];
    }

    for (i = I2C_REGS_WIDTH; i < (I2C_REGS_WIDTH + I2C_DATA_WIDTH); i++) {
        tran_array[i] = data_array[i - I2C_REGS_WIDTH];
    }
	
	msg.addr = MODULE_I2C_REAL_ADDRESS;
	msg.flags = 0;
	msg.len   = I2C_REGS_WIDTH + I2C_DATA_WIDTH;
	msg.buf   = tran_array;    
	ret = i2c_transfer(i2c_adap, &msg, 1);
	if (ret > 0) {
		ret = 0;
	}
	else if (ret < 0) {
	    printk("write register %s error %d",CAMERA_MODULE_NAME, ret);
	}
	
	return ret;	
}

static int camera_write_array(struct i2c_adapter *i2c_adap, const struct regval_list *vals)
{
	while (vals->reg_num != 0xff) {
		int ret = camera_i2c_write(i2c_adap,
							vals->reg_num,
							vals->value);
		if (ret < 0){
			printk("[camera] i2c write error!,i2c address is %x\n",MODULE_I2C_REAL_ADDRESS);
			return ret;
		}
		vals++;
	}
	return 0;
}
static int module_soft_reset(struct i2c_client *client)
{
	int ret;
	unsigned int reg_0xfe;
	struct i2c_adapter *i2c_adap = client->adapter;
    GC_INFO("");
    ret = camera_i2c_read(i2c_adap, 0xfe, &reg_0xfe);
    reg_0xfe |= (0x1 << 7);
    ret |= camera_i2c_write(i2c_adap, 0xfe, reg_0xfe);
    mdelay(1);
    reg_0xfe &= (~(0x1 << 7));
    ret |= camera_i2c_write(i2c_adap, 0xfe, reg_0xfe);
    return ret;
}
#if 0
/*
 *softstandby:ͨ\B9\FD\C5\E4\D6üĴ\E6\C6\F7ʹģ\D7\E9\BD\F8\C8\EB\B5͹\A6\BA\C4ģʽ\A3\AC\B4\CB״̬\CF\C2\CA\E4\B3\F6pin\B8\DF\D7衣\BCĴ\E6\C6\F7ֵ\B1\A3\C1\F4\A3\AC\C7ҼĴ\E6\C6\F7\BF\C9д
 *ͨ\B9\FD\C0\AD\B8\DFpwdn pin\C0\B4\BD\F8\B5͹\A6\BA\C4ģʽʱ\A3\AC\CA\E4\B3\F6pin\B8\DF\D7裬\BCĴ\E6\C6\F7ֵ\B1\A3\C1\F4\B2\BB\B1䣬\B5\AB\BCĴ\E6\C6\F7\B2\BB\BF\C9д\A1\A3
 */
static int module_soft_standby(struct i2c_client *client)
{
    unsigned int reg_0xfe = 0x00;
    unsigned int reg_0xf2 = 0x08;
    int ret = 0;	
		/*
    ret = camera_i2c_write(client, 0xfe, &reg_0xfe);
    ret |= camera_i2c_write(client, 0xf2, &reg_0xf2);
		*/
    return ret;
}

/*
 *\B4\D3softstandby״̬\BBָ\B4normal״̬
 */
static int module_normal(struct v4l2_subdev *sd)
{
	int ret = 0;
	/*
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_adapter *i2c_adap = client->adapter;
	
	unsigned int reg_0xfe = 0x00;
	unsigned int reg_0xf2 = 0x70;
	
	
	ret = camera_i2c_write(client, 0xfe, &reg_0xfe);
	ret |= camera_i2c_write(client, 0xf2, &reg_0xf2);
	*/
	return ret;
}
#endif
static int module_start_aec(struct v4l2_subdev *sd)
{
  int ret = 0;
  
  struct i2c_client *client = v4l2_get_subdevdata(sd);
//  struct camera_module_priv *priv = to_camera_priv(client);
  struct i2c_adapter *i2c_adap = client->adapter;
  
  unsigned int reg_0xfe = 0x00;
  unsigned int reg_0xb6 = 0x01;

  ret = camera_i2c_write(i2c_adap, 0xfe, reg_0xfe); //page 0
  ret |= camera_i2c_write(i2c_adap, 0xb6, reg_0xb6); 

  return ret;
}

static int module_freeze_aec(struct v4l2_subdev *sd)
{
 int ret = 0;
 
  struct i2c_client *client = v4l2_get_subdevdata(sd);
//  struct camera_module_priv *priv = to_camera_priv(client);
  struct i2c_adapter *i2c_adap = client->adapter;
 
  unsigned int reg_0xfe = 0x00;
  unsigned int reg_0xb6 = 0x00;

  ret = camera_i2c_write(i2c_adap, 0xfe, reg_0xfe); //page 0
  ret |= camera_i2c_write(i2c_adap, 0xb6, reg_0xb6); 
 
  return ret;
}

static int module_save_exposure_param(struct v4l2_subdev *sd)
{
	int ret = 0;
	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_module_priv *priv = to_camera_priv(client);
	struct i2c_adapter *i2c_adap = client->adapter;
	
	unsigned int reg_0xfe = 0x00;
	unsigned int reg_0x03;
	unsigned int reg_0x04;

	ret = camera_i2c_write(i2c_adap, 0xfe, reg_0xfe); //page 0
	ret |= camera_i2c_read(i2c_adap, 0x03, &reg_0x03);
	ret |= camera_i2c_read(i2c_adap, 0x04, &reg_0x04);
		
	priv->preview_exposure_param.shutter = (reg_0x03 << 8) | reg_0x04;
	priv->capture_exposure_param.shutter = (priv->preview_exposure_param.shutter)/2;
	
	//printk("GC2155 module_save_exposure_param, win->name:%s\n", priv->win->name);
	return ret;
}

static int module_set_exposure_param(struct v4l2_subdev *sd)
{
 	int ret = 0;
  
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_module_priv *priv = to_camera_priv(client);
	struct i2c_adapter *i2c_adap = client->adapter;
	
	unsigned int reg_0xfe = 0x00;
	unsigned char reg_0x03;
	unsigned char reg_0x04;

	if(priv->capture_exposure_param.shutter < 1) {
		priv->capture_exposure_param.shutter = 1;
	}

	reg_0x03 = ((priv->capture_exposure_param.shutter)>>8) & 0x1F ;
	reg_0x04 = (priv->capture_exposure_param.shutter) & 0xFF;

	ret = camera_i2c_write(i2c_adap, 0xfe, reg_0xfe); //page 0
	ret |= camera_i2c_write(i2c_adap, 0x03, reg_0x03);
	ret |= camera_i2c_write(i2c_adap, 0x04, reg_0x04);
	
	return ret;
}

static int module_set_auto_white_balance(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_module_priv *priv = to_camera_priv(client);
	struct i2c_adapter *i2c_adap = client->adapter;
	int auto_white_balance;
	int ret = 0;
  unsigned int reg_0x82;
  unsigned int reg_0xfe = 0x00;

	if(ctrl)
		auto_white_balance = ctrl->val;
	else
		auto_white_balance = 1;
  
	if (auto_white_balance < 0 || auto_white_balance > 1) {
		printk("[gc2155] set auto_white_balance over range, auto_white_balance = %d\n", auto_white_balance);
		return -ERANGE;
	}
	
	ret = camera_i2c_write(i2c_adap, 0xfe, reg_0xfe);
	ret |= camera_i2c_read(i2c_adap, 0x82, &reg_0x82); 

	switch(auto_white_balance)
	{
	case 0:
		ret = 0;
		goto change_val;
		
	case 1:	
		ret |=camera_write_array(i2c_adap, module_whitebance_auto_regs);
		break;
	}
	
	reg_0x82 |= 0x02;
	ret |= camera_i2c_write(i2c_adap, 0x82, reg_0x82); 
 
change_val:
	priv->auto_white_balance = auto_white_balance;
	if(ctrl)
		ctrl->cur.val = auto_white_balance;  
		

	return ret;
}

static int module_set_white_balance_temperature(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_module_priv *priv = to_camera_priv(client);
  int white_balance_temperature = ctrl->val;
  struct i2c_adapter *i2c_adap = client->adapter;
  unsigned int reg_0x82;
  unsigned int reg_0xfe = 0x00;
	int ret = 0;
	
	ret = camera_i2c_write(i2c_adap, 0xfe, reg_0xfe);
	ret |= camera_i2c_read(i2c_adap, 0x82, &reg_0x82); 
	 
	reg_0x82 &= (~0x02);  //\CA\D7\CF\C8disable awb\A3\AC\B7\F1\D4\F2\BCĴ\E6\C6\F7\B2\BB\BF\C9д\A1\A3
	ret |= camera_i2c_write(i2c_adap, 0x82, reg_0x82);  

	switch(white_balance_temperature)
	{
	case V4L2_WHITE_BALANCE_INCANDESCENT: /* \B0׳\E3\B9\E2 */
		ret = camera_write_array(i2c_adap, module_whitebance_incandescent_regs);
		break;
	
	case V4L2_WHITE_BALANCE_FLUORESCENT: /* ӫ\B9\E2\B5\C6 */
		ret = camera_write_array(i2c_adap, module_whitebance_fluorescent_regs);
		break;
	
	case V4L2_WHITE_BALANCE_DAYLIGHT: /* \C8չ\E2 (\C7\E7\CC\EC)*/
		ret = camera_write_array(i2c_adap, module_whitebance_sunny_regs);
		break;
	
	case V4L2_WHITE_BALANCE_CLOUDY: /* \B6\E0\D4\C6 \A3\A8\D2\F5\CC죩*/
		ret = camera_write_array(i2c_adap, module_whitebance_cloudy_regs);
		break;
	
	default:
		printk("[gc2145] set white_balance_temperature over range, white_balance_temperature = %d\n", white_balance_temperature);
		return -ERANGE;	
	}
	
	priv->auto_white_balance = 0;
	priv->white_balance_temperature = white_balance_temperature;
	ctrl->cur.val = white_balance_temperature;
	
	return ret;
}
#if 0
static int module_set_colorfx(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_module_priv *priv = to_camera_priv(client);
	struct i2c_adapter *i2c_adap = client->adapter;
  int colorfx = ctrl->val;
	int ret = 0;


	switch (colorfx) {
	case V4L2_COLORFX_NONE: /* normal */
		ret = camera_write_array(i2c_adap, module_effect_normal_regs);
		break;
	
	case V4L2_COLORFX_BW: /* black and white */
		ret = camera_write_array(i2c_adap, module_effect_white_black_regs);
		break;
	
	case V4L2_COLORFX_SEPIA: /* antique ,\B8\B4\B9\C5*/
		ret = camera_write_array(i2c_adap, module_effect_antique_regs);
		break;

	case V4L2_COLORFX_NEGATIVE: /* negative\A3\AC\B8\BAƬ */
		ret = camera_write_array(i2c_adap, module_effect_negative_regs);
		break;

    default:
        printk("[gc2145] set colorfx over range, colorfx = %d\n", colorfx);
        return -ERANGE;	
    }

    priv->colorfx = colorfx;
    ctrl->cur.val = colorfx;

    return 0;
}
#endif

static int module_set_exposure_auto(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_module_priv *priv = to_camera_priv(client);
	struct i2c_adapter *i2c_adap = client->adapter;
    int exposure_auto;
	int ret = 0;

	if(ctrl)
		exposure_auto = ctrl->val;
	else
		exposure_auto = V4L2_EXPOSURE_AUTO;

	if (exposure_auto < 0 || exposure_auto > 1) {
		return -ERANGE;
	}
  
	switch (exposure_auto) {
	case V4L2_EXPOSURE_AUTO:/*  auto */
        ret = camera_write_array(i2c_adap, module_scene_auto_regs);
		break;

    case V4L2_EXPOSURE_MANUAL: // non auto
        ret = 0;
        break;
    }

	priv->exposure_auto = exposure_auto;
	if(ctrl)
		ctrl->cur.val = exposure_auto;

    return 0;
}

static int module_set_scene_exposure(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	int ret = 0;
#if 0
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_module_priv *priv = to_camera_priv(client);
	struct i2c_adapter *i2c_adap = client->adapter;
  int scene_exposure = ctrl->val;
	unsigned int reg_0x3a00;
	
	switch(scene_exposure) {
	case V4L2_SCENE_MODE_HOUSE:  //\CA\D2\C4\DA
		ret = camera_write_array(i2c_adap, module_scene_night_regs);
		break;
	
	case V4L2_SCENE_MODE_SUNLIGHT:  //\CA\D2\CD\E2
		ret = camera_write_array(i2c_adap, module_scene_auto_regs);
		break;

    default:
        printk("[gc2145] set scene_exposure over range, scene_exposure = %d\n", scene_exposure);
        return -ERANGE;
    }

    priv->scene_exposure = scene_exposure;
    ctrl->cur.val = scene_exposure;

	
#endif
	return ret;
}


#if 0
static int module_set_prev_capt_mode(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct camera_module_priv *priv = to_camera_priv(client);
    int mode = ctrl->val;

    switch(mode) {
    case PREVIEW_MODE:
        priv->prev_capt_mode = mode;
        break;

    case CAPTURE_MODE:
        priv->prev_capt_mode = mode;
        break;

    default:
        printk("[gc2145] set_prev_capt_mode over range, prev_capt_mode = %d\n", mode);
        return -ERANGE;
    }
    ctrl->cur.val = mode;

    return 0;
}


static int module_pause_af(struct i2c_client *client)
{
    int ret = 0;

    return ret;	
}

static int module_set_af_mode(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    //	struct i2c_client *client = v4l2_get_subdevdata(sd);
    //	struct camera_module_priv *priv = to_camera_priv(client);

    return 0;
}

static int module_get_af_status(struct camera_module_priv *priv, struct v4l2_ctrl *ctrl)
{
    //	struct v4l2_subdev *sd = &priv->subdev;
    //	struct i2c_client *client = v4l2_get_subdevdata(sd);

    return 0;
}

static int module_set_exposure(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    //	struct i2c_client *client = v4l2_get_subdevdata(sd);
    //	struct camera_module_priv *priv = to_camera_priv(client);
    int ret = 0;

    return ret;
}
#endif
static int module_set_stream(struct i2c_client *client,int enable)
{
	struct camera_module_priv *priv = to_camera_priv(client);
	struct i2c_adapter *i2c_adap = client->adapter;
	int ret = 0;
	unsigned char reg_0xfe = 0;
	unsigned char reg_0xf2= 0x0f;

	GC_INFO("");
	   if (!enable) {
		   GC_INFO("stream down");
		   reg_0xf2 = 0x00;
		   camera_i2c_write(i2c_adap, 0xfe, reg_0xfe);
		   camera_i2c_write(i2c_adap, 0xf2, reg_0xf2);
		   return ret;
	   }
	
	   if (NULL == priv->win || NULL == priv->cfmt) {
		   GC_ERR("cfmt or win select error");
		   return (-EPERM);
	   }	
	   GC_INFO("stream on");
	   reg_0xf2 = 0x0f;
	   camera_i2c_write(i2c_adap, 0xfe, reg_0xfe);
	   camera_i2c_write(i2c_adap, 0xf2, reg_0xf2);
	   
	 return 0;   
}
static int module_get_exposure(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    //	struct i2c_client *client = v4l2_get_subdevdata(sd);
    int ret = 0;

    return ret;
}

static int module_get_gain(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    //	struct i2c_client *client = v4l2_get_subdevdata(sd);
    //	struct camera_module_priv *priv = to_camera_priv(client);
    //  unsigned int analog_gain = 0;
    //	unsigned int digital_gain = 0;
    unsigned int total_gain = 0;
    //	int ret = 0;

    return total_gain;
}

static int module_set_gain(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    //	struct i2c_client *client = v4l2_get_subdevdata(sd);
    //	struct camera_module_priv *priv = to_camera_priv(client);

    return 0;
}

static int module_set_ev(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    //	struct i2c_client *client = v4l2_get_subdevdata(sd);
    int ret = 0;

    return ret;
}
static int module_set_mbusformat(struct i2c_client *client, const struct module_color_format *cfmt)
{
	struct i2c_adapter *i2c_adap = client->adapter;
	enum v4l2_mbus_pixelcode code;
	unsigned char reg_0x84;
	unsigned char reg_0xfe = 0x00;  //pgae0
	int ret = 0;
	
	code = cfmt->code;
	
	switch (code) {
	case V4L2_MBUS_FMT_YUYV8_2X8:
		reg_0x84 = 0x02;
		break;
				
	case V4L2_MBUS_FMT_UYVY8_2X8:
		reg_0x84 = 0x00;
		break;
		
	case V4L2_MBUS_FMT_YVYU8_2X8:
		reg_0x84 = 0x03;
		break;
		
	case V4L2_MBUS_FMT_VYUY8_2X8:
		reg_0x84 = 0x01;
		break;
		
	default:
		printk("[gc2155] mbus code error in %s() line %d\n",__FUNCTION__, __LINE__);
		return -1;
	}
	
	ret = camera_i2c_write(i2c_adap, 0xfe, reg_0xfe);
	ret |= camera_i2c_write(i2c_adap, 0x84, reg_0x84);
	return ret;
}

static int  module_s_mirror_flip(struct v4l2_subdev *sd, unsigned int mirror, unsigned int flip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_adapter *i2c_adap = client->adapter;
	unsigned int reg_0x17 = 0x14;
    unsigned int reg_0xfe = 0x00;
    int ret = 0;

    if ((!mirror) && (!flip)) {
        return 0;
    }

    ret = camera_i2c_write(i2c_adap, 0xfe, reg_0xfe);

    if (mirror)	{
        reg_0x17 |= 0x1;
    } else {
        reg_0x17 &= (~0x1);
    }

    if (flip) {
        reg_0x17 |= (0x1<<0x1);
    } else {
        reg_0x17 &= (~(0x1<<0x1));	
    }

    ret |= camera_i2c_write(i2c_adap, 0x17, reg_0x17);
	return ret;
}

static int module_verify_pid(struct i2c_adapter *i2c_adap,struct camera_module_priv 	*priv)
{
	unsigned int pidh = 0;
    unsigned int pidl = 0;
    int ret;

	
	GC_INFO("");

    /*
	 * check and show product ID and manufacturer ID
	 */  
	ret = camera_i2c_read(i2c_adap, PIDH, &pidh); 
	ret |= camera_i2c_read(i2c_adap, PIDL, &pidl); 
	switch (VERSION(pidh, pidl)) 
    {
	case CAMERA_MODULE_PID:
/*		if(priv)
			{
		     priv->model= V4L2_IDENT_GC2145;
			}*/
		printk("[%s] Product ID verified %x\n",CAMERA_MODULE_NAME, VERSION(pidh, pidl));
		break;
	
	default:
		printk("[%s] Product ID error %x\n",CAMERA_MODULE_NAME, VERSION(pidh, pidl));
		return -ENODEV;
	}
	return ret;
}
static int  module_set_af_mode(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	return 0;
}
static int  module_get_af_status(struct camera_module_priv *priv, struct v4l2_ctrl *ctrl)
{
	return 0;
}

static int  module_set_power_line(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	
	return 0;
}
static int  module_get_power_line(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	
	ctrl->val = V4L2_CID_POWER_LINE_FREQUENCY_AUTO;
	return 0;
}

