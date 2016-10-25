/*
 * STMicroelectronics lng2dm Accelerometer driver
 * Based on lis3de input driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Armando Visconti <armando.visconti@st.com>
 *
 * Licensed under the GPL-2.
 */
#ifndef	__LIS3DE_H__
#define	__LIS3DE_H__

#define	LNG2DM_DEV_NAME		"lng2dm"

/************************************************/
/* 	Output data: ug				*/
/*	Sysfs enable: enable_device		*/
/*	Sysfs odr: pollrate_ms			*/
/*	Sysfs full scale: range			*/
/************************************************/

#define	LNG2DM_MIN_POLL_PERIOD_MS	1

#ifdef __KERNEL__

#define INPUT_EVENT_TYPE		EV_MSC
#define INPUT_EVENT_X			MSC_SERIAL
#define INPUT_EVENT_Y			MSC_PULSELED
#define INPUT_EVENT_Z			MSC_GESTURE
#define INPUT_EVENT_TIME_MSB		MSC_SCAN
#define INPUT_EVENT_TIME_LSB		MSC_MAX

//#define LIS3DE_SAD0L				(0x00)
//#define LIS3DE_SAD0H				(0x01)
#define LNG2DM_I2C_SADROOT			(0x0C)

/* I2C address if acc SA0 pin to GND */
#define LNG2DM_I2C_SAD_L		((LNG2DM_I2C_SADROOT<<1)| LIS3DE_SAD0L)

/* I2C address if acc SA0 pin to Vdd */
#define LNG2DM_I2C_SAD_H		((LNG2DM_I2C_SADROOT<<1)| LIS3DE_SAD0H)

/* to set gpios numb connected to interrupt pins,
* the unused ones have to be set to -EINVAL
#define LNG2DM_DEFAULT_INT1_GPIO		(-EINVAL)
#define LNG2DM_DEFAULT_INT2_GPIO		(-EINVAL)
*/

/* Accelerometer Sensor Full Scale */
#define	LNG2DM_FS_MASK		(0x30)
#define LNG2DM_G_2G			(0x00)
#define LNG2DM_G_4G			(0x10)
#define LNG2DM_G_8G			(0x20)
#define LNG2DM_G_16G		(0x30)

struct lng2dm_platform_data {
	unsigned int poll_interval;
	unsigned int min_interval;

	u8 fs_range;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
};

#endif	/* __KERNEL__ */

#endif	/* __LIS3DE_H__ */



