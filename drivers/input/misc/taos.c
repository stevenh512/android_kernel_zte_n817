/******************************************************************
*File Name: 	taos.c 	                                           *
*Description:	Linux device driver for Taos ambient light and    *
*			proximity sensors.                                          *                                
*******************************************************************
Geschichte:	                                                                        
Wenn               Wer          Was                                                                        Tag
2011-02-19    wangzy     Update ALS und prox source     
                                   
******************************************************************/
// includes
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <asm/delay.h>
#include <linux/irq.h>
#include <asm/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/mfd/pmic8058.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>

//local include
#include "taos_common.h"

//#define NR_MSM_IRQS 256

#define TAOS_INT_GPIO (86)
#define TAOS_TAG        "[taos]"

//#define FUNCTION_BOOT_PROX_CALIB

// device name/id/address/counts
#define TAOS_DEVICE_NAME		"taos"
//#define TAOS_DEVICE_ID			"tritonFN"
#define TAOS_DEVICE_ID                "taos"
#define TAOS_ID_NAME_SIZE		10
#define TAOS_TRITON_CHIPIDVAL   	0x00
#define TAOS_TRITON_MAXREGS     	32
#define TAOS_DEVICE_ADDR1		0x29
#define TAOS_DEVICE_ADDR2       	0x39
#define TAOS_DEVICE_ADDR3       	0x49
#define TAOS_MAX_NUM_DEVICES		3
#define TAOS_MAX_DEVICE_REGS		32
#define I2C_MAX_ADAPTERS		8

// TRITON register offsets
#define TAOS_TRITON_CNTRL 		0x00
#define TAOS_TRITON_ALS_TIME 		0X01
#define TAOS_TRITON_PRX_TIME		0x02
#define TAOS_TRITON_WAIT_TIME		0x03
#define TAOS_TRITON_ALS_MINTHRESHLO	0X04
#define TAOS_TRITON_ALS_MINTHRESHHI 	0X05
#define TAOS_TRITON_ALS_MAXTHRESHLO	0X06
#define TAOS_TRITON_ALS_MAXTHRESHHI	0X07
#define TAOS_TRITON_PRX_MINTHRESHLO 	0X08
#define TAOS_TRITON_PRX_MINTHRESHHI 	0X09
#define TAOS_TRITON_PRX_MAXTHRESHLO 	0X0A
#define TAOS_TRITON_PRX_MAXTHRESHHI 	0X0B
#define TAOS_TRITON_INTERRUPT		0x0C
#define TAOS_TRITON_PRX_CFG		0x0D
#define TAOS_TRITON_PRX_COUNT		0x0E
#define TAOS_TRITON_GAIN		0x0F
#define TAOS_TRITON_REVID		0x11
#define TAOS_TRITON_CHIPID      	0x12
#define TAOS_TRITON_STATUS		0x13
#define TAOS_TRITON_ALS_CHAN0LO		0x14
#define TAOS_TRITON_ALS_CHAN0HI		0x15
#define TAOS_TRITON_ALS_CHAN1LO		0x16
#define TAOS_TRITON_ALS_CHAN1HI		0x17
#define TAOS_TRITON_PRX_LO		0x18
#define TAOS_TRITON_PRX_HI		0x19
#define TAOS_TRITON_PRX_OFFSET		0x1E
#define TAOS_TRITON_TEST_STATUS		0x1F

// Triton cmd reg masks
#define TAOS_TRITON_CMD_REG		0X80
#define TAOS_TRITON_CMD_BYTE_RW		0x00
#define TAOS_TRITON_CMD_WORD_BLK_RW	0x20
#define TAOS_TRITON_CMD_SPL_FN		0x60
#define TAOS_TRITON_CMD_PROX_INTCLR	0X05
#define TAOS_TRITON_CMD_ALS_INTCLR	0X06
#define TAOS_TRITON_CMD_PROXALS_INTCLR 	0X07
#define TAOS_TRITON_CMD_TST_REG		0X08
#define TAOS_TRITON_CMD_USER_REG	0X09

// Triton cntrl reg masks
#define TAOS_TRITON_CNTL_PROX_INT_ENBL	0X20
#define TAOS_TRITON_CNTL_ALS_INT_ENBL	0X10
#define TAOS_TRITON_CNTL_WAIT_TMR_ENBL	0X08
#define TAOS_TRITON_CNTL_PROX_DET_ENBL	0X04
#define TAOS_TRITON_CNTL_ADC_ENBL	0x02
#define TAOS_TRITON_CNTL_PWRON		0x01

// Triton status reg masks
#define TAOS_TRITON_STATUS_ADCVALID	0x01
#define TAOS_TRITON_STATUS_PRXVALID	0x02
#define TAOS_TRITON_STATUS_ADCINTR	0x10
#define TAOS_TRITON_STATUS_PRXINTR	0x20

// lux constants
//[sensor wlg 20110729]ALS thereshold modify
//#define	TAOS_MAX_LUX			65535000
#define	TAOS_MAX_LUX			10240
#define TAOS_SCALE_MILLILUX		3
#define TAOS_FILTER_DEPTH		3
#define THRES_LO_TO_HI_RATIO  4/5
// forward declarations
static int taos_probe(struct i2c_client *clientp, const struct i2c_device_id *idp);
static int taos_remove(struct i2c_client *client);
static int taos_open(struct inode *inode, struct file *file);
static int taos_release(struct inode *inode, struct file *file);
static long taos_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int taos_read(struct file *file, char *buf, size_t count, loff_t *ppos);
static int taos_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
static loff_t taos_llseek(struct file *file, loff_t offset, int orig);
static int taos_get_lux(void);
static int taos_lux_filter(int raw_lux);
static int taos_device_name(unsigned char *bufp, char **device_name);
static int taos_prox_poll(struct taos_prox_info *prxp);
//static void taos_prox_poll_timer_func(unsigned long param);
//static void taos_prox_poll_timer_start(void);
//static void taos_als_work(struct work_struct *w);
static void do_taos_work(struct work_struct *w);
static void taos_report_value(int mask);
static int calc_distance(int value);
static int enable_light_and_proximity(int mask);	
static void taos_chip_diff_settings(void);
static int light_on=0;  
static int prox_on = 0;
static int is_tmd27723 = 0;

#if defined(FUNCTION_BOOT_PROX_CALIB)
static int taos_ps_means_quick_get(int times);
static void taos_ps_th_reset(int prox_mean);
int prox_max = 0, prox_min = 0xFFFF;
#endif

enum taos_chip_type {
	TSL2771 = 0,
	TMD2771,	
};

struct alsprox_data {
	struct input_dev *input_dev;
};

static struct alsprox_data *light;
static struct alsprox_data *proximity;
// first device number
static dev_t taos_dev_number;

// class structure for this device
struct class *taos_class;

// module device table
static struct i2c_device_id taos_idtable[] = {
        {TAOS_DEVICE_ID, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, taos_idtable);

// board and address info
struct i2c_board_info taos_board_info[] = {
	{I2C_BOARD_INFO(TAOS_DEVICE_ID, TAOS_DEVICE_ADDR1),},
	{I2C_BOARD_INFO(TAOS_DEVICE_ID, TAOS_DEVICE_ADDR2),},
	{I2C_BOARD_INFO(TAOS_DEVICE_ID, TAOS_DEVICE_ADDR3),},
};
unsigned short const taos_addr_list[4] = {TAOS_DEVICE_ADDR1, TAOS_DEVICE_ADDR2, TAOS_DEVICE_ADDR3, I2C_CLIENT_END};

// client and device
struct i2c_client *my_clientp;
struct i2c_client *bad_clientp[TAOS_MAX_NUM_DEVICES];
//static int num_bad = 0;
//static int device_found = 0;

#ifdef CONFIG_OF
//added for dts
static struct of_device_id taos_match_table[] = {
	{ .compatible = "taos,2771",},
	{ },
};
//
#endif
// driver definition
static struct i2c_driver taos_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "taos",
#ifdef CONFIG_OF
		//added for dts
		.of_match_table = taos_match_table,
		//
#endif
	 },
	.id_table = taos_idtable,
	.probe = taos_probe,
	.remove = __devexit_p(taos_remove),
};

struct taos_intr_data {
    int int_gpio;
    int irq;
};

// per-device data
struct taos_data {
	struct i2c_client *client;
	struct cdev cdev;
	unsigned int addr;
	char taos_id;
	char taos_name[TAOS_ID_NAME_SIZE];
	struct semaphore update_lock;
	char valid;
	unsigned long last_updated;
    	struct taos_intr_data *pdata;
	struct work_struct  taos_work;
	enum taos_chip_type chip_type;
	struct mutex proximity_calibrating;
	struct mutex read_i2c_data;
	struct wake_lock taos_wakelock;
} *taos_datap;

static struct taos_intr_data taos_irq= {
    .int_gpio = TAOS_INT_GPIO,
    .irq = MSM_GPIO_TO_INT(TAOS_INT_GPIO),
/*
    .int_gpio = TAOS_INT_GPIO,
    .irq = PM8058_GPIO_IRQ(PM8058_IRQ_BASE, (TAOS_INT_GPIO-1)),
*/
};


// file operations
static struct file_operations taos_fops = {
	.owner = THIS_MODULE,
	.open = taos_open,
	.release = taos_release,
	.read = taos_read,
	.write = taos_write,
	.llseek = taos_llseek,
	.unlocked_ioctl = taos_ioctl,
};

// device configuration
struct taos_cfg *taos_cfgp;
static u32 calibrate_target_param = 300000;
static u16 als_time_param = 27;
static u16 scale_factor_param = 1;
static u8 filter_history_param = 3;
static u8 filter_count_param = 1;
static u8 gain_param = 1;
static u8 prox_in_sun_flag = 0;

#if defined(CONFIG_BOARD_ADA)
static u16 gain_trim_param = 160;
#elif defined(CONFIG_MACH_RACER2)
static u16 gain_trim_param = 50;
#elif defined(CONFIG_BOARD_APUS)
static u16 gain_trim_param = 150;
#elif defined(CONFIG_BOARD_DEMI)
static u16 gain_trim_param = 110;
#elif defined(CONFIG_BOARD_BONNIE)
static u16 gain_trim_param = 270;
#elif defined(CONFIG_BOARD_FAERIE)
static u16 gain_trim_param = 280;
#elif defined(CONFIG_BOARD_EROS)
static u16 gain_trim_param = 225;
#elif defined(CONFIG_BOARD_GIANT)
static u16 gain_trim_param = 230;
#elif defined(CONFIG_BOARD_HERA)
static u16 gain_trim_param = 390;

#else

static u16 gain_trim_param = 150; //this value is set according to specific device
#endif

static u16 prox_threshold_hi_param = 1023; 
static u16 prox_threshold_lo_param = 818;
//static u16 prox_threshold_hi_param = 700; 
//static u16 prox_threshold_lo_param = 600;

static u8 prox_int_time_param = 0xF6;
static u8 prox_adc_time_param = 0xFF;
static u8 prox_wait_time_param = 0xFF;
static u8 prox_intr_filter_param = 0x00;
static u8 prox_config_param = 0x00;

static u8 prox_pulse_cnt_param = 10;
static u8 prox_gain_param = 0x60;

// device reg init values
u8 taos_triton_reg_init[16] = {0x00,0xFF,0XFF,0XFF,0X00,0X00,0XFF,0XFF,0X00,0X00,0XFF,0XFF,0X00,0X00,0X00,0X00};

//
static u16 als_intr_threshold_hi_param = 0;
static u16 als_intr_threshold_lo_param = 0;
int g_nlux = 0;
//static int AlsorProx=0;
		

// prox info
struct taos_prox_info prox_cal_info[20];
struct taos_prox_info prox_cur_info;
struct taos_prox_info *prox_cur_infop = &prox_cur_info;
//static u8 prox_history_hi = 0;
//static u8 prox_history_lo = 0;
//static struct timer_list prox_poll_timer;

static int device_released = 0;
static u16 sat_als = 0;
static u16 sat_prox = 0;

// lux time scale
struct time_scale_factor  {
	u16	numerator;
	u16	denominator;
	u16	saturation;
};
struct time_scale_factor TritonTime = {1, 0, 0};
struct time_scale_factor *lux_timep = &TritonTime;

// gain table
u8 taos_triton_gain_table[] = {1, 8, 16, 120};

// lux data
struct lux_data {
	u16	ratio;
	u16	clear;
	u16	ir;
};
struct lux_data TritonFN_lux_data[] = {
        { 9830,  8320,  15360 },
        { 12452, 10554, 22797 },
        { 14746, 6234,  11430 },
        { 17695, 3968,  6400  },
        { 0,     0,     0     }
};
struct lux_data *lux_tablep = TritonFN_lux_data;
static int lux_history[TAOS_FILTER_DEPTH] = {-ENODATA, -ENODATA, -ENODATA};

//prox data
struct prox_data {
	u16	ratio;
	u16	hi;
	u16	lo;
};
struct prox_data TritonFN_prox_data[] = {
        { 1,  22,  20 },
        { 3, 20, 16 },
        { 6, 18, 14 },
        { 10, 16,  16 },      
        { 0,  15,   12 }
};
struct prox_data *prox_tablep = TritonFN_prox_data;

/* ----------------------*
* config gpio for intr utility        *
*-----------------------*/
int taos_config_int_gpio(int int_gpio)
{
    int rc=0;
/*
	struct pm8058_gpio taos_irq_gpio_config = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
	};
//		rc = pm8058_gpio_config(35, &taos_irq_gpio_config);	
		rc = pm8058_gpio_config(33, &taos_irq_gpio_config);
//		rc = pm8058_gpio_config(34, &taos_irq_gpio_config);
		//rc = pm8058_gpio_config(int_gpio, &taos_irq_gpio_config);

                        printk(TAOS_TAG "%s: pm8058_gpio_config(%#x)=%d\n",
                        __func__, int_gpio, rc);

		if (rc) {
                        printk(TAOS_TAG "%s: pm8058_gpio_config(%#x)=%d\n",
                        __func__, int_gpio, rc);

			return rc;
		}
		rc = gpio_request(PM8058_GPIO_PM_TO_SYS(int_gpio),
				  "gpio_sensor");

			    printk(TAOS_TAG "%s: gpio_request(%#x)=%d\n",
                          __func__, (PM8058_GPIO_PM_TO_SYS(int_gpio)), rc);

		if (rc) {
			    printk(TAOS_TAG "%s: gpio_request(%#x)=%d\n",
                          __func__, (PM8058_GPIO_PM_TO_SYS(int_gpio)), rc);

			return rc;
		}
		gpio_set_value_cansleep(
			PM8058_GPIO_PM_TO_SYS(int_gpio), 0);

    mdelay(1);

    rc = gpio_direction_input(PM8058_GPIO_PM_TO_SYS(int_gpio));

        printk(TAOS_TAG "%s: gpio_direction_input(%#x)=%d\n",
                __func__, PM8058_GPIO_PM_TO_SYS(int_gpio), rc);

    if (rc) {
        printk(TAOS_TAG "%s: gpio_direction_input(%#x)=%d\n",
                __func__, PM8058_GPIO_PM_TO_SYS(int_gpio), rc);
        return rc;
    }
*/	
    uint32_t  gpio_config_data = GPIO_CFG(int_gpio,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA);

#ifdef CONFIG_OF
	if (!(rc = gpio_is_valid(int_gpio))) {
	    printk(TAOS_TAG "%s: irq gpio not provided.\n",
        __func__);
		return rc;
    }	
#endif
	


    rc = gpio_request(int_gpio, "gpio_sensor");
    if (rc) {
        printk(TAOS_TAG "%s: gpio_request(%#x)=%d\n",
                __func__, int_gpio, rc);
        return rc;
    }

    rc = gpio_tlmm_config(gpio_config_data, GPIO_CFG_ENABLE);
    if (rc) {
        printk(TAOS_TAG "%s: gpio_tlmm_config(%#x)=%d\n",
                __func__, gpio_config_data, rc);
        return rc;
    }

    mdelay(1);

    rc = gpio_direction_input(int_gpio);
    if (rc) {
        printk(TAOS_TAG "%s: gpio_direction_input(%#x)=%d\n",
                __func__, int_gpio, rc);
        return rc;
    }

#ifdef CONFIG_OF
	taos_datap->pdata->irq = taos_datap->client->irq = gpio_to_irq(int_gpio);
#endif
    return 0;
}

/* ----------------------*
* taos interrupt function         *
*-----------------------*/
static irqreturn_t taos_interrupt(int irq, void *data)
{
    //printk(TAOS_TAG "taos_interrupt\n");	
    if( !(1 == prox_on || 1 == light_on) ) {
		return IRQ_HANDLED;
    }
    disable_irq_nosync(taos_datap->pdata->irq);
    schedule_work(&taos_datap->taos_work);
    wake_lock_timeout(&taos_datap->taos_wakelock, HZ);
	
    return IRQ_HANDLED;
}

static void do_taos_work(struct work_struct *w)
{

    int ret =0;	
    //int prx_hi, prx_lo;
    u16 status = 0;	
	u16 tmp_val = 0;
	int count = 3;
#if defined(FUNCTION_BOOT_PROX_CALIB)	
	struct prox_data *prox_pt;
	u16 ratio;			
    prox_pt = prox_tablep;	
#endif
    //if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | 0x13)))) < 0) {  
	//	printk("taos write_byte fail\n");
	//	goto read_reg_fail;
	//}

	mutex_lock(&taos_datap->read_i2c_data);

	while(count >= 0) {
		printk("do_taos_work count: %d\n", count);
		if (count <= 0) goto read_reg_fail;
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | 0x13)))) < 0) {  
			printk("taos write_byte fail\n");
			msleep(10);
			count --;
	    } else {
	     break;
	    }
	}
    status = i2c_smbus_read_byte(taos_datap->client);
	printk("taos,cgh do_taos_work %d\n", status);

	//for tmd27723 IC, when prox in sun, can not report ALS data because interrupt status is NOT right.
	if(1 == prox_in_sun_flag) {
		printk("taos,prox_in_sun-->get als data\n");
		ret = taos_get_lux();
		if(ret >= 0){
			g_nlux = ret;
		}

		if(g_nlux>=0)	
			taos_report_value(0);
    }
   
    //als interrupt
    //if(((status & 0x10) != 0)&&((status & 0x20)==0))
	if((status & 0x10) != 0)
    {
     printk("taos, interrupt status OK-->get als data\n");
	 ret = taos_get_lux();
         if(ret >= 0){
             g_nlux = ret;
         }
    		
	//clear intr  for als intr case	
    	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|TAOS_TRITON_CMD_PROXALS_INTCLR)))) < 0) {
   		printk(KERN_CRIT "TAOS: i2c_smbus_write_byte failed in clear als interrupt\n");
	}
	if(g_nlux>=0)	
		taos_report_value(0);
	if(prox_on&&(prox_cur_infop->prox_event == 1)&&(g_nlux>1000*(taos_cfgp->gain_trim))){
		printk("taos, get als data ---> turn_screen_on\n");
		goto turn_screen_on;
	}
    }
	
    //prox interrupt
    if((status & 0x20)!=0)
    { 
     prox_in_sun_flag = 0;
     //wake_lock_timeout(&taos_datap->taos_wakelock, HZ);
    	//printk(TAOS_TAG "taos_interrupt\n");	
	if((ret = taos_prox_poll(prox_cur_infop))<0){
		printk(KERN_CRIT "TAOS: get prox poll  failed in  taos interrupt()\n");  
		if(ret==-ENODATA)
			goto turn_screen_on;
		}

#if defined(FUNCTION_BOOT_PROX_CALIB)
//	if(prox_cur_infop->prox_data > prox_max){
//	    printk(KERN_CRIT "TAOS: update prox max %d -> %d\n",prox_cur_infop->prox_data,prox_max);
//        prox_max = prox_cur_infop->prox_data;
//	}

	if((prox_min/2 > prox_cur_infop->prox_data) && (prox_cur_infop->prox_data > 100)){
    	    printk(KERN_CRIT "TAOS: update prox min %d -> %d\n",prox_cur_infop->prox_data,prox_min);
            if(prox_min != 0xFFFF){
                if(taos_datap->chip_type==TMD2771){				
                    ratio = 10*prox_cur_infop->prox_data/sat_prox;
                    for (prox_pt = prox_tablep; prox_pt->ratio && prox_pt->ratio <= ratio; prox_pt++);
                        if(!prox_pt->ratio){
                            prox_pt->lo = 12;
		        		    prox_pt->hi = 15;
                        }							
                }else{
                    prox_pt->lo = 12;
				    prox_pt->hi = 15;
                }	
			    taos_cfgp->prox_threshold_hi = prox_pt->hi * prox_cur_infop->prox_data / 10;
			    taos_cfgp->prox_threshold_lo = prox_pt->lo * prox_cur_infop->prox_data / 10;
			    if (taos_cfgp->prox_threshold_lo < ((sat_prox*15)/100)) {				
				    taos_cfgp->prox_threshold_hi = ((sat_prox*20)/100);
				    taos_cfgp->prox_threshold_lo = (taos_cfgp->prox_threshold_hi *THRES_LO_TO_HI_RATIO);
                }
            }
            prox_min = prox_cur_infop->prox_data;
    }
#endif	
	if(prox_cur_infop->prox_data > taos_cfgp->prox_threshold_hi)       
	 {           
         if(light_on&&(g_nlux>10000*(taos_cfgp->gain_trim)))
		 {
		     printk(KERN_CRIT "TAOS: porx(%d) > hi(%d) when als is %d\n",prox_cur_infop->prox_data,taos_cfgp->prox_threshold_hi,g_nlux);//如果看到这个打印在强光下频繁发生，可以考虑跳过下面的阈值设置和事件上报，但中断操作还要做，且这里可能中断过于频繁。也可以上报一个错误。  
			 prox_in_sun_flag = 1;
			 goto prox_in_sun;
		 }
		

    	 if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MAXTHRESHLO),sat_prox))) < 0) {        	        
			pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MAXTHRESHLO\n");                	
			    	
			}           
		if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MINTHRESHLO),taos_cfgp->prox_threshold_lo))) < 0) {        	       
			pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MINTHRESHLO\n");                	
			       
			}            
		prox_cur_infop->prox_event = 1;           
		//pr_crit(TAOS_TAG "screen off:prox_cur_infop->prox_data=%d\n",prox_cur_infop->prox_data);            
	} 

	else if(prox_cur_infop->prox_data <= taos_cfgp->prox_threshold_lo)        
prox_in_sun:
turn_screen_on:	
	{            
		if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MAXTHRESHLO),taos_cfgp->prox_threshold_hi))) < 0) {    	       
			pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MAXTHRESHLO\n");            	
			      	 
			}
        tmp_val = 0;			
		if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MINTHRESHLO),tmp_val))) < 0) {    	    
			pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MINTHRESHLO\n");            
			    	 
			}             
		prox_cur_infop->prox_event = 0;             
		//pr_crit(TAOS_TAG "screen on:prox_cur_infop->prox_data=%d\n",prox_cur_infop->prox_data);                     
	}   
        
	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|TAOS_TRITON_CMD_PROXALS_INTCLR)))) < 0) {        
		pr_crit(TAOS_TAG "i2c_smbus_write_byte failed in clear interrupt\n");                                   
	}    
	
	taos_report_value(1);
	
    }
	
read_reg_fail:
	mutex_unlock(&taos_datap->read_i2c_data);
    enable_irq(taos_datap->pdata->irq);
    return;

}	
		
/*calc_distance, using prox value*/
static int calc_distance(int value)
{
	int temp=0;
	if(prox_cur_infop->prox_event == 1)
		//temp=4-((value-(taos_cfgp->prox_threshold_lo))/3000);
		temp=4-((value-(taos_cfgp->prox_threshold_lo))/((sat_prox-(taos_cfgp->prox_threshold_lo))/4));
	else if(prox_cur_infop->prox_event == 0)
		temp=5;
	return temp;
}

/*report als and prox data to input sub system, 
for data poll utility in hardware\alsprox.c
*/
static void taos_report_value(int mask)
{
	struct taos_prox_info *val = prox_cur_infop;
	int lux_val=g_nlux;
	int  dist;
	/*ergate*/
	//printk("ergate lux=%d,gain_trim=%d\n",lux_val,taos_cfgp->gain_trim);
	lux_val/=taos_cfgp->gain_trim;

	if (mask==0) {
		input_report_abs(light->input_dev, ABS_MISC, lux_val);
//[sensor wlg 20110729]ALS thereshold modify add log
 		printk(KERN_CRIT "TAOS: als_interrupt lux_val(%d)=g_nlux(%d)/taos_cfgp->gain_trim(%d)\n", lux_val, g_nlux, taos_cfgp->gain_trim);
		input_sync(light->input_dev);
	}

	if (mask==1) {
		dist=calc_distance(val->prox_data);
		//input_report_als(alsprox->input_dev, ALS_PROX, val->prox_clear);
		input_report_abs(proximity->input_dev, ABS_DISTANCE, dist);
		//input_report_als(alsprox->input_dev, ALS_PROX, val->prox_event);
		printk(KERN_CRIT "TAOS: prox_interrupt =%d, distance=%d\n",  val->prox_data,dist);
		input_sync(proximity->input_dev);
	}

	//input_sync(alsprox->input_dev);	
	//enable_irq(taos_datap->pdata->irq);
}



/* ------------*
* driver init        *
*------------*/
static int __init taos_init(void) {
	int ret = 0;
	//struct i2c_adapter *my_adap;
	printk(KERN_ERR "TAOS: comes into taos_init\n");
	if ((ret = (alloc_chrdev_region(&taos_dev_number, 0, TAOS_MAX_NUM_DEVICES, TAOS_DEVICE_NAME))) < 0) {
		printk(KERN_ERR "TAOS: alloc_chrdev_region() failed in taos_init()\n");
                return (ret);
	}
        taos_class = class_create(THIS_MODULE, TAOS_DEVICE_NAME);
        taos_datap = kmalloc(sizeof(struct taos_data), GFP_KERNEL);
        if (!taos_datap) {
		printk(KERN_ERR "TAOS: kmalloc for struct taos_data failed in taos_init()\n");
                return -ENOMEM;
	}
        memset(taos_datap, 0, sizeof(struct taos_data));
        cdev_init(&taos_datap->cdev, &taos_fops);
        taos_datap->cdev.owner = THIS_MODULE;
        if ((ret = (cdev_add(&taos_datap->cdev, taos_dev_number, 1))) < 0) {
		printk(KERN_ERR "TAOS: cdev_add() failed in taos_init()\n");
                return (ret);
	}
	//device_create(taos_class, NULL, MKDEV(MAJOR(taos_dev_number), 0), &taos_driver ,"taos");
        ret = i2c_add_driver(&taos_driver);
	if(ret){
		printk(KERN_ERR "TAOS: i2c_add_driver() failed in taos_init(),%d\n",ret);
                return (ret);
	}
    	//pr_crit(TAOS_TAG "%s:%d\n",__func__,ret);
        return (ret);
}



// driver exit
static void __exit taos_exit(void) {
/*	if (my_clientp)
		i2c_unregister_device(my_clientp);
	*/
        i2c_del_driver(&taos_driver);
        unregister_chrdev_region(taos_dev_number, TAOS_MAX_NUM_DEVICES);
	device_destroy(taos_class, MKDEV(MAJOR(taos_dev_number), 0));
	cdev_del(&taos_datap->cdev);
	class_destroy(taos_class);
	wake_lock_destroy(&taos_datap->taos_wakelock);
	mutex_destroy(&taos_datap->proximity_calibrating);
	mutex_destroy(&taos_datap->read_i2c_data);
        kfree(taos_datap);
}


//***************************************************
/*static struct file_operations taos_device_fops = {
	.owner = THIS_MODULE,
	.open = taos_open,
	.release = taos_release,
	.unlocked_ioctl = taos_ioctl,
};


static struct miscdevice taos_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "taos",
	.fops = &taos_device_fops,
};
*/

void suspend_to_disable_light_sensor(void)
{
	int ret = 0;
	if( 0 == prox_on ) {
	mutex_lock(&taos_datap->read_i2c_data);
	ret =enable_light_and_proximity(0x20);
	mutex_unlock(&taos_datap->read_i2c_data);
	if(ret >= 0) {
		pr_crit(TAOS_TAG"suspend_to_disable_light success\n");
	} else {
		pr_crit(TAOS_TAG"suspend_to_disable_light fail\n");
	}
}
	pr_crit(TAOS_TAG"suspend_to_disable_light success prox_on = %d\n", prox_on);
}

void resume_to_recover_light_sensor(void)
{
	int ret = 0;
	if( 0 == prox_on ) {
	if(1 == light_on) {
		mutex_lock(&taos_datap->read_i2c_data);
		ret = enable_light_and_proximity(0x10);
		mutex_unlock(&taos_datap->read_i2c_data);
		if(ret >= 0) {
				pr_crit(TAOS_TAG"cgh suspend_to_enable_light success\n");
		} else {
			pr_crit(TAOS_TAG"suspend_to_enable_light fail\n");
		}
	} else {
		pr_crit(TAOS_TAG"Nothing need to do\n");
	}
}
	pr_crit(TAOS_TAG"suspend_to_enable_light prox_on = %d\n", prox_on);
}


void (*suspend_to_disable_light)(void);
void (*resume_to_recover_light)(void);


// client probe
static int taos_probe(struct i2c_client *clientp, const struct i2c_device_id *idp) {
	int ret = 0;
	int i = 0;
	unsigned char buf[TAOS_MAX_DEVICE_REGS];
	char *device_name;
    u16 tmp_val = 0;
//	if (device_found)
//		return -ENODEV;
	if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk(KERN_ERR "TAOS: taos_probe() - i2c smbus byte data functions unsupported\n");
		return -EOPNOTSUPP;
		}
    if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		printk(KERN_ERR "TAOS: taos_probe() - i2c smbus word data functions unsupported\n");
        }
    if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_BLOCK_DATA)) {
		printk(KERN_ERR "TAOS: taos_probe() - i2c smbus block data functions unsupported\n");
        }
	taos_datap->client = clientp;
	i2c_set_clientdata(clientp, taos_datap);
	
	//write bytes to address control registers
    for(i = 0; i < TAOS_MAX_DEVICE_REGS; i++) {
		if((ret = (i2c_smbus_write_byte(clientp, (TAOS_TRITON_CMD_REG | (TAOS_TRITON_CNTRL + i))))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to address control regs failed in taos_probe()\n");
			return(ret);
        }
		buf[i] = i2c_smbus_read_byte(clientp);
		printk(KERN_ERR "TAOS: i2c_smbus_read_byte(%d) is %d\n",i,buf[i]);//wlg test
	}	
	//check device ID "tritonFN"
	if ((ret = taos_device_name(buf, &device_name)) == 0) {
		printk(KERN_ERR "TAOS: chip id that was read found mismatched by taos_device_name(), in taos_probe()\n");
 		return -ENODEV;
        }
	if (strcmp(device_name, TAOS_DEVICE_ID)) {
		printk(KERN_ERR "TAOS: chip id that was read does not match expected id in taos_probe()\n");
		return -ENODEV;
        }
	else{
		printk(KERN_ERR "TAOS: chip id of %s that was read matches expected id in taos_probe()\n", device_name);
		//device_found = 1;
	}
	device_create(taos_class, NULL, MKDEV(MAJOR(taos_dev_number), 0), &taos_driver ,"taos");
	if ((ret = (i2c_smbus_write_byte(clientp, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to control reg failed in taos_probe()\n");
		return(ret);
        }
	tmp_val = 0;
	if ((ret = (i2c_smbus_write_word_data(clientp, (0xA0 | TAOS_TRITON_ALS_MAXTHRESHLO),tmp_val))) < 0) {
        	printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to chan0/1/lo/hi reg failed in taos_probe()\n");
        }
	tmp_val = 0;
    if ((ret = (i2c_smbus_write_word_data(clientp, (0xA0 | TAOS_TRITON_ALS_MINTHRESHLO),tmp_val))) < 0) {
        	printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to chan0/1/lo/hi reg failed in taos_probe()\n");
        }
	
	wake_lock_init(&taos_datap->taos_wakelock,WAKE_LOCK_SUSPEND, "taos_input_wakelock");
	INIT_WORK(&taos_datap->taos_work, do_taos_work); 
	mutex_init(&taos_datap->proximity_calibrating);
	mutex_init(&taos_datap->read_i2c_data);
	taos_datap->pdata = &taos_irq;
	if(clientp->irq){		
		taos_datap->pdata->irq=taos_datap->client->irq;
#ifdef CONFIG_OF
		if (clientp->dev.of_node) {
	        taos_datap->pdata->int_gpio = of_get_named_gpio(clientp->dev.of_node, "taos,irq-gpio", 0);
	        }
#else
		taos_datap->pdata->int_gpio=INT_TO_MSM_GPIO(taos_datap->pdata->irq);
#endif
		}
	printk(KERN_CRIT "8610 taos use gpio %d  irq %d\n",taos_datap->pdata->int_gpio,taos_datap->pdata->irq);

#ifdef CONFIG_OF
	if (clientp->dev.of_node) {
	    taos_datap->pdata->int_gpio = of_get_named_gpio(clientp->dev.of_node, "taos,irq-gpio", 0);
	}
#endif
    	ret=taos_config_int_gpio(taos_datap->pdata->int_gpio);
    	if (ret) {
		printk(KERN_CRIT "taos configure int_gpio%d failed\n",
                taos_datap->pdata->int_gpio);
        	return ret;
    	}

/*
    	ret = request_threaded_irq(taos_datap->pdata->irq, NULL, taos_interrupt, IRQF_TRIGGER_FALLING, 
			taos_datap->taos_name, prox_cur_infop);
*/

    	ret = request_irq(taos_datap->pdata->irq, taos_interrupt, IRQF_TRIGGER_FALLING, 
			taos_datap->taos_name, prox_cur_infop);
    	if (ret) {
		printk(KERN_CRIT "taos request interrupt failed\n");
        	return ret;
    	}

	ret = enable_irq_wake(taos_datap->pdata->irq);
	if (ret) {
		printk(KERN_CRIT "taos enable_irq_wake failed\n");
        	return ret;
    	}
	
	strlcpy(clientp->name, TAOS_DEVICE_ID, I2C_NAME_SIZE);
	strlcpy(taos_datap->taos_name, TAOS_DEVICE_ID, TAOS_ID_NAME_SIZE);
	taos_datap->valid = 0;
	
	init_MUTEX(&taos_datap->update_lock);
	if (!(taos_cfgp = kmalloc(sizeof(struct taos_cfg), GFP_KERNEL))) {
		printk(KERN_ERR "TAOS: kmalloc for struct taos_cfg failed in taos_probe()\n");
		return -ENOMEM;
	}	

	//update settings
	//this should behind taos_device_name	
	taos_cfgp->calibrate_target = calibrate_target_param;
	taos_cfgp->als_time = als_time_param;
	taos_cfgp->scale_factor = scale_factor_param;
	taos_cfgp->gain_trim = gain_trim_param;
	taos_cfgp->filter_history = filter_history_param;
	taos_cfgp->filter_count = filter_count_param;
	taos_cfgp->gain = gain_param;
	taos_cfgp->prox_threshold_hi = prox_threshold_hi_param;
	taos_cfgp->prox_threshold_lo = prox_threshold_lo_param;
	taos_cfgp->prox_int_time = prox_int_time_param;
	taos_cfgp->prox_adc_time = prox_adc_time_param;
	taos_cfgp->prox_wait_time = prox_wait_time_param;
	taos_cfgp->prox_intr_filter = prox_intr_filter_param;
	taos_cfgp->prox_config = prox_config_param;
	taos_cfgp->prox_pulse_cnt = prox_pulse_cnt_param;
	taos_cfgp->prox_gain = prox_gain_param;
	sat_als = (256 - taos_cfgp->prox_int_time) << 10; //10240
	sat_prox = (256 - taos_cfgp->prox_adc_time) << 10; //1024

	light = kzalloc(sizeof(struct alsprox_data), GFP_KERNEL);
	if (!light) {
		ret = -ENOMEM;
		goto exit_alloc_data_failed;
	}
	proximity= kzalloc(sizeof(struct alsprox_data), GFP_KERNEL);
	if (!proximity) {
		ret = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	light->input_dev = input_allocate_device();
	if (!light->input_dev) {
		ret = -ENOMEM;
		printk(KERN_ERR "taos_probe: Failed to allocate light input device\n");
		goto exit_input_dev_alloc_failed;
	}
	proximity->input_dev = input_allocate_device();
	if (!proximity->input_dev) {
		ret = -ENOMEM;
		printk(KERN_ERR "taos_probe: Failed to allocate prox input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	/* lux */
	set_bit(EV_ABS, light->input_dev->evbit);
	input_set_abs_params(light->input_dev,  ABS_MISC, 0, 65535, 0, 0);
	light->input_dev->name = "light";
	/* prox */
	set_bit(EV_ABS, proximity->input_dev->evbit);
	input_set_abs_params(proximity->input_dev, ABS_DISTANCE, 0, 65535, 0, 0);
	proximity->input_dev->name = "proximity";
	
	ret = input_register_device(light->input_dev);
	if (ret) {
		printk("taos_probe: Unable to register input device: %s\n", light->input_dev->name);
		goto exit_input_register_device_failed;
	}
	ret = input_register_device(proximity->input_dev);
	if (ret) {
		printk("taos_probe: Unable to register input device: %s\n", proximity->input_dev->name);
		goto exit_input_register_device_failed;
	}

	suspend_to_disable_light = suspend_to_disable_light_sensor;
    resume_to_recover_light = resume_to_recover_light_sensor;
	
	return (ret);
//exit_taos_device_register_failed:	
exit_input_register_device_failed:
	if(light->input_dev)
		input_free_device(light->input_dev);
	if(proximity->input_dev)
		input_free_device(proximity->input_dev);
exit_input_dev_alloc_failed:
exit_alloc_data_failed:
	wake_lock_destroy(&taos_datap->taos_wakelock);
	if(light)
		kfree(light);
	if(proximity)
		kfree(proximity);
	return (ret);		
}

//#define CONFIG_SUS
#ifdef CONFIG_SUS

static int taos_suspend(struct i2c_client *client, pm_message_t mesg)
{
    int ret = 0;
	return ret;
}

static int taos_resume(struct i2c_client *client)
{
    int ret = 0;
	//clear intr  for als intr case	
   	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|TAOS_TRITON_CMD_PROXALS_INTCLR)))) < 0) {
   		printk(KERN_CRIT "TAOS: i2c_smbus_write_byte failed in clear als interrupt\n");
	}	
	return ret;
}

#else

#define taos_suspend		NULL
#define taos_resume		NULL

#endif /* CONFIG_PM */
// client remove
static int __devexit taos_remove(struct i2c_client *client) {
	int ret = 0;

	return (ret);
}

// open
static int taos_open(struct inode *inode, struct file *file) {
	struct taos_data *taos_datap;
	int ret = 0;
	device_released = 0;
	taos_datap = container_of(inode->i_cdev, struct taos_data, cdev);
	if (strcmp(taos_datap->taos_name, TAOS_DEVICE_ID) != 0) {
		printk(KERN_ERR "TAOS: device name incorrect during taos_open(), get %s\n", taos_datap->taos_name);
		ret = -ENODEV;
	}
	return (ret);
}

// release
static int taos_release(struct inode *inode, struct file *file) {
	struct taos_data *taos_datap;
	int ret = 0;

	device_released = 1;
	taos_datap = container_of(inode->i_cdev, struct taos_data, cdev);	
	if (strcmp(taos_datap->taos_name, TAOS_DEVICE_ID) != 0) {
		printk(KERN_ERR "TAOS: device name incorrect during taos_release(), get %s\n", taos_datap->taos_name);
		ret = -ENODEV;
	}
	return (ret);
}

// read
static int taos_read(struct file *file, char *buf, size_t count, loff_t *ppos) {
	struct taos_data *taos_datap;
	u8 i = 0, xfrd = 0, reg = 0;
	u8 my_buf[TAOS_MAX_DEVICE_REGS];
	int ret = 0;

	//if (prox_on)
		//del_timer(&prox_poll_timer);
        if ((*ppos < 0) || (*ppos >= TAOS_MAX_DEVICE_REGS)  || (count > TAOS_MAX_DEVICE_REGS)) {
		printk(KERN_ERR "TAOS: reg limit check failed in taos_read()\n");
		return -EINVAL;
	}
	reg = (u8)*ppos;
	taos_datap = container_of(file->f_dentry->d_inode->i_cdev, struct taos_data, cdev);
	while (xfrd < count) {
			if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | reg)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in taos_read()\n");
			return (ret);
			}
      		my_buf[i++] = i2c_smbus_read_byte(taos_datap->client);
			reg++;
			xfrd++;
        }
        if ((ret = copy_to_user(buf, my_buf, xfrd))){
		printk(KERN_ERR "TAOS: copy_to_user failed in taos_read()\n");
		return -ENODATA;
	}
	//if (prox_on)
		//taos_prox_poll_timer_start();
        return ((int)xfrd);
}

// write
static int taos_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {
	struct taos_data *taos_datap;
	u8 i = 0, xfrd = 0, reg = 0;
	u8 my_buf[TAOS_MAX_DEVICE_REGS];
	int ret = 0;

	//if (prox_on)
		//del_timer(&prox_poll_timer);
        if ((*ppos < 0) || (*ppos >= TAOS_MAX_DEVICE_REGS) || ((*ppos + count) > TAOS_MAX_DEVICE_REGS)) {
		printk(KERN_ERR "TAOS: reg limit check failed in taos_write()\n");
		return -EINVAL;
	}
	reg = (u8)*ppos;
        if ((ret =  copy_from_user(my_buf, buf, count))) {
		printk(KERN_ERR "TAOS: copy_to_user failed in taos_write()\n");
 		return -ENODATA;
	}
        taos_datap = container_of(file->f_dentry->d_inode->i_cdev, struct taos_data, cdev);
        while (xfrd < count) {
                if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | reg), my_buf[i++]))) < 0) {
                        printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in taos_write()\n");
                        return (ret);
                }
                reg++;
                xfrd++;
        }
	//if (prox_on)
		//taos_prox_poll_timer_start();
        return ((int)xfrd);
}

// llseek
static loff_t taos_llseek(struct file *file, loff_t offset, int orig) {
	int ret = 0;
	loff_t new_pos = 0;

	//if (prox_on)
		//del_timer(&prox_poll_timer);
	if ((offset >= TAOS_MAX_DEVICE_REGS) || (orig < 0) || (orig > 1)) {
                printk(KERN_ERR "TAOS: offset param limit or origin limit check failed in taos_llseek()\n");
                return -EINVAL;
        }
        switch (orig) {
        	case 0:
                	new_pos = offset;
                	break;
        	case 1:
                	new_pos = file->f_pos + offset;
	                break;
		default:
			return -EINVAL;
			break;
	}
	if ((new_pos < 0) || (new_pos >= TAOS_MAX_DEVICE_REGS) || (ret < 0)) {
		printk(KERN_ERR "TAOS: new offset limit or origin limit check failed in taos_llseek()\n");
		return -EINVAL;
	}
	file->f_pos = new_pos;
       // if (prox_on)
              //  taos_prox_poll_timer_start();
	return new_pos;
}

/*enable_light_and_proximity, mask values' indication*/
/*10 : light on*/
/*01 : prox on*/
/*20 : light off*/
/*02 : prox off*/
static int enable_light_and_proximity(int mask)
{
	u8 ret = 0, reg_val = 0; //itime = 0;

	if(mask==0x10) 
	{
		if(!prox_on){
			pr_crit(TAOS_TAG "light on while prox off\n");
			if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x00), 0x00))) < 0) {
				printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                                return (ret);
			}
		}else	
			pr_crit(TAOS_TAG "light on while prox on\n");
                if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|TAOS_TRITON_CMD_PROXALS_INTCLR)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_on\n");
                                return (ret);
		}
		
		if(!prox_on){
		/*	
		itime = (((taos_cfgp->als_time/50) * 18) - 1);
		itime = (~itime);*/
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_ALS_TIME), 0xEE))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
                                return (ret);
		}
			if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x03), 0xDC))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
                                return (ret);
			}	
		}
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_on\n");
                                return (ret);
		}
		reg_val = i2c_smbus_read_byte(taos_datap->client);
		reg_val = reg_val & 0xFC;
		reg_val = reg_val | (taos_cfgp->gain & 0x03);
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN), reg_val))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als gain\n");
                                return (ret);
		}
		reg_val=prox_on? 0x3F:0x1B;
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_val)) )< 0) {
	                        printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als enable\n");
        	                return (ret);
		}
		reg_val=prox_on? 0x21:0x01;
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_INTERRUPT), reg_val))) < 0) {
	                        printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
        	                return (ret);
		}
		return ret;
	}	
	if(mask==0x01)
	{
	    if(!light_on){	
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x00), 0x00))) < 0) {
				printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                                return (ret);
				}
	    	}
                if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x01), 0xEE))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                                return (ret);
		}
                if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x02), taos_cfgp->prox_adc_time))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                                return (ret);
		}
                if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x03), taos_cfgp->prox_wait_time))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                                return (ret);
		}
        if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0C), taos_cfgp->prox_intr_filter))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                                return (ret);
		}
                if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0E), taos_cfgp->prox_pulse_cnt))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                                return (ret);
		}			 
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl prox_on\n");
                                return (ret);
		}
			reg_val = i2c_smbus_read_byte(taos_datap->client);
			reg_val = reg_val & 0x03;
			reg_val = reg_val | (taos_cfgp->prox_gain & 0xFC);
		if(1 == is_tmd27723) reg_val |= 0x0c; //setting prox 8X gain
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN), reg_val))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox gain\n");
                                return (ret);
		}
		if(1 == is_tmd27723){ // setting prox offset to 0x0
			if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_PRX_OFFSET), 0x0))) < 0) {
	                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox gain\n");
	                                return (ret);
			}
		}
            	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|TAOS_TRITON_CMD_PROXALS_INTCLR)))) < 0) {
                		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_on\n");
                		return (ret);
            	}				
		if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MAXTHRESHLO),taos_cfgp->prox_threshold_hi))) < 0) {    	       
				pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MAXTHRESHLO\n");            			      	 
		}     
		if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MINTHRESHLO),taos_cfgp->prox_threshold_lo))) < 0) {    	       
				pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MINTHRESHLO\n");            			      	 
		}    
		prox_cur_infop->prox_event = 0;
            	prox_cur_infop->prox_clear = 0;
            	prox_cur_infop->prox_data = 0;
		if(light_on)
			pr_crit(TAOS_TAG "prox on while light on\n");
		reg_val=light_on? 0x3F:0x3F;
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x00), reg_val))) < 0) { 
                		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                		return (ret);
            	}	
		reg_val=light_on? 0x23:0x23;
                if((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0C), reg_val))) < 0)		 
                {                    	
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
                                return (ret);
                }
		return ret;
	}	
	if(mask==0x20)
	{
                if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
                                return (ret);
                }
                reg_val = i2c_smbus_read_byte(taos_datap->client);
		if(prox_on){
			pr_crit(TAOS_TAG "cgh light off while prox still on reg_val = %d\n", reg_val);
			//reg_val = reg_val & (~TAOS_TRITON_CNTL_ALS_INT_ENBL);
			}
		else
			reg_val = reg_val & (~(TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON));
                if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_val))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_off\n");
                                return (ret);
                }
		return ret;
	}
	if(mask==0x02)
	{
		if(light_on){
		pr_crit(TAOS_TAG "prox off while light still on\n");
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x03), 0xDC))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
                                return (ret);
			}
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
                                return (ret);
                	}
                	reg_val = i2c_smbus_read_byte(taos_datap->client);
			reg_val = reg_val & (~(TAOS_TRITON_CNTL_PROX_INT_ENBL | TAOS_TRITON_CNTL_PROX_DET_ENBL));
		}else
			reg_val = 0x00;
		if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x00), reg_val))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_off\n");
                                return (ret);
                }
		return ret;
	}
	return ret;
}

/*ergate*/
static int  auto_calibrate(void)
{
	struct taos_prox_info ps_data[3];
	int ret,i,sum;
	int prox_mean = 0;
	u16 ratio;
	struct prox_data *prox_pt;

	printk("taos software version: 2014/07/31.\n");
	if(!prox_on)
	{
		printk("%s ps is off\n",__func__);
		return -1;
	}

	sum = 0;
	for(i = 0; i < 3; i++)
	{
	ret =	taos_prox_poll(&ps_data[i]);
	if(ret < 0)
	{
		printk("%s read ps data error\n",__func__);
		return -1;
	}
	sum += ps_data[i].prox_data;
	msleep(50);
	}
	
	printk("[TAOS]:%s orignal hi=%d,lo=%d\n",__func__, taos_cfgp->prox_threshold_hi,taos_cfgp->prox_threshold_lo);

//new add


	prox_mean = sum / 3;
	if(sat_prox <= 0)
		return -1;
	ratio = 10 * prox_mean / sat_prox;

	for (prox_pt = prox_tablep; prox_pt->ratio && prox_pt->ratio <= ratio; prox_pt++);
	if(!prox_pt->ratio)
	printk(KERN_ERR "TAOS: ioctl prox_calibrate failde : ratio = %d prox_mean = %d sat_prox = %d\n", ratio, prox_mean, sat_prox);

	taos_cfgp->prox_threshold_hi = (prox_mean*prox_pt->hi)/10;
	taos_cfgp->prox_threshold_lo = taos_cfgp->prox_threshold_hi*THRES_LO_TO_HI_RATIO;

	if (taos_cfgp->prox_threshold_lo < ((sat_prox*15)/100)) {				
		taos_cfgp->prox_threshold_hi = ((sat_prox*20)/100);
		taos_cfgp->prox_threshold_lo = (taos_cfgp->prox_threshold_hi *THRES_LO_TO_HI_RATIO);
	}

	if (taos_cfgp->prox_threshold_hi > ((sat_prox*90)/100)) {				
		taos_cfgp->prox_threshold_hi = ((sat_prox*90)/100);
		taos_cfgp->prox_threshold_lo = (taos_cfgp->prox_threshold_hi *THRES_LO_TO_HI_RATIO);
	}


	
//new add end

#if 0




	taos_cfgp->prox_threshold_hi = sum/3+80;
	taos_cfgp->prox_threshold_lo = sum/3+50;



	
	
#endif

	printk("[TAOS]:%s new hi=%d,lo=%d,mean=%d\n",__func__, taos_cfgp->prox_threshold_hi,taos_cfgp->prox_threshold_lo,sum/3);
			
			if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|TAOS_TRITON_CMD_PROXALS_INTCLR)))) < 0) {
                		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed\n");
                		
            		}
			if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MAXTHRESHLO),taos_cfgp->prox_threshold_hi))) < 0) {    	       
				pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MAXTHRESHLO\n");            			      	 
			}     
			if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MINTHRESHLO),taos_cfgp->prox_threshold_lo))) < 0) {    	       
				pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MINTHRESHLO\n");            			      	 
			}
	
	return 0;
}

// ioctls
static long taos_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct taos_data *taos_datap;
	int prox_sum = 0, prox_mean = 0, prox_max = 0;
	int lux_val = 0,  i = 0, tmp = 0;
	long ret = 0;
	u16 gain_trim_val = 0;
	u8 reg_val = 0, prox_pulse=0, prox_gain=0;
	int count=0;
	struct prox_data *prox_pt;
	u16 ratio;
	taos_datap = container_of(file->f_dentry->d_inode->i_cdev, struct taos_data, cdev);
//[sensor wlg 20110729]ALS thereshold modify add log
//printk(KERN_ERR "TAOS_wlg: taos_ioctl() cmd=%d\n", cmd);
	switch (cmd) {
		case TAOS_IOCTL_ALS_ON:
			if(prox_on)
				break;
			ret=enable_light_and_proximity(0x10);
			if(ret>=0)
			{
				light_on=1;	
            			pr_crit(TAOS_TAG "TAOS_IOCTL_ALS_ON,lux=%d\n", g_nlux);
			}			
			return (ret);
			break;
                case TAOS_IOCTL_ALS_OFF:	
			if(prox_on)
				break;
                        for (i = 0; i < TAOS_FILTER_DEPTH; i++)
                                lux_history[i] = -ENODATA;
			ret=enable_light_and_proximity(0x20);
			if(ret>=0)
			{
				light_on=0;
				//g_nlux=0;
            			pr_crit(TAOS_TAG"TAOS_IOCTL_ALS_OFF\n");
			}			
			return (ret);
                	break;
		case TAOS_IOCTL_ALS_DATA:
                        if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_data\n");
                                return (ret);
                        }
			reg_val = i2c_smbus_read_byte(taos_datap->client);
                        if ((reg_val & (TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON)) != (TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON))
                                return -ENODATA;
                        if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_STATUS)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_data\n");
                                return (ret);
                        }
                        reg_val = i2c_smbus_read_byte(taos_datap->client);
			if ((reg_val & TAOS_TRITON_STATUS_ADCVALID) != TAOS_TRITON_STATUS_ADCVALID)
				return -ENODATA;
			if ((lux_val = taos_get_lux()) < 0)
				printk(KERN_ERR "TAOS: call to taos_get_lux() returned error %d in ioctl als_data\n", lux_val);
			lux_val = taos_lux_filter(lux_val);
			return (lux_val);
			break;
		case TAOS_IOCTL_ALS_CALIBRATE:
                        if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
                                return (ret);
                        }
			reg_val = i2c_smbus_read_byte(taos_datap->client);
			if ((reg_val & 0x03) != 0x03)
				return -ENODATA;
	                if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_STATUS)))) < 0) {
        	        	printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
                                return (ret);
                        }
			reg_val = i2c_smbus_read_byte(taos_datap->client);
			if ((reg_val & 0x01) != 0x01)
                                return -ENODATA;
                        if ((lux_val = taos_get_lux()) < 0) {
                                printk(KERN_ERR "TAOS: call to lux_val() returned error %d in ioctl als_data\n", lux_val);
	                        return (lux_val);
			}
			gain_trim_val = (u16)(((taos_cfgp->calibrate_target) * 512)/lux_val);
			taos_cfgp->gain_trim = (int)gain_trim_val;		
			return ((int)gain_trim_val);
			break;
			
		case TAOS_IOCTL_CONFIG_GET:
			ret = copy_to_user((struct taos_cfg *)arg, taos_cfgp, sizeof(struct taos_cfg));
			if (ret) {
				printk(KERN_ERR "TAOS: copy_to_user failed in ioctl config_get\n");
				return -ENODATA;
			}
			return (ret);
			break;
                case TAOS_IOCTL_CONFIG_SET:
                        ret = copy_from_user(taos_cfgp, (struct taos_cfg *)arg, sizeof(struct taos_cfg));
			if (ret) {
				printk(KERN_ERR "TAOS: copy_from_user failed in ioctl config_set\n");
                                return -ENODATA;
			}
    		if(taos_cfgp->als_time < 50)
		               	taos_cfgp->als_time = 50;
			if(taos_cfgp->als_time > 650)
		               	taos_cfgp->als_time = 650;
			tmp = (taos_cfgp->als_time + 25)/50;
        		taos_cfgp->als_time = tmp*50;
		        sat_als = (256 - taos_cfgp->prox_int_time) << 10;
	       		sat_prox = (256 - taos_cfgp->prox_adc_time) << 10;
			if(!taos_cfgp->prox_pulse_cnt)
				taos_cfgp->prox_pulse_cnt=prox_pulse_cnt_param;
#if defined(FUNCTION_BOOT_PROX_CALIB)
///////////////add ps quick calibrate///////////////////////////////
            pr_crit(KERN_ERR "taos prox th_hi=%d,th_lo=%d (from file)\n",taos_cfgp->prox_threshold_hi,taos_cfgp->prox_threshold_lo);
			ret=enable_light_and_proximity(0x01);
			if(ret < 0)
			{
            	pr_crit(TAOS_TAG "error: can't PROX_ON for quick calib.\n");
				return -EPERM;
			}
	        mutex_lock(&taos_datap->proximity_calibrating);
	        ret=taos_ps_means_quick_get(5);
			if(ret < 0){
            	pr_crit(TAOS_TAG "error: quick calib failed.\n");
			}else{
			    taos_ps_th_reset(ret);
			}
			mutex_unlock(&taos_datap->proximity_calibrating);
			ret=enable_light_and_proximity(0x02);
			if(ret < 0)
			{
            	pr_crit(TAOS_TAG "error: can't PROX_OFF for quick calib.\n");
				return -EPERM;
			}
			pr_crit(KERN_ERR "taos [ioct_set]prox_cal_threshold_hi=%d,prox_cal_threshold_lo=%d\n",taos_cfgp->prox_threshold_hi,taos_cfgp->prox_threshold_lo);
			return (ret);

///////////////end//////////////////////////////////////////////////
#endif
                	break;
		case TAOS_IOCTL_PROX_ON:				
			ret=enable_light_and_proximity(0x01);
			if(ret>=0)
			{
				prox_on = 1;	
            			pr_crit(TAOS_TAG "TAOS_IOCTL_PROX_ON\n");
			}
			/*ergate*/
			msleep(250);
			auto_calibrate();
			return ret;	
			break;
                case TAOS_IOCTL_PROX_OFF:
			ret=enable_light_and_proximity(0x02);
			if(ret>=0)
			{
				prox_on = 0;	
            			pr_crit(TAOS_TAG"TAOS_IOCTL_PROX_OFF\n");
			}	
			return ret;	
			break;
		case TAOS_IOCTL_PROX_DATA:
                        ret = copy_to_user((struct taos_prox_info *)arg, prox_cur_infop, sizeof(struct taos_prox_info));
                        if (ret) {
                                printk(KERN_ERR "TAOS: copy_to_user failed in ioctl prox_data\n");
                                return -ENODATA;
                        }
                        return (ret);
                        break;
                case TAOS_IOCTL_PROX_EVENT:
 			return (prox_cur_infop->prox_event);
                        break;
		case TAOS_IOCTL_PROX_CALIBRATE:
			if (!prox_on)			
			 {
				printk(KERN_ERR "TAOS: ioctl prox_calibrate was called before ioctl prox_on was called\n");
				return -EPERM;
			}

			mutex_lock(&taos_datap->proximity_calibrating);
			count=0;
			prox_pulse=prox_pulse_cnt_param;
			prox_gain=prox_gain_param;
			if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0E), prox_pulse))) < 0) {
                          printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed\n");
                          
			}
			if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN)))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl prox_on\n");
                                
			}
			reg_val = i2c_smbus_read_byte(taos_datap->client);
			reg_val = reg_val & 0x03;
			reg_val = reg_val | (prox_gain & 0xFC);
			if(1 == is_tmd27723) reg_val |= 0x0c; //set prox 8X gain
			if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN), reg_val))) < 0) {
                                printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox gain\n");
                                
			}	
			msleep(50);	
			do{			
				prox_sum = 0;
				prox_max = 0;
				prox_mean = 0;
				count++;
				for (i = 0; i < 20; i++) {
	                        if ((ret = taos_prox_poll(&prox_cal_info[i])) < 0) {
        	                        printk(KERN_ERR "TAOS: call to prox_poll failed in ioctl prox_calibrate\n");
						mutex_unlock(&taos_datap->proximity_calibrating);			
                	                return (ret);
                        		}					
					prox_sum += prox_cal_info[i].prox_data;
					if (prox_cal_info[i].prox_data > prox_max)
						prox_max = prox_cal_info[i].prox_data;
					msleep(50);
				}
				prox_mean = prox_sum/20;
			 
				if(taos_datap->chip_type==TMD2771){				
				ratio = 10*prox_mean/sat_prox;
				
				for (prox_pt = prox_tablep; prox_pt->ratio && prox_pt->ratio <= ratio; prox_pt++);
        		if(!prox_pt->ratio){
				    printk(KERN_ERR "TAOS: ioctl prox_calibrate failde : ratio = %d prox_mean = %d sat_prox = %d\n", ratio, prox_mean, sat_prox);
					//mutex_unlock(&taos_datap->proximity_calibrating);
                	//return -ERANGE;
					}
					 
				taos_cfgp->prox_threshold_hi = (prox_mean*prox_pt->hi)/10;
				taos_cfgp->prox_threshold_lo = taos_cfgp->prox_threshold_hi*THRES_LO_TO_HI_RATIO;	
				}
				else{
				taos_cfgp->prox_threshold_hi = 15*prox_mean/10;
				taos_cfgp->prox_threshold_lo = taos_cfgp->prox_threshold_hi*THRES_LO_TO_HI_RATIO;					
				}
				if(taos_cfgp->prox_threshold_hi>=(sat_prox*3/10)){
					prox_pulse-=1;	
				}	
				if(taos_cfgp->prox_threshold_hi<=(sat_prox/20)){
					prox_pulse+=1;	
				}
				printk(KERN_ERR "taos:prox_pulse_cnt_param=%d, s(%d), hi(%d), lo(%d), c(%d), g(0x%0x)\n",prox_pulse,sat_prox,taos_cfgp->prox_threshold_hi,taos_cfgp->prox_threshold_lo,count,prox_gain);
				if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0E), prox_pulse))) < 0) {
                          	printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed\n");                         	
				}	           	
				msleep(50);	
			}while(((taos_cfgp->prox_threshold_hi>=(sat_prox*3/10))||(taos_cfgp->prox_threshold_hi<(sat_prox/20))) && (prox_pulse > 0) && (prox_pulse <= 32));
			
			if(prox_pulse < 1){
				prox_pulse = 1;
				printk("TAOS: ----- prox_pulse = 1, prox_mean = %d\n", prox_mean);
				if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0E), prox_pulse))) < 0) {
                          	printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed\n");                         	
				}
				if(prox_mean >= 685){
					mutex_unlock(&taos_datap->proximity_calibrating);
					return -ERANGE;
				}
			}

			if(prox_pulse <= 0 || prox_pulse > 32) {
				mutex_unlock(&taos_datap->proximity_calibrating);
				return -ERANGE;
			}


			taos_cfgp->prox_pulse_cnt = prox_pulse;	
			taos_cfgp->prox_gain = prox_gain;	
			if (taos_cfgp->prox_threshold_lo < ((sat_prox*15)/100)) {				
				taos_cfgp->prox_threshold_hi = ((sat_prox*20)/100);
				taos_cfgp->prox_threshold_lo = (taos_cfgp->prox_threshold_hi *THRES_LO_TO_HI_RATIO);
			}

			if (taos_cfgp->prox_threshold_hi > ((sat_prox*90)/100)) {				
				taos_cfgp->prox_threshold_hi = ((sat_prox*90)/100);
				taos_cfgp->prox_threshold_lo = (taos_cfgp->prox_threshold_hi *THRES_LO_TO_HI_RATIO);
			}
			
			if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|TAOS_TRITON_CMD_PROXALS_INTCLR)))) < 0) {
                		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed\n");
                		
            		}
			if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MAXTHRESHLO),taos_cfgp->prox_threshold_hi))) < 0) {    	       
				pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MAXTHRESHLO\n");            			      	 
			}     
			if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_PRX_MINTHRESHLO),taos_cfgp->prox_threshold_lo))) < 0) {    	       
				pr_crit(TAOS_TAG "i2c_smbus_write_byte() to TAOS_TRITON_PRX_MINTHRESHLO\n");            			      	 
			}  
			reg_val=light_on? 0x3F:0x2F;
			if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x00), reg_val))) < 0) { 
                		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed\n");
                		
            		}	
			mutex_unlock(&taos_datap->proximity_calibrating);
			pr_crit(KERN_ERR "taos [ioct_cal]prox_cal_threshold_hi=%d,prox_cal_threshold_lo=%d\n",taos_cfgp->prox_threshold_hi,taos_cfgp->prox_threshold_lo);
			return (ret);
			break;
					
		case TAOS_IOCTL_PROX_GET_ENABLED:
			return put_user(prox_on, (unsigned long __user *)arg);
			break;	
		case TAOS_IOCTL_ALS_GET_ENABLED:
			return put_user(light_on, (unsigned long __user *)arg);
			break;
			
		default:
			return -EINVAL;
			break;
	}
	return (ret);
}

// read and calculate lux value
static int taos_get_lux(void) {
        u16 raw_clear = 0, raw_ir = 0, raw_lux = 0;
	u32 lux = 0;
	u32 ratio = 0;
	u8 dev_gain = 0;
	struct lux_data *p;
	int ret = 0;
	u8 chdata[4];
	int tmp = 0, i = 0;
    	//pr_crit(TAOS_TAG "taos start to calc lux value\n");

	for (i = 0; i < 4; i++) {
	        if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | (TAOS_TRITON_ALS_CHAN0LO + i))))) < 0) {
        	        printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to chan0/1/lo/hi reg failed in taos_get_lux()\n");
                	return (ret);
        	}
        	chdata[i] = i2c_smbus_read_byte(taos_datap->client);
//[sensor wlg 20110729]ALS thereshold modify	
//		printk(KERN_ERR "TAOS_wlg: i2c_smbus_read_byte() i=%d ret=%d \n", i, chdata[i]);
	}
	
	tmp = (taos_cfgp->als_time + 25)/50;
        TritonTime.numerator = 1;
        TritonTime.denominator = tmp;

        tmp = 300 * taos_cfgp->als_time;
        if(tmp > 65535)
                tmp = 65535;
        TritonTime.saturation = tmp;
	raw_clear = chdata[1];
	raw_clear <<= 8;
	raw_clear |= chdata[0];
	raw_ir    = chdata[3];
	raw_ir    <<= 8;
	raw_ir    |= chdata[2];
//[sensor wlg 20110729]ALS thereshold modify	
    if( raw_clear < ((TAOS_MAX_LUX*5)>>2))
    {
    	als_intr_threshold_hi_param = raw_clear + (raw_clear>>2);
    	als_intr_threshold_lo_param = raw_clear - (raw_clear>>2);
    }
    else
    {
        als_intr_threshold_hi_param = raw_clear + (TAOS_MAX_LUX>>2);        
        als_intr_threshold_lo_param = TAOS_MAX_LUX;
    }
printk(KERN_ERR "TAOS: lux_timep->saturation=%d hi=%d lo=%d\n", lux_timep->saturation, als_intr_threshold_hi_param, als_intr_threshold_lo_param);

	//update threshold 
    	//printk(TAOS_TAG "als_intr_threshold_hi_param=%x,als_intr_threshold_lo_param=%x\n",als_intr_threshold_hi_param,als_intr_threshold_lo_param);
    	if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_ALS_MAXTHRESHLO),als_intr_threshold_hi_param))) < 0) {
        	        printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to chan0/1/lo/hi reg failed in taos_get_lux()\n");
        	}
    	if ((ret = (i2c_smbus_write_word_data(taos_datap->client, (0xA0 | TAOS_TRITON_ALS_MINTHRESHLO),als_intr_threshold_lo_param))) < 0) {
        	        printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to chan0/1/lo/hi reg failed in taos_get_lux()\n");
        	}
	
	if(raw_ir > raw_clear) {
		raw_lux = raw_ir;
		raw_ir = raw_clear;
		raw_clear = raw_lux;
	}
	raw_clear *= taos_cfgp->scale_factor;	
	raw_ir *= taos_cfgp->scale_factor;
	dev_gain = taos_triton_gain_table[taos_cfgp->gain & 0x3];
        if(raw_clear >= lux_timep->saturation)
                return((TAOS_MAX_LUX)*(taos_cfgp->gain_trim));//[sensor wlg 20110729]ALS thereshold modify
        if(raw_ir >= lux_timep->saturation)
                return((TAOS_MAX_LUX)*(taos_cfgp->gain_trim));//[sensor wlg 20110729]ALS thereshold modify
        if(raw_clear == 0)
                return(0);
        if(dev_gain == 0 || dev_gain > 127) {
		printk(KERN_ERR "TAOS: dev_gain = 0 or > 127 in taos_get_lux()\n");
                return -1;
	}
        if(lux_timep->denominator == 0) {
		printk(KERN_ERR "TAOS: lux_timep->denominator = 0 in taos_get_lux()\n");
                return -1;
	}
	ratio = (raw_ir<<15)/raw_clear;
	for (p = lux_tablep; p->ratio && p->ratio < ratio; p++);
        if(!p->ratio)
                return -1;
	lux = ((raw_clear*(p->clear)) - (raw_ir*(p->ir)));
	lux = ((lux + (lux_timep->denominator >>1))/lux_timep->denominator) * lux_timep->numerator;
        lux = (lux + (dev_gain >> 1))/dev_gain;
	lux >>= TAOS_SCALE_MILLILUX;
//[sensor wlg 20110729]ALS thereshold modify
        if(lux > (TAOS_MAX_LUX)*(taos_cfgp->gain_trim))
                lux = (TAOS_MAX_LUX)*(taos_cfgp->gain_trim);
	return(lux);
}

static int taos_lux_filter(int lux)
{
        static u8 middle[] = {1,0,2,0,0,2,0,1};
        int index;

        lux_history[2] = lux_history[1];
        lux_history[1] = lux_history[0];
        lux_history[0] = lux;
        if((lux_history[2] < 0) || (lux_history[1] < 0) || (lux_history[0] < 0))
		return -ENODATA;
        index = 0;
        if( lux_history[0] > lux_history[1] ) index += 4;
        if( lux_history[1] > lux_history[2] ) index += 2;
        if( lux_history[0] > lux_history[2] ) index++;
        return(lux_history[middle[index]]);
}

// verify device
static int taos_device_name(unsigned char *bufp, char **device_name) {
		if( ((bufp[0x12]&0xf0)!=0x00)&&((bufp[0x12]&0xf0)!=0x20) && ((bufp[0x12]&0xff)!=0x39))
			return(0);
		//distinguish TSL2771 and TMD2771
		if((bufp[0x12]&0xf0)==0x20 || (bufp[0x12]&0xff)==0x39)
			taos_datap->chip_type=TMD2771;
		else
			taos_datap->chip_type=TSL2771;
		if ((bufp[0x12]&0xff)==0x39)  is_tmd27723 = 1;
		printk(KERN_ERR "TAOS: taos_device_name is %d, is_tmd27723 is %d\n",taos_datap->chip_type, is_tmd27723);
		taos_chip_diff_settings();
		if((bufp[0x12]&0xff) !=0x39){
			if(bufp[0x10]|bufp[0x1a]|bufp[0x1b]|bufp[0x1c]|bufp[0x1d]|bufp[0x1e])
				return(0);
	  		if(bufp[0x13]&0x0c)
				return(0);
		}
		//*device_name="tritonFN";
		*device_name="taos";
		return(1);
}

static void taos_chip_diff_settings()
{
	if(taos_datap->chip_type==TMD2771){
		prox_pulse_cnt_param = 4;
	}
	if(taos_datap->chip_type==TSL2771)
		prox_pulse_cnt_param = 10;
}

// proximity poll
static int taos_prox_poll(struct taos_prox_info *prxp) {
	//static int event = 0;
        //u16 status = 0;
        int i = 0, ret = 0;//wait_count = 0
        u8 chdata[6];
/*
        if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | 0x13)))) < 0) {
                printk(KERN_ERR "TAOS: i2c_write_byte address 0x13  failed in taos_prox_poll()\n");
                return (ret);
        }
        status = i2c_smbus_read_byte(taos_datap->client);
        //pr_crit(TAOS_TAG "status=%d\n",status);
        while ((status & 0x30) != 0x30) {
                status = i2c_smbus_read_byte(taos_datap->client);
                wait_count++;
                if (wait_count > 10) {
                        printk(KERN_ERR "TAOS: Prox status invalid for 100 ms in taos_prox_poll()\n");
                        return -ENODATA;
                }
                mdelay(10);
        }
        pr_crit(TAOS_TAG "success after read:wait_count=%d,status=%d\n",wait_count,status);
        if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x05)))) < 0) {
                printk(KERN_ERR "TAOS: i2c_write_byte prox intr clear  failed in taos_prox_poll()\n");
                return (ret);
        }
        if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x06)))) < 0) {
                printk(KERN_ERR "TAOS: i2c_write_byte als intr clear failed in taos_prox_poll()\n");
                return (ret);
        }*/
        for (i = 0; i < 6; i++) {
                if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | (TAOS_TRITON_ALS_CHAN0LO + i))))) < 0) {
                        printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to als/prox data reg failed in taos_prox_poll()\n");
                        return (ret);
                }
                chdata[i] = i2c_smbus_read_byte(taos_datap->client);
        }
        prxp->prox_data = chdata[5];
        prxp->prox_data <<= 8;
        prxp->prox_data |= chdata[4];

        prxp->prox_clear = chdata[1];
        prxp->prox_clear <<= 8;
        prxp->prox_clear |= chdata[0];
        if (prxp->prox_clear > ((sat_als*80)/100)){
		    printk(KERN_ERR "TAOS: failed in taos_prox_poll() prxp->prox_clear = %d prxp->prox_data = %d\n", prxp->prox_clear, prxp->prox_data);
                return -ENODATA;
			}

/*	prox_history_hi <<= 1; 
	prox_history_hi |= ((prxp->prox_data > taos_cfgp->prox_threshold_hi)? 1:0);

	prox_history_hi &= 0x07;
        prox_history_lo <<= 1;
        prox_history_lo |= ((prxp->prox_data > taos_cfgp->prox_threshold_lo)? 1:0);
    
        prox_history_lo &= 0x07;
	if (prox_history_hi == 0x07)
		event = 1;
	else {
		if (prox_history_lo == 0)
			event = 0;
	}

	prxp->prox_event = event;*/
        return (ret);
}

/* prox poll timer function
static void taos_prox_poll_timer_func(unsigned long param) {
	int ret = 0;

	if (!device_released) {
		if ((ret = taos_prox_poll(prox_cur_infop)) < 0) {
			printk(KERN_ERR "TAOS: call to prox_poll failed in taos_prox_poll_timer_func()\n");
			return;
		}
		taos_prox_poll_timer_start();
	}
	return;
}

// start prox poll timer
static void taos_prox_poll_timer_start(void) {
        init_timer(&prox_poll_timer);
        prox_poll_timer.expires = jiffies + (HZ/10);
        prox_poll_timer.function = taos_prox_poll_timer_func;
        add_timer(&prox_poll_timer);

        return;
}*/
#if defined(FUNCTION_BOOT_PROX_CALIB)
static int taos_ps_means_quick_get(int times)
{
	int prox_sum = 0, prox_mean = 0;
	int i, ret;
	struct prox_data *prox_pt;
	u16 ratio;			

	msleep(50);	
	for (i = 0; i < 7; i++) {
		msleep(50);	
        if ((ret = taos_prox_poll(&prox_cal_info[i])) < 0) {
        	printk(KERN_ERR "TAOS: call to prox_poll failed in ioctl prox_calibrate\n");
            return -1;
        }
		if (ret > 0){
		    i = 7;
		}	
    }		
	prox_max = 0; 
	prox_min = 0xFFFF;

	for (i = 0; i < (times+2); i++) {
		msleep(50);	
        if ((ret = taos_prox_poll(&prox_cal_info[i])) < 0) {
        	printk(KERN_ERR "TAOS: call to prox_poll failed in ioctl prox_calibrate\n");
            return -1;
        }
		printk(KERN_ERR "TAOS: [%d] read ps data is %d\n",i,prox_cal_info[i].prox_data);
		prox_sum += prox_cal_info[i].prox_data;
		if (prox_cal_info[i].prox_data > prox_max){
			prox_max = prox_cal_info[i].prox_data;
		}
		if (prox_cal_info[i].prox_data < prox_min){
			prox_min = prox_cal_info[i].prox_data;
		}
	}
	printk(KERN_ERR "TAOS: read ps data max:%d,min:%d,sum:%d\n",prox_max,prox_min,prox_sum);
	if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|TAOS_TRITON_CMD_PROXALS_INTCLR)))) < 0) {
    	printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed\n");                		
    }

	prox_mean = (prox_sum-prox_max-prox_min)/times;

	if( (prox_max-prox_min) > 100){
	    printk(KERN_ERR "TAOS: error: delta to big.\n");
       	return -1;
	}

	if(taos_datap->chip_type==TMD2771){				
		ratio = 10*prox_mean/sat_prox;
		for (prox_pt = prox_tablep; prox_pt->ratio && prox_pt->ratio <= ratio; prox_pt++);
       		if(!prox_pt->ratio)
              	return -1;				
	}else{
        prox_pt->lo = 12;
    }	
    if( prox_max >= (prox_mean*prox_pt->lo)/10 ){
        printk(KERN_ERR "TAOS: error: max more then threshold lo.\n");
   	    return -1;
    }
    printk(KERN_ERR "TAOS: prox quick get ok.(%d)\n",prox_mean);
	return prox_mean;
}


static void taos_ps_th_reset(int prox_mean)
{
	struct prox_data *prox_pt;
	u16 prox_threshold_hi;
	u16 prox_threshold_lo;
	u16 ratio;			
	if(taos_datap->chip_type==TMD2771){				
		ratio = 10*prox_mean/sat_prox;

		for (prox_pt = prox_tablep; prox_pt->ratio && prox_pt->ratio <= ratio; prox_pt++);
       		if(!prox_pt->ratio)
              	return;
					 
		prox_threshold_hi = (prox_mean*prox_pt->hi)/10;
	}else{
		prox_threshold_hi = 15*prox_mean/10;
	}
	prox_threshold_lo = prox_threshold_hi*THRES_LO_TO_HI_RATIO;	
		
	if(prox_threshold_hi>=sat_prox){
	    return;
	}	
	if(prox_threshold_hi<=(sat_prox*2/5)){
	    if(taos_datap->chip_type!=TMD2771){	
	        return;
		}
	}

	taos_cfgp->prox_threshold_hi = prox_threshold_hi;
	taos_cfgp->prox_threshold_lo = prox_threshold_lo;
    if (taos_cfgp->prox_threshold_lo < ((sat_prox*15)/100)) {				
	    taos_cfgp->prox_threshold_hi = ((sat_prox*20)/100);
	    taos_cfgp->prox_threshold_lo = (taos_cfgp->prox_threshold_hi *THRES_LO_TO_HI_RATIO);
    }	
}
#endif
MODULE_AUTHOR("John Koshi - Surya Software");
MODULE_DESCRIPTION("TAOS ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");

module_init(taos_init);
module_exit(taos_exit);

