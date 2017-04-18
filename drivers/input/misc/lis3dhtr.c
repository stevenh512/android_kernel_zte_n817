/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
 *
 * File Name          : lis3dh_acc.c
 * Authors            : MSH - Motion Mems BU - Application Team
 *		      : Matteo Dameno (matteo.dameno@st.com)
 *		      : Carmine Iascone (carmine.iascone@st.com)
 *		      : Both authors are willing to be considered the contact
 *		      : and update points for the driver.
 * Version            : V.1.0.9
 * Date               : 2010/May/23
 * Description        : LIS3DH accelerometer sensor API
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 ******************************************************************************
 Revision 1.0.0 05/11/09
 First Release;
 Revision 1.0.3 22/01/2010
  Linux K&R Compliant Release;
 Revision 1.0.5 16/08/2010
  modified _get_acceleration_data function;
  modified _update_odr function;
  manages 2 interrupts;
 Revision 1.0.6 15/11/2010
  supports sysfs;
  no more support for ioctl;
 Revision 1.0.7 26/11/2010
  checks for availability of interrupts pins
  correction on FUZZ and FLAT values;
 Revision 1.0.8 2010/Apr/01
  corrects a bug in interrupt pin management in 1.0.7
 Revision 1.0.9: 2011/May/23
  update_odr func correction;
 Revision 1.0.9.1: 2012/Mar/21 by morris chen
  change the sysf attribute for android CTS
 Revision 1.0.9.2: 2012/May/11 by morris chen
  add early_suspend and late_resume
  add procfs
 Revision 1.0.9.3: 2012/May/ by morris chen
  add and modify the sysfs
  add calibration
 ******************************************************************************/

#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>
#include	<linux/input.h>
#include	<linux/uaccess.h>
#include	<linux/workqueue.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#include	<linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

#define ZTE_ST_ACCEL_CALI //xym

#include	<linux/lis3dh.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>//xym

#define	DEBUG	0

#define	G_MAX		16000


#define SENSITIVITY_2G		1	/**	mg/LSB	*/
#define SENSITIVITY_4G		2	/**	mg/LSB	*/
#define SENSITIVITY_8G		4	/**	mg/LSB	*/
#define SENSITIVITY_16G		12	/**	mg/LSB	*/




/* Accelerometer Sensor Operating Mode */
#define LIS3DH_ACC_ENABLE	0x01
#define LIS3DH_ACC_DISABLE	0x00

#define	HIGH_RESOLUTION		0x08

#define	AXISDATA_REG		0x28
#define WHOAMI_LIS3DH_ACC	0x33	/*	Expected content for WAI */

/*	CONTROL REGISTERS	*/
#define WHO_AM_I		0x0F	/*	WhoAmI register		*/
#define	TEMP_CFG_REG		0x1F	/*	temper sens control reg	*/
/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define	CTRL_REG1		0x20	/*	control reg 1		*/
#define	CTRL_REG2		0x21	/*	control reg 2		*/
#define	CTRL_REG3		0x22	/*	control reg 3		*/
#define	CTRL_REG4		0x23	/*	control reg 4		*/
#define	CTRL_REG5		0x24	/*	control reg 5		*/
#define	CTRL_REG6		0x25	/*	control reg 6		*/

#define	FIFO_CTRL_REG		0x2E	/*	FiFo control reg	*/

#define	INT_CFG1		0x30	/*	interrupt 1 config	*/
#define	INT_SRC1		0x31	/*	interrupt 1 source	*/
#define	INT_THS1		0x32	/*	interrupt 1 threshold	*/
#define	INT_DUR1		0x33	/*	interrupt 1 duration	*/


#define	TT_CFG			0x38	/*	tap config		*/
#define	TT_SRC			0x39	/*	tap source		*/
#define	TT_THS			0x3A	/*	tap threshold		*/
#define	TT_LIM			0x3B	/*	tap time limit		*/
#define	TT_TLAT			0x3C	/*	tap time latency	*/
#define	TT_TW			0x3D	/*	tap time window		*/
/*	end CONTROL REGISTRES	*/


#define ENABLE_HIGH_RESOLUTION	1

#define LIS3DH_ACC_PM_OFF		0x00
#define LIS3DH_ACC_ENABLE_ALL_AXES	0x07


#define PMODE_MASK			0x08
#define ODR_MASK			0XF0

#define ODR1		0x10  /* 1Hz output data rate */
#define ODR10		0x20  /* 10Hz output data rate */
#define ODR25		0x30  /* 25Hz output data rate */
#define ODR50		0x40  /* 50Hz output data rate */
#define ODR100		0x50  /* 100Hz output data rate */
#define ODR200		0x60  /* 200Hz output data rate */
#define ODR400		0x70  /* 400Hz output data rate */
#define ODR1250		0x90  /* 1250Hz output data rate */



#define	IA			0x40
#define	ZH			0x20
#define	ZL			0x10
#define	YH			0x08
#define	YL			0x04
#define	XH			0x02
#define	XL			0x01
/* */
/* CTRL REG BITS*/
#define	CTRL_REG3_I1_AOI1	0x40
#define	CTRL_REG6_I2_TAPEN	0x80
#define	CTRL_REG6_HLACTIVE	0x02
/* */
#define NO_MASK			0xFF
#define INT1_DURATION_MASK	0x7F
#define	INT1_THRESHOLD_MASK	0x7F
#define TAP_CFG_MASK		0x3F
#define	TAP_THS_MASK		0x7F
#define	TAP_TLIM_MASK		0x7F
#define	TAP_TLAT_MASK		NO_MASK
#define	TAP_TW_MASK		NO_MASK


/* TAP_SOURCE_REG BIT */
#define	DTAP			0x20
#define	STAP			0x10
#define	SIGNTAP			0x08
#define	ZTAP			0x04
#define	YTAP			0x02
#define	XTAZ			0x01


#define	FUZZ			0
#define	FLAT			0
#define	I2C_RETRY_DELAY		5
#define	I2C_RETRIES		5
#define	I2C_AUTO_INCREMENT	0x80

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

/*Procfs*/
#define BUF_SIZE 100;
#define PROC_SENSOR_DIR "sensors"
#define PROC_SENSOR_NAME "accel_info"
//#define PROC_SENSOR_ADDR "addr"

static struct regulator *vdd, *vbus;//xym

/*Direction*/
static int lis3dh_acc_direction[8][6] = {
  {0,1,2,0,0,0},
  {1,0,2,1,0,0},
  {0,1,2,1,1,0},
  {1,0,2,0,1,0},
  {1,0,2,1,1,1},
  {0,1,2,0,1,1},
  {1,0,2,0,0,1},
  {0,1,2,1,0,1}
};

#ifdef CONFIG_BOARD_ADA
static int direction[6] = {0,1,2,0,1,1};//backside  pin1 left-top P821T02
#elif defined CONFIG_BOARD_EOS //V72M
static int direction[6] = {0,1,2,0,1,1};
#elif defined CONFIG_BOARD_APUS || defined CONFIG_BOARD_GIANT ||defined CONFIG_BOARD_HERA//lcdside pin1 left-bottom P821A64 AND P821T05 Z793C
static int direction[6] = {1,0,2,0,1,0};
#else
static int direction[6] = {0,1,2,1,0,1};//backside  pin1 left-bottom  P821A32( Z777 )
#endif


static int direction_type = 0;

/*calibration*/
static int cali_value[3] = {0,0,0};

//#define CALI_TIMES 10
#define CALI_TIMES 100  //xym for calibration



static struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lis3dh_acc_odr_table[] = {
		{    1, ODR1250 },
		{    3, ODR400  },
		{    5, ODR200  },
		{   10, ODR100  },
		{   20, ODR50   },
		{   40, ODR25   },
		{  100, ODR10   },
		{ 1000, ODR1    },
};

struct lis3dh_acc_data {
	struct i2c_client *client;
	struct lis3dh_acc_platform_data *pdata;

	struct mutex lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	atomic_t cali;
	int on_before_suspend;

	u8 sensitivity;

	u8 resume_state[RESUME_ENTRIES];

	int irq1;
	struct work_struct irq1_work;
	struct workqueue_struct *irq1_work_queue;
	int irq2;
	struct work_struct irq2_work;
	struct workqueue_struct *irq2_work_queue;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

#ifdef DEBUG
	u8 reg_addr;
#endif
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lis3dh_acc_early_suspend(struct early_suspend *h);
static void lis3dh_acc_late_resume(struct early_suspend *h);
#endif

//static struct proc_dir_entry *proc_dir, *proc_file1, *proc_file2;
static struct proc_dir_entry *proc_dir, *proc_file1;

static struct i2c_client *proc_client;


static int lis3dh_acc_read_proc1(char *page, char **start, off_t off,
                       int count, int *eof, void *data)
{
	unsigned long info = get_zeroed_page(GFP_KERNEL);
	int len = BUF_SIZE;
	if (!info)
		return 0;
	len = sprintf(page, "name:%s\naddr:%d-00%x\n",
                      LIS3DH_ACC_DEV_NAME,
                      proc_client->adapter->nr,
                      proc_client->addr);

	free_page(info);
	return len;
}

#if 0

static int lis3dh_acc_read_proc2(char *page, char **start, off_t off,
                       int count, int *eof, void *data)
{
	unsigned long info = get_zeroed_page(GFP_KERNEL);
	int len = BUF_SIZE;
	if (!info)
		return 0;
	len = sprintf(page, "%x, %d, %s, %d, %s,\n",
                      proc_client->addr,
                      proc_client->adapter->id,
                      proc_client->name,
                      proc_client->adapter->nr,
                      proc_client->adapter->name);

	free_page(info);
	return len;
}

#endif


static int lis3dh_acc_i2c_read(struct lis3dh_acc_data *acc,
				u8 * buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg	msgs[] = {
		{
			.addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = acc->client->addr,
			.flags = (acc->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&acc->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_acc_i2c_write(struct lis3dh_acc_data *acc, u8 * buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
		 .addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

//read and print all reg value
static void zte_print_reg(struct lis3dh_acc_data *acc)
{
	u8 buf[7] = { 0 };
	int i=0;
	printk("lis3dh reg data------------------\n");


	buf[0] = 0x07;
	lis3dh_acc_i2c_read(acc, buf, 1);
	printk("lis3dh---reg[0x%X]=0x%X\n", 0x07, buf[0]);
	
	buf[0] = 0x0E;
	lis3dh_acc_i2c_read(acc, buf, 1);
	printk("lis3dh---reg[0x%X]=0x%X\n", 0x0E, buf[0]);
	
	buf[0] = 0x0F;
	lis3dh_acc_i2c_read(acc, buf, 1);
	printk("lis3dh---reg[0x%X]=0x%X\n", 0x0F, buf[0]);

	 for(i=0x1F;i<=0x25;i++)
	{
	   	buf[0] = i;
	    lis3dh_acc_i2c_read(acc, buf, 1);
        printk( "lis3dh---reg[0x%X]=0x%X\n", i, buf[0]);
	}

	buf[0] = 0x27;
	lis3dh_acc_i2c_read(acc, buf, 1);
	printk("lis3dh---reg[0x%X]=0x%X\n", 0x27, buf[0]);

	 for(i=0x2E;i<=0x30;i++)
	{
	   	buf[0] = i;
	    lis3dh_acc_i2c_read(acc, buf, 1);
        printk( "lis3dh---reg[0x%X]=0x%X\n", i, buf[0]);
	}

	 for(i=0x32;i<=0x33;i++)
	{
	   	buf[0] = i;
	    lis3dh_acc_i2c_read(acc, buf, 1);
        printk( "lis3dh---reg[0x%X]=0x%X\n", i, buf[0]);
	}
}


static int lis3dh_acc_hw_init(struct lis3dh_acc_data *acc)
{
	int err = -1;
	u8 buf[7];

	printk(KERN_INFO "%s: hw init start\n", LIS3DH_ACC_DEV_NAME);

	buf[0] = WHO_AM_I;
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	if (err < 0) {
	dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
		"available/working?\n");
		goto err_firstread;
	} else
		acc->hw_working = 1;
	if (buf[0] != WHOAMI_LIS3DH_ACC) {
	dev_err(&acc->client->dev,
		"device unknown. Expected: 0x%x,"
		" Replies: 0x%x\n", WHOAMI_LIS3DH_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}

	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TEMP_CFG_REG;
	buf[1] = acc->resume_state[RES_TEMP_CFG_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = acc->resume_state[RES_FIFO_CTRL_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
	buf[1] = acc->resume_state[RES_TT_THS];
	buf[2] = acc->resume_state[RES_TT_LIM];
	buf[3] = acc->resume_state[RES_TT_TLAT];
	buf[4] = acc->resume_state[RES_TT_TW];
	err = lis3dh_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto err_resume_state;
	buf[0] = TT_CFG;
	buf[1] = acc->resume_state[RES_TT_CFG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
	buf[1] = acc->resume_state[RES_INT_THS1];
	buf[2] = acc->resume_state[RES_INT_DUR1];
	err = lis3dh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;
	buf[0] = INT_CFG1;
	buf[1] = acc->resume_state[RES_INT_CFG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;


	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
	buf[1] = acc->resume_state[RES_CTRL_REG2];
	buf[2] = acc->resume_state[RES_CTRL_REG3];
	buf[3] = acc->resume_state[RES_CTRL_REG4];
	buf[4] = acc->resume_state[RES_CTRL_REG5];
	buf[5] = acc->resume_state[RES_CTRL_REG6];
	err = lis3dh_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		goto err_resume_state;

	acc->hw_initialized = 1;
	printk(KERN_INFO "%s: hw init done\n", LIS3DH_ACC_DEV_NAME);
	zte_print_reg(acc);
	return 0;

err_firstread:
	acc->hw_working = 0;
err_unknown_device:
err_resume_state:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%x,0x%x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static void lis3dh_acc_device_power_off(struct lis3dh_acc_data *acc)
{
	int err;
	u8 buf[2] = { CTRL_REG1, LIS3DH_ACC_PM_OFF };

	printk("%s: write reg[0x%X]=0x%X\n", __func__, buf[0], buf[1]);
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed: %d\n", err);

	if (acc->pdata->power_off) {
		if(acc->pdata->gpio_int1 >= 0)
			disable_irq_nosync(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			disable_irq_nosync(acc->irq2);
		acc->pdata->power_off();
		acc->hw_initialized = 0;
	}
	if (acc->hw_initialized) {
		if(acc->pdata->gpio_int1 >= 0)
			disable_irq_nosync(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			disable_irq_nosync(acc->irq2);
		acc->hw_initialized = 0;
	}

}

static int lis3dh_acc_device_power_on(struct lis3dh_acc_data *acc)
{
	int err = -1;

	if (acc->pdata->power_on) {
		err = acc->pdata->power_on();
		if (err < 0) {
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
		if(acc->pdata->gpio_int1 >= 0)
			enable_irq(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			enable_irq(acc->irq2);
	}

	if (!acc->hw_initialized) {
		err = lis3dh_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			lis3dh_acc_device_power_off(acc);
			return err;
		}
	}

	if (acc->hw_initialized) {
		if(acc->pdata->gpio_int1 >= 0)
			enable_irq(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			enable_irq(acc->irq2);
	}
	return 0;
}

static irqreturn_t lis3dh_acc_isr1(int irq, void *dev)
{
	struct lis3dh_acc_data *acc = dev;

	disable_irq_nosync(irq);
	queue_work(acc->irq1_work_queue, &acc->irq1_work);
	printk(KERN_INFO "%s: isr1 queued\n", LIS3DH_ACC_DEV_NAME);

	return IRQ_HANDLED;
}

static irqreturn_t lis3dh_acc_isr2(int irq, void *dev)
{
	struct lis3dh_acc_data *acc = dev;

	disable_irq_nosync(irq);
	queue_work(acc->irq2_work_queue, &acc->irq2_work);
	printk(KERN_INFO "%s: isr2 queued\n", LIS3DH_ACC_DEV_NAME);

	return IRQ_HANDLED;
}

static void lis3dh_acc_irq1_work_func(struct work_struct *work)
{

	struct lis3dh_acc_data *acc =
	container_of(work, struct lis3dh_acc_data, irq1_work);
	/* TODO  add interrupt service procedure.
		 ie:lis3dh_acc_get_int1_source(acc); */
	;
	/*  */
	printk(KERN_INFO "%s: IRQ1 triggered\n", LIS3DH_ACC_DEV_NAME);

	enable_irq(acc->irq1);
}

static void lis3dh_acc_irq2_work_func(struct work_struct *work)
{

	struct lis3dh_acc_data *acc =
	container_of(work, struct lis3dh_acc_data, irq2_work);
	/* TODO  add interrupt service procedure.
		 ie:lis3dh_acc_get_tap_source(acc); */
	;
	/*  */

	printk(KERN_INFO "%s: IRQ2 triggered\n", LIS3DH_ACC_DEV_NAME);

	enable_irq(acc->irq2);
}

static int lis3dh_acc_update_g_range(struct lis3dh_acc_data *acc, u8 new_g_range)
{
	int err=-1;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = LIS3DH_ACC_FS_MASK | HIGH_RESOLUTION;
	printk("%s: +++  with new_g_range=%d\n", __func__, new_g_range);

	switch (new_g_range) {
	case LIS3DH_ACC_G_2G:

		sensitivity = SENSITIVITY_2G;
		break;
	case LIS3DH_ACC_G_4G:

		sensitivity = SENSITIVITY_4G;
		break;
	case LIS3DH_ACC_G_8G:

		sensitivity = SENSITIVITY_8G;
		break;
	case LIS3DH_ACC_G_16G:

		sensitivity = SENSITIVITY_16G;
		break;
	default:
		dev_err(&acc->client->dev, "invalid g range requested: %u\n",
				new_g_range);
		return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Updates configuration register 4,
		* which contains g range setting */
		buf[0] = CTRL_REG4;
		err = lis3dh_acc_i2c_read(acc, buf, 1);
		if (err < 0)
			goto error;
		init_val = buf[0];
		acc->resume_state[RES_CTRL_REG4] = init_val;
		new_val = new_g_range | HIGH_RESOLUTION;
		updated_val = ((mask & new_val) | ((~mask) & init_val));
		buf[1] = updated_val;
		buf[0] = CTRL_REG4;
	    printk("%s: write reg[0x23]=0x%X\n", __func__, buf[1]);
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG4] = updated_val;
		acc->sensitivity = sensitivity;
	}


	return err;
error:
	dev_err(&acc->client->dev, "update g range failed 0x%x,0x%x: %d\n",
			buf[0], buf[1], err);

	return err;
}

static int lis3dh_acc_update_odr(struct lis3dh_acc_data *acc, int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];

	printk("%s: +++  with poll_interval_ms=%d\n", __func__, poll_interval_ms);
	
	/* Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the poll_interval requested by the system.
	 * It must be the longest interval lower then the poll interval.*/
	for (i = ARRAY_SIZE(lis3dh_acc_odr_table) - 1; i >= 0; i--) {
		if ((lis3dh_acc_odr_table[i].cutoff_ms <= poll_interval_ms)
								|| (i == 0))
			break;
	}
	config[1] = lis3dh_acc_odr_table[i].mask;

	config[1] |= LIS3DH_ACC_ENABLE_ALL_AXES;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&acc->enabled)) {
		config[0] = CTRL_REG1;
	    printk("%s: write reg[0x20]=0x%X\n", __func__, config[1]);
		err = lis3dh_acc_i2c_write(acc, config, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG1] = config[1];
	}

	return err;

error:
	dev_err(&acc->client->dev, "update odr failed 0x%x,0x%x: %d\n",
			config[0], config[1], err);

	return err;
}



static int lis3dh_acc_register_write(struct lis3dh_acc_data *acc, u8 *buf,
		u8 reg_address, u8 new_value)
{
	int err = -1;

	/* Sets configuration register at reg_address
	 *  NOTE: this is a straight overwrite  */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			return err;
	return err;
}
/*
static int lis3dh_acc_register_read(struct lis3dh_acc_data *acc, u8 *buf,
		u8 reg_address)
{

	int err = -1;
	buf[0] = (reg_address);
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	return err;
}

static int lis3dh_acc_register_update(struct lis3dh_acc_data *acc, u8 *buf,
		u8 reg_address, u8 mask, u8 new_bit_values)
{
	int err = -1;
	u8 init_val;
	u8 updated_val;
	err = lis3dh_acc_register_read(acc, buf, reg_address);
	if (!(err < 0)) {
		init_val = buf[1];
		updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
		err = lis3dh_acc_register_write(acc, buf, reg_address,
				updated_val);
	}
	return err;
}

*/

static int lis3dh_acc_get_acceleration_data(struct lis3dh_acc_data *acc,
		int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s16 hw_d[3] = { 0 };

	acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
	err = lis3dh_acc_i2c_read(acc, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = (((s16) ((acc_data[1] << 8) | acc_data[0])) >> 4);
	hw_d[1] = (((s16) ((acc_data[3] << 8) | acc_data[2])) >> 4);
	hw_d[2] = (((s16) ((acc_data[5] << 8) | acc_data[4])) >> 4);

	hw_d[0] = hw_d[0] * acc->sensitivity;
	hw_d[1] = hw_d[1] * acc->sensitivity;
	hw_d[2] = hw_d[2] * acc->sensitivity;

#if 0
	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
		   : (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
		   : (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
		   : (hw_d[acc->pdata->axis_map_z]));
#else  
	xyz[0] = ((direction[3]) ? (-hw_d[direction[0]])
		   : (hw_d[direction[0]]));
	xyz[1] = ((direction[4]) ? (-hw_d[direction[1]])
		   : (hw_d[direction[1]]));
	xyz[2] = ((direction[5]) ? (-hw_d[direction[2]])
		   : (hw_d[direction[2]]));  

	//printk(KERN_DEBUG "%d %d %d %d %d %d %d\n", direction[0], direction[1], direction[2],
		//direction[3], direction[4], direction[5]);
#endif

	//#ifdef DEBUG
	//		printk(KERN_INFO "%s read x=%d, y=%d, z=%d\n",
	//		LIS3DH_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);
	//#endif
	return err;
}

static void lis3dh_acc_report_values(struct lis3dh_acc_data *acc,
					int *xyz)
{
#ifdef ZTE_ST_ACCEL_CALI
	xyz[0] -= cali_value[0];
	xyz[1] -= cali_value[1];
	xyz[2] -= cali_value[2];
#endif //xym, add for calibration

	input_report_abs(acc->input_dev, ABS_X, xyz[0]);
	input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
	input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
	input_sync(acc->input_dev);
}

static int lis3dh_acc_enable(struct lis3dh_acc_data *acc)
{
	int err;
	printk("%s: \n", __func__);
	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {
		err = lis3dh_acc_device_power_on(acc);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}
		schedule_delayed_work(&acc->input_work,
			msecs_to_jiffies(acc->pdata->poll_interval));
	}

	return 0;
}

static int lis3dh_acc_disable(struct lis3dh_acc_data *acc)
{
	printk("%s: \n", __func__);
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		cancel_delayed_work_sync(&acc->input_work);
		lis3dh_acc_device_power_off(acc);
	}

	return 0;
}


static ssize_t read_single_reg(struct device *dev, char *buf, u8 reg)
{
	ssize_t ret;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int err;

	u8 data = reg;
	err = lis3dh_acc_i2c_read(acc, &data, 1);
	if (err < 0)
		return err;
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;

}

static int write_reg(struct device *dev, const char *buf, u8 reg,
		u8 mask, int resumeIndex)
{
	int err = -1;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	u8 new_val;
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;

	new_val=((u8) val & mask);
	x[0] = reg;
	x[1] = new_val;
	err = lis3dh_acc_register_write(acc, x,reg,new_val);
	if (err < 0)
		return err;
	acc->resume_state[resumeIndex] = new_val;
	return err;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int val;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	mutex_lock(&acc->lock);
	val = acc->pdata->poll_interval;
	mutex_unlock(&acc->lock);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	interval_ms = max((unsigned int)interval_ms,acc->pdata->min_interval);
	mutex_lock(&acc->lock);
	acc->pdata->poll_interval = interval_ms;
	lis3dh_acc_update_odr(acc, interval_ms);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_range(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	char val;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	char range = 2;
	mutex_lock(&acc->lock);
	val = acc->pdata->g_range ;
	switch (val) {
	case LIS3DH_ACC_G_2G:
		range = 2;
		break;
	case LIS3DH_ACC_G_4G:
		range = 4;
		break;
	case LIS3DH_ACC_G_8G:
		range = 8;
		break;
	case LIS3DH_ACC_G_16G:
		range = 16;
		break;
	}
	mutex_unlock(&acc->lock);
	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->pdata->g_range = val;
	lis3dh_acc_update_g_range(acc, val);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_direct(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int direct_value = direction_type;
	
	return sprintf(buf, "%d\n", direct_value);
}

static ssize_t attr_set_direct(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	switch (val) {
		case 0:
			direction[0] = lis3dh_acc_direction[0][0];
			direction[1] = lis3dh_acc_direction[0][1];
			direction[2] = lis3dh_acc_direction[0][2];
			direction[3] = lis3dh_acc_direction[0][3];
			direction[4] = lis3dh_acc_direction[0][4];
			direction[5] = lis3dh_acc_direction[0][5];
			direction_type = 0;
			break;
		case 1:
			direction[0] = lis3dh_acc_direction[1][0];
			direction[1] = lis3dh_acc_direction[1][1];
			direction[2] = lis3dh_acc_direction[1][2];
			direction[3] = lis3dh_acc_direction[1][3];
			direction[4] = lis3dh_acc_direction[1][4];
			direction[5] = lis3dh_acc_direction[1][5];
			direction_type = 1;
			break;
		case 2:
			direction[0] = lis3dh_acc_direction[2][0];
			direction[1] = lis3dh_acc_direction[2][1];
			direction[2] = lis3dh_acc_direction[2][2];
			direction[3] = lis3dh_acc_direction[2][3];
			direction[4] = lis3dh_acc_direction[2][4];
			direction[5] = lis3dh_acc_direction[2][5];
			direction_type = 2;
			break;
		case 3:
			direction[0] = lis3dh_acc_direction[3][0];
			direction[1] = lis3dh_acc_direction[3][1];
			direction[2] = lis3dh_acc_direction[3][2];
			direction[3] = lis3dh_acc_direction[3][3];
			direction[4] = lis3dh_acc_direction[3][4];
			direction[5] = lis3dh_acc_direction[3][5];
			direction_type = 3;
			break;
		case 4:
			direction[0] = lis3dh_acc_direction[4][0];
			direction[1] = lis3dh_acc_direction[4][1];
			direction[2] = lis3dh_acc_direction[4][2];
			direction[3] = lis3dh_acc_direction[4][3];
			direction[4] = lis3dh_acc_direction[4][4];
			direction[5] = lis3dh_acc_direction[4][5];
			direction_type = 4;
			break;
		case 5:
			direction[0] = lis3dh_acc_direction[5][0];
			direction[1] = lis3dh_acc_direction[5][1];
			direction[2] = lis3dh_acc_direction[5][2];
			direction[3] = lis3dh_acc_direction[5][3];
			direction[4] = lis3dh_acc_direction[5][4];
			direction[5] = lis3dh_acc_direction[5][5];
			direction_type = 5;
			break;
		case 6:
			direction[0] = lis3dh_acc_direction[6][0];
			direction[1] = lis3dh_acc_direction[6][1];
			direction[2] = lis3dh_acc_direction[6][2];
			direction[3] = lis3dh_acc_direction[6][3];
			direction[4] = lis3dh_acc_direction[6][4];
			direction[5] = lis3dh_acc_direction[6][5];
			direction_type = 6;
			break;
		case 7:
			direction[0] = lis3dh_acc_direction[7][0];
			direction[1] = lis3dh_acc_direction[7][1];
			direction[2] = lis3dh_acc_direction[7][2];
			direction[3] = lis3dh_acc_direction[7][3];
			direction[4] = lis3dh_acc_direction[7][4];
			direction[5] = lis3dh_acc_direction[7][5];
			direction_type = 7;
			break;			
	}
	mutex_unlock(&acc->lock);

	return size;
}
#ifdef ZTE_ST_ACCEL_CALI	//xym
static ssize_t attr_get_cali(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	//struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	//int val = atomic_read(&acc->cali);

	return sprintf(buf, "%d %d %d\n", cali_value[0], cali_value[1], cali_value[2]);
}

static ssize_t attr_set_cali(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	int i;
	int xyz[3] = {0};
	int temp_xyz[3] = {0};

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val) 
	{
		atomic_set(&acc->cali, 1);
		printk(KERN_INFO "%s %s: calibrate begin\n", LIS3DH_ACC_DEV_NAME, __func__);
		
		for( i = 0; i < CALI_TIMES; i++)
		{
			msleep(50); //xym, add for calibration
			
			//mutex_lock(&acc->lock);
  			lis3dh_acc_get_acceleration_data(acc, xyz);
			//mutex_unlock(&acc->lock); 
			temp_xyz[0]+= xyz[0];
			temp_xyz[1]+= xyz[1];
			temp_xyz[2]+= xyz[2];
			
		   printk(KERN_INFO "%s %s: xyz = %d, %d, %d\n", 
						LIS3DH_ACC_DEV_NAME, __func__, xyz[0], xyz[1], xyz[2]);			
			
		   printk(KERN_INFO "%s %s: temp_xyz = %d, %d, %d\n", 
						LIS3DH_ACC_DEV_NAME, __func__, temp_xyz[0], temp_xyz[1], temp_xyz[2]);			
			 
		}
		
		cali_value[0] = temp_xyz[0]/CALI_TIMES - 0;
		cali_value[1] = temp_xyz[1]/CALI_TIMES - 0;
		cali_value[2] = ((temp_xyz[2]/CALI_TIMES) - 1024);//xym

		printk(KERN_INFO "%s %s: calibrate end, cali_value = %d, %d, %d\n", LIS3DH_ACC_DEV_NAME, __func__, 
		                cali_value[0],	cali_value[1], cali_value[2]);
	}
	else
	{
		atomic_set(&acc->cali, 0);
		cali_value[0] = 0;
		cali_value[1] = 0;
		cali_value[2] = 0;
	}
	
	return size;
}

//xym, add for calibration, begin
static ssize_t attr_get_cali_data(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, " %d %d %d \n", cali_value[0], cali_value[1], cali_value[2]);
}

static ssize_t attr_set_cali_data(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
    char valuestr[5];
	int nvalue = 0;
	int countxyz = 0; 
	long val = 0;
	
    while ((*buf != '\0')&&(*buf != '\n')&&(countxyz<=2)) 
	{
		nvalue = 0;
		
		while((*buf != ' ')&&(*buf != '\0')&&(*buf != '\n'&&(countxyz<=2))) 
		{
			valuestr[nvalue++] = *buf++;
		}
		valuestr[nvalue] = '\0';
		if( strict_strtol(valuestr, 10, &val))
			return -EINVAL;
		
		cali_value[countxyz] = (int)val;
		countxyz++;
		buf++;
    }
	return size;
}
//xym, add for calibration, end

#endif

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int val = atomic_read(&acc->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lis3dh_acc_enable(acc);
	else
		lis3dh_acc_disable(acc);

	return size;
}

static ssize_t attr_set_intconfig1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,INT_CFG1,NO_MASK,RES_INT_CFG1);
}

static ssize_t attr_get_intconfig1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,INT_CFG1);
}

static ssize_t attr_set_duration1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,INT_DUR1,INT1_DURATION_MASK,RES_INT_DUR1);
}

static ssize_t attr_get_duration1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,INT_DUR1);
}

static ssize_t attr_set_thresh1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,INT_THS1,INT1_THRESHOLD_MASK,RES_INT_THS1);
}

static ssize_t attr_get_thresh1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,INT_THS1);
}

static ssize_t attr_get_source1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return read_single_reg(dev,buf,INT_SRC1);
}

static ssize_t attr_set_click_cfg(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_CFG,TAP_CFG_MASK,RES_TT_CFG);
}

static ssize_t attr_get_click_cfg(struct device *dev,
		struct device_attribute *attr,	char *buf)
{

	return read_single_reg(dev,buf,TT_CFG);
}

static ssize_t attr_get_click_source(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_SRC);
}

static ssize_t attr_set_click_ths(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_THS,TAP_THS_MASK,RES_TT_THS);
}

static ssize_t attr_get_click_ths(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_THS);
}

static ssize_t attr_set_click_tlim(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_LIM,TAP_TLIM_MASK,RES_TT_LIM);
}

static ssize_t attr_get_click_tlim(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_LIM);
}

static ssize_t attr_set_click_tlat(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_TLAT,TAP_TLAT_MASK,RES_TT_TLAT);
}

static ssize_t attr_get_click_tlat(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_TLAT);
}

static ssize_t attr_set_click_tw(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev,buf,TT_TLAT,TAP_TW_MASK,RES_TT_TLAT);
}

static ssize_t attr_get_click_tw(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev,buf,TT_TLAT);
}

static ssize_t attr_get_sensor_data(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
  int xyz[3] = { 0 };

	//mutex_lock(&acc->lock);
  lis3dh_acc_get_acceleration_data(acc, xyz);
	//mutex_unlock(&acc->lock);
	return sprintf(buf, "%d %d %d\n", xyz[0], xyz[1], xyz[2]);
}
#ifdef DEBUG
/* PAY ATTENTION: These DEBUG funtions don't manage resume_state */
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int rc;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	x[0] = acc->reg_addr;
	mutex_unlock(&acc->lock);
	x[1] = val;
	rc = lis3dh_acc_i2c_write(acc, x, 1);
	/*TODO: error need to be managed */
	return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int rc;
	u8 data;

	mutex_lock(&acc->lock);
	data = acc->reg_addr;
	mutex_unlock(&acc->lock);
	rc = lis3dh_acc_i2c_read(acc, &data, 1);
	/*TODO: error need to be managed */
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->reg_addr = val;
	mutex_unlock(&acc->lock);
	return size;
}
#endif

static struct device_attribute attributes[] = {
  	__ATTR(data, 0600, attr_get_sensor_data, NULL),
	__ATTR(delay, 0600, attr_get_polling_rate, attr_set_polling_rate),
	__ATTR(range, 0600, attr_get_range, attr_set_range),
	__ATTR(direct, 0600, attr_get_direct, attr_set_direct),
	__ATTR(cali, 0600, attr_get_cali, attr_set_cali),
	__ATTR(enable, 0600, attr_get_enable, attr_set_enable),
	__ATTR(int1_config, 0600, attr_get_intconfig1, attr_set_intconfig1),
	__ATTR(int1_duration, 0600, attr_get_duration1, attr_set_duration1),
	__ATTR(int1_threshold, 0600, attr_get_thresh1, attr_set_thresh1),
	__ATTR(int1_source, 0400, attr_get_source1, NULL),
	__ATTR(click_config, 0600, attr_get_click_cfg, attr_set_click_cfg),
	__ATTR(click_source, 0400, attr_get_click_source, NULL),
	__ATTR(click_threshold, 0600, attr_get_click_ths, attr_set_click_ths),
	__ATTR(click_timelimit, 0600, attr_get_click_tlim, attr_set_click_tlim),
	__ATTR(click_timelatency, 0600, attr_get_click_tlat,
							attr_set_click_tlat),
	__ATTR(click_timewindow, 0600, attr_get_click_tw, attr_set_click_tw),
#ifdef ZTE_ST_ACCEL_CALI	
	__ATTR(cali_data, 0600, attr_get_cali_data,  attr_set_cali_data),	//xym, add for calibration
#endif
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
	for ( ; i >= 0; i--)
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

static void remove_procfs_interfaces(void)
{
	//remove_proc_entry(PROC_SENSOR_ADDR, proc_dir);
	remove_proc_entry(PROC_SENSOR_NAME, proc_dir);
	remove_proc_entry(PROC_SENSOR_DIR, NULL);
}

static void lis3dh_acc_input_work_func(struct work_struct *work)
{
	struct lis3dh_acc_data *acc;

	int xyz[3] = { 0 };
	int err;

	acc = container_of((struct delayed_work *)work,
			struct lis3dh_acc_data,	input_work);

	mutex_lock(&acc->lock);
	err = lis3dh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lis3dh_acc_report_values(acc, xyz);

	schedule_delayed_work(&acc->input_work, msecs_to_jiffies(
			acc->pdata->poll_interval));
	mutex_unlock(&acc->lock);
}

int lis3dh_acc_input_open(struct input_dev *input)
{
	struct lis3dh_acc_data *acc = input_get_drvdata(input);

	return lis3dh_acc_enable(acc);
}

void lis3dh_acc_input_close(struct input_dev *dev)
{
	struct lis3dh_acc_data *acc = input_get_drvdata(dev);

	lis3dh_acc_disable(acc);
}

static int lis3dh_acc_validate_pdata(struct lis3dh_acc_data *acc)
{
	/* checks for correctness of minimal polling period */
	acc->pdata->min_interval =
		max((unsigned int)LIS3DLH_ACC_MIN_POLL_PERIOD_MS,
						acc->pdata->min_interval);

  acc->pdata->poll_interval = LIS3DH_ACC_DEFAULT_POLL_INTERVAL;

	acc->pdata->poll_interval = max(acc->pdata->poll_interval,
			acc->pdata->min_interval);

	if (acc->pdata->axis_map_x > 2 ||
		acc->pdata->axis_map_y > 2 ||
		 acc->pdata->axis_map_z > 2) {
		dev_err(&acc->client->dev, "invalid axis_map value "
			"x:%u y:%u z%u\n", acc->pdata->axis_map_x,
				acc->pdata->axis_map_y, acc->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1
			|| acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev, "invalid negate value "
			"x:%u y:%u z:%u\n", acc->pdata->negate_x,
				acc->pdata->negate_y, acc->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lis3dh_acc_input_init(struct lis3dh_acc_data *acc)
{
	int err;

	INIT_DELAYED_WORK(&acc->input_work, lis3dh_acc_input_work_func);
	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocation failed\n");
		goto err0;
	}

	acc->input_dev->open = lis3dh_acc_input_open;
	acc->input_dev->close = lis3dh_acc_input_close;
	acc->input_dev->name = LIS3DH_ACC_DEV_NAME;
	//acc->input_dev->name = "accelerometer";
	acc->input_dev->id.bustype = BUS_I2C;
	acc->input_dev->dev.parent = &acc->client->dev;

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	/*	next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, acc->input_dev->absbit);
	/*	next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, acc->input_dev->absbit);

	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);
	/*	next is used for interruptA sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/*	next is used for interruptB sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_WHEEL, INT_MIN, INT_MAX, 0, 0);


	err = input_register_device(acc->input_dev);
	if (err) {
		dev_err(&acc->client->dev,
				"unable to register input device %s\n",
				acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void lis3dh_acc_input_cleanup(struct lis3dh_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

#ifdef CONFIG_OF
static int lis3dh_acc_parse_dt(struct device *dev, struct lis3dh_acc_platform_data *pdata)
{
        pdata->poll_interval = 50;          //Driver polling interval as 50ms
        pdata->min_interval = 10;             //Driver polling interval minimum 10ms
        pdata->g_range = LIS3DH_ACC_G_2G;      //Full Scale of LSM303DLH Accelerometer
        pdata->axis_map_x = 1;               //x = x
        pdata->axis_map_y = 0;               //y = y
        pdata->axis_map_z = 2;                //z = z
        pdata->negate_x = 0;                  //x = +x
        pdata->negate_y = 1;                  //y = +y
        pdata->negate_z = 0;                 //z = +z
        
        pdata->gpio_int1 = -1;
	pdata->gpio_int2 = -1;

	if (dev->of_node) {
		pdata->gpio_int1 = of_get_named_gpio(dev->of_node, "lis3dh_acc,gpio_int1", 0);
		pdata->gpio_int2 = of_get_named_gpio(dev->of_node, "lis3dh_acc,gpio_int2", 0);
	}

	pdata->vdd = "8110_l19";
	pdata->vbus = "8110_l14";
	
	return 0;
}
#endif



static int lis3dh_config_irq_gpio(int irq_gpio) //xym add 
{
    int rc=0;
    uint32_t  gpio_config_data = GPIO_CFG(irq_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);

#ifdef CONFIG_OF
	if (!(rc = gpio_is_valid(irq_gpio))) {
	    pr_err("%s: irq gpio not provided.\n",  __func__);
		return rc;
    }	
#endif

    rc = gpio_tlmm_config(gpio_config_data, GPIO_CFG_ENABLE);
    if (rc) {
        pr_err("%s: gpio_tlmm_config(%#x)=%d\n", __func__, gpio_config_data, rc);
        return rc;
    }

    msleep(1);

    rc = gpio_direction_input(irq_gpio);
    if (rc) {
        pr_err("%s: gpio_direction_input(%#x)=%d\n",__func__, irq_gpio, rc);
        return rc;
    }

    return 0;
}


static int lis3dh_gpio_init(struct device *dev, int flag, 
	                                          char *vreg_vdd, char *vreg_vbus, 
	                                          int irq_gpio1, int irq_gpio2)//xym
{
	int ret = -EINVAL;
	pr_info("%s: init flag=%d\n", __func__, flag);

	//init
	if ( flag == 1 )
	{
		vdd = vbus = NULL;

		vdd = regulator_get(dev, vreg_vdd);
		if (!vdd) {
			pr_err("%s get failed\n", vreg_vdd);
			return -1;
		}
		if ( regulator_set_voltage(vdd, 2850000,2850000) ){   //3000000,3000000
			pr_err("%s set failed\n", vreg_vdd);
			return -1;
		}

		vbus = regulator_get(dev, vreg_vbus);
		if (!vbus) {
			pr_err("%s get failed\n", vreg_vbus);
			return -1;
		}

		if ( regulator_set_voltage(vbus, 1800000,1800000)) {
			pr_err(" %s set failed\n", vreg_vbus);
			return -1;
		}

		if(irq_gpio1>=0)
		{
			ret = gpio_request(irq_gpio1, "lis3dh_int_gpio1");
			if (ret){
				pr_err("gpio %d request is error!\n", irq_gpio1);
				return -2;
			}
			if ( lis3dh_config_irq_gpio(irq_gpio1)) {
				pr_err(" gpio %d config failed\n", irq_gpio1);
				return -2;
			}
		}
		if(irq_gpio2>=0)
		{
			ret = gpio_request(irq_gpio2, "lis3dh_int_gpio2");
			if (ret){
				pr_err("gpio %d request is error!\n", irq_gpio2);
				return -2;
			}
			if ( lis3dh_config_irq_gpio(irq_gpio2)) {
				pr_err(" gpio %d config failed\n", irq_gpio2);
				return -2;
			}						
		}
	}

	//deinit
	if ( flag == 0)
	{
		regulator_put(vdd);
		regulator_put(vbus);
		if(irq_gpio1>=0)
		{
			gpio_free(irq_gpio1);
		}
		if(irq_gpio2>=0)
		{		
			gpio_free(irq_gpio2);
		}
	}

	return 0;
}



static int lis3dh_power( int on )//xym
{
	int rc = -EINVAL;
	pr_info("%s: on=%d\n", __func__, on);
	
	if ( !vdd || !vbus )
		return rc;
	
	if (on){
		rc = regulator_enable(vdd);
		if (rc) {
			pr_err("vdd enable failed\n");
			return rc;
		}
		rc = regulator_enable(vbus);
		if (rc) {
			pr_err("vbus enable failed\n");
			return rc;
		}
	}
	else 
	{
		rc = regulator_disable(vdd);
		if (rc) {
			pr_err("vdd disable failed\n");
			return rc;
		}
		rc = regulator_disable(vbus);
		if (rc) {
			pr_err("vbus disable failed\n");
			return rc;
		}
	}

	return 0;
}

extern int (*gsensor_suspend_pm)(void);
extern int (*gsensor_resume_pm)(void);

struct lis3dh_acc_data * lis3dh_data;

int lis3dh_suspend_pm(void)
{
	printk("%s:\n", __func__);  

	lis3dh_data->on_before_suspend = atomic_read(&lis3dh_data->enabled);
	lis3dh_acc_disable(lis3dh_data);

	printk("%s: lis3dh_data->enabled = %d, lis3dh_data->on_before_suspend = %d \n", __func__,
	           atomic_read(&lis3dh_data->enabled), lis3dh_data->on_before_suspend);	
	return 0;

}

int lis3dh_resume_pm(void)
{
	printk("%s:\n", __func__);  
	
	if (lis3dh_data->on_before_suspend)
		return lis3dh_acc_enable(lis3dh_data);
	
	printk("%s: lis3dh_data->enabled = %d, lis3dh_data->on_before_suspend = %d \n", __func__,
	           atomic_read(&lis3dh_data->enabled), lis3dh_data->on_before_suspend);	

	return 0;
}

static int lis3dh_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	struct lis3dh_acc_data *acc;

	int err = -1;

	pr_info("%s: probe start.\n", __func__);

/*if(client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}*/

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}
	

	/*
	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_BYTE |
					I2C_FUNC_SMBUS_BYTE_DATA |
					I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "client not smb-i2c capable:2\n");
		err = -EIO;
		goto exit_check_functionality_failed;
	}


	if (!i2c_check_functionality(client->adapter,
						I2C_FUNC_SMBUS_I2C_BLOCK)){
		dev_err(&client->dev, "client not smb-i2c capable:3\n");
		err = -EIO;
		goto exit_check_functionality_failed;
	}
	*/

	acc = kzalloc(sizeof(struct lis3dh_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_check_functionality_failed;
	}	

	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);

	acc->client = client;
	i2c_set_clientdata(client, acc);

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_mutexunlock;
	}

	memset(acc->pdata, 0, sizeof(*acc->pdata));
	 
#ifdef CONFIG_OF
	lis3dh_acc_parse_dt(&client->dev, acc->pdata);
#endif

	err = lis3dh_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err_pdata_init;
		}
	}

	if(acc->pdata->gpio_int1 >= 0){
		acc->irq1 = gpio_to_irq(acc->pdata->gpio_int1);
		printk(KERN_INFO "%s: %s has set irq1 to irq: %d "
							"mapped on gpio:%d\n",
			LIS3DH_ACC_DEV_NAME, __func__, acc->irq1,
							acc->pdata->gpio_int1);
	}

	if(acc->pdata->gpio_int2 >= 0){
		acc->irq2 = gpio_to_irq(acc->pdata->gpio_int2);
		printk(KERN_INFO "%s: %s has set irq2 to irq: %d "
							"mapped on gpio:%d\n",
			LIS3DH_ACC_DEV_NAME, __func__, acc->irq2,
							acc->pdata->gpio_int2);
	}

	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));

	acc->resume_state[RES_CTRL_REG1] = LIS3DH_ACC_ENABLE_ALL_AXES;
	acc->resume_state[RES_CTRL_REG2] = 0x00;
	acc->resume_state[RES_CTRL_REG3] = 0x00;
	acc->resume_state[RES_CTRL_REG4] = 0x00;
	acc->resume_state[RES_CTRL_REG5] = 0x00;
	acc->resume_state[RES_CTRL_REG6] = 0x00;

	acc->resume_state[RES_TEMP_CFG_REG] = 0x00;
	acc->resume_state[RES_FIFO_CTRL_REG] = 0x00;
	acc->resume_state[RES_INT_CFG1] = 0x00;
	acc->resume_state[RES_INT_THS1] = 0x00;
	acc->resume_state[RES_INT_DUR1] = 0x00;

	acc->resume_state[RES_TT_CFG] = 0x00;
	acc->resume_state[RES_TT_THS] = 0x00;
	acc->resume_state[RES_TT_LIM] = 0x00;
	acc->resume_state[RES_TT_TLAT] = 0x00;
	acc->resume_state[RES_TT_TW] = 0x00;

	err = lis3dh_gpio_init(&client->dev, 1, acc->pdata->vdd, acc->pdata->vbus, acc->pdata->gpio_int1, acc->pdata->gpio_int2);//xym
	if ( err < -1 ){
		pr_err("%s, gpio init failed! err = %d\n", __func__, err);
		goto err_gpio_init;
	}
	else 	if ( err < 0 ){
		pr_err("%s, gpio init failed! err = %d\n", __func__, err);
		goto err_pdata_init;
	}
	
	err = lis3dh_power(1);//xym
	if (err != 0)
		goto err_gpio_init;
	msleep(5);//xym 20131204

	err = lis3dh_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_power;
	}

	atomic_set(&acc->enabled, 1);

	err = lis3dh_acc_update_g_range(acc, acc->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto  err_power_off;
	}

	err = lis3dh_acc_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_power_off;
	}

	err = lis3dh_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}

	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
		   "device LIS3DH_ACC_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
	}

	proc_dir = proc_mkdir(PROC_SENSOR_DIR, NULL);
	if (proc_dir < 0 ) {
		dev_err(&client->dev,
		   "device LIS3DH_ACC_DEV_NAME proc dir register failed\n");
		goto err_input_cleanup;
	}

	proc_file1 = create_proc_read_entry(PROC_SENSOR_NAME, 0444, proc_dir,
			       lis3dh_acc_read_proc1, NULL);
	if (proc_file1 < 0 ) {
		dev_err(&client->dev,
		   "device LIS3DH_ACC_DEV_NAME proc file1 register failed\n");
		goto err_input_cleanup;
	}

	proc_client = acc->client;

#if 0
	proc_file2 = create_proc_read_entry(PROC_SENSOR_ADDR, 0444, proc_dir, lis3dh_acc_read_proc2, NULL);

	if (proc_file2 < 0 ) {
		dev_err(&client->dev,
		   "device LIS3DH_ACC_DEV_NAME proc file2 register failed\n");
		goto err_input_cleanup;
	}
#endif

	lis3dh_acc_device_power_off(acc);

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);
	atomic_set(&acc->cali, 0);

	if(acc->pdata->gpio_int1 >= 0){
		INIT_WORK(&acc->irq1_work, lis3dh_acc_irq1_work_func);
		acc->irq1_work_queue =
			create_singlethread_workqueue("lis3dh_acc_wq1");
		if (!acc->irq1_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev,
					"cannot create work queue1: %d\n", err);
			goto err_remove_sysfs_int;
		}
		err = request_irq(acc->irq1, lis3dh_acc_isr1,
				IRQF_TRIGGER_RISING, "lis3dh_acc_irq1", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq1 failed: %d\n", err);
			goto err_destoyworkqueue1;
		}
		disable_irq_nosync(acc->irq1);
	}

	if(acc->pdata->gpio_int2 >= 0){
		INIT_WORK(&acc->irq2_work, lis3dh_acc_irq2_work_func);
		acc->irq2_work_queue =
			create_singlethread_workqueue("lis3dh_acc_wq2");
		if (!acc->irq2_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev,
					"cannot create work queue2: %d\n", err);
			goto err_free_irq1;
		}
		err = request_irq(acc->irq2, lis3dh_acc_isr2,
				IRQF_TRIGGER_RISING, "lis3dh_acc_irq2", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq2 failed: %d\n", err);
			goto err_destoyworkqueue2;
		}
		disable_irq_nosync(acc->irq2);
	}

	mutex_unlock(&acc->lock);

	lis3dh_data=acc;
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	acc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	acc->early_suspend.suspend = lis3dh_acc_early_suspend;
	acc->early_suspend.resume = lis3dh_acc_late_resume;
	register_early_suspend(&acc->early_suspend);
#endif
	gsensor_suspend_pm = lis3dh_suspend_pm;
	gsensor_resume_pm = lis3dh_resume_pm;	

	dev_info(&client->dev, "%s: probe exit\n", __func__);

	return 0;

err_destoyworkqueue2:
	if(acc->pdata->gpio_int2 >= 0)
		destroy_workqueue(acc->irq2_work_queue);
err_free_irq1:
	free_irq(acc->irq1, acc);
err_destoyworkqueue1:
	if(acc->pdata->gpio_int1 >= 0)
		destroy_workqueue(acc->irq1_work_queue);
err_remove_sysfs_int:
	remove_sysfs_interfaces(&client->dev);
	remove_procfs_interfaces();
err_input_cleanup:
	lis3dh_acc_input_cleanup(acc);
err_power_off:
	lis3dh_acc_device_power_off(acc);
err_power:
	lis3dh_power(0);//xym
err_gpio_init:
	lis3dh_gpio_init(&client->dev, 0, acc->pdata->vdd, acc->pdata->vbus, acc->pdata->gpio_int1, acc->pdata->gpio_int2);//xym	
err_pdata_init:
	if (acc->pdata->exit)
		acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_mutexunlock:
	mutex_unlock(&acc->lock);
//err_freedata:
	kfree(acc);
exit_check_functionality_failed:
	pr_info("%s: probe exit...err=%d\n", __func__, err);
	return err;
}

static int __devexit lis3dh_acc_remove(struct i2c_client *client)
{

	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	if(acc->pdata->gpio_int1 >= 0){
		free_irq(acc->irq1, acc);
		gpio_free(acc->pdata->gpio_int1);
		destroy_workqueue(acc->irq1_work_queue);
	}

	if(acc->pdata->gpio_int2 >= 0){
		free_irq(acc->irq2, acc);
		gpio_free(acc->pdata->gpio_int2);
		destroy_workqueue(acc->irq2_work_queue);
	}

	lis3dh_acc_input_cleanup(acc);
	lis3dh_acc_device_power_off(acc);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&acc->early_suspend);
#endif
	remove_sysfs_interfaces(&client->dev);
	remove_procfs_interfaces();


	if (acc->pdata->exit)
		acc->pdata->exit();
	kfree(acc->pdata);
	kfree(acc);

	return 0;
}

//#ifdef CONFIG_PM
/*(static int lis3dh_acc_resume(struct i2c_client *client)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);
  printk(KERN_INFO "%s accelerometer driver: lis3dh_acc_resume\n",
						LIS3DH_ACC_DEV_NAME);
	if (acc->on_before_suspend)
		return lis3dh_acc_enable(acc);
	return 0;
}

static int lis3dh_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);
   printk(KERN_INFO "%s accelerometer driver: lis3dh_acc_suspend\n",
						LIS3DH_ACC_DEV_NAME);
	acc->on_before_suspend = atomic_read(&acc->enabled);
	return lis3dh_acc_disable(acc);
}
*/
//#else
//#define lis3dh_acc_suspend	NULL
//#define lis3dh_acc_resume	NULL
//#endif /* CONFIG_PM */

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lis3dh_acc_early_suspend(struct early_suspend *h)
{
	struct lis3dh_acc_data *acc;
	acc = container_of(h, struct lis3dh_acc_data, early_suspend);
  printk(KERN_INFO "%s accelerometer driver: lis3dh_acc_early_suspend\n",
						LIS3DH_ACC_DEV_NAME);
	lis3dh_acc_suspend(acc->client, PMSG_SUSPEND);
}

static void lis3dh_acc_late_resume(struct early_suspend *h)
{
	struct lis3dh_acc_data *acc;
	acc = container_of(h, struct lis3dh_acc_data, early_suspend);
    printk(KERN_INFO "%s accelerometer driver: lis3dh_acc_late_resume\n",
						LIS3DH_ACC_DEV_NAME);
	lis3dh_acc_resume(acc->client);
}
#endif

#ifdef CONFIG_OF
static struct of_device_id lis3dh_acc_match_table[] = {
	{ .compatible = "lis3dh_acc,1234",},
	{ },
};
#endif

static const struct i2c_device_id lis3dh_acc_id[]
		= { { LIS3DH_ACC_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, lis3dh_acc_id);

static struct i2c_driver lis3dh_acc_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LIS3DH_ACC_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = lis3dh_acc_match_table,
#endif			
	},
	.probe = lis3dh_acc_probe,
	.remove = __devexit_p(lis3dh_acc_remove),
/*#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = lis3dh_acc_suspend,
	.resume = lis3dh_acc_resume,
#endif*/
	.id_table = lis3dh_acc_id,
};
int socinfo_get_ftm_flag(void);
static int __init lis3dh_acc_init(void)
{
	printk(KERN_INFO "%s accelerometer driver: init\n",
						LIS3DH_ACC_DEV_NAME);
	#ifdef CONFIG_ZTE_MANUFACTURING_VERSION
	if (socinfo_get_ftm_flag())
		return i2c_add_driver_async(&lis3dh_acc_driver);		
	else
        #endif
		return i2c_add_driver(&lis3dh_acc_driver);
}

static void __exit lis3dh_acc_exit(void)
{
#ifdef DEBUG
	printk(KERN_INFO "%s accelerometer driver exit\n",
						LIS3DH_ACC_DEV_NAME);
#endif /* DEBUG */
	i2c_del_driver(&lis3dh_acc_driver);

	return;
}

module_init(lis3dh_acc_init);
module_exit(lis3dh_acc_exit);

MODULE_DESCRIPTION("lis3dh digital accelerometer sysfs driver");
MODULE_AUTHOR("Matteo Dameno, Carmine Iascone, STMicroelectronics");
MODULE_LICENSE("GPL");

