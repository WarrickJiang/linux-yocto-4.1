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

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/input/lng2dm.h>
#include <linux/acpi.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#define MS_TO_NS(msec)          ((msec) * 1000 * 1000)
/*#define	DEBUG		1*/

#define	G_MAX		16000000


/* Accelerometer Sensor Operating Mode */
#define LNG2DM_ENABLE	(0x01)
#define LNG2DM_DISABLE	(0x00)


#define	AXISDATA_REG		(0x28)
#define WHOAMI_LNG2DM	(0x33)	/*	Expctd content for WAI	*/

/*	CONTROL REGISTERS	*/
#define WHO_AM_I		(0x0F)	/*	WhoAmI register		*/
#define	TEMP_CFG_REG		(0x1F)	/*	temper sens control reg	*/
/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define	CTRL_REG1		(0x20)	/*	control reg 1		*/
#define	CTRL_REG2		(0x21)	/*	control reg 2		*/
#define	CTRL_REG3		(0x22)	/*	control reg 3		*/
#define	CTRL_REG4		(0x23)	/*	control reg 4		*/
#define	CTRL_REG5		(0x24)	/*	control reg 5		*/
#define	CTRL_REG6		(0x25)	/*	control reg 6		*/

#define	FIFO_CTRL_REG		(0x2E)	/*	FiFo control reg	*/

#define	INT_CFG1		(0x30)	/*	interrupt 1 config	*/
#define	INT_SRC1		(0x31)	/*	interrupt 1 source	*/
#define	INT_THS1		(0x32)	/*	interrupt 1 threshold	*/
#define	INT_DUR1		(0x33)	/*	interrupt 1 duration	*/


#define	TT_CFG			(0x38)	/*	tap config		*/
#define	TT_SRC			(0x39)	/*	tap source		*/
#define	TT_THS			(0x3A)	/*	tap threshold		*/
#define	TT_LIM			(0x3B)	/*	tap time limit		*/
#define	TT_TLAT			(0x3C)	/*	tap time latency	*/
#define	TT_TW			(0x3D)	/*	tap time window		*/
/*	end CONTROL REGISTRES	*/


#define ALL_ZEROES			(0x00)

#define LNG2DM_PM_OFF		(0x00)
#define LNG2DM_ENABLE_ALL_AXES	(0x07)

#define LNG2DM_SENSITIVITY_2G	15600	/** ug/digit */
#define LNG2DM_SENSITIVITY_4G	31200	/** ug/digit */
#define LNG2DM_SENSITIVITY_8G	62500	/** ug/digit */
#define LNG2DM_SENSITIVITY_16G	187500	/** ug/digit */

#define PMODE_MASK		(0x08)
#define ODR_MASK		(0XF0)

#define LNG2DM_ODR1		(0x10)  /* 1Hz output data rate */
#define LNG2DM_ODR10	(0x20)  /* 10Hz output data rate */
#define LNG2DM_ODR25	(0x30)  /* 25Hz output data rate */
#define LNG2DM_ODR50	(0x40)  /* 50Hz output data rate */
#define LNG2DM_ODR100	(0x50)  /* 100Hz output data rate */
#define LNG2DM_ODR200	(0x60)  /* 200Hz output data rate */
#define LNG2DM_ODR400	(0x70)  /* 400Hz output data rate */
#define LNG2DM_ODR1250	(0x90)  /* 1250Hz output data rate */

#define	IA			(0x40)
#define	ZH			(0x20)
#define	ZL			(0x10)
#define	YH			(0x08)
#define	YL			(0x04)
#define	XH			(0x02)
#define	XL			(0x01)
/* */
/* CTRL REG BITS*/
#define	CTRL_REG3_I1_AOI1	(0x40)
#define	CTRL_REG4_BDU_ENABLE	(0x80)
#define	CTRL_REG4_BDU_MASK	(0x80)
#define	CTRL_REG6_I2_TAPEN	(0x80)
#define	CTRL_REG6_HLACTIVE	(0x02)
/* */
#define NO_MASK			(0xFF)
#define INT1_DURATION_MASK	(0x7F)
#define	INT1_THRESHOLD_MASK	(0x7F)
#define TAP_CFG_MASK		(0x3F)
#define	TAP_THS_MASK		(0x7F)
#define	TAP_TLIM_MASK		(0x7F)
#define	TAP_TLAT_MASK		NO_MASK
#define	TAP_TW_MASK		NO_MASK


/* TAP_SOURCE_REG BIT */
#define	DTAP			(0x20)
#define	STAP			(0x10)
#define	SIGNTAP			(0x08)
#define	ZTAP			(0x04)
#define	YTAP			(0x02)
#define	XTAZ			(0x01)


#define	FUZZ			0
#define	FLAT			0
#define	I2C_RETRY_DELAY		5
#define	I2C_RETRIES		5
#define	I2C_AUTO_INCREMENT	(0x80)

/* RESUME STATE INDICES */
#define	RES_CTRL_REG1		0
#define	RES_CTRL_REG2		1
#define	RES_CTRL_REG3		2
#define	RES_CTRL_REG4		3
#define	RES_CTRL_REG5		4
#define	RES_CTRL_REG6		5

#define	RES_INT_CFG1		6
#define	RES_INT_THS1		7
#define	RES_INT_DUR1		8

#define	RES_TT_CFG		9
#define	RES_TT_THS		10
#define	RES_TT_LIM		11
#define	RES_TT_TLAT		12
#define	RES_TT_TW		13

#define	RES_TEMP_CFG_REG	14
#define	RES_REFERENCE_REG	15
#define	RES_FIFO_CTRL_REG	16

#define	RESUME_ENTRIES		17
/* end RESUME STATE INDICES */


struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lng2dm_odr_table[] = {
		{    1, LNG2DM_ODR1250 },
		{    3, LNG2DM_ODR400  },
		{    5, LNG2DM_ODR200  },
		{   10, LNG2DM_ODR100  },
		{   20, LNG2DM_ODR50   },
		{   40, LNG2DM_ODR25   },
		{  100, LNG2DM_ODR10   },
		{ 1000, LNG2DM_ODR1    },
};

struct lng2dm_status {
	struct i2c_client *client;
	struct lng2dm_platform_data *pdata;

	struct mutex lock;
	struct work_struct input_work;

	struct hrtimer hr_timer;
	ktime_t ktime;

	struct input_dev *input_dev;

	int64_t timestamp;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int on_before_suspend;
	int use_smbus;

	unsigned int sensitivity;

	u8 resume_state[RESUME_ENTRIES];

#ifdef DEBUG
	u8 reg_addr;
#endif
};

#ifndef CONFIG_OF
static struct lng2dm_platform_data default_lng2dm_pdata = {
	.fs_range = LNG2DM_G_2G,
	.poll_interval = 100,
	.min_interval = LNG2DM_MIN_POLL_PERIOD_MS,
};
#endif

static int lng2dm_i2c_read(struct lng2dm_status *stat, u8 *buf,
									int len)
{
	int ret;
	u8 reg = buf[0];
	u8 cmd = reg;

	if (len > 1)
		cmd = (I2C_AUTO_INCREMENT | reg);

	if (stat->use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_read_byte_data(stat->client, cmd);
			buf[0] = ret & 0xff;
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_read_byte_data: ret=0x%02x, len:%d ,"
				"command=0x%02x, buf[0]=0x%02x\n",
				ret, len, cmd , buf[0]);
#endif
		} else if (len > 1) {
			ret = i2c_smbus_read_i2c_block_data(stat->client,
								cmd, len, buf);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_read_i2c_block_data: ret:%d len:%d, "
				"command=0x%02x, ",
				ret, len, cmd);
			unsigned int ii;
			for (ii = 0; ii < len; ii++)
				dev_dbg(&stat->client->dev,
					"buf[%d]=0x%02x,\n", ii, buf[ii]);
#endif
		} else
			ret = -EINVAL;

		if (ret < 0) {
			dev_err(&stat->client->dev,
				"read transfer error: len:%d, command=0x%02x\n",
				len, cmd);
			return ret; /* failure */
		}
		return len; /* success */
	}

	ret = i2c_master_send(stat->client, &cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return -EIO;

	return i2c_master_recv(stat->client, buf, len);
}

static int lng2dm_i2c_write(struct lng2dm_status *stat, u8 *buf,
									int len)
{
	int ret;
	u8 reg, value;

	if (len > 1)
		buf[0] = (I2C_AUTO_INCREMENT | buf[0]);

	reg = buf[0];
	value = buf[1];

	if (stat->use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_write_byte_data(stat->client,
								reg, value);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_write_byte_data: ret=%d, len:%d, "
				"command=0x%02x, value=0x%02x\n",
				ret, len, reg , value);
#endif
			return ret;
		} else if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(stat->client,
							reg, len, buf + 1);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_write_i2c_block_data: ret=%d, "
				"len:%d, command=0x%02x, ",
				ret, len, reg);
			unsigned int ii;
			for (ii = 0; ii < (len + 1); ii++)
				dev_dbg(&stat->client->dev,
					"value[%d]=0x%02x,\n", ii, buf[ii]);
#endif
			return ret;
		}
	}

	ret = i2c_master_send(stat->client, buf, len+1);
	return (ret == len+1) ? 0 : ret;
}

static inline int64_t lng2dm_get_time_ns(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);

	return timespec_to_ns(&ts);
}

static struct workqueue_struct *lng2dm_workqueue = 0;

static int lng2dm_hw_init(struct lng2dm_status *stat)
{
	int err = -1;
	u8 buf[7];

#ifdef DEBUG
	pr_info("%s: hw init start\n", LNG2DM_DEV_NAME);
#endif

	buf[0] = WHO_AM_I;
	err = lng2dm_i2c_read(stat, buf, 1);
	if (err < 0) {
		dev_warn(&stat->client->dev,
		"Error reading WHO_AM_I: is device available/working?\n");
		goto err_firstread;
	} else
		stat->hw_working = 1;

	if (buf[0] != WHOAMI_LNG2DM) {
		dev_err(&stat->client->dev,
			"device unknown. Expected: 0x%02x,"
			" Replies: 0x%02x\n",
			WHOAMI_LNG2DM, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}


	buf[0] = CTRL_REG1;
	buf[1] = stat->resume_state[RES_CTRL_REG1];
	err = lng2dm_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TEMP_CFG_REG;
	buf[1] = stat->resume_state[RES_TEMP_CFG_REG];
	err = lng2dm_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = stat->resume_state[RES_FIFO_CTRL_REG];
	err = lng2dm_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TT_THS;
	buf[1] = stat->resume_state[RES_TT_THS];
	buf[2] = stat->resume_state[RES_TT_LIM];
	buf[3] = stat->resume_state[RES_TT_TLAT];
	buf[4] = stat->resume_state[RES_TT_TW];
	err = lng2dm_i2c_write(stat, buf, 4);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TT_CFG;
	buf[1] = stat->resume_state[RES_TT_CFG];
	err = lng2dm_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = INT_THS1;
	buf[1] = stat->resume_state[RES_INT_THS1];
	buf[2] = stat->resume_state[RES_INT_DUR1];
	err = lng2dm_i2c_write(stat, buf, 2);
	if (err < 0)
		goto err_resume_state;

	buf[0] = INT_CFG1;
	buf[1] = stat->resume_state[RES_INT_CFG1];
	err = lng2dm_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;


	buf[0] = CTRL_REG2;
	buf[1] = stat->resume_state[RES_CTRL_REG2];
	buf[2] = stat->resume_state[RES_CTRL_REG3];
	buf[3] = stat->resume_state[RES_CTRL_REG4];
	buf[4] = stat->resume_state[RES_CTRL_REG5];
	buf[5] = stat->resume_state[RES_CTRL_REG6];
	err = lng2dm_i2c_write(stat, buf, 5);
	if (err < 0)
		goto err_resume_state;

	stat->hw_initialized = 1;
#ifdef DEBUG
	pr_info("%s: hw init done\n", LNG2DM_DEV_NAME);
#endif
	return 0;

err_firstread:
	stat->hw_working = 0;
err_unknown_device:
err_resume_state:
	stat->hw_initialized = 0;
	dev_err(&stat->client->dev,
		"hw init error 0x%02x,0x%02x: %d\n", buf[0], buf[1], err);
	return err;
}

static void lng2dm_device_power_off(struct lng2dm_status *stat)
{
	int err;
	u8 buf[2] = { CTRL_REG1, LNG2DM_PM_OFF };

	err = lng2dm_i2c_write(stat, buf, 1);
	if (err < 0)
		dev_err(&stat->client->dev, "soft power off failed: %d\n", err);

	if (stat->pdata->power_off) {
		stat->pdata->power_off();
		stat->hw_initialized = 0;
	}
	if (stat->hw_initialized) {
		stat->hw_initialized = 0;
	}

}

static int lng2dm_device_power_on(struct lng2dm_status *stat)
{
	int err = -1;

	if (stat->pdata->power_on) {
		err = stat->pdata->power_on();
		if (err < 0) {
			dev_err(&stat->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
	}

	if (!stat->hw_initialized) {
		err = lng2dm_hw_init(stat);
		if (stat->hw_working == 1 && err < 0) {
			lng2dm_device_power_off(stat);
			return err;
		}
	}

	return 0;
}

/*
 * hrtimer callback - it is called every polling_rate msec.
 */
enum hrtimer_restart lng2dm_timer_func_queue_work(struct hrtimer *timer)
{
	struct lng2dm_status *stat;

	stat = container_of((struct hrtimer *)timer,
				struct lng2dm_status, hr_timer);

	stat->timestamp = lng2dm_get_time_ns();
	queue_work(lng2dm_workqueue, &stat->input_work);

	return HRTIMER_NORESTART;
}

static int lng2dm_update_fs_range(struct lng2dm_status *stat,
							u8 new_fs_range)
{
	int err = -1;

	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = LNG2DM_FS_MASK;
	unsigned int new_sensitivity = 0;

	switch (new_fs_range) {
	case LNG2DM_G_2G:
		new_sensitivity = LNG2DM_SENSITIVITY_2G;
		break;
	case LNG2DM_G_4G:
		new_sensitivity = LNG2DM_SENSITIVITY_4G;
		break;
	case LNG2DM_G_8G:
		new_sensitivity = LNG2DM_SENSITIVITY_8G;
		break;
	case LNG2DM_G_16G:
		new_sensitivity = LNG2DM_SENSITIVITY_16G;
		break;
	default:
		dev_err(&stat->client->dev, "invalid fs range requested: %u\n",
				new_fs_range);
		return -EINVAL;
	}

	/* Updates configuration register 4,
	* which contains fs range setting */
	buf[0] = CTRL_REG4;
	err = lng2dm_i2c_read(stat, buf, 1);
	if (err < 0)
		goto error;
	init_val = buf[0];
	stat->resume_state[RES_CTRL_REG4] = init_val;
	new_val = new_fs_range;
	updated_val = ((mask & new_val) | ((~mask) & init_val));
	buf[1] = updated_val;
	buf[0] = CTRL_REG4;
	err = lng2dm_i2c_write(stat, buf, 1);
	if (err < 0)
		goto error;

	stat->resume_state[RES_CTRL_REG4] = updated_val;

	stat->sensitivity = new_sensitivity;

	return err;
error:
	dev_err(&stat->client->dev,
			"update fs range failed 0x%02x,0x%02x: %d\n",
			buf[0], buf[1], err);

	return err;
}

static int lng2dm_update_odr(struct lng2dm_status *stat,
							int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the poll_interval requested by the system.
	 * It must be the longest interval lower then the poll interval.*/
	for (i = ARRAY_SIZE(lng2dm_odr_table) - 1; i >= 0; i--) {
		if ((lng2dm_odr_table[i].cutoff_ms <= poll_interval_ms)
								|| (i == 0))
			break;
	}
	config[1] = lng2dm_odr_table[i].mask;

	config[1] |= LNG2DM_ENABLE_ALL_AXES;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&stat->enabled)) {
		flush_workqueue(lng2dm_workqueue);

		config[0] = CTRL_REG1;
		err = lng2dm_i2c_write(stat, config, 1);
		if (err < 0)
			goto error;
	}
	stat->resume_state[RES_CTRL_REG1] = config[1];
	stat->ktime = ktime_set(0, MS_TO_NS(poll_interval_ms));

	return err;

error:
	dev_err(&stat->client->dev, "update odr failed 0x%02x,0x%02x: %d\n",
			config[0], config[1], err);

	return err;
}

static int lng2dm_register_write(struct lng2dm_status *stat,
					u8 *buf, u8 reg_address, u8 new_value)
{
	int err = -1;

		/* Sets configuration register at reg_address
		 *  NOTE: this is a straight overwrite  */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = lng2dm_i2c_write(stat, buf, 1);
		if (err < 0)
			return err;
	return err;
}

static int lng2dm_get_acceleration_data(
				struct lng2dm_status *stat, int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s32 hw_d[3] = { 0 };

	acc_data[0] = (AXISDATA_REG);
	err = lng2dm_i2c_read(stat, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = ((s32)((s8)acc_data[1]));
	hw_d[1] = ((s32)((s8)acc_data[3]));
	hw_d[2] = ((s32)((s8)acc_data[5]));

	xyz[0] = hw_d[0] * stat->sensitivity;
	xyz[1] = hw_d[1] * stat->sensitivity;
	xyz[2] = hw_d[2] * stat->sensitivity;

#ifdef DEBUG
	dev_info(&stat->client->dev, "%s read x=%d, y=%d, z=%d\n",
				LNG2DM_DEV_NAME, xyz[0], xyz[1], xyz[2]);
#endif
	return err;
}

static void lng2dm_report_values(struct lng2dm_status *stat,
					int *xyz)
{
	struct input_dev  *input = stat->input_dev;

	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_X, xyz[0]);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_Y, xyz[1]);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_Z, xyz[2]);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_MSB,
							stat->timestamp >> 32);
	input_event(input, INPUT_EVENT_TYPE, INPUT_EVENT_TIME_LSB,
							stat->timestamp & 0xffffffff);
	input_sync(input);
}

static int lng2dm_enable(struct lng2dm_status *stat)
{
	int err;

	if (!atomic_cmpxchg(&stat->enabled, 0, 1)) {
		err = lng2dm_device_power_on(stat);
		if (err < 0) {
			atomic_set(&stat->enabled, 0);
			return err;
		}

		hrtimer_start(&stat->hr_timer, stat->ktime, HRTIMER_MODE_REL);
	}

	return 0;
}

static int lng2dm_disable(struct lng2dm_status *stat)
{
	if (atomic_cmpxchg(&stat->enabled, 1, 0)) {
		cancel_work_sync(&stat->input_work);
		hrtimer_cancel(&stat->hr_timer);
		lng2dm_device_power_off(stat);
	}

	return 0;
}


static ssize_t read_single_reg(struct device *dev, char *buf, u8 reg)
{
	ssize_t ret;
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	int err;

	u8 data = reg;
	err = lng2dm_i2c_read(stat, &data, 1);
	if (err < 0)
		return err;
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;

}

static int write_reg(struct device *dev, const char *buf, u8 reg,
						u8 mask, int resume_index)
{
	int err = -1;
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	u8 x[2];
	u8 new_val;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	new_val = ((u8) val & mask);
	x[0] = reg;
	x[1] = new_val;

	err = lng2dm_register_write(stat, x, reg, new_val);
	if (err < 0)
		return err;

	stat->resume_state[resume_index] = new_val;

	return err;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int val;
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	mutex_lock(&stat->lock);
	val = stat->pdata->poll_interval;
	mutex_unlock(&stat->lock);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;

	if (!interval_ms)
		return -EINVAL;

	interval_ms = max_t(unsigned int, (unsigned int)interval_ms,
						stat->pdata->min_interval);
	mutex_lock(&stat->lock);
	stat->pdata->poll_interval = interval_ms;
	lng2dm_update_odr(stat, interval_ms);
	mutex_unlock(&stat->lock);
	return size;
}

static ssize_t attr_get_range(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	char range = 2;
	mutex_lock(&stat->lock);

	switch (stat->pdata->fs_range) {
	case LNG2DM_G_2G:
		range = 2;
		break;
	case LNG2DM_G_4G:
		range = 4;
		break;
	case LNG2DM_G_8G:
		range = 8;
		break;
	case LNG2DM_G_16G:
		range = 16;
		break;
	}
	mutex_unlock(&stat->lock);
	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	unsigned long val;
	u8 range;
	int err;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	switch (val) {
	case 2:
		range = LNG2DM_G_2G;
		break;
	case 4:
		range = LNG2DM_G_4G;
		break;
	case 8:
		range = LNG2DM_G_8G;
		break;
	case 16:
		range = LNG2DM_G_16G;
		break;
	default:
		dev_err(&stat->client->dev,
			"invalid range request: %lu, discarded\n", val);
		return -EINVAL;
	}
	mutex_lock(&stat->lock);
	err = lng2dm_update_fs_range(stat, range);
	if (err < 0) {
		mutex_unlock(&stat->lock);
		return err;
	}
	stat->pdata->fs_range = range;
	mutex_unlock(&stat->lock);
#ifdef DEBUG
	dev_info(&stat->client->dev, "range set to: %lu g\n", val);
#endif

	return size;
}

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	int val = atomic_read(&stat->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lng2dm_enable(stat);
	else
		lng2dm_disable(stat);

	return size;
}

static ssize_t attr_set_intconfig1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_CFG1, NO_MASK, RES_INT_CFG1);
}

static ssize_t attr_get_intconfig1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_CFG1);
}

static ssize_t attr_set_duration1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_DUR1, INT1_DURATION_MASK, RES_INT_DUR1);
}

static ssize_t attr_get_duration1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_DUR1);
}

static ssize_t attr_set_thresh1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_THS1, INT1_THRESHOLD_MASK, RES_INT_THS1);
}

static ssize_t attr_get_thresh1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_THS1);
}

static ssize_t attr_get_source1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return read_single_reg(dev, buf, INT_SRC1);
}

static ssize_t attr_set_click_cfg(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_CFG, TAP_CFG_MASK, RES_TT_CFG);
}

static ssize_t attr_get_click_cfg(struct device *dev,
		struct device_attribute *attr,	char *buf)
{

	return read_single_reg(dev, buf, TT_CFG);
}

static ssize_t attr_get_click_source(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_SRC);
}

static ssize_t attr_set_click_ths(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_THS, TAP_THS_MASK, RES_TT_THS);
}

static ssize_t attr_get_click_ths(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_THS);
}

static ssize_t attr_set_click_tlim(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_LIM, TAP_TLIM_MASK, RES_TT_LIM);
}

static ssize_t attr_get_click_tlim(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_LIM);
}

static ssize_t attr_set_click_tlat(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_TLAT, TAP_TLAT_MASK, RES_TT_TLAT);
}

static ssize_t attr_get_click_tlat(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_TLAT);
}

static ssize_t attr_set_click_tw(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_TLAT, TAP_TW_MASK, RES_TT_TLAT);
}

static ssize_t attr_get_click_tw(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_TLAT);
}


#ifdef DEBUG
/* PAY ATTENTION: These DEBUG functions don't manage resume_state */
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int rc;
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	u8 x[2];
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&stat->lock);
	x[0] = stat->reg_addr;
	mutex_unlock(&stat->lock);
	x[1] = val;
	rc = lng2dm_i2c_write(stat, x, 1);
	/*TODO: error need to be managed */
	return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	int rc;
	u8 data;

	mutex_lock(&stat->lock);
	data = stat->reg_addr;
	mutex_unlock(&stat->lock);
	rc = lng2dm_i2c_read(stat, &data, 1);
	/*TODO: error need to be managed */
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lng2dm_status *stat = dev_get_drvdata(dev);
	unsigned long val;
	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&stat->lock);
	stat->reg_addr = val;
	mutex_unlock(&stat->lock);
	return size;
}
#endif

static struct device_attribute attributes[] = {

	__ATTR(pollrate_ms, 0664, attr_get_polling_rate, attr_set_polling_rate),
	__ATTR(range, 0664, attr_get_range, attr_set_range),
	__ATTR(enable_device, 0664, attr_get_enable, attr_set_enable),
	__ATTR(int1_config, 0664, attr_get_intconfig1, attr_set_intconfig1),
	__ATTR(int1_duration, 0664, attr_get_duration1, attr_set_duration1),
	__ATTR(int1_threshold, 0664, attr_get_thresh1, attr_set_thresh1),
	__ATTR(int1_source, 0444, attr_get_source1, NULL),
	__ATTR(click_config, 0664, attr_get_click_cfg, attr_set_click_cfg),
	__ATTR(click_source, 0444, attr_get_click_source, NULL),
	__ATTR(click_threshold, 0664, attr_get_click_ths, attr_set_click_ths),
	__ATTR(click_timelimit, 0664, attr_get_click_tlim, attr_set_click_tlim),
	__ATTR(click_timelatency, 0664, attr_get_click_tlat,
							attr_set_click_tlat),
	__ATTR(click_timewindow, 0664, attr_get_click_tw, attr_set_click_tw),

#ifdef DEBUG
	__ATTR(reg_value, 0600, attr_reg_get, attr_reg_set),
	__ATTR(reg_addr, 0200, NULL, attr_addr_set),
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);

	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

static void lng2dm_input_work_func(struct work_struct *work)
{
	struct lng2dm_status *stat;

	int xyz[3] = { 0 };
	int err;

	stat = container_of((struct work_struct *)work,
			struct lng2dm_status, input_work);

	hrtimer_start(&stat->hr_timer, stat->ktime, HRTIMER_MODE_REL);

	err = lng2dm_get_acceleration_data(stat, xyz);
	if (err < 0)
		dev_err(&stat->client->dev, "get_acceleration_data failed\n");
	else
		lng2dm_report_values(stat, xyz);
}

static int lng2dm_validate_pdata(struct lng2dm_status *stat)
{
	/* checks for correctness of minimal polling period */
	stat->pdata->min_interval =
		max((unsigned int)LNG2DM_MIN_POLL_PERIOD_MS,
						stat->pdata->min_interval);

	stat->pdata->poll_interval = max(stat->pdata->poll_interval,
			stat->pdata->min_interval);

	/* Enforce minimum polling interval */
	if (stat->pdata->poll_interval < stat->pdata->min_interval) {
		dev_err(&stat->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lng2dm_input_init(struct lng2dm_status *stat)
{
	int err;

	if (!lng2dm_workqueue)
		lng2dm_workqueue = create_workqueue(stat->client->name);

	if (!lng2dm_workqueue)
		return -EINVAL;

	INIT_WORK(&stat->input_work, lng2dm_input_work_func);
	stat->input_dev = input_allocate_device();
	if (!stat->input_dev) {
		err = -ENOMEM;
		dev_err(&stat->client->dev, "input device allocation failed\n");
		goto err0;
	}

	stat->input_dev->name = LNG2DM_DEV_NAME;
	stat->input_dev->id.bustype = BUS_I2C;
	stat->input_dev->dev.parent = &stat->client->dev;

	input_set_drvdata(stat->input_dev, stat);

	set_bit(INPUT_EVENT_TYPE, stat->input_dev->evbit);
	__set_bit(INPUT_EVENT_X, stat->input_dev->mscbit);
	__set_bit(INPUT_EVENT_Y, stat->input_dev->mscbit);
	__set_bit(INPUT_EVENT_Z, stat->input_dev->mscbit);
	__set_bit(INPUT_EVENT_TIME_MSB, stat->input_dev->mscbit);
	__set_bit(INPUT_EVENT_TIME_LSB, stat->input_dev->mscbit);

	err = input_register_device(stat->input_dev);
	if (err) {
		dev_err(&stat->client->dev,
				"unable to register input device %s\n",
				stat->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(stat->input_dev);
err0:
	return err;
}

static void lng2dm_input_cleanup(struct lng2dm_status *stat)
{
	input_unregister_device(stat->input_dev);
	input_free_device(stat->input_dev);
}

#ifdef CONFIG_OF
static const struct of_device_id lng2dm_dt_id[] = {
        {.compatible = "st,lng2dm",},
        {},
};
MODULE_DEVICE_TABLE(of, lng2dm_dt_id);

static u32 lng2dm_parse_dt(struct lng2dm_status *stat, struct device* dev)
{
	u32 val;
	struct device_node *np;

	np = dev->of_node;
	if (!np)
		return -EINVAL;

	if (!of_property_read_u32(np, "poll-interval", &val))
		stat->pdata->poll_interval = val;
	else
		stat->pdata->poll_interval = 10;

	stat->pdata->init = 0x0;
	stat->pdata->exit = 0x0;
	stat->pdata->power_on = 0x0;
	stat->pdata->power_off = 0x0;

	return 0;
}
#endif

static int lng2dm_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct lng2dm_status *stat;

	u32 smbus_func = (I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);

	int err = -1;

	dev_info(&client->dev, "probe start.\n");

	stat = kzalloc(sizeof(struct lng2dm_status), GFP_KERNEL);
	if (stat == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_check_functionality_failed;
	}

	/* Support for both I2C and SMBUS adapter interfaces. */
	stat->use_smbus = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev, "client not i2c capable\n");
		if (i2c_check_functionality(client->adapter, smbus_func)) {
			stat->use_smbus = 1;
			dev_warn(&client->dev, "client using SMBUS\n");
		} else {
			err = -ENODEV;
			dev_err(&client->dev, "client nor SMBUS capable\n");
			goto exit_check_functionality_failed;
		}
	}


	mutex_init(&stat->lock);
	mutex_lock(&stat->lock);

	stat->client = client;
	i2c_set_clientdata(client, stat);

	stat->pdata = kzalloc(sizeof(*stat->pdata), GFP_KERNEL);
	if (stat->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_mutexunlock;
	}

#ifdef CONFIG_OF
	lng2dm_parse_dt(stat, &client->dev);
#else
	if (client->dev.platform_data == NULL) {
		memcpy(stat->pdata, &default_lng2dm_pdata,
							sizeof(*stat->pdata));
		dev_info(&client->dev, "using default plaform_data\n");
	} else {
		memcpy(stat->pdata, client->dev.platform_data,
							sizeof(*stat->pdata));
	}
#endif

	err = lng2dm_validate_pdata(stat);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}


	if (stat->pdata->init) {
		err = stat->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err_pdata_init;
		}
	}

	memset(stat->resume_state, 0, ARRAY_SIZE(stat->resume_state));

	stat->resume_state[RES_CTRL_REG1] = (ALL_ZEROES |
					LNG2DM_ENABLE_ALL_AXES);
	stat->resume_state[RES_CTRL_REG4] = (ALL_ZEROES | CTRL_REG4_BDU_ENABLE);

	err = lng2dm_input_init(stat);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}

	err = lng2dm_device_power_on(stat);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_pdata_init;
	}

	atomic_set(&stat->enabled, 1);

	err = lng2dm_update_fs_range(stat, stat->pdata->fs_range);
	if (err < 0) {
		dev_err(&client->dev, "update_fs_range failed\n");
		goto  err_power_off;
	}

	err = lng2dm_update_odr(stat, stat->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_power_off;
	}

	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
		   "device LNG2DM_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
	}

	lng2dm_device_power_off(stat);

	/* As default, do not report information */
	atomic_set(&stat->enabled, 0);

	hrtimer_init(&stat->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	stat->hr_timer.function = &lng2dm_timer_func_queue_work;

	mutex_unlock(&stat->lock);

	dev_info(&client->dev, "%s: probed\n", LNG2DM_DEV_NAME);

	return 0;

err_input_cleanup:
	lng2dm_input_cleanup(stat);
err_power_off:
	lng2dm_device_power_off(stat);
err_pdata_init:
	if (stat->pdata->exit)
		stat->pdata->exit();
exit_kfree_pdata:
	kfree(stat->pdata);
err_mutexunlock:
	mutex_unlock(&stat->lock);
/* err_freedata: */
	kfree(stat);
exit_check_functionality_failed:
	pr_err("%s: Driver Init failed\n", LNG2DM_DEV_NAME);
	return err;
}

static int lng2dm_remove(struct i2c_client *client)
{

	struct lng2dm_status *stat = i2c_get_clientdata(client);

	dev_info(&stat->client->dev, "driver removing\n");

	lng2dm_disable(stat);
	lng2dm_input_cleanup(stat);

	remove_sysfs_interfaces(&client->dev);

	if (stat->pdata->exit)
		stat->pdata->exit();
	kfree(stat->pdata);
	kfree(stat);

	if(!lng2dm_workqueue) {
		flush_workqueue(lng2dm_workqueue);
		destroy_workqueue(lng2dm_workqueue);
	}

	return 0;
}

#ifdef CONFIG_PM
static int lng2dm_resume(struct i2c_client *client)
{
	struct lng2dm_status *stat = i2c_get_clientdata(client);

	if (stat->on_before_suspend)
		return lng2dm_enable(stat);
	return 0;
}

static int lng2dm_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lng2dm_status *stat = i2c_get_clientdata(client);

	stat->on_before_suspend = atomic_read(&stat->enabled);
	return lng2dm_disable(stat);
}

static SIMPLE_DEV_PM_OPS(lng2dm_pm_ops, lng2dm_suspend, lng2dm_resume);
#define LNG2DM_PM_OPS (&lng2dm_pm_ops)
#else
#define LNG2DM_PM_OPS NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id lng2dm_id[]
		= { { LNG2DM_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, lng2dm_id);


static const struct acpi_device_id lng2dm_acpi_match[] = {
        { "SMO8A90", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, lng2dm_acpi_match);

static struct i2c_driver lng2dm_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LNG2DM_DEV_NAME,
			.pm = LNG2DM_PM_OPS,
			.acpi_match_table = ACPI_PTR(lng2dm_acpi_match),
		  },
	.probe = lng2dm_probe,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,3,0)
	.remove = __devexit_p(lng2dm_remove),
#else
	.remove = lng2dm_remove,
#endif
	.id_table = lng2dm_id,
};

static int __init lng2dm_init(void)
{
	pr_info("%s accelerometer driver: init\n",
						LNG2DM_DEV_NAME);
	return i2c_add_driver(&lng2dm_driver);
}

static void __exit lng2dm_exit(void)
{

	pr_info("%s accelerometer driver exit\n",
						LNG2DM_DEV_NAME);

	i2c_del_driver(&lng2dm_driver);
	return;
}

module_init(lng2dm_init);
module_exit(lng2dm_exit);

MODULE_DESCRIPTION("lng2dm accelerometer driver");
MODULE_AUTHOR("Armando Visconti, STMicroelectronics");
MODULE_LICENSE("GPL");

