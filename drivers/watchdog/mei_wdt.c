/*
 * Intel Management Engine Interface (Intel MEI) Linux driver
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/watchdog.h>

#include <linux/uuid.h>
#include <linux/mei_cl_bus.h>

/*
 * iAMT Watchdog Device
 */
#define INTEL_AMT_WATCHDOG_ID "iamt_wdt"

#define MEI_WDT_DEFAULT_TIMEOUT   120  /* seconds */
#define MEI_WDT_MIN_TIMEOUT       120  /* seconds */
#define MEI_WDT_MAX_TIMEOUT     65535  /* seconds */

/* Commands */
#define MEI_MANAGEMENT_CONTROL 0x02

/* MEI Management Control version number */
#define MEI_MC_VERSION_NUMBER  0x10

/* Sub Commands */
#define MEI_MC_START_WD_TIMER_REQ  0x13
#define MEI_MC_STOP_WD_TIMER_REQ   0x14

/**
 * enum mei_wdt_state - internal watchdog state
 *
 * @MEI_WDT_IDLE: wd is idle and not opened
 * @MEI_WDT_START: wd was opened, start was called
 * @MEI_WDT_RUNNING: wd is expecting keep alive pings
 * @MEI_WDT_STOPPING: wd is stopping and will move to IDLE
 */
enum mei_wdt_state {
	MEI_WDT_IDLE,
	MEI_WDT_START,
	MEI_WDT_RUNNING,
	MEI_WDT_STOPPING,
};

#if IS_ENABLED(CONFIG_DEBUG_FS)
static const char *mei_wdt_state_str(enum mei_wdt_state state)
{
	switch (state) {
	case MEI_WDT_IDLE:
		return "IDLE";
	case MEI_WDT_START:
		return "START";
	case MEI_WDT_RUNNING:
		return "RUNNING";
	case MEI_WDT_STOPPING:
		return "STOPPING";
	default:
		return "unknown";
	}
}
#endif /* CONFIG_DEBUG_FS */

struct mei_wdt;

/**
 * struct mei_wdt_dev - watchdog device wrapper
 *
 * @wdd: watchdog device
 * @wdt: back pointer to mei_wdt driver
 * @refcnt: reference counter
 */
struct mei_wdt_dev {
	struct watchdog_device wdd;
	struct mei_wdt *wdt;
	struct kref refcnt;
};

/**
 * struct mei_wdt - mei watchdog driver
 * @mwd: watchdog device wrapper
 *
 * @cldev: mei watchdog client device
 * @state: watchdog internal state
 * @timeout: watchdog current timeout
 *
 * @dbgfs_dir: debugfs dir entry
 */
struct mei_wdt {
	struct mei_wdt_dev *mwd;

	struct mei_cl_device *cldev;
	enum mei_wdt_state state;
	u16 timeout;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs_dir;
#endif /* CONFIG_DEBUG_FS */
};

/*
 * struct mei_mc_hdr - Management Control Command Header
 *
 * @command: Management Control (0x2)
 * @bytecount: Number of bytes in the message beyond this byte
 * @subcommand: Management Control Subcommand
 * @versionnumber: Management Control Version (0x10)
 */
struct mei_mc_hdr {
	u8 command;
	u8 bytecount;
	u8 subcommand;
	u8 versionnumber;
};

/**
 * struct mei_wdt_start_request watchdog start/ping
 *
 * @hdr: Management Control Command Header
 * @timeout: timeout value
 * @reserved: reserved (legacy)
 */
struct mei_wdt_start_request {
	struct mei_mc_hdr hdr;
	u16 timeout;
	u8 reserved[17];
} __packed;

/**
 * struct mei_wdt_stop_request - watchdog stop
 *
 * @hdr: Management Control Command Header
 */
struct mei_wdt_stop_request {
	struct mei_mc_hdr hdr;
} __packed;

/**
 * mei_wdt_ping - send wd start/ping command
 *
 * @wdt: mei watchdog device
 *
 * Return: 0 on success,
 *         negative errno code on failure
 */
static int mei_wdt_ping(struct mei_wdt *wdt)
{
	struct mei_wdt_start_request req;
	const size_t req_len = sizeof(req);
	int ret;

	memset(&req, 0, req_len);
	req.hdr.command = MEI_MANAGEMENT_CONTROL;
	req.hdr.bytecount = req_len - offsetof(struct mei_mc_hdr, subcommand);
	req.hdr.subcommand = MEI_MC_START_WD_TIMER_REQ;
	req.hdr.versionnumber = MEI_MC_VERSION_NUMBER;
	req.timeout = wdt->timeout;

	ret = mei_cldev_send(wdt->cldev, (u8 *)&req, req_len);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * mei_wdt_stop - send wd stop command
 *
 * @wdt: mei watchdog device
 *
 * Return: 0 on success,
 *         negative errno code on failure
 */
static int mei_wdt_stop(struct mei_wdt *wdt)
{
	struct mei_wdt_stop_request req;
	const size_t req_len = sizeof(req);
	int ret;

	memset(&req, 0, req_len);
	req.hdr.command = MEI_MANAGEMENT_CONTROL;
	req.hdr.bytecount = req_len - offsetof(struct mei_mc_hdr, subcommand);
	req.hdr.subcommand = MEI_MC_STOP_WD_TIMER_REQ;
	req.hdr.versionnumber = MEI_MC_VERSION_NUMBER;

	ret = mei_cldev_send(wdt->cldev, (u8 *)&req, req_len);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * mei_wdt_ops_start - wd start command from the watchdog core.
 *
 * @wdd: watchdog device
 *
 * Return: 0 on success or -ENODEV;
 */
static int mei_wdt_ops_start(struct watchdog_device *wdd)
{
	struct mei_wdt_dev *mwd = watchdog_get_drvdata(wdd);
	struct mei_wdt *wdt;

	if (!mwd)
		return -ENODEV;

	wdt = mwd->wdt;

	wdt->state = MEI_WDT_START;
	wdd->timeout = wdt->timeout;
	return 0;
}

/**
 * mei_wdt_ops_stop - wd stop command from the watchdog core.
 *
 * @wdd: watchdog device
 *
 * Return: 0 if success, negative errno code for failure
 */
static int mei_wdt_ops_stop(struct watchdog_device *wdd)
{
	struct mei_wdt_dev *mwd = watchdog_get_drvdata(wdd);
	struct mei_wdt *wdt;
	int ret;

	if (!mwd)
		return -ENODEV;

	wdt = mwd->wdt;

	if (wdt->state != MEI_WDT_RUNNING)
		return 0;

	wdt->state = MEI_WDT_STOPPING;

	ret = mei_wdt_stop(wdt);
	if (ret)
		return ret;

	wdt->state = MEI_WDT_IDLE;

	return 0;
}

/**
 * mei_wdt_ops_ping - wd ping command from the watchdog core.
 *
 * @wdd: watchdog device
 *
 * Return: 0 if success, negative errno code on failure
 */
static int mei_wdt_ops_ping(struct watchdog_device *wdd)
{
	struct mei_wdt_dev *mwd = watchdog_get_drvdata(wdd);
	struct mei_wdt *wdt;
	int ret;

	if (!mwd)
		return -ENODEV;

	wdt = mwd->wdt;

	if (wdt->state != MEI_WDT_START && wdt->state != MEI_WDT_RUNNING)
		return 0;

	ret = mei_wdt_ping(wdt);
	if (ret)
		return ret;

	wdt->state = MEI_WDT_RUNNING;

	return 0;
}

/**
 * mei_wdt_ops_set_timeout - wd set timeout command from the watchdog core.
 *
 * @wdd: watchdog device
 * @timeout: timeout value to set
 *
 * Return: 0 if success, negative errno code for failure
 */
static int mei_wdt_ops_set_timeout(struct watchdog_device *wdd,
				   unsigned int timeout)
{

	struct mei_wdt_dev *mwd = watchdog_get_drvdata(wdd);
	struct mei_wdt *wdt;

	if (!mwd)
		return -ENODEV;

	wdt = mwd->wdt;

	/* valid value is already checked by the caller */
	wdt->timeout = timeout;
	wdd->timeout = timeout;

	return 0;
}

static void mei_wdt_release(struct kref *ref)
{
	struct mei_wdt_dev *mwd = container_of(ref, struct mei_wdt_dev, refcnt);

	kfree(mwd);
}

static void mei_wdt_ops_ref(struct watchdog_device *wdd)
{
	struct mei_wdt_dev *mwd = watchdog_get_drvdata(wdd);

	kref_get(&mwd->refcnt);
}

static void mei_wdt_ops_unref(struct watchdog_device *wdd)
{
	struct mei_wdt_dev *mwd = watchdog_get_drvdata(wdd);

	kref_put(&mwd->refcnt, mei_wdt_release);
}

static const struct watchdog_ops wd_ops = {
	.owner       = THIS_MODULE,
	.start       = mei_wdt_ops_start,
	.stop        = mei_wdt_ops_stop,
	.ping        = mei_wdt_ops_ping,
	.set_timeout = mei_wdt_ops_set_timeout,
	.ref         = mei_wdt_ops_ref,
	.unref       = mei_wdt_ops_unref,
};

static struct watchdog_info wd_info = {
	.identity = INTEL_AMT_WATCHDOG_ID,
	.options  = WDIOF_KEEPALIVEPING |
		    WDIOF_SETTIMEOUT |
		    WDIOF_ALARMONLY,
};

/**
 * mei_wdt_unregister - unregister from the watchdog subsystem
 *
 * @wdt: mei watchdog device
 */
static void mei_wdt_unregister(struct mei_wdt *wdt)
{
	struct mei_wdt_dev *mwd = wdt->mwd;

	if (!mwd)
		return;

	watchdog_unregister_device(&mwd->wdd);
	wdt->mwd = NULL;
	wdt->state = MEI_WDT_IDLE;
	kref_put(&mwd->refcnt, mei_wdt_release);
}

/**
 * mei_wdt_register - register with the watchdog subsystem
 *
 * @wdt: mei watchdog device
 *
 * Return: 0 if success, negative errno code for failure
 */
static int mei_wdt_register(struct mei_wdt *wdt)
{
	struct mei_wdt_dev *mwd;
	struct device *dev;
	int ret;

	if (!wdt || !wdt->cldev)
		return -EINVAL;

	dev = &wdt->cldev->dev;

	mwd = kzalloc(sizeof(struct mei_wdt_dev), GFP_KERNEL);
	if (!mwd)
		return -ENOMEM;

	mwd->wdt = wdt;
	mwd->wdd.info = &wd_info;
	mwd->wdd.ops = &wd_ops;
	mwd->wdd.parent = dev;
	mwd->wdd.timeout = MEI_WDT_DEFAULT_TIMEOUT;
	mwd->wdd.min_timeout = MEI_WDT_MIN_TIMEOUT;
	mwd->wdd.max_timeout = MEI_WDT_MAX_TIMEOUT;
	kref_init(&mwd->refcnt);

	watchdog_set_drvdata(&mwd->wdd, mwd);
	ret = watchdog_register_device(&mwd->wdd);
	if (ret) {
		dev_err(dev, "unable to register watchdog device = %d.\n", ret);
		kref_put(&mwd->refcnt, mei_wdt_release);
		return ret;
	}

	wdt->mwd = mwd;
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)

static ssize_t mei_dbgfs_read_state(struct file *file, char __user *ubuf,
				    size_t cnt, loff_t *ppos)
{
	struct mei_wdt *wdt = file->private_data;
	const size_t bufsz = 32;
	char buf[32];
	ssize_t pos = 0;

	pos += scnprintf(buf + pos, bufsz - pos, "state: %s\n",
			 mei_wdt_state_str(wdt->state));

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, pos);
}

static const struct file_operations dbgfs_fops_state = {
	.open = simple_open,
	.read = mei_dbgfs_read_state,
	.llseek = generic_file_llseek,
};

static void dbgfs_unregister(struct mei_wdt *wdt)
{
	if (!wdt->dbgfs_dir)
		return;
	debugfs_remove_recursive(wdt->dbgfs_dir);
	wdt->dbgfs_dir = NULL;
}

static int dbgfs_register(struct mei_wdt *wdt)
{
	struct dentry *dir, *f;

	dir = debugfs_create_dir(KBUILD_MODNAME, NULL);
	if (!dir)
		return -ENOMEM;

	wdt->dbgfs_dir = dir;
	f = debugfs_create_file("state", S_IRUSR, dir, wdt, &dbgfs_fops_state);
	if (!f)
		goto err;

	return 0;
err:
	dbgfs_unregister(wdt);
	return -ENODEV;
}

#else

static inline void dbgfs_unregister(struct mei_wdt *wdt) {}

static inline int dbgfs_register(struct mei_wdt *wdt)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static int mei_wdt_probe(struct mei_cl_device *cldev,
			 const struct mei_cl_device_id *id)
{
	struct mei_wdt *wdt;
	int ret;

	wdt = kzalloc(sizeof(struct mei_wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->timeout = MEI_WDT_DEFAULT_TIMEOUT;
	wdt->state = MEI_WDT_IDLE;
	wdt->cldev = cldev;
	mei_cldev_set_drvdata(cldev, wdt);

	ret = mei_cldev_enable(cldev);
	if (ret < 0) {
		dev_err(&cldev->dev, "Could not enable cl device\n");
		goto err_out;
	}

	wd_info.firmware_version = mei_cldev_ver(cldev);

	ret = mei_wdt_register(wdt);
	if (ret)
		goto err_disable;

	dbgfs_register(wdt);

	return 0;

err_disable:
	mei_cldev_disable(cldev);

err_out:
	kfree(wdt);

	return ret;
}

static int mei_wdt_remove(struct mei_cl_device *cldev)
{
	struct mei_wdt *wdt = mei_cldev_get_drvdata(cldev);

	mei_wdt_unregister(wdt);

	mei_cldev_disable(cldev);

	dbgfs_unregister(wdt);

	kfree(wdt);

	return 0;
}

#define MEI_UUID_WD UUID_LE(0x05B79A6F, 0x4628, 0x4D7F, \
			    0x89, 0x9D, 0xA9, 0x15, 0x14, 0xCB, 0x32, 0xAB)

static struct mei_cl_device_id mei_wdt_tbl[] = {
	{ .uuid = MEI_UUID_WD, .version = 0x1},
	/* required last entry */
	{ }
};
MODULE_DEVICE_TABLE(mei, mei_wdt_tbl);

static struct mei_cl_driver mei_wdt_driver = {
	.id_table = mei_wdt_tbl,
	.name = KBUILD_MODNAME,

	.probe = mei_wdt_probe,
	.remove = mei_wdt_remove,
};

static int __init mei_wdt_init(void)
{
	int ret;

	ret = mei_cldev_driver_register(&mei_wdt_driver);
	if (ret) {
		pr_err(KBUILD_MODNAME ": module registration failed\n");
		return ret;
	}
	return 0;
}

static void __exit mei_wdt_exit(void)
{
	mei_cldev_driver_unregister(&mei_wdt_driver);
}

module_init(mei_wdt_init);
module_exit(mei_wdt_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Device driver for Intel MEI iAMT watchdog");
