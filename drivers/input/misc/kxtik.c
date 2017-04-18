/* drivers/input/misc/kxtik.c - KXTIK accelerometer driver
 *
 * Copyright (C) 2012 Kionix, Inc.
 * Written by Kuching Tan <kuchingtan@kionix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kxtik.h>
#include <linux/version.h>
#ifdef    CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>//xym

#define ZTE_K_ACCEL_CALI //xym

#define NAME			"kxtik"
#define G_MAX			8096
/* OUTPUT REGISTERS */
#define XOUT_L			0x06
#define WHO_AM_I		0x0F
/* CONTROL REGISTERS */
#define INT_REL			0x1A
#define CTRL_REG1		0x1B
#define INT_CTRL1		0x1E
#define DATA_CTRL		0x21
/* CONTROL REGISTER 1 BITS */
#define PC1_OFF			0x7F
#define PC1_ON			(1 << 7)
/* Data ready funtion enable bit: set during probe if using irq mode */
#define DRDYE			(1 << 5)
/* INTERRUPT CONTROL REGISTER 1 BITS */
/* Set these during probe if using irq mode */
#define KXTIK_IEL		(1 << 3)
#define KXTIK_IEA		(1 << 4)
#define KXTIK_IEN		(1 << 5)
/* INPUT_ABS CONSTANTS */
#define FUZZ			3
#define FLAT			3

#define KXTIK_DBG	0
/* Earlysuspend Contants */
#define KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT	5000	/* Timeout (miliseconds) */

/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate (ODR). Adjust by commenting off the ODR entry
 * that you want to omit.
 */
static const struct {
	unsigned int cutoff;
	u8 mask;
} kxtik_odr_table[] = {
/*	{ 3,	ODR800F },
	{ 5,	ODR400F }, */
	{ 10,	ODR200F },
	{ 20,	ODR100F },
	{ 40,	ODR50F  },
	{ 80,	ODR25F  },
	{ 0,	ODR12_5F},
};

struct kxtik_data {
	struct i2c_client *client;
	struct kxtik_platform_data pdata;
	struct input_dev *input_dev;
	struct work_struct irq_work;
	struct workqueue_struct *irq_workqueue;
	unsigned int poll_interval;
	unsigned int poll_delay;
	u8 shift;
	u8 ctrl_reg1;
	u8 data_ctrl;
	u8 int_ctrl;
	atomic_t acc_enabled;
	atomic_t acc_input_event;
#ifdef ZTE_K_ACCEL_CALI
	atomic_t cali;//xym, add for calibrate
#endif	
	wait_queue_head_t wqh_suspend;
	
	atomic_t accel_suspended;
	atomic_t accel_suspend_continue;
	atomic_t accel_enable_resume;
	spinlock_t		lock;

#ifdef    CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif /* CONFIG_HAS_EARLYSUSPEND */
};

static struct regulator *vdd, *vbus;//xym
#ifdef ZTE_K_ACCEL_CALI
static int cali_value[3] = {0,0,0};//xym for calibration
#define CALI_TIMES 100  //xym for calibration
#endif

static int kxtik_i2c_read(struct kxtik_data *tik, u8 addr, u8 *data, int len)
{
	struct i2c_msg msgs[] = {
		{
			.addr = tik->client->addr,
			.flags = tik->client->flags,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = tik->client->addr,
			.flags = tik->client->flags | I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	return i2c_transfer(tik->client->adapter, msgs, 2);
}

static int kxtik_report_regs(struct kxtik_data *tik)
{
	#if KXTIK_DBG
	int err;
	
	err = i2c_smbus_read_byte_data(tik->client, CTRL_REG1);
		if (err < 0)
			return err;
	printk(">>>>>ctrl_reg1 = %x.\n", err);
	
	err = i2c_smbus_read_byte_data(tik->client, INT_CTRL1);
		if (err < 0)
			return err;
	printk(">>>>>int_ctrl1 = %x.\n", err);
			
	err = i2c_smbus_read_byte_data(tik->client, DATA_CTRL);
		if (err < 0)
			return err;
	printk(">>>>>data_ctrl = %x.\n", err);
	#endif
	return 0;
}

static void kxtik_report_acceleration_data(struct kxtik_data *tik)
{
	s16 acc_data[3]; /* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = tik->input_dev;
	int loop = 10;

#if KXTIK_DBG
	dev_err(&tik->client->dev, "kxtik_report_acceleration_data\n");
#endif

	//printk("%s:\n", __func__);
	//printk("%s: tik->acc_enabled = %d\n", __func__, atomic_read(&tik->acc_enabled));

 if(atomic_read(&tik->acc_enabled) > 0) { //grace modified in 2012.12.06
	while(loop) {
	mutex_lock(&input_dev->mutex);
	err = kxtik_i2c_read(tik, XOUT_L, (u8 *)acc_data, 6);
	mutex_unlock(&input_dev->mutex);
		if(err < 0){
			loop--;
			msleep(1);
		}
		else
			loop = 0;
	}
	if (err < 0) {
		dev_err(&tik->client->dev, "accelerometer data read failed. err(%d)\n", err);
	}
	else {
	x = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_x])) >> tik->shift;
	y = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_y])) >> tik->shift;
	z = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_z])) >> tik->shift;

#if KXTIK_DBG	
	dev_err(&tik->client->dev, "accelerometer data: x= %d, y= %d, z= %d.\n", x, y, z);
#endif
	
	if(atomic_read(&tik->acc_input_event) > 0) 
	{
		x = tik->pdata.negate_x ? -x : x;
		y = tik->pdata.negate_y ? -y : y;
		z = tik->pdata.negate_z ? -z : z;
#ifdef ZTE_K_ACCEL_CALI		
	    //xym, add for calibration, begin
	    x -= cali_value[0];
		y -= cali_value[1];
		z -= cali_value[2];
	    //xym, add for calibration, end		
#endif			
		input_report_abs(tik->input_dev, ABS_X, x);
		input_report_abs(tik->input_dev, ABS_Y, y);
		input_report_abs(tik->input_dev, ABS_Z, z);
		input_sync(tik->input_dev);
	}
}
}
}

static irqreturn_t kxtik_isr(int irq, void *dev)
{
	struct kxtik_data *tik = dev;
	if((atomic_read(&tik->acc_enabled) <= 0) ||(atomic_read(&tik->accel_suspended) == 1)  )
		return IRQ_HANDLED;	//xym,  skip unexpected int
	queue_work(tik->irq_workqueue, &tik->irq_work);
	return IRQ_HANDLED;
}

static void kxtik_irq_work(struct work_struct *work)
{
	struct kxtik_data *tik = container_of(work,	struct kxtik_data, irq_work);
	int err;
	int loop = 10;

	/* data ready is the only possible interrupt type */
	kxtik_report_acceleration_data(tik);

	while(loop) {
	err = i2c_smbus_read_byte_data(tik->client, INT_REL);
		if(err < 0){
			loop--;
			msleep(1);
		}
		else
			loop = 0;
	}
	if (err < 0)
	{
		dev_err(&tik->client->dev,
			"error clearing interrupt status: %d\n", err);
		printk("%s: device id=%d\n", __func__, i2c_smbus_read_byte_data(tik->client, 0xf));//grace modified in 2012.12.06
	}	
	
}

static int kxtik_update_g_range(struct kxtik_data *tik, u8 new_g_range)
{
	switch (new_g_range) {
	case KXTIK_G_2G:
		tik->shift = 4;
		break;
	case KXTIK_G_4G:
		tik->shift = 3;
		break;
	case KXTIK_G_8G:
		tik->shift = 2;
		break;
	default:
		return -EINVAL;
	}

	tik->ctrl_reg1 &= 0xE7;
	tik->ctrl_reg1 |= new_g_range;

	return 0;
}

static int kxtik_update_odr(struct kxtik_data *tik, unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kxtik_odr_table); i++) {
		odr = kxtik_odr_table[i].mask;
		if (poll_interval < kxtik_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update DATA_CTRL_REG register if the ODR is not changed */
	if(tik->data_ctrl == odr)
		return 0;
	else
		tik->data_ctrl = odr;

	/* Do not need to update DATA_CTRL_REG register if the sensor is not currently turn on */
	if(atomic_read(&tik->acc_enabled) > 0) {
		err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, 0);
		if (err < 0)
			return err;

		err = i2c_smbus_write_byte_data(tik->client, DATA_CTRL, tik->data_ctrl);
		if (err < 0)
			return err;

		err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, tik->ctrl_reg1 | PC1_ON);
		if (err < 0)
			return err;
	}

	return 0;
}


static int kxtik_power( int on )//xym
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

static int kxtik_device_power_on(struct kxtik_data *tik)
{
	if (tik->pdata.power_on)
		return tik->pdata.power_on();
	else
		return kxtik_power(1);//xym

	return 0;
}

static void kxtik_device_power_off(struct kxtik_data *tik)
{
	if (tik->pdata.power_off)
		tik->pdata.power_off();
	else
		kxtik_power(0);//xym
}

static int kxtik_power_on_init(struct kxtik_data *tik)
{
	int err;

	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(tik->client,
					CTRL_REG1, 0);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(tik->client,
					DATA_CTRL, tik->data_ctrl);
	if (err < 0)
		return err;

	/* only write INT_CTRL_REG1 if in irq mode */
	if (tik->client->irq) {
		err = i2c_smbus_write_byte_data(tik->client,
						INT_CTRL1, tik->int_ctrl);
		if (err < 0)
			return err;
	}

	if(atomic_read(&tik->acc_enabled) > 0) {
		err = i2c_smbus_write_byte_data(tik->client,
						CTRL_REG1, tik->ctrl_reg1 | PC1_ON);
		if (err < 0)
			return err;
	}
	else {
		err = i2c_smbus_write_byte_data(tik->client,
						CTRL_REG1, tik->ctrl_reg1);
		if (err < 0)
			return err;
	}
  printk("kxtik_power_on_init device id=%d\n",i2c_smbus_read_byte_data(tik->client, 0xf));//grace modified in 2012.12.06
	//i2c_smbus_read_byte_data(tik->client, INT_REL);	// Clear Interrupt
	err = i2c_smbus_read_byte_data(tik->client, INT_REL);//grace modified in 2012.12.06
	if (err < 0)
		dev_err(&tik->client->dev,
			"kxtik_power_on_init---error clearing interrupt status: %d\n", err);
	return 0;
}

static int kxtik_operate(struct kxtik_data *tik)
{
	int err;
	int loop = 10;
	printk("%s:\n", __func__);  
	
	atomic_set(&tik->accel_suspend_continue, 0);
	while(loop) {
	err = i2c_smbus_write_byte_data(tik->client, INT_CTRL1, tik->int_ctrl | KXTIK_IEL);
		if(err < 0){
			loop--;
			msleep(20);
		}
		else
			loop = 0;
	}	
		
	if (err < 0)
		return err;

	loop = 10;
	while(loop) {
	err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, tik->ctrl_reg1 | PC1_ON);
		if(err < 0){
			loop--;
			msleep(20);
		}
		else
			loop = 0;
	}	
		
	if (err < 0)
		return err;

	kxtik_report_regs(tik);
	return 0;
}

static int kxtik_standby(struct kxtik_data *tik)
{
	int err;
	int loop = 10;
	printk("%s:\n", __func__);  

	atomic_set(&tik->accel_suspend_continue, 1);

	while(loop) {
	err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, 0);
		if(err < 0){
			loop--;
			msleep(20);
		}
		else
			loop = 0;
	}	
	
	if (err < 0)
		return err;

	kxtik_report_regs(tik);
	return 0;
}

static int kxtik_enable(struct kxtik_data *tik)
{
	int err;
	unsigned long flags;
	
	printk("%s: tik->acc_enabled = %d, tik->accel_suspended = %d \n", __func__,
	           atomic_read(&tik->acc_enabled),atomic_read(&tik->accel_suspended));	

	spin_lock_irqsave(&tik->lock, flags);
	if (atomic_read(&tik->accel_suspended) == 1) {
		atomic_inc(&tik->acc_enabled);
		spin_unlock_irqrestore(&tik->lock, flags);
		printk("%s: tik->acc_enabled = %d\n", __func__, atomic_read(&tik->acc_enabled));
		return 0;
	}
	spin_unlock_irqrestore(&tik->lock, flags);
		
	atomic_set(&tik->accel_suspend_continue, 0);
#if 0		
	if(atomic_read(&tik->accel_suspended) == 1) {
		printk("%s: waiting for resume\n", __func__);
		remaining = wait_event_interruptible_timeout(tik->wqh_suspend, \
				atomic_read(&tik->accel_suspended) == 0, \
				msecs_to_jiffies(KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT));

		if(atomic_read(&tik->accel_suspended) == 1) {
			printk("%s: timeout waiting for resume\n", __func__);
		}
	}
#endif
	err = kxtik_operate(tik);
	if (err < 0)
		dev_err(&tik->client->dev, "%s: operate mode failed\n", __func__);


	atomic_inc(&tik->acc_enabled);
	
	printk("%s: tik->acc_enabled = %d\n", __func__, atomic_read(&tik->acc_enabled));
	printk("%s: device id = %d\n", __func__, i2c_smbus_read_byte_data(tik->client, 0xf));//grace modified in 2012.12.06
	err = i2c_smbus_read_byte_data(tik->client, INT_REL);//grace modified in 2012.12.06
	if (err < 0)
		dev_err(&tik->client->dev,
			"%s:---error clearing interrupt status: %d\n", __func__, err);
	return 0;
}

static void kxtik_disable(struct kxtik_data *tik)
{
	int err;
#if KXTIK_DBG
	s16 acc_data[3]; /* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	s16 x, y, z;
	
	err = kxtik_i2c_read(tik, XOUT_L, (u8 *)acc_data, 6);
	x = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_x])) >> tik->shift;
	y = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_y])) >> tik->shift;
	z = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_z])) >> tik->shift;
	dev_err(&tik->client->dev, "%s: accelerometer data: x= %d, y= %d, z= %d.\n", __func__, x, y, z);
#endif
	printk("%s: tik->acc_enabled = %d, tik->accel_suspended = %d \n", __func__,
	           atomic_read(&tik->acc_enabled),atomic_read(&tik->accel_suspended));	
	printk("%s: device id = %d\n", __func__, i2c_smbus_read_byte_data(tik->client, 0xf));//grace modified in 2012.12.06
	//i2c_smbus_read_byte_data(tik->client, INT_REL);	// Clear Interrupt
	err = i2c_smbus_read_byte_data(tik->client, INT_REL);//grace modified in 2012.12.06
	if (err < 0)
		dev_err(&tik->client->dev, "%s:---error clearing interrupt status: %d\n", __func__, err);

	atomic_set(&tik->accel_suspend_continue, 1);

	if(atomic_read(&tik->acc_enabled) > 0) {
		if(atomic_dec_and_test(&tik->acc_enabled)) {
			if(atomic_read(&tik->accel_enable_resume) > 0)
				atomic_set(&tik->accel_enable_resume, 0);
			err = kxtik_standby(tik);
			if (err < 0)
				dev_err(&tik->client->dev, "%s: standby mode failed\n", __func__);
		}
		wake_up_interruptible(&tik->wqh_suspend);
	}
	printk("%s: tik->acc_enabled = %d\n", __func__, atomic_read(&tik->acc_enabled));
}

static int kxtik_input_open(struct input_dev *input)
{
	struct kxtik_data *tik = input_get_drvdata(input);

	atomic_inc(&tik->acc_input_event);

	return 0;
}

static void kxtik_input_close(struct input_dev *dev)
{
	struct kxtik_data *tik = input_get_drvdata(dev);

	atomic_dec(&tik->acc_input_event);
}

static void __devinit kxtik_init_input_device(struct kxtik_data *tik,
					      struct input_dev *input_dev)
{
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);

	input_dev->name = "kxtik";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &tik->client->dev;
}

static int __devinit kxtik_setup_input_device(struct kxtik_data *tik)
{
	struct input_dev *input_dev;
	int err;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&tik->client->dev, "input device allocate failed\n");
		return -ENOMEM;
	}

	tik->input_dev = input_dev;

	input_dev->open = kxtik_input_open;
	input_dev->close = kxtik_input_close;
	input_set_drvdata(input_dev, tik);

	kxtik_init_input_device(tik, input_dev);

	err = input_register_device(tik->input_dev);
	if (err) {
		dev_err(&tik->client->dev,
			"unable to register input polled device %s: %d\n",
			tik->input_dev->name, err);
		input_free_device(tik->input_dev);
		return err;
	}

	return 0;
}

/*
 * When IRQ mode is selected, we need to provide an interface to allow the user
 * to change the output data rate of the part.  For consistency, we are using
 * the set_poll method, which accepts a poll interval in milliseconds, and then
 * calls update_odr() while passing this value as an argument.  In IRQ mode, the
 * data outputs will not be read AT the requested poll interval, rather, the
 * lowest ODR that can support the requested interval.  The client application
 * will be responsible for retrieving data from the input node at the desired
 * interval.
 */

/* Returns currently selected poll interval (in ms) */
static ssize_t kxtik_get_poll(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtik_data *tik = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", tik->poll_interval);
}

/* Allow users to select a new poll interval (in ms) */
static ssize_t kxtik_set_poll(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtik_data *tik = i2c_get_clientdata(client);
	struct input_dev *input_dev = tik->input_dev;
	unsigned int interval;

	#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
	int error;
	error = kstrtouint(buf, 10, &interval);
	if (error < 0)
		return error;
	#else
	interval = (unsigned int)simple_strtoul(buf, NULL, 10);
	#endif

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	disable_irq(client->irq);

	/*
	 * Set current interval to the greater of the minimum interval or
	 * the requested interval
	 */
	tik->poll_interval = max(interval, tik->pdata.min_interval);
	tik->poll_delay = msecs_to_jiffies(tik->poll_interval);

	kxtik_update_odr(tik, tik->poll_interval);

	enable_irq(client->irq);
	mutex_unlock(&input_dev->mutex);

	return count;
}

/* Allow users to enable/disable the device */
static ssize_t kxtik_set_enable(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtik_data *tik = i2c_get_clientdata(client);
	struct input_dev *input_dev = tik->input_dev;
	unsigned int enable;

	#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
	int error;
	error = kstrtouint(buf, 10, &enable);
	if (error < 0)
		return error;
	#else
	enable = (unsigned int)simple_strtoul(buf, NULL, 10);
	#endif

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if(enable)
		kxtik_enable(tik);
	else
		kxtik_disable(tik);

	mutex_unlock(&input_dev->mutex);

	return count;
}

#ifdef ZTE_K_ACCEL_CALI
//xym, add for calibration, begin
static ssize_t attr_get_cali(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d %d\n", cali_value[0], cali_value[1], cali_value[2]);
}

static int kxtik_reset_to_pollmode(struct kxtik_data *tik)
{
	int err;
	u8 ctrl_reg1;
	//u8 data_ctrl;
	u8 int_ctrl;

	err = i2c_smbus_read_byte_data(tik->client, INT_REL);
	if (err < 0)
		dev_err(&tik->client->dev,
			"%s: error clearing interrupt status: %d\n", __func__, err);

	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(tik->client,
					CTRL_REG1, 0);
	if (err < 0)
		return err;
	

	err = i2c_smbus_write_byte_data(tik->client,
					DATA_CTRL, tik->data_ctrl);
	if (err < 0)
		return err;

	int_ctrl = tik->int_ctrl &(~ (KXTIK_IEN | KXTIK_IEA | KXTIK_IEL));	
	if (tik->client->irq) {
		err = i2c_smbus_write_byte_data(tik->client,
						INT_CTRL1, int_ctrl);
		if (err < 0)
			return err;
	}
	
	ctrl_reg1 =	tik->ctrl_reg1 &(~ DRDYE);
	if(atomic_read(&tik->acc_enabled) > 0) {
		err = i2c_smbus_write_byte_data(tik->client,
						CTRL_REG1, ctrl_reg1 | PC1_ON);
		if (err < 0)
			return err;
	}
	else 
	{
		err = i2c_smbus_write_byte_data(tik->client,
						CTRL_REG1, ctrl_reg1);
		if (err < 0)
			return err;
	}
	
    printk("%s: device id=%d\n", __func__, i2c_smbus_read_byte_data(tik->client, 0xf));

	return 0;
}

static void kxtik_get_accel_data(struct kxtik_data *tik, int *xyz)
{
	s16 acc_data[3]; /* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = tik->input_dev;
	int loop = 10;

	//printk("%s:\n", __func__);
	//printk("%s: tik->acc_enabled = %d\n", __func__, atomic_read(&tik->acc_enabled));

 if(atomic_read(&tik->acc_enabled) > 0) 
 { 
	while(loop) {
	mutex_lock(&input_dev->mutex);
	err = kxtik_i2c_read(tik, XOUT_L, (u8 *)acc_data, 6);
	mutex_unlock(&input_dev->mutex);
		if(err < 0){
			loop--;
			msleep(1);
		}
		else
			loop = 0;
	}
	if (err < 0) {
		dev_err(&tik->client->dev, "accelerometer data read failed. err(%d)\n", err);
	}
	else {
	x = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_x])) >> tik->shift;
	y = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_y])) >> tik->shift;
	z = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_z])) >> tik->shift;

#if KXTIK_DBG	
	dev_err(&tik->client->dev, "accelerometer data: x= %d, y= %d, z= %d.\n", x, y, z);
#endif
	
	if(atomic_read(&tik->acc_input_event) > 0) 
	{
		x = tik->pdata.negate_x ? -x : x;
		y = tik->pdata.negate_y ? -y : y;
		z = tik->pdata.negate_z ? -z : z;
		xyz[0]=x; 
		xyz[1]=y; 
		xyz[2]=z;
	}
}
}
}

static ssize_t attr_set_cali(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct kxtik_data *tik = dev_get_drvdata(dev);
	unsigned long val;
	int i, err;
	int xyz[3] = {0};
	int temp_xyz[3] = {0};

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val) 
	{
		atomic_set(&tik->cali, 1);
		
		//disable irq, reset kxtik
		kxtik_reset_to_pollmode(tik);
		printk(KERN_INFO "%s %s: calibrate begin\n", "kxtik", __func__);		
		
		for( i = 0; i < CALI_TIMES; i++)
		{
			msleep(50); //xym, add for calibration
			
			//mutex_lock(&acc->lock);
  			kxtik_get_accel_data(tik, xyz);
			//mutex_unlock(&acc->lock); 
			temp_xyz[0]+= xyz[0];
			temp_xyz[1]+= xyz[1];
			temp_xyz[2]+= xyz[2];

		   printk(KERN_INFO "%s %s: xyz = %d, %d, %d\n", 
						"kxtik", __func__, xyz[0], xyz[1], xyz[2]);			
			
		   printk(KERN_INFO "%s %s: temp_xyz = %d, %d, %d\n", 
						"kxtik", __func__, temp_xyz[0], temp_xyz[1], temp_xyz[2]);			
		
		}
		
		cali_value[0] = temp_xyz[0]/CALI_TIMES - 0;
		cali_value[1] = temp_xyz[1]/CALI_TIMES - 0;
		cali_value[2] = ((temp_xyz[2]/CALI_TIMES) - (-1024));//xym

		printk(KERN_INFO "%s %s: calibrate end, cali_value = %d, %d, %d\n", "kxtik", __func__, 
		                cali_value[0],	cali_value[1], cali_value[2]);

		//enable irq, reset kxtik
		err = kxtik_power_on_init(tik);
		if (err) {
			pr_info("%s: power on init failed: %d\n", __func__, err);
			return -1;
		}

	}
	else
	{
		atomic_set(&tik->cali, 0);
		cali_value[0] = 0;
		cali_value[1] = 0;
		cali_value[2] = 0;
	}
	
	return size;
}

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


static DEVICE_ATTR(poll, S_IRUGO|S_IWUSR, kxtik_get_poll, kxtik_set_poll);
static DEVICE_ATTR(enable, S_IWUSR, NULL, kxtik_set_enable);
#ifdef ZTE_K_ACCEL_CALI //xym
static DEVICE_ATTR(cali, 00600, attr_get_cali, attr_set_cali); 
static DEVICE_ATTR(cali_data, 00600, attr_get_cali_data,  attr_set_cali_data); 
#endif

static struct attribute *kxtik_attributes[] = {
	&dev_attr_poll.attr,
	&dev_attr_enable.attr,
#ifdef ZTE_K_ACCEL_CALI	//xym
	&dev_attr_cali.attr, 
	&dev_attr_cali_data.attr, 
#endif	
	NULL
};

static struct attribute_group kxtik_attribute_group = {
	.attrs = kxtik_attributes
};

static int __devinit kxtik_verify(struct kxtik_data *tik)
{
	int retval;

	retval = i2c_smbus_read_byte_data(tik->client, WHO_AM_I);
	printk("cgh who_am_i retval = %d\n", retval);
	if (retval < 0){
		tik->client->addr = 0xe;
		retval = i2c_smbus_read_byte_data(tik->client, WHO_AM_I);
		if (retval < 0){
		dev_err(&tik->client->dev, "error reading WHO_AM_I register!\n");
		}
	}	
	
		retval = ( retval == 0x11 || retval == 0x05 || retval == 0x09)? 0: -EIO ;
		//retval = retval != 0x05 ? -EIO : 0;  cjn modify

	return retval;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void kxtik_earlysuspend_suspend(struct early_suspend *h)
{
	struct kxtik_data *tik = container_of(h, struct kxtik_data, early_suspend);
	int err;
	//long remaining;
	printk("---------kxtik_suspend_0-------\n");
	
#if 0	
	if(atomic_read(&tik->accel_suspend_continue) > 0) {	// No disable function is called before suspend
			printk(">>>>>>suspend_continue\n");
		if(atomic_read(&tik->acc_enabled) > 0) {
			printk(">>>>>>waiting for disable\n");
			remaining = wait_event_interruptible_timeout(tik->wqh_suspend, \
					atomic_read(&tik->acc_enabled) < 1, \
					msecs_to_jiffies(KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT));
			if(atomic_read(&tik->acc_enabled) > 0) {
				printk(">>>>>>timeout waiting for disable\n");
			}
		}
	}
	else	{ // Make sure it is disabled.
	err = kxtik_standby(tik);
	if (err < 0)
	dev_err(&tik->client->dev, "earlysuspend failed to suspend\n");
	}

		atomic_set(&tik->accel_suspended, 1);
#endif

   atomic_set(&tik->accel_suspended, 1);
	err = kxtik_standby(tik);
	if (err < 0)
	dev_err(&tik->client->dev, "earlysuspend failed to suspend\n");
	 printk("---------kxtik_suspend_1-------\n");

	#if KXTIK_DBG
	kxtik_report_regs(tik);
	printk("---------kxtik_suspend_1-------\n");
	#endif
	printk("kxtik_earlysuspend_suspend kxtik_disable. Count = %d tik->accel_suspended :%d \n", atomic_read(&tik->acc_enabled),atomic_read(&tik->accel_suspended));	
	return;
}

void kxtik_earlysuspend_resume(struct early_suspend *h)
{
	struct kxtik_data *tik = container_of(h, struct kxtik_data, early_suspend);
	int err;
	
	printk("---------kxtik_resume_0-------\n");	
	if(atomic_read(&tik->accel_suspended) == 1) {	// It is suspend status.
		unsigned long flags;
		printk(">>>>>>>trying to resume\n");
		spin_lock_irqsave(&tik->lock, flags);
		if(atomic_read(&tik->acc_enabled)>0)//grace modified in 2012.12.06
		{
			spin_unlock_irqrestore(&tik->lock, flags);
			err = kxtik_operate(tik);
			if (err < 0)
				dev_err(&tik->client->dev, "earlysuspend failed to resume\n");
			printk(">>>>>>>trying to resume---operate\n");
			spin_lock_irqsave(&tik->lock, flags);
		}
		atomic_set(&tik->accel_suspended, 0);
		spin_unlock_irqrestore(&tik->lock, flags);
	}
	
	
	wake_up_interruptible(&tik->wqh_suspend);
	
	printk("kxtik_earlysuspend_resume device id=%d\n",i2c_smbus_read_byte_data(tik->client, 0xf));//grace modified in 2012.12.06
	err = i2c_smbus_read_byte_data(tik->client, INT_REL);//grace modified in 2012.12.06
	if (err < 0)
		dev_err(&tik->client->dev,
			"kxtik_earlysuspend_resume---error clearing interrupt status: %d\n", err);

	#if KXTIK_DBG
	kxtik_report_regs(tik);
	err = i2c_smbus_read_byte_data(tik->client, INT_CTRL1);
		if (err < 0)
			printk("failed to read int_ctrl1\n");
		else if (err != tik->int_ctrl)
		{
			err = i2c_smbus_write_byte_data(tik->client, INT_CTRL1, tik->int_ctrl);
			if (err < 0)
				printk("failed to change int_ctrl1\n");

			err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, tik->ctrl_reg1 | PC1_ON);
			if (err < 0)
				printk("failed to change ctrl_reg1\n");
			
			kxtik_report_regs(tik);
		}	
	printk("---------kxtik_resume_1-------\n");		
	#endif
	printk("kxtik_earlysuspend_resume kxtik_enable. Count = %d tik->accel_suspended :%d n", atomic_read(&tik->acc_enabled),atomic_read(&tik->accel_suspended));
	return;
}
#endif

struct kxtik_data *tik_data;

int kxtik_suspend_pm(void)
{
	int err;
	printk("%s:\n", __func__);  

	atomic_set(&tik_data->accel_suspended, 1);
	err = kxtik_standby(tik_data);
	if (err < 0)
		dev_err(&tik_data->client->dev, "%s: failed to suspend\n", __func__);

#if KXTIK_DBG
	kxtik_report_regs(tik_data);
#endif
	printk("%s: tik_data->acc_enabled = %d, tik_data->accel_suspended = %d \n", __func__,
	           atomic_read(&tik_data->acc_enabled),atomic_read(&tik_data->accel_suspended));	
	return 0;

}
int kxtik_resume_pm(void)
{
	int err;
	printk("%s:\n", __func__);  
	
	if(atomic_read(&tik_data->accel_suspended) == 1) {	// It is suspend status.
		unsigned long flags;
		printk("%s: trying to resume\n", __func__);
		spin_lock_irqsave(&tik_data->lock, flags);
		if(atomic_read(&tik_data->acc_enabled)>0)//grace modified in 2012.12.06
		{
			spin_unlock_irqrestore(&tik_data->lock, flags);
			err = kxtik_operate(tik_data);
			if (err < 0)
				dev_err(&tik_data->client->dev, "%s: failed to resume\n", __func__);
			printk("%s: trying to resume---operate\n", __func__);
			spin_lock_irqsave(&tik_data->lock, flags);
		}
		atomic_set(&tik_data->accel_suspended, 0);
		spin_unlock_irqrestore(&tik_data->lock, flags);
	}	
	
	wake_up_interruptible(&tik_data->wqh_suspend);
	
	printk("%s: device id = %d\n", __func__, i2c_smbus_read_byte_data(tik_data->client, 0xf));//grace modified in 2012.12.06
	err = i2c_smbus_read_byte_data(tik_data->client, INT_REL);//grace modified in 2012.12.06
	if (err < 0)
		dev_err(&tik_data->client->dev,
			"%s:---error clearing interrupt status: %d\n", __func__, err);

#if KXTIK_DBG
	kxtik_report_regs(tik_data);
	err = i2c_smbus_read_byte_data(tik_data->client, INT_CTRL1);
		if (err < 0)
			printk("%s: failed to read int_ctrl1\n", __func__);
		else if (err != tik_data->int_ctrl)
		{
			err = i2c_smbus_write_byte_data(tik_data->client, INT_CTRL1, tik_data->int_ctrl);
			if (err < 0)
				printk("%s: failed to change int_ctrl1\n", __func__);

			err = i2c_smbus_write_byte_data(tik_data->client, CTRL_REG1, tik_data->ctrl_reg1 | PC1_ON);
			if (err < 0)
				printk("%s: failed to change ctrl_reg1\n", __func__);
			
			kxtik_report_regs(tik_data);
		}	
#endif
	printk("%s: tik_data->acc_enabled = %d, tik_data->accel_suspended = %d \n", __func__,
	           atomic_read(&tik_data->acc_enabled),atomic_read(&tik_data->accel_suspended));	

	return 0;
}


static int kxtik_parse_dt(struct device *dev, struct kxtik_platform_data *pdata)
{
	pdata->min_interval = 10, 
	pdata->poll_interval  = 200, 
	
	//backside
	pdata->axis_map_x  = 0, 
	pdata->axis_map_y  = 1, 
	pdata->axis_map_z  = 2, 
	
#ifdef CONFIG_BOARD_ADA  //P821T01 backside  pin1 left-top
	pdata->negate_x  = 0, 
	pdata->negate_y  = 1, 
	pdata->negate_z  = 1, 
#elif defined CONFIG_BOARD_EOS //V72M
	pdata->negate_x  = 0, 
	pdata->negate_y  = 1, 
	pdata->negate_z  = 1, 
#elif defined CONFIG_BOARD_APUS || defined CONFIG_BOARD_GIANT//lcdside pin1 left-bottom P821A64 AND P821T05, only estimate!!!
	pdata->axis_map_x  = 1, 
	pdata->axis_map_y  = 0, 
	pdata->axis_map_z  = 2, 
	pdata->negate_x  = 1, 
	pdata->negate_y  = 0, 
	pdata->negate_z  = 0, 

#elif defined CONFIG_BOARD_WELLINGTON //N817
	pdata->negate_x  = 1, 
	pdata->negate_y  = 1, 
	pdata->negate_z  = 0,
#else  //backside  pin1 left-bottom    P821A32
	pdata->negate_x  = 1, 
	pdata->negate_y  = 0, 
	pdata->negate_z  = 1, 
#endif

	pdata->res_12bit  = RES_12BIT, //(1 << 6)
	pdata->g_range  = KXTIK_G_2G, //0

	pdata->gpio_int1 = 81;
	pdata->gpio_int2 = 97;

	if (dev->of_node) {
		pdata->gpio_int1 = of_get_named_gpio(dev->of_node, "kxtik,gpio_int1", 0);
		pdata->gpio_int2 = of_get_named_gpio(dev->of_node, "kxtik,gpio_int2", 0);
	}

	pdata->vdd = "8110_l19";//xym
	pdata->vbus = "8110_l14";

	return 0;
}


/* ----------------------*
* config gpio for intr utility        *
*-----------------------*/
int kxtik_config_irq_gpio(int irq_gpio) //xym add 
{
    int rc=0;
    uint32_t  gpio_config_data = GPIO_CFG(irq_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);

#ifdef CONFIG_OF
	if (!(rc = gpio_is_valid(irq_gpio))) {
	    pr_err("%s: irq gpio not provided.\n",  __func__);
		return rc;
    }	
#endif

  /*  rc = gpio_request(gpio_int, "gpio_accel_sensor_int");
    if (rc) {
        pr_err("%s: gpio_request(%#x)=%d\n",__func__, gpio_int, rc);
        return rc;
    }*/

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


static int kxtik_gpio_init(struct device *dev, int flag, char *vreg_vdd, char *vreg_vbus, unsigned irq_gpio)//xym
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

		vbus =regulator_get(dev, vreg_vbus);
		if (!vbus) {
			pr_err("%s get failed\n", vreg_vbus);
			return -1;
		}

		if ( regulator_set_voltage(vbus, 1800000,1800000)) {
			pr_err(" %s set failed\n", vreg_vbus);
			return -1;
		}

		ret = gpio_request(irq_gpio, "kxtik_int_gpio");
		if (ret){
			pr_err("gpio %d request is error!\n", irq_gpio);
			return -1;
		}
		
		if ( kxtik_config_irq_gpio(irq_gpio)) {
			pr_err(" gpio %d config failed\n", irq_gpio);
			return -1;
		}		
	

	}

	//deinit
	if ( flag == 0)
	{
		regulator_put(vdd);
		regulator_put(vbus);
		gpio_free(irq_gpio);
	}

	return 0;

}

int (*gsensor_suspend_pm)(void);
int (*gsensor_resume_pm)(void);

static int  kxtik_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct kxtik_data *tik;
	int err;
	//int err1;
	
	pr_info("%s: probe start.\n", __func__);
	
	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "client is not i2c capable\n");
		return -ENXIO;
	}

	tik = kzalloc(sizeof(*tik), GFP_KERNEL);
	if (!tik) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	tik->client = client;
	memset(&tik->pdata, 0, sizeof(tik->pdata));
	kxtik_parse_dt(&client->dev, &tik->pdata);	
	client->irq = gpio_to_irq(tik->pdata.gpio_int1);//xym add

	err = kxtik_gpio_init(&client->dev, 1, tik->pdata.vdd, tik->pdata.vbus, tik->pdata.gpio_int1);//xym add
	if ( err < 0 ){
		pr_err("%s, gpio init failed! %d\n", __func__, err);
		goto err_free_mem;
	}

	err = kxtik_device_power_on(tik);
	if (err != 0)
		goto err_gpio_uninit;
	msleep(15);//xym 20131204

	if (tik->pdata.init) {
		err = tik->pdata.init();
		if (err < 0)
			goto err_pdata_power_off;
	}
	err = kxtik_verify(tik);
	
	if (err < 0) {
		dev_err(&client->dev, "device not recognized\n");
		goto err_pdata_exit;
	}

	i2c_set_clientdata(client, tik);
	
	init_waitqueue_head(&tik->wqh_suspend);
#ifdef ZTE_K_ACCEL_CALI	
	atomic_set(&tik->cali, 0);//xym add for calibrate
#endif
	atomic_set(&tik->acc_enabled, 0);
	atomic_set(&tik->acc_input_event, 0);
	spin_lock_init(&tik->lock);

	tik->ctrl_reg1 = tik->pdata.res_12bit | tik->pdata.g_range;
	tik->poll_interval = tik->pdata.poll_interval;
	tik->poll_delay = msecs_to_jiffies(tik->poll_interval);

	kxtik_update_odr(tik, tik->poll_interval);

	err = kxtik_update_g_range(tik, tik->pdata.g_range);
	if (err < 0) {
		dev_err(&client->dev, "invalid g range\n");
		goto err_pdata_exit;
	}

	if (client->irq) {
		err = kxtik_setup_input_device(tik);
		if (err)
			goto err_pdata_exit;

		tik->irq_workqueue = create_workqueue("KXTIK Workqueue");
		INIT_WORK(&tik->irq_work, kxtik_irq_work);
		/* If in irq mode, populate INT_CTRL_REG1 and enable DRDY. */
		tik->int_ctrl |= KXTIK_IEN | KXTIK_IEA | KXTIK_IEL;	//Andy Mi added IEL @ 2012-11-30
		tik->ctrl_reg1 |= DRDYE;

		err = request_threaded_irq(client->irq, NULL, kxtik_isr,
					   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					   "kxtik-irq", tik);
		if (err) {
			dev_err(&client->dev, "request irq failed: %d\n", err);
			goto err_destroy_input;
		}

		err = kxtik_power_on_init(tik);
		if (err) {
			dev_err(&client->dev, "power on init failed: %d\n", err);
			goto err_free_irq;
		}

		err = sysfs_create_group(&client->dev.kobj, &kxtik_attribute_group);
		if (err) {
			dev_err(&client->dev, "sysfs create failed: %d\n", err);
			goto err_free_irq;
		}

	} else {
		dev_err(&client->dev, "irq not defined\n");

		goto err_pdata_exit;
	}
	
        tik_data=tik;
	
#ifdef    CONFIG_HAS_EARLYSUSPEND
	//tik->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 10;
	tik->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 50;	//that is 200
	tik->early_suspend.suspend = kxtik_earlysuspend_suspend;
	tik->early_suspend.resume = kxtik_earlysuspend_resume;
	register_early_suspend(&tik->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	gsensor_suspend_pm=kxtik_suspend_pm;
	gsensor_resume_pm=kxtik_resume_pm;

	pr_info("%s: probe exit.\n", __func__);
	return 0;

err_free_irq:
if (client->irq) {
	free_irq(client->irq, tik);
	destroy_workqueue(tik->irq_workqueue);
}
err_destroy_input:
	input_unregister_device(tik->input_dev);
err_pdata_exit:
	if (tik->pdata.exit)
		tik->pdata.exit();
err_pdata_power_off:
	kxtik_device_power_off(tik);
err_gpio_uninit:
	kxtik_gpio_init(&client->dev, 0, tik->pdata.vdd, tik->pdata.vbus, tik->pdata.gpio_int1);
err_free_mem:
	kfree(tik);
	
	pr_info("%s: probe exit...\n", __func__);
	return err;
}

static int __devexit kxtik_remove(struct i2c_client *client)
{
	struct kxtik_data *tik = i2c_get_clientdata(client);

#ifdef    CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&tik->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	sysfs_remove_group(&client->dev.kobj, &kxtik_attribute_group);
	if (client->irq) {
		free_irq(client->irq, tik);
		destroy_workqueue(tik->irq_workqueue);
	}
	input_unregister_device(tik->input_dev);
	if (tik->pdata.exit)
		tik->pdata.exit();
	kxtik_device_power_off(tik);
	kfree(tik);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id kxtik_match_table[] = {
	{ .compatible = "kxtik,1013",},
	{ },
};
#endif

static const struct i2c_device_id kxtik_id[] = {
	{ NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, kxtik_id);

static struct i2c_driver kxtik_driver = {
	.driver = {
		.name	= NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = kxtik_match_table,
#endif	
	},
	.probe		= kxtik_probe,
	.remove		= __devexit_p(kxtik_remove),
	.id_table	= kxtik_id,
};

static int __init kxtik_init(void)
{
	return i2c_add_driver_async(&kxtik_driver);
}
module_init(kxtik_init);

static void __exit kxtik_exit(void)
{
	i2c_del_driver(&kxtik_driver);
}
module_exit(kxtik_exit);

MODULE_DESCRIPTION("KXTIK accelerometer driver");
MODULE_AUTHOR("Kuching Tan <kuchingtan@kionix.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.4.0");
