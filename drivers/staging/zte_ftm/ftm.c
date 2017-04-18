/*
 * drivers/staging/zte_ftm/ftm.c
 *
 * Copyright (C) 2012-2013 ZTE, Corp.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  Date         Author           Comment
 *  -----------  --------------   -------------------------------------
 *  2012-08-03   Jia              The kernel module for FTM,
 *                                created by ZTE_BOO_JIA_20120803 jia.jia
 *  -------------------------------------------------------------------
 *
*/

#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <mach/scm.h>
#include <mach/msm_iomap.h>
#include <mach/msm_iomap.h>
#include <mach/boot_shared_imem_cookie.h>
#include <mach/msm_smd.h>

/*
 * Macro Definition
 */
#define FTM_VERSION "1.0"
#define FTM_CLASS_NAME "zte_ftm"

#define QFPROM_WRITE_ROW_ID	           0x03
#define QFPROM_WRITE_MULTIPLE_ROWS_ID  0x04
#define QFPROM_READ_ROW_ID             0x05
#define QFPROM_ROLLBACK_WRITE_ROW_ID   0x06

#define FUSION_OPT_UNDEFINE	          0x00
#define FUSION_OPT_WRITE_SECURE_BOOT  0x01
#define FUSION_OPT_READ_SECURE_BOOT	  0x02
#define FUSION_OPT_WRITE_SIMLOCK      0x03
#define FUSION_OPT_READ_SIMLOCK	      0x04

#define FUSION_MAGIC_NUM_1  0x46555349
#define FUSION_MAGIC_NUM_2  0x4F4E2121

#define QFPROM_STATUS_FUSE_OK   0x01
#define QFPROM_STATUS_FUSE_FAIL	0x00
#define QFPROM_STATUS_UNKOWN    0xff

/*
 * Type Definition
 */
typedef struct {
	uint32_t secboot_status;
	uint32_t simlock_status;
} ftm_sysdev_attrs_t;

typedef struct {
	uint32_t raw_row_address;  /* Row address */
	uint32_t row_data_msb;	   /* MSB row data */
	uint32_t row_data_lsb;     /* LSB row data */
} write_row_type;

typedef enum
{
	QFPROM_ADDR_SPACE_RAW  = 0,          /* Raw address space. */
	QFPROM_ADDR_SPACE_CORR = 1,          /* Corrected address space. */
	QFPROM_ADDR_SPACE_MAX  = 0x7FFFFFFF  /* Last entry in the QFPROM_ADDR_SPACE enumerator. */
} QFPROM_ADDR_SPACE;

typedef struct {
	uint32_t row_address;
	uint32_t addr_type;
	uint32_t *fuse_data;
	uint32_t *status;
} read_row_cmd;

/*
 * Global Variables Definition
 */
static struct sys_device ftm_sysdev;
static ftm_sysdev_attrs_t ftm_sysdev_attrs;
static spinlock_t ftm_spinlock;

/*
 * Function declaration
 */
static int32_t ftm_register_sysdev(struct sys_device *sysdev);

static ssize_t show_simlock_status(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	uint32_t *smem_fuse_status = NULL;
	
    smem_fuse_status = (int *)smem_alloc2(SMEM_ID_VENDOR2, sizeof(uint32_t)*2);
	
    if (!smem_fuse_status)
	{
        pr_err("%s: alloc smem failed!\n", __func__);
		return -EFAULT;
	}
	
    /*
      * 0: No Blown
      * 1: Has Blown
      * 0xff: status error
      */
    ftm_sysdev_attrs.simlock_status = *smem_fuse_status;

    pr_info("%s: show_simlock_fuse_status.\n", __func__);
	
	return snprintf(buf, PAGE_SIZE, "%d\n", ftm_sysdev_attrs.simlock_status);
}

static SYSDEV_ATTR(simlock_status, S_IRUGO | S_IRUSR, show_simlock_status, NULL);

static ssize_t show_secboot_status(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	uint32_t *smem_fuse_status = NULL;

    smem_fuse_status = (int *)smem_alloc2(SMEM_ID_VENDOR2, sizeof(uint32_t)*2);

    if (!smem_fuse_status)
	{
        pr_err("%s: alloc smem failed!\n", __func__);
		return -EFAULT;
	}	
	
    /*
      * 0: No Blown
      * 1: Has Blown
      * 0xff: status error

      */
    ftm_sysdev_attrs.secboot_status = *(smem_fuse_status+1);

    pr_info("%s: show_simlock_fuse_status.\n", __func__);
	
	return snprintf(buf, PAGE_SIZE, "%d\n", ftm_sysdev_attrs.secboot_status);
	
}

static SYSDEV_ATTR(secboot_status, S_IRUGO | S_IRUSR, show_secboot_status, NULL);

/* ZTE_BOOT_20130123 huang.yanjun add for efs golden recovery ---->*/
static ssize_t store_efs_recovery(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t buf_sz)
{
	struct boot_shared_imem_cookie_type *boot_shared_imem_cookie_ptr = (struct boot_shared_imem_cookie_type *)MSM_IMEM_BASE;
	uint32_t efs_recovery_enable;

	pr_info("%s: e\n", __func__);

	if(!boot_shared_imem_cookie_ptr)
		return -EFAULT;

	sscanf(buf, "%d", &efs_recovery_enable);

	pr_info("%s: efs_recovery_enable: %d\n", __func__, efs_recovery_enable);

	if(efs_recovery_enable)
	{
		boot_shared_imem_cookie_ptr->efs_recovery_flag= 0x01;
	}
	else
	{
		boot_shared_imem_cookie_ptr->efs_recovery_flag = 0x00;
	}
	mb();

	pr_info("%s: x\n", __func__);

	return buf_sz;
}

static ssize_t show_efs_recovery(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	struct boot_shared_imem_cookie_type *boot_shared_imem_cookie_ptr = (struct boot_shared_imem_cookie_type *)MSM_IMEM_BASE;

	pr_info("%s: e\n", __func__);

	if(!boot_shared_imem_cookie_ptr)
		return -EFAULT;

	return snprintf(buf, PAGE_SIZE, "%d\n", boot_shared_imem_cookie_ptr->efs_recovery_flag);
}

static SYSDEV_ATTR(efs_recovery, S_IRUGO | S_IWUSR, show_efs_recovery, store_efs_recovery);
/* ZTE_BOOT_20130123 huang.yanjun add for efs golden recovery <----*/

static struct sysdev_attribute *ftm_attrs[] = {
	&attr_secboot_status,
	&attr_simlock_status,
	&attr_efs_recovery
};

static struct sysdev_class ftm_sysdev_class = {
	.name = FTM_CLASS_NAME
};

/*
 * Sys device register
 *
 * sysdev file:
 *
 * /sys/devices/system/zte_ftm/zte_ftm0/fusion
 * /sys/devices/system/zte_ftm/zte_ftm0/simlocking
 */
static int32_t ftm_register_sysdev(struct sys_device *sysdev)
{
	int32_t ret;
	int32_t i;

	ret = sysdev_class_register(&ftm_sysdev_class);
	if (ret)
	{
		return ret;
	}

	sysdev->id = 0;
	sysdev->cls = &ftm_sysdev_class;

	ret = sysdev_register(sysdev);
	if (ret)
	{
		sysdev_class_unregister(&ftm_sysdev_class);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(ftm_attrs); i++)
	{
		ret = sysdev_create_file(sysdev, ftm_attrs[i]);
		if (ret)
		{
			goto ftm_fail;
		}
	}

	return 0;

ftm_fail:

	while (--i >= 0) sysdev_remove_file(sysdev, ftm_attrs[i]);

	sysdev_unregister(sysdev);
	sysdev_class_unregister(&ftm_sysdev_class);

	return ret;
}

static int32_t __init ftm_probe(struct platform_device *pdev)
{
	int ret;
	uint32_t cmd_id;

	for (cmd_id = QFPROM_WRITE_ROW_ID; cmd_id <= QFPROM_ROLLBACK_WRITE_ROW_ID; cmd_id++)
	{
		ret = scm_is_call_available(SCM_SVC_FUSE, cmd_id);
		pr_info("%s: cmd_id: 0x%02x, ret: %d\n", __func__, cmd_id, ret);

		if (ret <= 0)
		{
			return ret;
		}
	}

	return 0;
}

static int32_t ftm_remove(struct platform_device *pdev)
{
	pr_info("%s: e\n", __func__);

	pr_info("%s: x\n", __func__);

	return 0;
}

static struct platform_driver ftm_driver = {
	.remove = ftm_remove,
	.driver = {
		.name = "zte_ftm",
		.owner = THIS_MODULE,
	},
};

/*
 * Initializes the module.
 */
static int32_t __init ftm_init(void)
{
	int ret;

	pr_info("%s: e\n", __func__);

	ret = platform_driver_probe(&ftm_driver, ftm_probe);
	if (ret)
	{
	    return ret;
	}

	/*
	 * Initialize spinlock
	 */
	spin_lock_init(&ftm_spinlock);

	/*
	 * Initialize sys device
	 */
	ftm_register_sysdev(&ftm_sysdev);

	pr_info("%s: x\n", __func__);

	return 0;
}

/*
 * Cleans up the module.
 */
static void __exit ftm_exit(void)
{
	platform_driver_unregister(&ftm_driver);
}

module_init(ftm_init);
module_exit(ftm_exit);

MODULE_DESCRIPTION("ZTE FTM Driver Ver %s" FTM_VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
