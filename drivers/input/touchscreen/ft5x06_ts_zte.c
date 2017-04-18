/* drivers/input/touchscreen/ft5x06_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
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
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/input/mt.h>
#include <linux/i2c/ft5x06_ts_zte.h>
#include <linux/timer.h>
#include "touchscreen_fw.h"
#include <linux/regulator/consumer.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/rtc.h>
#include "ft5x06_ex_fun.h"

//#include <mach/map.h>
//#include <mach/regs-clock.h>
//#include <mach/regs-gpio.h>
//#include <plat/gpio-cfg.h>

//#define FT5336_DOWNLOAD
#define SYSFS_DEBUG
#define FTS_APK_DEBUG
#define FTS_CTL_IIC
extern int (*touchscreen_suspend_pm)(void);
extern int (*touchscreen_resume_pm)(void);
static int ft5x0x_ts_suspend_pm (void);
static int ft5x0x_ts_resume_pm(void);
static struct workqueue_struct *focaltech_wq;
static int tpd_halt= 0;

struct ft5x0x_ts_data *ft_ts;
#define VREG_VDD		"8110_l19"		// 3V
#define VREG_VBUS		"8110_l14"		// 1.8V
#define GPIO_TS_IRQ		1
#define GPIO_TS_RST		0
#if (!defined CONFIG_BOARD_EOS)
#define CHAGER_IN_REG 0x8b
#define CHAGER_IN_VAL 0x1
#define CHAGER_OUT_VAL 0x0

#else
#define CHAGER_IN_REG 0x86
#define CHAGER_IN_VAL 0x3
#define CHAGER_OUT_VAL 0x1

#endif
#if 1
static struct timespec timespec;
static struct rtc_time time;

typedef struct _touchscreen_log{
	//struct rtc_time tm;
	int line;
	//char space[1];
	int month;
	char date[1];
	int day;
	//char space1[1];
	int hour;
	//char time1[1];
	int min;
	//char time2[1];
	int second;
	int nanosecond;
	int finger_id;
	int finger_status;
	//char space2[1];
	//char fingers[2];
	int pointer_nums;
}touchscreen_log;
#define touchscreen_log_max 2000
static int touchscreen_log_num=0;
static touchscreen_log touchscreen_logs[touchscreen_log_max]={};
#define NR_FINGERS	10
static DECLARE_BITMAP(pre_fingers, NR_FINGERS);
static int add_log = 0;
#endif
static struct regulator *vdd, *vbus;
char *ftc_fwfile_table[FTC_MOUDLE_NUM_MAX]=
{
FTC_TPK_FW_NAME,
FTC_TURLY_FW_NAME,
FTC_SUCCESS_FW_NAME,
FTC_OFILM_FW_NAME,
FTC_LEAD_FW_NAME,
FTC_WINTEK_FW_NAME,
FTC_LAIBAO_FW_NAME,
FTC_CMI_FW_NAME,
FTC_ECW_FW_NAME,
FTC_GOWORLD_FW_NAME,
FTC_BAOMING_FW_NAME,
FTC_JUNDA_FW_NAME,
FTC_JIAGUAN_FW_NAME,
FTC_MUDONG_FW_NAME,
FTC_EACHOPTO_FW_NAME,
FTC_AVC_FW_NAME
};
static int touch_moudle;
int focaltech_get_fw_ver(struct i2c_client *client, char *pfwfile );
int focaltech_fwupdate(struct i2c_client *client, char *pfwfile );

int ftc_update_flag=0;

#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif

#ifdef FT5336_DOWNLOAD
#include "ft5336_download_lib.h"
static struct i2c_client *g_i2c_client = NULL;
static unsigned char CTPM_MAIN_FW[]=
{
	#include "ft5336_all.i"
};
#endif

#ifdef SYSFS_DEBUG
#include "ft5x06_ex_fun.h"
#endif

struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS];	/*x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];	/*y coordinate */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];	/*touch event:
					0 -- down; 1-- contact; 2 -- contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];	/*touch ID */
	u16 pressure;
	u8 touch_point;
};

#ifdef CONFIG_BOARD_GIANT
#define FT_ESD_PROTECT  //for tp esd check
#endif

struct ft5x0x_ts_data {
	unsigned int irq;
	unsigned int x_max;
	unsigned int y_max;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct ts_event event;
	struct ft5x0x_platform_data *pdata;
	struct work_struct  work;	
	#ifdef CONFIG_PM
	struct early_suspend *early_suspend;
	#endif
#ifdef FT_ESD_PROTECT
	spinlock_t esd_lock;
	u8	esd_running;
#endif

	struct mutex lock;	
};

#define ANDROID_INPUT_PROTOCOL_B

//#define FT5X0X_RESET_PIN	0//S5PV210_GPB(2)
//#define FT5X0X_RESET_PIN_NAME	"ft5x0x-reset"



/*
*ft5x0x_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/

#ifdef FT_ESD_PROTECT
	
#define TPD_ESD_CHECK_CIRCLE        200
	static struct delayed_work gtp_esd_check_work;
	static struct workqueue_struct *gtp_esd_check_workqueue = NULL;
	static void gtp_esd_check_func(struct work_struct *);
	static int count_irq = 0; //add for esd
	static unsigned long esd_check_circle = TPD_ESD_CHECK_CIRCLE;
	static u8 run_check_91_register = 0;
	static int use_esd_check=0;
	static int in_update_mode=0;
	void ft_esd_switch(struct i2c_client *, s32);
	
#endif

int ft5x0x_i2c_Read(struct i2c_client *client, char *writebuf,
		    int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/
int ft5x0x_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);

	return ret;
}
#ifdef FT5336_DOWNLOAD
int ft5x0x_download_i2c_Read(unsigned char *writebuf,
		    int writelen, unsigned char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = 0x38,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = 0x38,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(g_i2c_client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&g_i2c_client->dev, "f%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = 0x38,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(g_i2c_client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&g_i2c_client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/
int ft5x0x_download_i2c_Write(unsigned char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
		 .addr = 0x38,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(g_i2c_client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&g_i2c_client->dev, "%s i2c write error.\n", __func__);

	return ret;
}

#endif
/*Read touch point information when the interrupt  is asserted.*/
static int ft5x0x_read_Touchdata(struct ft5x0x_ts_data *data)
{
	struct ts_event *event = &data->event;
	u8 buf[POINT_READ_BUF] = { 0 };
#if 0//def CONFIG_BOARD_EOS	
	u16 pointtemp;
#endif
	int ret = -1;
	int i = 0;
	u8 pointid = FT_MAX_ID;
	u8 pointnumber; //add atb
	static u8 last_touchpoint=0;  // add  for up issue @20140529

	ret = ft5x0x_i2c_Read(data->client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) {
		dev_err(&data->client->dev, "%s read touchdata failed.\n",
			__func__);
		return ret;
	}
	memset(event, 0, sizeof(struct ts_event));

	event->touch_point = 0;
	
	pointnumber = buf[2]; //add atb 
	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) {
		pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		if (pointid >= FT_MAX_ID)
			break;
		else
			event->touch_point++;
		event->au16_x[i] =
		    (s16) (buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i];
		event->au16_y[i] =
		    (s16) (buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i];
		event->au8_touch_event[i] =
		    buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
		event->au8_finger_id[i] =
		    (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;

		if((event->au16_x[i]>data->x_max)||
		//(event->au16_y[i]>data->y_max)||
		(event->au8_finger_id[i]>CFG_MAX_TOUCH_POINTS)){
			printk("%s warning: you get larger number and you should check your fw!\n",__func__);
			return -1;
		}
		
	}

	event->pressure = FT_PRESS;
	if((last_touchpoint>0)&&(pointnumber ==0))	  // add for up issue @20140529 
	{	 /* release all touches */ 
		 for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) 
		 { 
			  input_mt_slot(data->input_dev, i); 
			  input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0); 
		 } 
		 last_touchpoint=0; 
		 input_report_key(data->input_dev, BTN_TOUCH, 0); 
		 input_sync(data->input_dev); 
	} 
		last_touchpoint= pointnumber; // add  for up issue @20140529

	return 0;
}

/*
*report the point information
*/
static void ft5x0x_report_value(struct ft5x0x_ts_data *data)
{
	struct ts_event *event = &data->event;
	int i;
	int uppoint = 0;
	unsigned char touch_count = 0;
	unsigned char finger_status;
	
	mutex_lock(&data->lock);
	/*protocol B*/	
	for (i = 0; i < event->touch_point; i++)
	{
		input_mt_slot(data->input_dev, event->au8_finger_id[i]);
		
		if(event->au8_touch_event[i]==0||event->au8_touch_event[i]==2){
			if(!test_bit(event->au8_finger_id[i], pre_fingers))
				add_log = 1;				
			__set_bit(event->au8_finger_id[i], pre_fingers);
			}
		else if(test_bit(event->au8_finger_id[i], pre_fingers)){
			__clear_bit(event->au8_finger_id[i], pre_fingers);
			add_log = 1;
			}		
		if (event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2)
		{
			finger_status=1;
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
				true);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					event->pressure);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					event->au16_x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					event->au16_y[i]);
			touch_count++;
		}
		else
		{
			finger_status=0;			
			uppoint++;
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
				false);
		}
		if(add_log==1){
    		//printk("finger up and the pointer num is :%d\n",touch_count);
    		add_log = 0;
    		timespec= current_kernel_time();
    		rtc_time_to_tm(timespec.tv_sec, &time);
    		touchscreen_logs[touchscreen_log_num].line=touchscreen_log_num+1;
    		//strcpy(touchscreen_logs[touchscreen_log_num].space," ");
    		touchscreen_logs[touchscreen_log_num].month=time.tm_mon;
    		touchscreen_logs[touchscreen_log_num].day=time.tm_mday;
    		touchscreen_logs[touchscreen_log_num].hour=time.tm_hour;
    		touchscreen_logs[touchscreen_log_num].min=time.tm_min;
    		touchscreen_logs[touchscreen_log_num].second=time.tm_sec;
    		touchscreen_logs[touchscreen_log_num].nanosecond=timespec.tv_nsec;
    		//strcpy(touchscreen_logs[touchscreen_log_num].date,"-");
    		//strcpy(touchscreen_logs[touchscreen_log_num].space1," ");
    		//strcpy(touchscreen_logs[touchscreen_log_num].time1,":");
    		//strcpy(touchscreen_logs[touchscreen_log_num].time2,":");
    		//strcpy(touchscreen_logs[touchscreen_log_num].space2," ");
    		//strcpy(touchscreen_logs[touchscreen_log_num].fingers,"f:");
    		touchscreen_logs[touchscreen_log_num].finger_id=event->au8_finger_id[i];
    		touchscreen_logs[touchscreen_log_num].finger_status=finger_status;		
    		touchscreen_logs[touchscreen_log_num].pointer_nums=touch_count;
    		touchscreen_log_num++;
    		if (touchscreen_log_num>=touchscreen_log_max)
    			touchscreen_log_num=0;
		}		
	}

	if(event->touch_point == uppoint)
		input_report_key(data->input_dev, BTN_TOUCH, 0);
	else
		input_report_key(data->input_dev, BTN_TOUCH, event->touch_point > 0);
	input_sync(data->input_dev);
	mutex_unlock(&data->lock);


}
static void focaltech_ts_work_func(struct work_struct *work)
{
	int ret;
	//struct focaltech_ts_data *ts = container_of(work, struct focaltech_ts_data, work);
		ret = ft5x0x_read_Touchdata(ft_ts);
	if (ret == 0)
		ft5x0x_report_value(ft_ts);
	
}
/*The ft5x0x device will signal the host about TRIGGER_FALLING.
*Processed when the interrupt is asserted.
*/
static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x0x_ts_data *ft5x0x_ts = dev_id;
	//int ret = 0;
	disable_irq_nosync(ft5x0x_ts->irq);
	#if defined FT_ESD_PROTECT
	count_irq++;
	#endif
	//pr_info("interrupt is coming\n");
	queue_work(focaltech_wq, &ft5x0x_ts->work);


	enable_irq(ft5x0x_ts->irq);

	return IRQ_HANDLED;
}

void ft5x0x_reset_tp(int HighOrLow)
{
	pr_info("set tp reset pin to %d\n", HighOrLow);
	gpio_direction_output(GPIO_TS_RST, HighOrLow);
}

void ft5x0x_Enable_IRQ(struct i2c_client *client, int enable)
{
	if (1 == enable)
		enable_irq(client->irq);
	else
		disable_irq_nosync(client->irq);
}
#if 0
static int fts_init_gpio_hw(struct ft5x0x_ts_data *ft5x0x_ts)
{

	int ret = 0;
	
	ret = gpio_request(FT5X0X_RESET_PIN, FT5X0X_RESET_PIN_NAME);
	if (ret) {
		pr_err("%s: request GPIO %s for reset failed, ret = %d\n",
				__func__, FT5X0X_RESET_PIN_NAME, ret);
		return ret;
	}
	//s3c_gpio_cfgpin(FT5X0X_RESET_PIN, S3C_GPIO_OUTPUT);
	gpio_tlmm_config(GPIO_CFG(FT5X0X_RESET_PIN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	
	gpio_direction_output(FT5X0X_RESET_PIN, 1);

	return ret;
}

static void fts_un_init_gpio_hw(struct ft5x0x_ts_data *ft5x0x_ts)
{
	gpio_free(FT5X0X_RESET_PIN);
}
#endif
#ifdef FT5336_DOWNLOAD

int ft5336_Enter_Debug(void)
{
	ft5x0x_reset_tp(0);
	msleep(4);
	ft5x0x_reset_tp(1);
	return ft5336_Lib_Enter_Download_Mode();
}
//if return 0, main flash is ok, else download.
int ft5336_IsDownloadMain(void)
{
	//add condition to check main flash
	return -1;
}
int ft5336_DownloadMain(void)
{
	unsigned short fwlen = 0;
	if (ft5336_Enter_Debug() < 0) {
		pr_err("-----enter debug mode failed\n");
		return -1;
	}
	fwlen = sizeof(CTPM_MAIN_FW);
	pr_info("----fwlen=%d\n", fwlen);

	//return ft6x06_Lib_DownloadMain(CTPM_MAIN_FW, fwlen);
	return ft5336_Lib_DownloadMain(CTPM_MAIN_FW, fwlen);
}
#endif
static void touchscreen_reset( int hl ,unsigned gpio)
{
	gpio_direction_output(gpio, hl);
	return;
}
#if 1//defined (CONFIG_TOUCHSCREEN_FOCALTECH_USBNOTIFY)
static int usb_plug_status=0;

static int focaltech_ts_event(struct notifier_block *this, unsigned long event,void *ptr)
{
	int ret;

	switch(event)
	{
	case 0:
		//offline
		if ( usb_plug_status != 0 ){
	 		usb_plug_status = 0;
			printk("ts config change to offline status\n");
			i2c_smbus_write_byte_data( ft_ts->client, CHAGER_IN_REG,CHAGER_OUT_VAL);
		}
		break;
	case 1:
		//online
		if ( usb_plug_status != 1 ){
	 		usb_plug_status = 1;
			printk("ts config change to online status\n");
			i2c_smbus_write_byte_data( ft_ts->client, CHAGER_IN_REG,CHAGER_IN_VAL);
		}
		break;
	default:
		break;
	}

	ret = NOTIFY_DONE;

	return ret;
}

static struct notifier_block ts_notifier = {
	.notifier_call = focaltech_ts_event,
};


static BLOCKING_NOTIFIER_HEAD(ts_chain_head);

int focaltech_register_ts_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ts_chain_head, nb);
}
EXPORT_SYMBOL_GPL(focaltech_register_ts_notifier);

int focaltech_unregister_ts_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ts_chain_head, nb);
}
EXPORT_SYMBOL_GPL(focaltech_unregister_ts_notifier);

int focaltech_ts_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&ts_chain_head, val, NULL)
			== NOTIFY_BAD) ? -EINVAL : 0;
}

#endif

static int detect_device(struct i2c_client *i2c_client)
{
	int retry;//ret;
	signed int buf;
	
	retry = 3;
	while (retry-- > 0)
	{
		buf = i2c_smbus_read_byte_data(i2c_client, FT5x0x_REG_FW_VER);
		if ( buf >= 0 ){
			return true;
		}
		msleep(10);
	}
	printk("wly: focaltech touch is not exsit.\n");
	return false;

}

static int touchscreen_gpio_init(struct device *dev, int flag,char *vreg_vdd,char *vreg_vbus,
unsigned reset_gpio,unsigned irq_gpio)
{
	int ret = -EINVAL;


	//init
	if ( flag == 1 )
	{
		vdd = vbus = NULL;

		vdd = regulator_get(dev, vreg_vdd);
		if (IS_ERR(vdd)) {
			pr_err("%s get failed\n", vreg_vdd);
			return -1;
		}
		if ( regulator_set_voltage(vdd, 2850000,2850000) ){   //3000000,3000000
			pr_err("%s set failed\n", vreg_vdd);
			return -1;
		}

		vbus =regulator_get(dev, vreg_vbus);
		if (IS_ERR(vbus)) {
			pr_err("%s get failed\n", vreg_vbus);
			return -1;
		}

		if ( regulator_set_voltage(vbus, 1800000,1800000)) {
			pr_err(" %s set failed\n", vreg_vbus);
			//return -1;
		}

		ret = gpio_request(reset_gpio, "touch reset");
		if (ret){
			pr_err(" gpio %d request is error!\n", reset_gpio);
			return -1;
		}

		ret = gpio_request(irq_gpio, "touch irq");
		if (ret){
			pr_err("gpio %d request is error!\n", irq_gpio);
			return -1;
		}

	}


	//deinit
	if ( flag == 0)
	{
		regulator_put(vdd);
		regulator_put(vbus);
		gpio_free(GPIO_TS_IRQ);
		gpio_free(GPIO_TS_RST);
	}

	return 0;

}

static void touchscreen_power( int on )
{
	int rc = -EINVAL;
	pr_info("ftc %s: on=%d\n", __func__, on);
	
	if ( !vdd || !vbus )
		return;
	
	if (on){
		rc = regulator_enable(vdd);
		if (rc) {
			pr_err("vdd enable failed\n");
			return;
		}
		rc = regulator_enable(vbus);
		if (rc) {
			pr_err("vbus enable failed\n");
			return;
		}
	}
	else 
	{
		rc = regulator_disable(vdd);
		if (rc) {
			pr_err("vdd disable failed\n");
			return;
		}
		rc = regulator_disable(vbus);
		if (rc) {
			pr_err("vbus disable failed\n");
			return;
		}
	}

	return;
}

static int ft_parse_dt(struct device *dev, struct ft5x0x_platform_data *pdata)
{
	int rc;	
	pdata->irq_gpio			= GPIO_TS_IRQ;
	//pdata->reset_delay_ms	= 100;
	pdata->reset_gpio		= GPIO_TS_RST;
	pdata->vdd				= VREG_VDD;
	pdata->vbus				= VREG_VBUS;
	pdata->irq_flags		= IRQF_TRIGGER_FALLING;	

	rc = of_property_read_u32(dev->of_node, "ft,max_x", &pdata->x_max);
	if (rc) {
		dev_err(dev, "Failed to read display max_x !!!\n");
		//pdata->maxy_offset = 100;
		return -EINVAL;
	}
	rc = of_property_read_u32(dev->of_node, "ft,max_y", &pdata->y_max);
	if (rc) {
		dev_err(dev, "Failed to read display max_y !!!\n");
		//pdata->maxy_offset = 100;
		return -EINVAL;
	}	
	printk("%s: max_x:%d max_y:%d\n", __func__, pdata->x_max,pdata->y_max);
	
	return 0;
}

static void focaltech_get_vid(
	struct i2c_client *client,
	char *p_vid,
	int *p_fw_ver )
{
	char write_buf[]={0};
	char buf1, buf2;

	if ( !client )
		return;
	write_buf[0]=FT5X0X_REG_FT5201ID;
	ft5x0x_i2c_Read(client, write_buf,1, &buf1, 1);
	//buf1 = i2c_smbus_read_byte_data(client, FT5X0X_REG_FT5201ID);
	write_buf[0]=FT5x0x_REG_FW_VER;
	ft5x0x_i2c_Read(client, write_buf,1, &buf2, 1);	
	//buf2 = i2c_smbus_read_byte_data( client, FT5x0x_REG_FW_VER);
#if defined (CONFIG_TOUCHSCREEN_UP_TIMER_FT)
	focaltech_fw_support_up_timer_flag = buf2>>7;
	buf2 = buf2&0x7F;
	printk("%s: focaltech_fw_support_up_timer_flag=%d\n", __func__, focaltech_fw_support_up_timer_flag);
#endif
	pr_info("vendor id = 0x%x, fw version = 0x%x\n", buf1, buf2);
	
	//g_zte_vid = buf1;
	//g_zte_fw_ver = buf2;

	if ( !p_vid || !p_fw_ver )
		return;

	switch (buf1){
	case 0x57:// ³¬ÉùGoworld
		sprintf( p_vid, "Goworld(0x%x)", buf1 );
		touch_moudle=GOWORLD;
		break;
	case 0x51:// Å··Æ¹âofilm
		sprintf( p_vid, "ofilm(0x%x)", buf1  );
		touch_moudle=OFILM;
		break;
	case 0x53://ÄÁ¶«
		sprintf( p_vid, "mudong(0x%x)", buf1  );
		touch_moudle=MUDONG;
		break;
	case 0x55:// À³±¦
		sprintf( p_vid, "laibao(0x%x)", buf1  );
		touch_moudle=LAIBAO;
		break;
	case 0x5a://
		sprintf( p_vid, "TRULY(0x%x)", buf1  );
		touch_moudle=TRULY;
		break;
	case 0x5f:// ÓîË³
		sprintf( p_vid, "success(0x%x)", buf1  );
		touch_moudle=SUCCESS;
		break;
	case 0x60:// Á¢µÂ
		sprintf( p_vid, "lead(0x%x)", buf1  );
		touch_moudle=LEAD;
		break;
	case 0x5d:// ±¦Ã÷
		sprintf( p_vid, "BM(0x%x)", buf1  );
		touch_moudle=BAOMING;
		break;
	case 0x80:// ±¦Ã÷
		sprintf( p_vid, "EACH(0x%x)", buf1  );
		touch_moudle=EACHOPTO;
		break;		
	case 0x8E:// ÆæÃÀ
		sprintf( p_vid, "AVC(0x%x)", buf1  );
		touch_moudle=AVC;
		break;		
	case 0x8f:// ÆæÃÀ
		sprintf( p_vid, "CMI(0x%x)", buf1  );
		touch_moudle=CMI;
		break;
		
	case 0xA5:
		sprintf( p_vid, "jiaguan(0x%x)", buf1  );
		touch_moudle=JIAGUAN;
		break;
	default:
		sprintf( p_vid, "unknown(0x%x)", buf1  );
		touch_moudle=UNKNOW;
		break;
	}		

	*p_fw_ver = buf2;

	pr_info("vendor: %s, fw =0x%x \n", p_vid, *p_fw_ver);

}
static int
proc_log_read_val(char *page, char **start, off_t off, int count, int *eof,
			  void *data)
{
	static int i=0 ;
	int len = 0;
	if(off==0)
		i=0;

	off=len;

	for(;i<touchscreen_log_max;i++){
	if (touchscreen_logs[i].line==0){
		//i=0;
		break;
	}		
	len +=sprintf(page + len, "%4d %2d-%2d %2d:%2d:%2d.%-9d finger_num:%2d finger_id:%2d status:%s\n",
	touchscreen_logs[i].line,
	touchscreen_logs[i].month,
	touchscreen_logs[i].day,
	touchscreen_logs[i].hour,
	touchscreen_logs[i].min,
	touchscreen_logs[i].second,
	touchscreen_logs[i].nanosecond,
	touchscreen_logs[i].pointer_nums,
	touchscreen_logs[i].finger_id,
	(touchscreen_logs[i].finger_status>2)?"resume":(touchscreen_logs[i].finger_status?"dowm":"up")
	);
	//printk("len:%d off:%d count:%d\n",len,(int)off,count);

	if(len>=count+off)
		break;
}	
	//if (i>=touchscreen_log_max)
	//	i=0;
	
	if (off + count >= len){
		//printk("off:%d count:%d len:%d the file end\n",(int)off,count,len);
		*eof = 1;

}
	if (len < off)
		return 0;

	*start = page + off;
	return ((count < len - off) ? count : len - off);
}

static int
proc_read_val(char *page, char **start, off_t off, int count, int *eof,
	  void *data)
{
	int len = 0;
	int buf = 0;
	int ver = -1;
	char vid[16];

	len += sprintf(page + len, "manufacturer : %s\n", "Focaltech");
	len += sprintf(page + len, "chip type : %s\n", "FT5XX6");
	len += sprintf(page + len, "i2c address : %02x\n", 0x3e);

	focaltech_get_vid( ft_ts->client, (char *)vid, &buf );
	if(touch_moudle<FTC_MOUDLE_NUM_MAX)
	ver=focaltech_get_fw_ver(ft_ts->client,ftc_fwfile_table[touch_moudle]);
	len += sprintf(page + len, "module : %s\n", vid);
	len += sprintf(page + len, "fw version : %02x\n", buf );
#if 1//def CONFIG_TOUCHSCREEN_FOCALTECH_FW
	len += sprintf(page + len, "update flag : 0x%x\n", ftc_update_flag);
#endif
	//len += sprintf(page + len, "lastest flag : %x\n", zte_fw_latest(ver));
	//len = zte_fw_info_show(page, len);
	if(ver!=-1){
		//len += sprintf(page + len, "need update : %s\n", (ready_fw_ver>fw_ver?"yes":"no"));
		len += sprintf(page + len, "need update : %s\n", (ver>buf?"yes":"no"));
		len += sprintf(page + len, "ready fw version : %02x\n", ver );
	}else
	{
		len += sprintf(page + len, "no fw to update\n");
	}

	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	return ((count < len - off) ? count : len - off);
}

static int proc_write_val(struct file *file, const char *buffer,
           unsigned long count, void *data)
{
	unsigned long val;
	sscanf(buffer, "%lu", &val);

#if 1//def CONFIG_TOUCHSCREEN_FOCALTECH_FW

	printk("ftc Upgrade Start++++++++\n");
	ftc_update_flag=0;
	if(touch_moudle>=FTC_MOUDLE_NUM_MAX)
	{
		printk("touchscreen moudle unknow!");
		ftc_update_flag = 1;
		return -EINVAL;
	}    
	#ifdef FT_ESD_PROTECT
	in_update_mode=1;
	if(use_esd_check==1)
	ft_esd_switch(ft_ts->client,0);

	#endif
	if ( focaltech_fwupdate( ft_ts->client, ftc_fwfile_table[touch_moudle] ) < 0 )
	{
		printk("****ftc fw update failed!\n");
		ftc_update_flag=1;		
		return -EINVAL;
	}
	else
	{
		ftc_update_flag=2;
		pr_info("ftc fw update OK! \n" );
	}
#endif

#ifdef FT_ESD_PROTECT
	in_update_mode=0;
	if(use_esd_check==1)
	ft_esd_switch(ft_ts->client,1);

#endif


	return -EINVAL;
 	//return 0;
}
#ifdef FT_ESD_PROTECT
void ft_esd_switch(struct i2c_client *client, s32 on)
{
    struct ft5x0x_ts_data *ts;
    
    ts = i2c_get_clientdata(client);
    spin_lock(&ts->esd_lock);
    
    if (1 == on)     // switch on esd 
    {
        if (!ts->esd_running)
        {
            ts->esd_running = 1;
            spin_unlock(&ts->esd_lock);
            printk("Esd started");
            queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
        }
        else
        {
            spin_unlock(&ts->esd_lock);
        }
    }
    else    // switch off esd
    {
        if (ts->esd_running)
        {
            ts->esd_running = 0;
            spin_unlock(&ts->esd_lock);
            printk("Esd cancelled");
            cancel_delayed_work_sync(&gtp_esd_check_work);
        }
        else
        {
            spin_unlock(&ts->esd_lock);
        }
    }
}
#endif
static int ft5x0x_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct ft5x0x_platform_data *pdata; 
	struct ft5x0x_ts_data *ft5x0x_ts;
	struct input_dev *input_dev;
	struct proc_dir_entry *dir, *refresh;	
	int err = 0;
	int buf = 0;
	char vid[16];
	unsigned char uc_reg_value;
	unsigned char uc_reg_addr;
	pdata = kzalloc(sizeof(struct ft5x0x_platform_data), GFP_KERNEL);		
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -EINVAL;
	}
	err=ft_parse_dt(&client->dev, pdata);
		if(err<0)
			goto err_devm;	
		
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft5x0x_ts = kzalloc(sizeof(struct ft5x0x_ts_data), GFP_KERNEL);

	if (!ft5x0x_ts) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	i2c_set_clientdata(client, ft5x0x_ts);

	ft5x0x_ts->irq = gpio_to_irq(pdata->irq_gpio);//client->irq;	
	ft5x0x_ts->client = client;
	ft5x0x_ts->pdata = pdata;
	ft5x0x_ts->x_max = pdata->x_max - 1;
	ft5x0x_ts->y_max = pdata->y_max - 1;
	ft5x0x_ts->pdata->reset = GPIO_TS_RST;
	ft5x0x_ts->pdata->irq = ft5x0x_ts->irq;
	ft_ts=ft5x0x_ts;
	client->irq = ft5x0x_ts->irq;
	pr_info("irq = %d\n", client->irq);
	//pr_info("FT5X0X_INT_PIN = %d\n", FT5X0X_INT_PIN);

	//if(fts_init_gpio_hw(ft5x0x_ts)<0)
	//	goto exit_init_gpio;	
#ifdef CONFIG_PM
#if 0
	err = gpio_request(pdata->reset, "ft5x0x reset");
	if (err < 0) {
		dev_err(&client->dev, "%s:failed to set gpio reset.\n",
			__func__);
		goto exit_request_reset;
	}
	#endif
#endif
		err=touchscreen_gpio_init(&client->dev,1,pdata->vdd,pdata->vbus,
		pdata->reset_gpio,pdata->irq_gpio);
		if ( err < 0 ){
			pr_err("%s, gpio init failed! %d\n", __func__, err);
			goto err_regulator;
		}


		touchscreen_reset(0,pdata->reset_gpio);
		touchscreen_power(1);
		msleep(5);		
		touchscreen_reset(1,pdata->reset_gpio);
		msleep(300);


	if ( !detect_device( client )){
		pr_info("%s, device is not exsit.\n", __func__);
		err=-EIO;
		goto err_detect;
	}


	focaltech_get_vid(client, (char *)vid, &buf );	
	#ifdef FT_ESD_PROTECT
	printk("buf:0x%x vid:%s\n",buf,vid);
	if (buf>=0x11&&!strcmp(vid,"ofilm(0x51)")){
		printk("use the esd check function\n");
		use_esd_check=1;
		}
	#endif
		
	focaltech_wq= create_singlethread_workqueue("focaltech_wq");
	if(!focaltech_wq){
		err = -ESRCH;
		pr_err("%s: creare single thread workqueue failed!\n", __func__);
		goto err_create_singlethread;
	}

	INIT_WORK(&ft5x0x_ts->work, focaltech_ts_work_func);
	mutex_init(&ft5x0x_ts->lock);

	err = request_threaded_irq(client->irq, NULL, ft5x0x_ts_interrupt,
				   IRQF_TRIGGER_FALLING, client->dev.driver->name,
				   ft5x0x_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x0x_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}
	disable_irq(client->irq);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	ft5x0x_ts->input_dev = input_dev;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	
	input_mt_init_slots(input_dev, CFG_MAX_TOUCH_POINTS);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, ft5x0x_ts->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, ft5x0x_ts->y_max, 0, 0);

	input_dev->name = FT5X0X_NAME;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
			"ft5x0x_ts_probe: failed to register input device: %s\n",
			dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}
	/*make sure CTP already finish startup process */
	msleep(150);
#ifdef SYSFS_DEBUG
		ft5x0x_create_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
		if (ft_rw_iic_drv_init(client) < 0)
			dev_err(&client->dev, "%s:[FTS] create fts control iic driver failed\n",
					__func__);
#endif
#ifdef FTS_APK_DEBUG
	ft5x0x_create_apk_debug_channel(client);
#endif

#ifdef FT5336_DOWNLOAD
			g_i2c_client = client;
			FTS_I2c_Read_Function fun_i2c_read = ft5x0x_download_i2c_Read;
			FTS_I2c_Write_Function fun_i2c_write = ft5x0x_download_i2c_Write;
			Init_I2C_Read_Func(fun_i2c_read);
			Init_I2C_Write_Func(fun_i2c_write);
			 if(ft5336_IsDownloadMain() < 0) {
	 	#if 1
				pr_info("--------FTS---------download main\n");
				if(ft5336_DownloadMain()<0)
				{
					pr_err("---------FTS---------Download main failed\n");
				}
		#endif
			 } else
				pr_info("--------FTS---------no download main\n");
#endif


	/*get some register information */
	uc_reg_addr = FT5x0x_REG_FW_VER;
	ft5x0x_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	//dev_dbg(&client->dev, "[FTS] Firmware version = 0x%x\n", uc_reg_value);
	pr_info( "[FTS] Firmware version = 0x%x\n", uc_reg_value);

	uc_reg_addr = FT5x0x_REG_POINT_RATE;
	ft5x0x_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	//dev_dbg(&client->dev, "[FTS] report rate is %dHz.\n",
	//	uc_reg_value * 10);
	pr_info("[FTS] report rate is %dHz.\n", uc_reg_value * 10);

	uc_reg_addr = FT5X0X_REG_THGROUP;
	ft5x0x_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	//dev_dbg(&client->dev, "[FTS] touch threshold is %d.\n",
	//	uc_reg_value * 4);
	pr_info("[FTS] touch threshold is %d.\n", uc_reg_value * 4);
	
	dir = proc_mkdir("touchscreen", NULL);
	refresh = create_proc_entry("ts_information", 0664, dir);
	if (refresh) {
		refresh->data		= NULL;
		refresh->read_proc  = proc_read_val;
		refresh->write_proc = proc_write_val;
	}
	printk("touchscreen_logs address:%x\n",(unsigned int)&touchscreen_logs[0]);
	refresh = create_proc_entry("ts_log", 0664, dir);
	if (refresh) {
		refresh->data 	  = NULL;
		refresh->read_proc  = proc_log_read_val;
		refresh->write_proc = NULL;
	}	
#ifdef FT_ESD_PROTECT
		if(use_esd_check==1){
			ft5x0x_ts->esd_running=1;
			spin_lock_init(&ft5x0x_ts->esd_lock);
			INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
			gtp_esd_check_workqueue = create_workqueue("ft_esd_check");
			queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
			}
#endif

	touchscreen_suspend_pm=ft5x0x_ts_suspend_pm;
	touchscreen_resume_pm=ft5x0x_ts_resume_pm;
#if 1//defined(CONFIG_TOUCHSCREEN_FOCALTECH_USBNOTIFY)
		focaltech_register_ts_notifier(&ts_notifier);
#endif


	enable_irq(client->irq);
	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);

exit_input_dev_alloc_failed:
	free_irq(client->irq, ft5x0x_ts);
#ifdef CONFIG_PM
#if 0
exit_request_reset:
	gpio_free(ft5x0x_ts->pdata->reset);
#endif
#endif


exit_irq_request_failed:
	mutex_destroy(&ft5x0x_ts->lock);
	destroy_workqueue(focaltech_wq);

	
err_create_singlethread:	
	i2c_set_clientdata(client, NULL);
	kfree(pdata);pdata=NULL;	
	kfree(ft5x0x_ts);

err_detect:
	touchscreen_power(0);
	touchscreen_gpio_init(&client->dev,0,pdata->vdd,pdata->vbus,
				pdata->reset_gpio,pdata->irq_gpio);
	
err_regulator:
	//fts_un_init_gpio_hw(ft5x0x_ts);//HUANGJINYU

//exit_init_gpio:	
exit_alloc_data_failed:
exit_check_functionality_failed:
err_devm:
	if(pdata!=NULL)
	kfree(pdata);
	return err;
}
static void ft5x0x_report_release_fingers(struct ft5x0x_ts_data *data)
{
	struct ts_event *event = &data->event;
	int i;

	mutex_lock(&data->lock);
	/*protocol B*/	
	for (i = 0; i < event->touch_point; i++)
	{
		input_mt_slot(data->input_dev, event->au8_finger_id[i]);
		
		if (event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2)
		{
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
				false);
		}

	}

	input_sync(data->input_dev);
	mutex_unlock(&data->lock);

}

static int release_all_fingers(struct ft5x0x_ts_data *data)
{
	ft5x0x_read_Touchdata(data);
	ft5x0x_report_release_fingers(data);
	return 0;
}
#if 1//def CONFIG_PM
static int ft5x0x_ts_suspend_pm(void)
	{
		int ret = 0;

		disable_irq(ft_ts->client->irq);
		
#if defined (CONFIG_TOUCHSCREEN_UP_TIMER_FT)
		if(focaltech_fw_support_up_timer_flag)
		{
			hrtimer_cancel(&ts->up_timer);
			printk("%s: cancel up_timer!!! \n", __func__);
		}
#endif
#ifdef FT_ESD_PROTECT
	if(use_esd_check==1)
    //cancel_delayed_work_sync(&gtp_esd_check_work);
	ft_esd_switch(ft_ts->client,0);
#endif		
		tpd_halt = 1;

		ret = cancel_work_sync(&ft_ts->work);
		//if(ret & ts->use_irq)
			//enable_irq(ft_ts->client->irq);
	
		//if ( ft_ts->->irq )
		//	ts->irq(1, false);

		release_all_fingers(ft_ts);		
		gpio_direction_output(ft_ts->pdata->irq_gpio,1);	
	
		i2c_smbus_write_byte_data(ft_ts->client, 0xa5, 0x03);
	
#ifdef CONFIG_TOUCHSCREEN_RESUME_LOG
		if(focaltech_resume_flag)
			focaltech_log3();
#endif
		
		return 0;
	}


static int ft5x0x_ts_resume_pm(void)
{
	//struct ft5x0x_ts_data *ts = container_of(handler, struct ft5x0x_ts_data,
	//					early_suspend);

	//dev_dbg(&ft_ts->client->dev, "[FTS]ft5x0x resume.\n");
	//gpio_direction_output(ft_ts->pdata->reset, 0);
	//msleep(20);
	//gpio_direction_output(ft_ts->pdata->reset, 1);
	//enable_irq(ft_ts->pdata->irq);
	//return 0;
	//signed int buf=-1;
	//uint8_t retry=0;
	//struct focaltech_ts_data *ts = i2c_get_clientdata(client);
#if 0

focaltech_resume_start:	

	//if ( ts->irq ){
		gpio_direction_output(ft_ts->pdata->irq_gpio,0);	
		//ts->irq(0, false); 
		msleep(3);
		gpio_direction_output(ft_ts->pdata->irq_gpio,1);		
		//ts->irq(1, false); 
		msleep(220);
		gpio_direction_input(ft_ts->pdata->irq_gpio);				
		//ts->irq(1, true);
	//}


	//fix bug: fts failed set reg when usb plug in under suspend mode
#if 1//defined(CONFIG_TOUCHSCREEN_FOCALTECH_USBNOTIFY)
	if(usb_plug_status==1)
		i2c_smbus_write_byte_data( ft_ts->client, CHAGER_IN_REG,CHAGER_IN_VAL);
	else 
		i2c_smbus_write_byte_data( ft_ts->client, CHAGER_IN_REG,CHAGER_OUT_VAL);
#endif

	buf = i2c_smbus_read_byte_data(ft_ts->client, 0xa6 );
	printk("ts buf :%d\n",buf);
	if ( buf <0)
	{
		printk("%s: Fts FW ID read Error: retry=0x%X\n", __func__, retry);
		if ( ++retry < 3 ) goto focaltech_resume_start;
	}
	if (retry>=2){
		printk("reset the touchscreen\n");
		gpio_direction_output(ft_ts->pdata->reset_gpio,0);
		msleep(30);
		gpio_direction_output(ft_ts->pdata->reset_gpio,1);		
	}
	#else
		printk("reset the touchscreen\n");
		gpio_direction_output(ft_ts->pdata->reset_gpio,0);
		msleep(30);
		gpio_direction_output(ft_ts->pdata->reset_gpio,1);	
		msleep(200);
		gpio_direction_input(ft_ts->pdata->irq_gpio);						
	#endif
	//release_all_fingers(ts);
	enable_irq(ft_ts->client->irq);
	tpd_halt = 0;
#ifdef FT_ESD_PROTECT
	if(use_esd_check==1)

    //queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
	ft_esd_switch(ft_ts->client,1);

#endif	
#ifdef CONFIG_TOUCHSCREEN_RESUME_LOG
	focaltech_resume_flag=true;
#endif
#if 1
	 timespec= current_kernel_time();
	 rtc_time_to_tm(timespec.tv_sec, &time);
	 touchscreen_logs[touchscreen_log_num].line=touchscreen_log_num+1;
	 //strcpy(touchscreen_logs[touchscreen_log_num].space," ");
	 touchscreen_logs[touchscreen_log_num].month=time.tm_mon;
	 touchscreen_logs[touchscreen_log_num].day=time.tm_mday;
	 touchscreen_logs[touchscreen_log_num].hour=time.tm_hour;
	 touchscreen_logs[touchscreen_log_num].min=time.tm_min;
	 touchscreen_logs[touchscreen_log_num].second=time.tm_sec;
	 touchscreen_logs[touchscreen_log_num].nanosecond=timespec.tv_nsec;
	 //strcpy(touchscreen_logs[touchscreen_log_num].date,"-");
	 //strcpy(touchscreen_logs[touchscreen_log_num].space1," ");
	 //strcpy(touchscreen_logs[touchscreen_log_num].time1,":");
	 //strcpy(touchscreen_logs[touchscreen_log_num].time2,":");
	 //strcpy(touchscreen_logs[touchscreen_log_num].space2," ");
	 //strcpy(touchscreen_logs[touchscreen_log_num].fingers,"f:");
	 touchscreen_logs[touchscreen_log_num].finger_id=0xf;
	 touchscreen_logs[touchscreen_log_num].finger_status=0xf;		 
	 touchscreen_logs[touchscreen_log_num].pointer_nums=0xf;
	 touchscreen_log_num++;
	 if (touchscreen_log_num>=touchscreen_log_max)
		 touchscreen_log_num=0;
	#endif
	return 0;	
}
#else
#define ft5x0x_ts_suspend	NULL
#define ft5x0x_ts_resume		NULL
#endif

static int __devexit ft5x0x_ts_remove(struct i2c_client *client)
{
	struct ft5x0x_ts_data *ft5x0x_ts;
	ft5x0x_ts = i2c_get_clientdata(client);
	input_unregister_device(ft5x0x_ts->input_dev);
	#ifdef CONFIG_PM
	gpio_free(ft5x0x_ts->pdata->reset);
	#endif

	#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
	#endif
	#ifdef SYSFS_DEBUG
		ft5x0x_remove_sysfs(client);
	#endif

	#ifdef FTS_APK_DEBUG
		ft5x0x_release_apk_debug_channel();
	#endif

	//fts_un_init_gpio_hw(ft5x0x_ts);

	free_irq(client->irq, ft5x0x_ts);

	kfree(ft5x0x_ts);
	i2c_set_clientdata(client, NULL);
#ifdef FT_ESD_PROTECT
		if(use_esd_check==1)

		destroy_workqueue(gtp_esd_check_workqueue);
#endif

	return 0;
}
#ifdef FT_ESD_PROTECT
static void force_reset_guitar(void)
{
    //s32 i;
    //s32 ret;

    printk("force_reset_guitar\n");

//	hwPowerDown(MT6323_POWER_LDO_VGP1,  "TP");
//	msleep(200);
//	hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_2800, "TP");
//	msleep(5);

	//mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	//mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	
	gpio_direction_output(ft_ts->pdata->reset_gpio,0);
	msleep(20);
	
	gpio_direction_output(ft_ts->pdata->reset_gpio,1);
	printk(" fts ic reset\n");
	//mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	//mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);

//	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
//	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
//	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
//	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

	msleep(300);
	
#ifdef TPD_PROXIMITY
	if (FT_PROXIMITY_ENABLE == tpd_proximity_flag) {
		tpd_enable_ps(FT_PROXIMITY_ENABLE);
	}
#endif
}

//extern int apk_debug_flag; //0 for no apk upgrade, 1 for apk upgrade
#define A3_REG_VALUE	0x12
static u8 g_old_91_Reg_Value = 0x00;
static u8 g_first_read_91 = 0x01;
static u8 g_91value_same_count = 0;
#define RESET_91_REGVALUE_SAMECOUNT 5
static void gtp_esd_check_func(struct work_struct *work)
{
	int i;
	u8 data;
	u8 flag_error = 0;
	int reset_flag = 0;
	//u8 check_91_reg_flag = 0;

	if (tpd_halt ) {
		return;
	}
	//if(apk_debug_flag) {
	//	queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, esd_check_circle);
	//	return;
	//}
	if(in_update_mode==1)
		return;
		
	run_check_91_register = 0;
	for (i = 0; i < 3; i++) {
		//ret = fts_i2c_smbus_read_i2c_block_data(i2c_client, 0xA3, 1, &data);
		data = i2c_smbus_read_byte_data(ft_ts->client, 0xA3 );

		if (A3_REG_VALUE==data) {
		    break;
		}
	}

	if (i >= 3) {
		force_reset_guitar();
		printk("focal--tpd reset. i >= 3 	A3_Reg_Value = 0x%02x\n ", data);
		reset_flag = 1;
		goto FOCAL_RESET_A3_REGISTER;
	}

	//esd check for count
  	//ret = fts_i2c_smbus_read_i2c_block_data(i2c_client, 0x8F, 1, &data);
  	
	data = i2c_smbus_read_byte_data(ft_ts->client, 0x8f );
	//printk("0x8F:%d, count_irq is %d\n", data, count_irq);
			
	flag_error = 0;
	if((count_irq - data) > 10) {
		if((data+200) > (count_irq+10) )
		{
			flag_error = 1;
		}
	}
	
	if((data - count_irq ) > 10) {
		flag_error = 1;		
	}
		
	if(1 == flag_error) {	
		printk("focal--tpd reset.1 == flag_error...data=%d	count_irq:%d\n ", data, count_irq);
	    force_reset_guitar();
		reset_flag = 1;
		goto FOCAL_RESET_INT;
	}

	run_check_91_register = 1;
	//ret = fts_i2c_smbus_read_i2c_block_data(i2c_client, 0x91, 1, &data);
	
	data = i2c_smbus_read_byte_data(ft_ts->client, 0x91 );
	//printk("focal---------91 register value = 0x%02x	old value = 0x%02x\n",
	//	data, g_old_91_Reg_Value);
	if(0x01 == g_first_read_91) {
		g_old_91_Reg_Value = data;
		g_first_read_91 = 0x00;
	} else {
		if(g_old_91_Reg_Value == data){
			g_91value_same_count++;
			//printk("focal 91 value ==============, g_91value_same_count=%d\n", g_91value_same_count);
			if(RESET_91_REGVALUE_SAMECOUNT == g_91value_same_count) {
				force_reset_guitar();
				printk("focal--tpd reset. g_91value_same_count = 5\n");
				g_91value_same_count = 0;
				reset_flag = 1;
			}
			
			//run_check_91_register = 1;
			esd_check_circle = TPD_ESD_CHECK_CIRCLE / 2;
			g_old_91_Reg_Value = data;
		} else {
			g_old_91_Reg_Value = data;
			g_91value_same_count = 0;
			//run_check_91_register = 0;
			esd_check_circle = TPD_ESD_CHECK_CIRCLE;
		}
	}
FOCAL_RESET_INT:
FOCAL_RESET_A3_REGISTER:
	count_irq=0;
	data=0;
	//fts_i2c_smbus_write_i2c_block_data(i2c_client, 0x8F, 1, &data);
	
	i2c_smbus_write_byte_data( ft_ts->client, 0x8F,0);
	if(0 == run_check_91_register)
		g_91value_same_count = 0;

#ifdef TPD_PROXIMITY
	
	if( (1 == reset_flag) && ( FT_PROXIMITY_ENABLE == tpd_proximity_flag) )
	{
		if((tpd_enable_ps(FT_PROXIMITY_ENABLE) != 0))
		{
			APS_ERR("enable ps fail\n"); 
				return -1;
		}
	}
#endif
	//end esd check for count// add by zhaofei - 2014-02-18-17-04

    if (!tpd_halt)
    {
        //queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
        queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, esd_check_circle);
    }

    return;
}
#endif

static const struct i2c_device_id ft5x0x_ts_id[] = {
	{FT5X0X_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

#ifdef CONFIG_OF
static struct of_device_id ft_match_table[] = {
	{ .compatible = "foctal,ft-ts",},
	{ },
};

#endif

static struct i2c_driver ft5x0x_ts_driver = {
	.probe = ft5x0x_ts_probe,
	.remove = __devexit_p(ft5x0x_ts_remove),
	.id_table = ft5x0x_ts_id,
	//.suspend = ft5x0x_ts_suspend,
	//.resume = ft5x0x_ts_resume,
	.driver = {
		   .name = FT5X0X_NAME,
		   .owner = THIS_MODULE,
		   #ifdef CONFIG_OF
		   .of_match_table = ft_match_table,
		   #endif
		   },
};

static int __init ft5x0x_ts_init(void)
{
	int ret;
	ret = i2c_add_driver(&ft5x0x_ts_driver);
	if (ret) {
		printk(KERN_WARNING "Adding ft5x0x driver failed "
		       "(errno = %d)\n", ret);
	} else {
		pr_info("Successfully added driver %s\n",
			ft5x0x_ts_driver.driver.name);
	}
	return ret;
}

static void __exit ft5x0x_ts_exit(void)
{
	i2c_del_driver(&ft5x0x_ts_driver);
}

module_init(ft5x0x_ts_init);
module_exit(ft5x0x_ts_exit);

MODULE_AUTHOR("<luowj>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");
