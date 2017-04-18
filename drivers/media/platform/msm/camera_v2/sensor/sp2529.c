/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#define SP2529_SENSOR_NAME "sp2529"
#define PLATFORM_DRIVER_NAME "msm_camera_sp2529"

//#define CONFIG_MSMB_CAMERA_DEBUG

/*
  * Added for accurate  standby
  *
  * by ZTE_YCM_20140324 yi.changming
  */
// --->
#define SENSOR_USE_STANDBY
#define TAG "ZTE_YCM"
// <---

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

//#define DEBUG_SENSOR_SP

#ifdef DEBUG_SENSOR_SP
#define SP_OP_CODE_INI		0x00		/* Initial value. */
#define SP_OP_CODE_REG		0x01		/* Register */
#define SP_OP_CODE_DLY		0x02		/* Delay */
#define SP_OP_CODE_END		0x03		/* End of initial setting. */
uint16_t fromsd;

struct msm_camera_i2c_reg_conf SP_Init_Reg[1000];
struct msm_camera_i2c_reg_conf SP_SVGA_Reg[1000];

u32 strtol(const char *nptr, u8 base)
{
	u8 ret;
	if(!nptr || (base!=16 && base!=10 && base!=8))
	{
		printk("%s(): NULL pointer input\n", __FUNCTION__);
		return -1;
	}
	for(ret=0; *nptr; nptr++)
	{
		if((base==16 && *nptr>='A' && *nptr<='F') || 
				(base==16 && *nptr>='a' && *nptr<='f') || 
				(base>=10 && *nptr>='0' && *nptr<='9') ||
				(base>=8 && *nptr>='0' && *nptr<='7') )
		{
			ret *= base;
			if(base==16 && *nptr>='A' && *nptr<='F')
				ret += *nptr-'A'+10;
			else if(base==16 && *nptr>='a' && *nptr<='f')
				ret += *nptr-'a'+10;
			else if(base>=10 && *nptr>='0' && *nptr<='9')
				ret += *nptr-'0';
			else if(base>=8 && *nptr>='0' && *nptr<='7')
				ret += *nptr-'0';
		}
		else
			return ret;
	}
	return ret;
}

u32 reg_num = 0;
u32 reg_sgva_num = 0;

u8 SP_Initialize_from_T_Flash(void)
{
	//FS_HANDLE fp = -1;				/* Default, no file opened. */
	//u8 *data_buff = NULL;
	u8 *curr_ptr = NULL;
	u32 file_size = 0;
	//u32 bytes_read = 0;
	u32 i = 0;
	u8 func_ind[4] = {0};	/* REG or DLY */


	struct file *fp; 
	mm_segment_t fs; 
	loff_t pos = 0; 
	static u8 data_buff[10*1024] ;

	fp = filp_open("/system/lib/sp_sd", O_RDONLY , 0); 
	if (IS_ERR(fp)) { 
		printk("create file error %x \n", (unsigned int)fp); 
		return 0; 
	} 
	fs = get_fs(); 
	set_fs(KERNEL_DS); 

	file_size = vfs_llseek(fp, 0, SEEK_END);
	vfs_read(fp, data_buff, file_size, &pos); 
	//printk("%s %d %d\n", buf,iFileLen,pos); 
	filp_close(fp, NULL); 
	set_fs(fs);

	reg_num = 0;

	/* Start parse the setting witch read from t-flash. */
	curr_ptr = data_buff;
	while (curr_ptr < (data_buff + file_size))
	{
		while ((*curr_ptr == ' ') || (*curr_ptr == '\t'))/* Skip the Space & TAB */
			curr_ptr++;				

		if (((*curr_ptr) == '/') && ((*(curr_ptr + 1)) == '*'))
		{
			while (!(((*curr_ptr) == '*') && ((*(curr_ptr + 1)) == '/')))
			{
				curr_ptr++;		/* Skip block comment code. */
			}

			while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
			{
				curr_ptr++;
			}

			curr_ptr += 2;						/* Skip the enter line */

			continue ;
		}

		if (((*curr_ptr) == '/') || ((*curr_ptr) == '{') || ((*curr_ptr) == '}'))		/* Comment line, skip it. */
		{
			while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
			{
				curr_ptr++;
			}

			curr_ptr += 2;						/* Skip the enter line */

			continue ;
		}
		/* This just content one enter line. */
		if (((*curr_ptr) == 0x0D) && ((*(curr_ptr + 1)) == 0x0A))
		{
			curr_ptr += 2;
			continue ;
		}
		//printk(" curr_ptr1 = %s\n",curr_ptr);
		memcpy(func_ind, curr_ptr, 3);


		if (strcmp((const char *)func_ind, "REG") == 0)		/* REG */
		{
			curr_ptr += 6;				/* Skip "REG(0x" or "DLY(" */

			SP_Init_Reg[i].reg_addr = strtol((const char *)curr_ptr, 16);
			curr_ptr += 5;	/* Skip "00, 0x" */

			SP_Init_Reg[i].reg_data = strtol((const char *)curr_ptr, 16);
			curr_ptr += 4;	/* Skip "00);" */

			reg_num = i;
			//printk("i %d, reg_num %d \n", i, reg_num);
		}
		
		i++;


		/* Skip to next line directly. */
		while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
		{
			curr_ptr++;
		}
		curr_ptr += 2;
	}

	
	return 1;	
}
u8 SP_Svga_Initialize_from_T_Flash(void)
{
	//FS_HANDLE fp = -1;				/* Default, no file opened. */
	//u8 *data_buff = NULL;
	u8 *curr_ptr = NULL;
	u32 file_size = 0;
	//u32 bytes_read = 0;
	u32 i = 0;
	u8 func_ind[4] = {0};	/* REG or DLY */


	struct file *fp; 
	mm_segment_t fs; 
	loff_t pos = 0; 
	static u8 data_buff[10*1024] ;

	fp = filp_open("/system/lib/sp_svga_sd", O_RDONLY , 0); 
	if (IS_ERR(fp)) { 
		printk("create file error %x \n", (unsigned int)fp); 
		return 0; 
	} 
	fs = get_fs(); 
	set_fs(KERNEL_DS); 

	file_size = vfs_llseek(fp, 0, SEEK_END);
	vfs_read(fp, data_buff, file_size, &pos); 
	//printk("%s %d %d\n", buf,iFileLen,pos); 
	filp_close(fp, NULL); 
	set_fs(fs);

	reg_sgva_num = 0;

	/* Start parse the setting witch read from t-flash. */
	curr_ptr = data_buff;
	while (curr_ptr < (data_buff + file_size))
	{
		while ((*curr_ptr == ' ') || (*curr_ptr == '\t'))/* Skip the Space & TAB */
			curr_ptr++;				

		if (((*curr_ptr) == '/') && ((*(curr_ptr + 1)) == '*'))
		{
			while (!(((*curr_ptr) == '*') && ((*(curr_ptr + 1)) == '/')))
			{
				curr_ptr++;		/* Skip block comment code. */
			}

			while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
			{
				curr_ptr++;
			}

			curr_ptr += 2;						/* Skip the enter line */

			continue ;
		}

		if (((*curr_ptr) == '/') || ((*curr_ptr) == '{') || ((*curr_ptr) == '}'))		/* Comment line, skip it. */
		{
			while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
			{
				curr_ptr++;
			}

			curr_ptr += 2;						/* Skip the enter line */

			continue ;
		}
		/* This just content one enter line. */
		if (((*curr_ptr) == 0x0D) && ((*(curr_ptr + 1)) == 0x0A))
		{
			curr_ptr += 2;
			continue ;
		}
		//printk(" curr_ptr1 = %s\n",curr_ptr);
		memcpy(func_ind, curr_ptr, 3);


		if (strcmp((const char *)func_ind, "REG") == 0)		/* REG */
		{
			curr_ptr += 6;				/* Skip "REG(0x" or "DLY(" */

			SP_SVGA_Reg[i].reg_addr = strtol((const char *)curr_ptr, 16);
			curr_ptr += 5;	/* Skip "00, 0x" */

			SP_SVGA_Reg[i].reg_data = strtol((const char *)curr_ptr, 16);
			curr_ptr += 4;	/* Skip "00);" */

			reg_sgva_num = i;
			//printk("i %d, reg_sgva_num %d \n", i, reg_sgva_num);
		}
		
		i++;


		/* Skip to next line directly. */
		while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
		{
			curr_ptr++;
		}
		curr_ptr += 2;
	}

	
	return 1;	
}
#endif

DEFINE_MSM_MUTEX(sp2529_mut);
static struct msm_sensor_ctrl_t sp2529_s_ctrl;
#if 0
static struct msm_sensor_power_setting sp2529_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.sleep_val= GPIO_OUT_LOW,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.sleep_val= GPIO_OUT_LOW,
		.delay = 0,
	},	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VDIG,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
#if defined CONFIG_BOARD_FAUN

#else
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 5,
	},
#endif	
	{
	.seq_type = SENSOR_CLK,
	.seq_val = SENSOR_CAM_MCLK,
	.config_val = 24000000,
	.delay = 10,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_STANDBY,
	.config_val = GPIO_OUT_HIGH,
	.delay = 5,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_STANDBY,
	.config_val = GPIO_OUT_LOW,
	.delay = 20,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_RESET,
	.config_val = GPIO_OUT_HIGH,
	.delay = 10,
	},
	{
	.seq_type = SENSOR_I2C_MUX,
	.seq_val = 0,
	.config_val = 0,
	.delay = 0,
	},
};
#else //ZTE_YCM_20140215 lead current from 26 to 1.45
static struct msm_sensor_power_setting sp2529_power_setting[] = {
	{
	.seq_type = SENSOR_CLK,
	.seq_val = SENSOR_CAM_MCLK,
	.config_val = 24000000,
	.delay = 0,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_STANDBY,
	.config_val = GPIO_OUT_HIGH,
	.sleep_val = GPIO_OUT_HIGH,
	.delay = 1,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_STANDBY,
	.config_val = GPIO_OUT_LOW,
	.sleep_val = GPIO_OUT_HIGH,
	.delay = 0,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_RESET,
	.config_val = GPIO_OUT_LOW,
	.sleep_val = GPIO_OUT_HIGH,
	.delay = 1,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_RESET,
	.config_val = GPIO_OUT_HIGH,
	.sleep_val = GPIO_OUT_HIGH,
	.delay = 7,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_STANDBY,
	.config_val = GPIO_OUT_LOW,
	.sleep_val = GPIO_OUT_HIGH,
	.delay = 0,
	},
	{
	.seq_type = SENSOR_I2C_MUX,
	.seq_val = 0,
	.config_val = 0,
	.delay = 0,
	},
};
#endif
/*
  * Added for accurate  standby
  *
  * by ZTE_YCM_20140324 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
static int32_t sp2529_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
	msleep(1);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,0);
	printk("%s:%s X\n",TAG,__func__);
	return 0;
}
static int32_t sp2529_sensor_power_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,1);
	msleep(1);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_LOW);
	printk("%s:%s X\n",TAG,__func__);
	return 0;
}
#endif
// <---
static struct msm_camera_i2c_reg_conf sp2529_capture_uxga_settings[] = {
	//uxga
 {0xfd,0x00},
 {0x19,0x00},
 {0x30,0x0c},//0c 
 {0x31,0x00},
 {0x33,0x00},
//MIPI      
 {0x95,0x06},
 {0x94,0x40},
 {0x97,0x04},
 {0x96,0xb0},
 
{0xfd,0x00},
{0x2f,0x04},///1.5 ±¶Æµ 9 

	
 
};
#define UXGA 
#if 0
static struct msm_camera_i2c_reg_conf sp2529_uxga_settings[] = {
{0xfd,0x00},
{0x2f,0x04},
{0x30,0x0c},
{0x03,0x02}, 
{0x04,0xa0},
{0x05,0x00},
{0x06,0x00},
{0x07,0x00},
{0x08,0x00},
{0x09,0x00},
{0x0a,0x95},
{0xfd,0x01},
{0xf0,0x00},
{0xf7,0x70},
{0xf8,0x5d},
{0x02,0x0b},
{0x03,0x01},
{0x06,0x70},
{0x07,0x00},
{0x08,0x01},
{0x09,0x00},
{0xfd,0x02},
{0x3d,0x0d},
{0x3e,0x5d},
{0x3f,0x00},
{0x88,0x92},
{0x89,0x81},
{0x8a,0x54},
{0xfd,0x02},
{0xbe,0xd0},
{0xbf,0x04},
{0xd0,0xd0},
{0xd1,0x04},
{0xc9,0xd0},
{0xca,0x04},
{0xfd,0x01},
{0x36,0x00},
};
#endif
#if 1
//20141017 lvcheng,modify mipi setting
static struct msm_camera_i2c_reg_conf sp2529_start_settings[] = {
  {0xfd,0x00},
  {0xfd,0x00},
  {0x92,0x81},
  {0xfd,0x01},
  {0x36,0x00},
  {0xfd,0x00},
  //{0xe7,0x03},
 // {0xe7,0x00},
  {0xfd,0x00},
};

static struct msm_camera_i2c_reg_conf sp2529_stop_settings[] = {
  {0xfd,0x00},
  {0xfd,0x00},
  {0x92,0x81},
  {0xfd,0x01},
  {0x36,0x02},
  {0xfd,0x00},
  {0xe7,0x03},
  {0xe7,0x00},
  {0xfd,0x00},

};
#endif
static struct msm_camera_i2c_reg_conf sp2529_recommend_settings[] = {
{0xfd,0x01},
{0x36,0x02},
{0xfd,0x00},
{0xe7,0x03},
{0xe7,0x00},
{0xfd,0x00},
//bininig svga
 {0xfd,0x00},
 {0x19,0x03},
 {0x30,0x0c},//01 0c
 {0x31,0x04},
 {0x33,0x01},
 //MIPI     
 {0x95,0x03},
 {0x94,0x20},
 {0x97,0x02},
 {0x96,0x58},
 {0xac,0x03},//lvcheng 20141128 from sbk fxp,for 2529s,DPHY
 {0xa2,0x16},//data lane hs_zero
 
 {0x98,0x3a},
 
{0x1c,0x17},
{0x09,0x01},
{0xfd,0x00},
{0x0c,0x55},
{0x27,0xa5},
{0x1a,0x4b},
{0x20,0x2f},
{0x22,0x5a},
{0x25,0x8f},
{0x21,0x10},
{0x28,0x0b},
{0x1d,0x01},
{0x7a,0x41},
{0x70,0x41},
{0x74,0x40},
{0x75,0x40},
{0x15,0x3e},
{0x71,0x3f},
{0x7c,0x3f},
{0x76,0x3f},
{0x7e,0x29},
{0x72,0x29},
{0x77,0x28},
{0x1e,0x01},
{0x1c,0x0f},
{0x2e,0xc0},
{0x1f,0xc0},
{0xfd,0x01},
{0x32,0x00},
{0xfd,0x02},
{0x85,0x00},

//sp2529 48M 1±¶Æµ  50Hz 17.0574-20fps ABinning+DBinning AE_Parameters_20140123125710
{0xfd,0x00},
{0x2f,0x04},//09
//ae setting
//sp2529 48M 1±¶Æµ  50Hz 17.0574-20fps ABinning+DBinning AE_Parameters   yyj0313
{0xfd,0x00},//lvcheng 20140701 ,100%<saturation<130% 
{0x03,0x06},//lvcheng 20140728,20fps
{0x04,0xf6},
{0x05,0x00},
{0x06,0x00},
{0x07,0x00},
{0x08,0x00},
{0x09,0x01},
{0x0a,0x23},
{0xfd,0x01},
{0xf0,0x01},
{0xf7,0x29},
{0xf8,0xf8},
{0x02,0x05},
{0x03,0x01},
{0x06,0x29},
{0x07,0x01},
{0x08,0x01},
{0x09,0x00},
{0xfd,0x02},
{0x3d,0x06},
{0x3e,0xf8},
{0x3f,0x00},
{0x88,0xb9},
{0x89,0x10},
{0x8a,0x21},
{0xfd,0x02},
{0xbe,0xcd},
{0xbf,0x05},
{0xd0,0xcd},
{0xd1,0x05},
{0xc9,0xcd},
{0xca,0x05},


{0xb8,0x90},
{0xb9,0x85},
{0xba,0x30},
{0xbb,0x45},
{0xbc,0xc0},
{0xbd,0x60},
{0xfd,0x03},
{0x77,0x48},
//rpc
{0xfd,0x01},
{0xe0,0x48},
{0xe1,0x38},
{0xe2,0x30},
{0xe3,0x2c},
{0xe4,0x2c},
{0xe5,0x2a},
{0xe6,0x2a},
{0xe7,0x28},
{0xe8,0x28},
{0xe9,0x28},
{0xea,0x26},
{0xf3,0x26},
{0xf4,0x26},
{0xfd,0x01},
{0x04,0xc0},
{0x05,0x26},
{0x0a,0x48},
{0x0b,0x26},

{0xfd,0x01},
{0xf2,0x09},
{0xeb,0x78},
{0xec,0x78},
{0xed,0x06},
{0xee,0x0a},

//È¥»µÏñÊý
{0xfd,0x02},
{0x4f,0x46},

{0xfd,0x03},
{0x52,0xff},
{0x53,0x60},
{0x94,0x00},
{0x54,0x00},
{0x55,0x00},
{0x56,0x80},
{0x57,0x80},
{0x95,0x00},
{0x58,0x00},
{0x59,0x00},
{0x5a,0xf6},
{0x5b,0x00},
{0x5c,0x88},
{0x5d,0x00},
{0x96,0x00},
{0xfd,0x03},
{0x8a,0x00},
{0x8b,0x00},
{0x8c,0xff},
{0x22,0xff},
{0x23,0xff},
{0x24,0xff},
{0x25,0xff},
{0x5e,0xff},
{0x5f,0xff},
{0x60,0xff},
{0x61,0xff},
{0x62,0x00},
{0x63,0x00},
{0x64,0x00},
{0x65,0x00},
//lsc
{0xfd,0x01},
{0x21,0x00},
{0x22,0x00},
{0x26,0x60},
{0x27,0x14},
{0x28,0x05},
{0x29,0x00},
{0x2a,0x01},
{0xfd,0x01},
{0xa1,0x20},
{0xa2,0x20},
{0xa3,0x1d},
{0xa4,0x1d},
{0xa5,0x1d},
{0xa6,0x1d},
{0xa7,0x1b},
{0xa8,0x1a},
{0xa9,0x1b},
{0xaa,0x1c},
{0xab,0x1b},
{0xac,0x17},
{0xad,0x03},
{0xae,0x03},
{0xaf,0x03},
{0xb0,0x03},
{0xb1,0x03},
{0xb2,0x03},
{0xb3,0x03},
{0xb4,0x03},
{0xb5,0x03},
{0xb6,0x03},
{0xb7,0x03},
{0xb8,0x03},
{0xfd,0x02},
{0x26,0xa0},
{0x27,0x96},
{0x28,0xcc},
{0x29,0x01},
{0x2a,0x02},
{0x2b,0x08},
{0x2c,0x20},
{0x2d,0xdc},
{0x2e,0x20},
{0x2f,0x96},
{0x1b,0x80},
{0x1a,0x80},
{0x18,0x16},
{0x19,0x26},
{0x1d,0x04},
{0x1f,0x06},
{0x66,0x27},
{0x67,0x57},
{0x68,0xc9},
{0x69,0xec},
{0x6a,0xa5},
{0x7c,0x17},
{0x7d,0x39},
{0x7e,0xec},
{0x7f,0x11},
{0x80,0xa6},
{0x70,0x0f},
{0x71,0x27},
{0x72,0x11},
{0x73,0x3e},
{0x74,0xaa},
{0x6b,0xf4},
{0x6c,0x0f},
{0x6d,0x12},
{0x6e,0x39},
{0x6f,0x6a},
{0x61,0xe3},
{0x62,0x06},
{0x63,0x39},
{0x64,0x60},
{0x65,0x6a},
{0x75,0x00},
{0x76,0x09},
{0x77,0x02},
{0x0e,0x16},
{0x3b,0x09},
{0xfd,0x02},
{0x02,0x00},
{0x03,0x10},
{0x04,0xf0},
{0xf5,0xb3},
{0xf6,0x80},
{0xf7,0xe0},
{0xf8,0x89},
{0xfd,0x02},
{0x08,0x00},
{0x09,0x04},
{0xfd,0x02},
{0xdd,0x0f},
{0xde,0x0f},
{0xfd,0x02},
{0x57,0x30},
{0x58,0x10},
{0x59,0xe0},
{0x5a,0x00},
{0x5b,0x12},
//sharpen
{0xcb,0x10},//0x04
{0xcc,0x16},//0x0b
{0xcd,0x10},
{0xce,0x1a},
{0xfd,0x03},
{0x87,0x04},
{0x88,0x08},
{0x89,0x10},
{0xfd,0x02},
#if 0
{0xe8,0x40},
{0xec,0x68},
{0xe9,0x48},
{0xed,0x68},
{0xea,0x58},
{0xee,0x60},
{0xeb,0x48},
{0xef,0x40},
#else
{0xe8,0x58},//fuxiuyun 20140708,sharpness
{0xec,0x58},
{0xe9,0x58},
{0xed,0x58},
{0xea,0x60},
{0xee,0x68},
{0xeb,0x50},
{0xef,0x48},
#endif
{0xfd,0x02},
{0xdc,0x04},
{0x05,0x6f},
{0xfd,0x02},
{0xf4,0x30},
{0xfd,0x03},
{0x97,0x98},
{0x98,0x88},
{0x99,0x88},
{0x9a,0x80},
{0xfd,0x02},
{0xe4,0xff},
{0xe5,0xff},
{0xe6,0xff},
{0xe7,0xff},
{0xfd,0x03},
{0x72,0x18},
{0x73,0x28},
{0x74,0x28},
{0x75,0x30},
{0xfd,0x02},
{0x78,0x20},
{0x79,0x20},
{0x7a,0x14},
{0x7b,0x08},
{0x81,0x02},
{0x82,0x20},
{0x83,0x20},
{0x84,0x08},
{0xfd,0x03},
//smooth
{0x7e,0x03},//0x06
{0x7f,0x03},//0x0d
{0x80,0x0d},
{0x81,0x10},
{0x7c,0xff},
{0x82,0x54},
{0x83,0x43},
{0x84,0x00},
{0x85,0x20},
{0x86,0x40},
{0xfd,0x03},
{0x66,0x18},
{0x67,0x28},
{0x68,0x20},
{0x69,0x88},
{0x9b,0x18},
{0x9c,0x28},
{0x9d,0x20},
//gamma
{0xfd,0x01},
{0x8b,0x00},
{0x8c,0x0f},
{0x8d,0x20},
{0x8e,0x2e},
{0x8f,0x3c},
{0x90,0x51},
{0x91,0x63},
{0x92,0x72},
{0x93,0x7d},
{0x94,0x8e},
{0x95,0x9e},
{0x96,0xae},
{0x97,0xbb},
{0x98,0xc6},
{0x99,0xd0},
{0x9a,0xd9},
{0x9b,0xe2},
{0x9c,0xea},
{0x9d,0xf1},
{0x9e,0xf8},
{0x9f,0xfd},
{0xa0,0xff},

{0xfd,0x02},
{0x15,0xb0},
{0x16,0x82},
{0xa0,0x94},//fuxiuyun 20140708,ccm
{0xa1,0xec},
{0xa2,0xfe},
{0xa3,0x0e},
{0xa4,0x77},
{0xa5,0xfa},
{0xa6,0x08},
{0xa7,0xcb},
{0xa8,0xad},
{0xa9,0x3c},
{0xaa,0x30},
{0xab,0x0c},
{0xac,0x80},
{0xad,0x0c},
{0xae,0xf4},
{0xaf,0xfa},
{0xb0,0x8c},
{0xb1,0xfa},
{0xb2,0xed},
{0xb3,0xad},
{0xb4,0xe6},
{0xb5,0x30},
{0xb6,0x33},
{0xb7,0x0f},
{0xfd,0x01},
{0xd2,0x2d},
{0xd1,0x38},
{0xdd,0x3f},
{0xde,0x37},
{0xfd,0x02},
{0xc1,0x40},
{0xc2,0x40},
{0xc3,0x40},
{0xc4,0x40},
{0xc5,0x80},
{0xc6,0x60},
{0xc7,0x00},
{0xc8,0x00},
{0xfd,0x01},
{0xd3,0xa3},//fuxiuyun 20140708,saturation
{0xd4,0xa3},
{0xd5,0xa3},
{0xd6,0xa3},
{0xd7,0xa3},
{0xd8,0xa3},
{0xd9,0xa3},
{0xda,0xa3},
{0xfd,0x03},
{0x76,0x0a},
{0x7a,0x40},
{0x7b,0x40},
{0xfd,0x01},
{0xc2,0xaa},
{0xc3,0xaa},
{0xc4,0x66},
{0xc5,0x66},
{0xfd,0x01},
{0xcd,0x08},
{0xce,0x18},
{0xfd,0x02},
{0x32,0x60},
{0x35,0x60},
{0x37,0x13},
{0xfd,0x01},
{0xdb,0x00},
{0x10,0x88},
{0x11,0x88},
{0x12,0x77},//lvcheng 20140701 ,100%<saturation<130% 
{0x13,0x77},
{0x14,0x9a},
{0x15,0x9a},
{0x16,0x77},
{0x17,0x77},
{0xfd,0x03},
{0x00,0x80},
{0x03,0x68},
{0x06,0xd8},
{0x07,0x28},
{0x0a,0xfd},
#if 0
{0x01,0x16},
{0x02,0x16},
{0x04,0x16},
{0x05,0x16},
{0x0b,0x40},
{0x0c,0x40},
{0x0d,0x40},
{0x0e,0x40},
#else
{0x01,0x16},
{0x02,0x16},
{0x04,0x16},
{0x05,0x16},
{0x0b,0x48},
{0x0c,0x48},
{0x0d,0x48},
{0x0e,0x48},
#endif
{0x08,0x0c},
{0x09,0x0c},
{0xfd,0x02},
{0x8e,0x0a},
{0x90,0x40},
{0x91,0x40},
{0x92,0x60},
{0x93,0x80},
{0x9e,0x44},
{0x9f,0x44},
{0xfd,0x02},
{0x85,0x00},
{0xfd,0x01},
{0x00,0x00},
{0xfb,0x25},
{0x32,0x15},
{0x33,0xef},
{0x34,0xef},
{0x35,0x40},
{0xfd,0x00},
{0x3f,0x03},
{0xfd,0x01},
{0x50,0x00},                                             
{0x66,0x00},
{0xfd,0x02},                                               
{0xd6,0x0f},
};

static struct v4l2_subdev_info sp2529_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt	= 1,
		.order	= 0,
	},
};

static struct msm_camera_i2c_reg_conf sp2529_svga_settings[] = {
//bininig svga
 {0xfd,0x00},
 {0x19,0x03},
 {0x30,0x0c},//01 0c
 {0x31,0x04},
 {0x33,0x01},
 
 //MIPI     
 {0x95,0x03},
 {0x94,0x20},
 {0x97,0x02},
 {0x96,0x58},
  {0xfd,0x00},
  {0x2f,0x04},//09

//sp2529 48M 1±¶Æµ  50Hz 17.0574-20fps ABinning+DBinning AE_Parameters   yyj0313
//lvcheng 20140728,20fps
#if 0
  {0xfd,0x00},
  {0x03,0x04},
  {0x04,0xf2},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x02},
  {0x0a,0x6c},
  {0xfd,0x01},
  {0xf0,0x00},
  {0xf7,0xd3},
  {0xf8,0xb0},
  {0x02,0x0c},
  {0x03,0x01},
  {0x06,0xd3},
  {0x07,0x00},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x0f},
  {0x3e,0xb0},
  {0x3f,0x00},
  {0x88,0x6d},
  {0x89,0xe8},
  {0x8a,0x22},
  {0xfd,0x02},
  {0xbe,0xe4},
  {0xbf,0x09},
  {0xd0,0xe4},
  {0xd1,0x09},
  {0xc9,0xe4},
  {0xca,0x09},
  #else
//sp2529 48M 1±¶Æµ  50Hz 15.0364-20fps ABinning+DBinning AE_Parameters_20140506100749  //yyj0506
  {0xfd,0x00},
  {0x03,0x06},
  {0x04,0xf6},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x01},
  {0x0a,0x23},
  {0xfd,0x01},
  {0xf0,0x01},
  {0xf7,0x29},
  {0xf8,0xf8},
  {0x02,0x05},
  {0x03,0x01},
  {0x06,0x29},
  {0x07,0x01},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x06},
  {0x3e,0xf8},
  {0x3f,0x00},
  {0x88,0xb9},
  {0x89,0x10},
  {0x8a,0x21},
  {0xfd,0x02},
  {0xbe,0xcd},
  {0xbf,0x05},
  {0xd0,0xcd},
  {0xd1,0x05},
  {0xc9,0xcd},
  {0xca,0x05},

#endif
{0xfd,0x01},
{0x36,0x00},
 };
#ifndef DEBUG_SENSOR_SP
static struct msm_camera_i2c_reg_conf SP2529_reg_saturation[11][9] = {
	{
		{0xfd,0x01},	//sat u
		{0xd3,0x00},
		{0xd4,0x00},
		{0xd5,0x00},
		{0xd6,0x00},
		{0xd7,0x00},	//sat v
		{0xd8,0x00},
		{0xd9,0x00},
		{0xda,0x00},
	}, /* SATURATION LEVEL0*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0x2a},
		{0xd4,0x2a},  
		{0xd5,0x2a},
		{0xd6,0x2a},
		{0xd7,0x2a},	//sat v
		{0xd8,0x2a},
		{0xd9,0x2a},
		{0xda,0x2a},
	}, /* SATURATION LEVEL1*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0x3a},
		{0xd4,0x3a},
		{0xd5,0x3a},
		{0xd6,0x3a},
		{0xd7,0x3a},	//sat v
		{0xd8,0x3a},
		{0xd9,0x3a},
		{0xda,0x3a},
	}, /* SATURATION LEVEL2*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0x5a},
		{0xd4,0x5a},
		{0xd5,0x5a},
		{0xd6,0x5a},
		{0xd7,0x5a},	//sat v
		{0xd8,0x5a},
		{0xd9,0x5a},
		{0xda,0x5a},
	}, /* SATURATION LEVEL3*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0x6a},
		{0xd4,0x6a},
		{0xd5,0x6a},
		{0xd6,0x6a},
		{0xd7,0x6a},	//sat v
		{0xd8,0x6a},
		{0xd9,0x6a},
		{0xda,0x6a},
	}, /* SATURATION LEVEL4*/
	//lvcheng 20140701 ,set default saturation

	{
		{0xfd,0x01},	//sat u
		{0xd3,0xa3},//fuxiuyun 20140708,saturation
		{0xd4,0xa3},
		{0xd5,0xa3},
		{0xd6,0xa3},
		{0xd7,0xa3},
		{0xd8,0xa3},
		{0xd9,0xa3},
		{0xda,0xa3},

	}, /* SATURATION LEVEL5*/

			{
		//sat u                                                                                            
		 {0xfd,0x01},                                                                                     
		 {0xd3,0xb2},                                                                                     
		 {0xd4,0xb2},                                                                                     
		 {0xd5,0xb2},                                                                                     
		 {0xd6,0xb2},                                                                                     
		//sat v                                                                                            
		 {0xd7,0xb2},                                                                                     
		 {0xd8,0xb2},                                                                                     
		 {0xd9,0xb2},                                                                                     
		 {0xda,0xb2},  
	}, /* SATURATION LEVEL6*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0xcc},
		{0xd4,0xcc},
		{0xd5,0xb0},
		{0xd6,0x90},
		{0xd7,0xcc},	//sat v
		{0xd8,0xcc},
		{0xd9,0xb0},
		{0xda,0x90},
	}, /* SATURATION LEVEL7*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0xdd},
		{0xd4,0xdd},
		{0xd5,0xd0},
		{0xd6,0xc0},
		{0xd7,0xdd},	//sat v
		{0xd8,0xdd},
		{0xd9,0xd0},
		{0xda,0xc0},
	}, /* SATURATION LEVEL8*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0xee},
		{0xd4,0xee},
		{0xd5,0xe0},
		{0xd6,0xd0},
		{0xd7,0xee},	//sat v
		{0xd8,0xee},
		{0xd9,0xe0},
		{0xda,0xd0},
	}, /* SATURATION LEVEL9*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0xff},
		{0xd4,0xff},
		{0xd5,0xf0},
		{0xd6,0xe0},
		{0xd7,0xff},	//sat v
		{0xd8,0xff},
		{0xd9,0xf0},
		{0xda,0xe0},
	}, /* SATURATION LEVEL10*/
};
#endif
#ifndef DEBUG_SENSOR_SP
static struct msm_camera_i2c_reg_conf SP2529_reg_contrast[11][9] = {
	{
		//standard curve   3.0
		{0xfd,0x01},
		{0x10,0x2e},
		{0x11,0x2e},
		{0x12,0x2e},
		{0x13,0x2e},
		{0x14,0x30},
		{0x15,0x30},
		{0x16,0x30},
		{0x17,0x30},
	}, /* CONTRAST L0*/
	{
		//standard curve   3.5
		{0xfd,0x01},
		{0x10,0x34},
		{0x11,0x34},
		{0x12,0x34},
		{0x13,0x34},
		{0x14,0x44},
		{0x15,0x44},
		{0x16,0x44},
		{0x17,0x44},



	}, /* CONTRAST L1*/
	{
		//standard curve   4.0
		{0xfd,0x01},
		{0x10,0x4e},
		{0x11,0x4e},
		{0x12,0x4e},
		{0x13,0x4e},
		{0x14,0x58},
		{0x15,0x58},
		{0x16,0x58},
		{0x17,0x58},

	}, /* CONTRAST L2*/
	{
		//standard curve   4.5
		{0xfd,0x01},
		{0x10,0x60},
		{0x11,0x60},
		{0x12,0x60},
		{0x13,0x60},
		{0x14,0x74},
		{0x15,0x74},
		{0x16,0x74},
		{0x17,0x74},

	}, /* CONTRAST L3*/
	{
		//standard curve   5.0
		{0xfd,0x01},
		{0x10,0x78},
		{0x11,0x78},
		{0x12,0x78},
		{0x13,0x78},
		{0x14,0x80},
		{0x15,0x80},
		{0x16,0x80},
		{0x17,0x80},
	}, /* CONTRAST L4*/
	{                  
		//default        
		{0xfd,0x01},     
		{0x10,0x80},     
		{0x11,0x80},     
		{0x12,0x80},     
		{0x13,0x80},     
		{0x14,0x8D},     
		{0x15,0x8D},     
		{0x16,0x8D},     
		{0x17,0x8D},		                                                                                          
       	             
	}, /* CONTRAST L5*/
	{           
		//standard curve   6.0
		{0xfd,0x01},
		{0x10,0x98},
		{0x11,0x98},
		{0x12,0x98},
		{0x13,0x98},
		{0x14,0xa0},
		{0x15,0xa0},
		{0x16,0xa0},
		{0x17,0xa0},
	}, /* CONTRAST L6*/
	{
		//standard curve   6.5
		{0xfd,0x01},
		{0x10,0xa4},
		{0x11,0xa4},
		{0x12,0xa4},
		{0x13,0xa4},
		{0x14,0xb4},
		{0x15,0xb4},
		{0x16,0xb4},
		{0x17,0xb4},
	}, /* CONTRAST L7*/
	{
		//standard curve   7.0
		{0xfd,0x01},
		{0x10,0xb0},
		{0x11,0xb0},
		{0x12,0xb0},
		{0x13,0xb0},
		{0x14,0xc8},
		{0x15,0xc8},
		{0x16,0xc8},
		{0x17,0xc8},
	}, /* CONTRAST L8*/
	{
		//standard curve   7.5
		{0xfd,0x01},
		{0x10,0xc4},
		{0x11,0xc4},
		{0x12,0xc4},
		{0x13,0xc4},
		{0x14,0xe4},
		{0x15,0xe4},
		{0x16,0xe4},
		{0x17,0xe4},
	}, /* CONTRAST L9*/
	{
		//standard curve   8.0
		{0xfd,0x01},
		{0x10,0xdd},
		{0x11,0xdd},
		{0x12,0xdd},
		{0x13,0xdd},
		{0x14,0xff},
		{0x15,0xff},
		{0x16,0xff},
		{0x17,0xff},
	},/* CONTRAST L10*/
};
#endif
#ifndef DEBUG_SENSOR_SP
static struct msm_camera_i2c_reg_conf SP2529_reg_sharpness[7][9] = {
	{
		{0xfd,0x02},
		{0xe8,0x00},		//ÂÖÀªÇ¿¶È
		{0xe9,0x00},
		{0xea,0x00},
		{0xeb,0x00},
		{0xec,0x00},
		{0xed,0x00},
		{0xee,0x00},
		{0xef,0x00},
	}, /* SHARPNESS LEVEL 0*/
	{
		{0xfd,0x02},
		{0xe8,0x10},		//ÂÖÀªÇ¿¶È
		{0xe9,0x10},
		{0xea,0x10},
		{0xeb,0x10},
		{0xec,0x10},
		{0xed,0x10},
		{0xee,0x10},
		{0xef,0x10},
	}, /* SHARPNESS LEVEL 1*/
	//lvcheng 20140701 ,set default sharpness
	{
		{0xfd,0x02},
		{0xe8,0x58},//fuxiuyun 20140708,sharpness
		{0xec,0x58},
		{0xe9,0x58},
		{0xed,0x58},
		{0xea,0x60},
		{0xee,0x68},
		{0xeb,0x50},
		{0xef,0x48},

	}, /* SHARPNESS LEVEL 2*/
	{
		{0xfd,0x02},
		{0xe8,0x58},//fuxiuyun 20140708,sharpness
		{0xec,0x58},
		{0xe9,0x58},
		{0xed,0x58},
		{0xea,0x60},
		{0xee,0x68},
		{0xeb,0x50},
		{0xef,0x48},
			/*
		{0xe8,0x30},   //sharpness gain for increasing pixel¡¯s Y, in outdoor                             
		{0xec,0x40},   //sharpness gain for decreasing pixel¡¯s Y, in outdoor                             
		{0xe9,0x38},//38   //sharpness gain for increasing pixel¡¯s Y, in normal                           
		{0xed,0x40},//40   //sharpness gain for decreasing pixel¡¯s Y, in normal                           
		{0xea,0x28},   //sharpness gain for increasing pixel¡¯s Y,in dummy                                
		{0xee,0x38},   //sharpness gain for decreasing pixel¡¯s Y, in dummy                               
		{0xeb,0x20},   //sharpness gain for increasing pixel¡¯s Y,in lowlight                             
		{0xef,0x30},   //sharpness gain for decreasing pixel¡¯s Y, in low light                           */
	}, /* SHARPNESS LEVEL 3*/    
	{       
		{0xfd,0x02},
		{0xe8,0x78},		//ÂÖÀªÇ¿¶È
		{0xe9,0x78},
		{0xea,0x78},
		{0xeb,0x78},
		{0xec,0xa0},
		{0xed,0xa0},
		{0xee,0xa0},
		{0xef,0xa0},
	}, /* SHARPNESS LEVEL 4*/
	{
		{0xfd,0x02},
		{0xe8,0xa8},		//ÂÖÀªÇ¿¶È
		{0xe9,0xa8},
		{0xea,0xa8},
		{0xeb,0xa8},
		{0xec,0xc0},
		{0xed,0xc0},
		{0xee,0xc0},
		{0xef,0xc0},
	}, /* SHARPNESS LEVEL 5*/
	{
		{0xfd,0x02},
		{0xe8,0xe0},		//ÂÖÀªÇ¿¶È
		{0xe9,0xe0},
		{0xea,0xe0},
		{0xeb,0xe0},
		{0xec,0xff},
		{0xed,0xff},
		{0xee,0xff},
		{0xef,0xff},
	}, /* SHARPNESS LEVEL 6*/
};
#endif
#ifndef DEBUG_SENSOR_SP
//lvcheng 20140526:	fix iso problem from sbk yqb
static struct msm_camera_i2c_reg_conf SP2529_reg_iso[7][3] = {
	/* auto */
	{

		{0xfd,0x01},
		//{0x16,0x75},
		//{0x17,0x75},
		{0xdb,0x00},
		
			
	},
	/* auto hjt */
	{
		{0xfd,0x01},
		//{0x16,0x65},
		//{0x17,0x65},
		{0xdb,0x10},
	},
	/* iso 100 */
	{
		{0xfd,0x01},
		//{0x16,0x55},
		//{0x17,0x55},
		{0xdb,0x10},
	},
	/* iso 200 */
	{
		{0xfd,0x01},
		//{0x16,0x55},
		//{0x17,0x55},
		{0xdb,0x20},
	},
	/* iso 400 */
	{
		{0xfd,0x01},
		//{0x16,0x55},
		//{0x17,0x55},
		{0xdb,0x28},
	},
	/* iso 800 */
	{
		{0xfd,0x01},
		//{0x16,0x55},
		//{0x17,0x55},
		{0xdb,0x30},
	},
	/* iso 1600 */
	{
		{0xfd,0x01},
		//{0x16,0x55},
		//{0x17,0x55},
		{0xdb,0x40}, 
	},
};
#endif
#ifndef DEBUG_SENSOR_SP
static struct msm_camera_i2c_reg_conf SP2529_reg_exposure_compensation[5][2] = {
	{
		{0xfd,0x01},
		{0xeb,0x4d},  //modified by lvcheng for setting exposure_compensation 20140310,yyj310		                                                          
	}, /*EXPOSURECOMPENSATIONN2*/
	{
		{0xfd,0x01},
		{0xeb,0x60},  //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIONN1*/
	{
		{0xfd,0x01},  //ae target                                                                         
		{0xeb,0x78}, //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIOND*/
	{
		{0xfd,0x01},
		{0xeb,0x88}, //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIONP1*/
	{
		{0xfd,0x01},
		{0xeb,0x98},  //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIONP2*/
};
#endif
//modified by lvcheng for setting brigthness 20140307
#ifndef DEBUG_SENSOR_SP //yyj214
static struct msm_camera_i2c_reg_conf SP2529_reg_brightness[5][2] = {
	{
		{0xfd,0x01},
		{0xdb,0xe0},  //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIONN2*/
	{
		{0xfd,0x01},
		{0xdb,0xf0},  //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIONN1*/
	{
		{0xfd,0x01},  //ae target                                                                         
		{0xdb,0x00},  //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIOND*/
	{
		{0xfd,0x01},
		{0xdb,0x10},  //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIONP1*/
	{
		{0xfd,0x01},
		{0xdb,0x20},  //target_indr		                                                          
	}, /*EXPOSURECOMPENSATIONP2*/
};
#endif
//modified by lvcheng for setting brigthness 20140307
#ifndef DEBUG_SENSOR_SP
static struct msm_camera_i2c_reg_conf SP2529_reg_antibanding[][33] = {
	
	/* OFF */
	{ 
	//lvcheng 20140728,20fps
#if 0	
//sp2529 48M 1±¶Æµ  50Hz 17.0574-8fps ABinning+DBinning AE_Parameters   yyj0313
  {0xfd,0x00},
  {0x03,0x04},
  {0x04,0xf2},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x02},
  {0x0a,0x6c},
  {0xfd,0x01},
  {0xf0,0x00},
  {0xf7,0xd3},
  {0xf8,0xb0},
  {0x02,0x0c},
  {0x03,0x01},
  {0x06,0xd3},
  {0x07,0x00},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x0f},
  {0x3e,0xb0},
  {0x3f,0x00},
  {0x88,0x6d},
  {0x89,0xe8},
  {0x8a,0x22},
  {0xfd,0x02},
  {0xbe,0xe4},
  {0xbf,0x09},
  {0xd0,0xe4},
  {0xd1,0x09},
  {0xc9,0xe4},
  {0xca,0x09},   
  #else
//sp2529 48M 1±¶Æµ  50Hz 15.0364-20fps ABinning+DBinning AE_Parameters_20140506100749  //yyj0506
  {0xfd,0x00},
  {0x03,0x06},
  {0x04,0xf6},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x01},
  {0x0a,0x23},
  {0xfd,0x01},
  {0xf0,0x01},
  {0xf7,0x29},
  {0xf8,0xf8},
  {0x02,0x05},
  {0x03,0x01},
  {0x06,0x29},
  {0x07,0x01},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x06},
  {0x3e,0xf8},
  {0x3f,0x00},
  {0x88,0xb9},
  {0x89,0x10},
  {0x8a,0x21},
  {0xfd,0x02},
  {0xbe,0xcd},
  {0xbf,0x05},
  {0xd0,0xcd},
  {0xd1,0x05},
  {0xc9,0xcd},
  {0xca,0x05},

#endif
	},
	/* 60Hz */
	{
	//lvcheng 20140728,20fps
#if 0
//sp2529 48M 1±¶Æµ  60Hz 17.0736-8fps ABinning+DBinning AE_Parameters  yyj0313
  {0xfd,0x00},
  {0x03,0x04},
  {0x04,0x20},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x02},
  {0x0a,0x6b},
  {0xfd,0x01},
  {0xf0,0x00},
  {0xf7,0xb0},
  {0xf8,0xb0},
  {0x02,0x0f},
  {0x03,0x01},
  {0x06,0xb0},
  {0x07,0x00},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x0f},
  {0x3e,0xb0},
  {0x3f,0x00},
  {0x88,0xe8},
  {0x89,0xe8},
  {0x8a,0x22},
  {0xfd,0x02},
  {0xbe,0x50},
  {0xbf,0x0a},
  {0xd0,0x50},
  {0xd1,0x0a},
  {0xc9,0x50},
  {0xca,0x0a},
  #else
////sp2529 48M 1±¶Æµ  60Hz 15.0364-20fps ABinning+DBinning AE_Parameters_20140506100749  //yyj0506
  {0xfd,0x00},
   {0x03,0x05},
   {0x04,0xd6},
   {0x05,0x00},
   {0x06,0x00},
   {0x07,0x00},
   {0x08,0x00},
   {0x09,0x01},
   {0x0a,0x1e},
   {0xfd,0x01},
   {0xf0,0x00},
   {0xf7,0xf9},
   {0xf8,0xf9},
   {0x02,0x06},
   {0x03,0x01},
   {0x06,0xf9},
   {0x07,0x00},
   {0x08,0x01},
   {0x09,0x00},
   {0xfd,0x02},
   {0x3d,0x06},
   {0x3e,0xf9},
   {0x3f,0x00},
   {0x88,0x0e},
   {0x89,0x0e},
   {0x8a,0x22},
   {0xfd,0x02},
   {0xbe,0xd6},
   {0xbf,0x05},
   {0xd0,0xd6},
   {0xd1,0x05},
   {0xc9,0xd6},
   {0xca,0x05},


#endif

	},
	/* 50Hz */
	{
	//lvcheng 20140728,20fps
#if 0	
//sp2529 48M 1±¶Æµ  50Hz 17.0574-8fps ABinning+DBinning AE_Parameters   yyj0313
  {0xfd,0x00},
  {0x03,0x04},
  {0x04,0xf2},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x02},
  {0x0a,0x6c},
  {0xfd,0x01},
  {0xf0,0x00},
  {0xf7,0xd3},
  {0xf8,0xb0},
  {0x02,0x0c},
  {0x03,0x01},
  {0x06,0xd3},
  {0x07,0x00},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x0f},
  {0x3e,0xb0},
  {0x3f,0x00},
  {0x88,0x6d},
  {0x89,0xe8},
  {0x8a,0x22},
  {0xfd,0x02},
  {0xbe,0xe4},
  {0xbf,0x09},
  {0xd0,0xe4},
  {0xd1,0x09},
  {0xc9,0xe4},
  {0xca,0x09},  
  #else
//sp2529 48M 1±¶Æµ  50Hz 15.0364-20fps ABinning+DBinning AE_Parameters_20140506100749  //yyj0506
  {0xfd,0x00},
  {0x03,0x06},
  {0x04,0xf6},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x01},
  {0x0a,0x23},
  {0xfd,0x01},
  {0xf0,0x01},
  {0xf7,0x29},
  {0xf8,0xf8},
  {0x02,0x05},
  {0x03,0x01},
  {0x06,0x29},
  {0x07,0x01},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x06},
  {0x3e,0xf8},
  {0x3f,0x00},
  {0x88,0xb9},
  {0x89,0x10},
  {0x8a,0x21},
  {0xfd,0x02},
  {0xbe,0xcd},
  {0xbf,0x05},
  {0xd0,0xcd},
  {0xd1,0x05},
  {0xc9,0xcd},
  {0xca,0x05},


#endif
	},
	/* AUTO */
	{
	//lvcheng 20140728,20fps
#if 0	
//sp2529 48M 1±¶Æµ  50Hz 17.0574-8fps ABinning+DBinning AE_Parameters   yyj0313
  {0xfd,0x00},
  {0x03,0x04},
  {0x04,0xf2},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x02},
  {0x0a,0x6c},
  {0xfd,0x01},
  {0xf0,0x00},
  {0xf7,0xd3},
  {0xf8,0xb0},
  {0x02,0x0c},
  {0x03,0x01},
  {0x06,0xd3},
  {0x07,0x00},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x0f},
  {0x3e,0xb0},
  {0x3f,0x00},
  {0x88,0x6d},
  {0x89,0xe8},
  {0x8a,0x22},
  {0xfd,0x02},
  {0xbe,0xe4},
  {0xbf,0x09},
  {0xd0,0xe4},
  {0xd1,0x09},
  {0xc9,0xe4},
  {0xca,0x09},
  #else
//sp2529 48M 1±¶Æµ  50Hz 15.0364-20fps ABinning+DBinning AE_Parameters_20140506100749  //yyj0506
  {0xfd,0x00},
  {0x03,0x06},
  {0x04,0xf6},
  {0x05,0x00},
  {0x06,0x00},
  {0x07,0x00},
  {0x08,0x00},
  {0x09,0x01},
  {0x0a,0x23},
  {0xfd,0x01},
  {0xf0,0x01},
  {0xf7,0x29},
  {0xf8,0xf8},
  {0x02,0x05},
  {0x03,0x01},
  {0x06,0x29},
  {0x07,0x01},
  {0x08,0x01},
  {0x09,0x00},
  {0xfd,0x02},
  {0x3d,0x06},
  {0x3e,0xf8},
  {0x3f,0x00},
  {0x88,0xb9},
  {0x89,0x10},
  {0x8a,0x21},
  {0xfd,0x02},
  {0xbe,0xcd},
  {0xbf,0x05},
  {0xd0,0xcd},
  {0xd1,0x05},
  {0xc9,0xcd},
  {0xca,0x05},

#endif
 },
};
#endif
#ifndef DEBUG_SENSOR_SP
static struct msm_camera_i2c_reg_conf SP2529_reg_effect_normal[] = {
	{0xfd,0x01},
	{0x66,0x00},
	{0x67,0x80},
	{0x68,0x80},
	//{0xdb,0x00},
	//{0x34,0xc7},

};

static struct msm_camera_i2c_reg_conf SP2529_reg_effect_black_white[] = {
	/* B&W: */
	{0xfd,0x01},
	{0x66,0x20},
	{0x67,0x80},
	{0x68,0x80},
	//{0xdb,0x00},
	//{0x34,0xc7},

};

static struct msm_camera_i2c_reg_conf SP2529_reg_effect_negative[] = {
	/* Negative: */
	{0xfd,0x01},
	{0x66,0x08},
	{0x67,0x80},
	{0x68,0x80},
	//{0xdb,0x00},
	//{0x34,0xc7},

};

static struct msm_camera_i2c_reg_conf SP2529_reg_effect_old_movie[] = {
	/* Sepia(antique): */
	{0xfd,0x01},
	{0x66,0x10},
	{0x67,0x98},
	{0x68,0x58},
	//{0xdb,0x00},
	//{0x34,0xc7},

};

static struct msm_camera_i2c_reg_conf SP2529_reg_effect_sepiablue[] = {
	/* Sepiabule: */
	{0xfd,0x01},
	{0x66,0x10},
	{0x67,0x80},
	{0x68,0xb0},
	//{0xdb,0x00},
	//{0x34,0xc7},

};
static struct msm_camera_i2c_reg_conf SP2529_reg_effect_solarize[] = {
	{0xfd,0x01},
	{0x66,0x80},
	{0x67,0x80},
	{0x68,0x80}, 
	//{0xdb,0x00},
	{0xdf,0x80},
	//{0x34,0xc7},

};
static struct msm_camera_i2c_reg_conf SP2529_reg_effect_emboss[] = {
	{0xfd,0x01},
	{0x66,0x01},
	{0x67,0x80},
	{0x68,0x80},
	//{0xdb,0x00},
	//{0x34,0xc7},



};
#endif
#ifndef DEBUG_SENSOR_SP
static struct msm_camera_i2c_reg_conf SP2529_reg_scene_auto[] = {
	/* <SCENE_auto> */
	#if 1
	//Band 50Hz 24M fix15fps
 {0xfd,0x00},
 {0x03,0x04},
 {0x04,0x5c},
 {0x05,0x00},
 {0x06,0x00},
 {0x07,0x00},
 {0x08,0x00},
 {0x09,0x00},
 {0x0a,0x80},
 {0xfd,0x01},
 {0xf0,0x00},
 {0xf7,0xba},
 {0xf8,0x9b},
 {0x02,0x06},
 {0x03,0x01},
 {0x06,0xba},
 {0x07,0x00},
 {0x08,0x01},
 {0x09,0x00},
 {0xfd,0x02},
 {0x3d,0x08},
 {0x3e,0x9b},
 {0x3f,0x00},
 {0x88,0xc0},
 {0x89,0x4d},
 {0x8a,0x32},
 {0xfd,0x02},
 {0xbe,0x5c},
 {0xbf,0x04},
 {0xd0,0x5c},
 {0xd1,0x04},
 {0xc9,0x5c},
 {0xca,0x04},
#endif
};

static struct msm_camera_i2c_reg_conf SP2529_reg_scene_portrait[] = {
	/* <CAMTUNING_SCENE_PORTRAIT> */

};

static struct msm_camera_i2c_reg_conf SP2529_reg_scene_landscape[] = {
	/* <CAMTUNING_SCENE_LANDSCAPE> */

};

static struct msm_camera_i2c_reg_conf SP2529_reg_scene_night[] = {
	/* <SCENE_NIGHT> */
	#if 1
	//Band 50Hz 24M fix10fps
{0xfd,0x00}, 
{0x03,0x02}, 
{0x04,0xee}, 
{0x05,0x00}, 
{0x06,0x00}, 
{0x07,0x00}, 
{0x08,0x00}, 
{0x09,0x01}, 
{0x0a,0xbb}, 
{0xfd,0x01}, 
{0xf0,0x00}, 
{0xf7,0x7d}, 
{0xf8,0x68}, 
{0x02,0x0a}, 
{0x03,0x01}, 
{0x06,0x7d}, 
{0x07,0x00}, 
{0x08,0x01}, 
{0x09,0x00}, 
{0xfd,0x02}, 
{0x3d,0x0c}, 
{0x3e,0x68}, 
{0x3f,0x00}, 
{0x88,0x18}, 
{0x89,0xec}, 
{0x8a,0x44}, 
{0xfd,0x02}, 
{0xbe,0xe2}, 
{0xbf,0x04}, 
{0xd0,0xe2}, 
{0xd1,0x04}, 
{0xc9,0xe2}, 
{0xca,0x04}, 
#endif
};
#endif
#ifndef DEBUG_SENSOR_SP
static struct msm_camera_i2c_reg_conf SP2529_reg_wb_auto[] = {
	/* Auto: */
	{0xfd,0x00},
	{0xe7,0x03},
 
	{0xfd,0x02},
	{0x26,0xa0},//0xac
	{0x27,0x96},//0x91     

	{0xfd,0x01},
	{0x32,0x15},

	{0xfd,0x00},
	//{0xfd,0x00},
    //{0x3f,0x03},
	{0xe7,0x00},
};

static struct msm_camera_i2c_reg_conf SP2529_reg_wb_sunny[] = {
	/*DAYLIGHT*/
		{0xfd,0x01},
		{0x32,0x05},

		{0xfd,0x00},
		{0xe7,0x03},

		{0xfd,0x02},
		{0x26,0xca},
		{0x27,0x73},

		{0xfd,0x00},
		//{0xfd,0x00},
        // {0x3f,0x01},
		{0xe7,0x00},
	};

static struct msm_camera_i2c_reg_conf SP2529_reg_wb_cloudy[] = {
	/*CLOUDY*/
		{0xfd,0x01},
		{0x32,0x05},

		{0xfd,0x00},
		{0xe7,0x03},

		{0xfd,0x02},
		{0x26,0xdb},
		{0x27,0x63},

		{0xfd,0x00},
		//{0xfd,0x00},
        //{0x3f,0x02},
		{0xe7,0x00},
	};

static struct msm_camera_i2c_reg_conf SP2529_reg_wb_office[] = {
/* Office: *//*INCANDISCENT*/
		{0xfd,0x01},
		{0x32,0x05},

		{0xfd,0x00},
		{0xe7,0x03},

		{0xfd,0x02},
		{0x26,0x8c},
		{0x27,0xb3},
        //{0xfd,0x01},
        //{0x35,0x00},
		{0xfd,0x00},
		{0xe7,0x00},
	};

static struct msm_camera_i2c_reg_conf SP2529_reg_wb_home[] = {
	/* Home: */
		{0xfd,0x01},
		{0x32,0x05},

		{0xfd,0x00},
		{0xe7,0x03},

		{0xfd,0x02},
		{0x26,0x90},
		{0x27,0xa5},
        //{0xfd,0x01},
        //{0x35,0x00},
		{0xfd,0x00},
		//{0xfd,0x00},
        //{0x3f,0x00},
		{0xe7,0x00},
};
#endif
static const struct i2c_device_id sp2529_i2c_id[] = {
	{SP2529_SENSOR_NAME, (kernel_ulong_t)&sp2529_s_ctrl},
	{ }
};

static int32_t msm_sp2529_i2c_probe(struct i2c_client *client,
	   const struct i2c_device_id *id)
{
	   return msm_sensor_i2c_probe(client, id, &sp2529_s_ctrl);
}

static struct i2c_driver sp2529_i2c_driver = {
	.id_table = sp2529_i2c_id,
	.probe  = msm_sp2529_i2c_probe,
	.driver = {
		.name = SP2529_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client sp2529_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static const struct of_device_id sp2529_dt_match[] = {
	{.compatible = "qcom,sp2529", .data = &sp2529_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, sp2529_dt_match);

static struct platform_driver sp2529_platform_driver = {
	.driver = {
		.name = "qcom,sp2529",
		.owner = THIS_MODULE,
		.of_match_table = sp2529_dt_match,
	},
};

static int32_t sp2529_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_camera_i2c_reg_conf *table,
		int num)
{
	int i = 0;
	int rc = 0;
	for (i = 0; i < num; ++i) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
			s_ctrl->sensor_i2c_client, table->reg_addr,
			table->reg_data,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			msleep(100);
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
				s_ctrl->sensor_i2c_client, table->reg_addr,
				table->reg_data,
				MSM_CAMERA_I2C_BYTE_DATA);
		}
		table++;
	}
	return rc;

}


 static int SP2529_read_cmos_sensor(int reg_addr)
{
	int rc = 0;
	int result = 0;
	rc = (&sp2529_s_ctrl)->sensor_i2c_client->i2c_func_tbl->
		i2c_read(
		(&sp2529_s_ctrl)->sensor_i2c_client, reg_addr,
		(uint16_t*)&result,MSM_CAMERA_I2C_BYTE_DATA);
	return result;
}

  static void SP2529_write_cmos_sensor(int reg_addr , int reg_data)
{
	(&sp2529_s_ctrl)->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
			(&sp2529_s_ctrl)->sensor_i2c_client, reg_addr,
			reg_data,
			MSM_CAMERA_I2C_BYTE_DATA);
}


static int32_t sp2529_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(sp2529_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init sp2529_init_module(void)
{
	int32_t rc;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&sp2529_platform_driver,
		sp2529_platform_probe);
	if (!rc)
		return rc;
	return i2c_add_driver(&sp2529_i2c_driver);
}

static void __exit sp2529_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (sp2529_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&sp2529_s_ctrl);
		platform_driver_unregister(&sp2529_platform_driver);
	} else
		i2c_del_driver(&sp2529_i2c_driver);
	return;
}

static int32_t sp2529_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
		s_ctrl->sensor_i2c_client,
		0x02,//s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
		&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: %s: sp2529 read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	pr_err("%s: read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);
	if (chipid != 0x25 /*s_ctrl->sensordata->slave_info->sensor_id*/) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}


#ifndef DEBUG_SENSOR_SP
static void sp2529_set_stauration(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	CDBG("%s %d", __func__, value);
	sp2529_i2c_write_table(s_ctrl, &SP2529_reg_saturation[value][0],
		ARRAY_SIZE(SP2529_reg_saturation[value]));
}
#endif
#ifndef DEBUG_SENSOR_SP
static void sp2529_set_contrast(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	sp2529_i2c_write_table(s_ctrl, &SP2529_reg_contrast[value][0],
		ARRAY_SIZE(SP2529_reg_contrast[value]));
}
static void sp2529_set_sharpness(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	int val = value / 6;
	CDBG("%s %d", __func__, value);
	sp2529_i2c_write_table(s_ctrl, &SP2529_reg_sharpness[val][0],
		ARRAY_SIZE(SP2529_reg_sharpness[val]));
}
#endif
#ifndef DEBUG_SENSOR_SP
static void sp2529_set_iso(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	CDBG("%s %d", __func__, value);
	sp2529_i2c_write_table(s_ctrl, &SP2529_reg_iso[value][0],
		ARRAY_SIZE(SP2529_reg_iso[value]));
}

static void sp2529_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	int val = (value + 12) / 6;
	pr_debug("%s %d", __func__, val);
	sp2529_i2c_write_table(s_ctrl, &SP2529_reg_exposure_compensation[val][0],
		ARRAY_SIZE(SP2529_reg_exposure_compensation[val]));
}
#endif
//modified by lvcheng for setting brigthness 20140307
#ifndef DEBUG_SENSOR_SP  //yyj214
static void sp2529_set_brightness(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	int val = (value *5) / 7;   //modified by lvcheng for setting brightness 20140310,yyj310
	CDBG("%s %d", __func__, val);
	sp2529_i2c_write_table(s_ctrl, &SP2529_reg_brightness[val][0],
		ARRAY_SIZE(SP2529_reg_brightness[val]));
}
#endif
//modified by lvcheng for setting brigthness 20140307
#ifndef DEBUG_SENSOR_SP
static void sp2529_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_EFFECT_MODE_OFF: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_effect_normal[0],
			ARRAY_SIZE(SP2529_reg_effect_normal));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_MONO: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_effect_black_white[0],
			ARRAY_SIZE(SP2529_reg_effect_black_white));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_NEGATIVE: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_effect_negative[0],
			ARRAY_SIZE(SP2529_reg_effect_negative));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_SEPIA: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_effect_old_movie[0],
			ARRAY_SIZE(SP2529_reg_effect_old_movie));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_POSTERIZE:
	{
		
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_effect_emboss[0],
			ARRAY_SIZE(SP2529_reg_effect_emboss));
		break;
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_AQUA: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_effect_sepiablue[0],
			ARRAY_SIZE(SP2529_reg_effect_sepiablue));
		break;
	}  
	case MSM_CAMERA_EFFECT_MODE_SOLARIZE: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_effect_solarize[0],
			ARRAY_SIZE(SP2529_reg_effect_solarize));
		break;
	}
	default:
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_effect_normal[0],
			ARRAY_SIZE(SP2529_reg_effect_normal));
	}
}
#endif
#ifndef DEBUG_SENSOR_SP
static void sp2529_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	sp2529_i2c_write_table(s_ctrl, &SP2529_reg_antibanding[value][0],
		ARRAY_SIZE(SP2529_reg_antibanding[value]));
}
#endif
#ifndef DEBUG_SENSOR_SP
static void sp2529_set_scene_mode(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_SCENE_MODE_OFF: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_scene_auto[0],
			ARRAY_SIZE(SP2529_reg_scene_auto));
					break;
	}
	case MSM_CAMERA_SCENE_MODE_NIGHT: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_scene_night[0],
			ARRAY_SIZE(SP2529_reg_scene_night));
					break;
	}
	case MSM_CAMERA_SCENE_MODE_LANDSCAPE: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_scene_landscape[0],
			ARRAY_SIZE(SP2529_reg_scene_landscape));
					break;
	}
	case MSM_CAMERA_SCENE_MODE_PORTRAIT: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_scene_portrait[0],
			ARRAY_SIZE(SP2529_reg_scene_portrait));
					break;
	}
	default:
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_scene_auto[0],
			ARRAY_SIZE(SP2529_reg_scene_auto));
	}
}
#endif
#ifndef DEBUG_SENSOR_SP
static void sp2529_set_white_balance_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_WB_MODE_AUTO: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_wb_auto[0],
			ARRAY_SIZE(SP2529_reg_wb_auto));
		break;
	}
	case MSM_CAMERA_WB_MODE_INCANDESCENT: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_wb_home[0],
			ARRAY_SIZE(SP2529_reg_wb_home));
		break;
	}
	case MSM_CAMERA_WB_MODE_DAYLIGHT: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_wb_sunny[0],
			ARRAY_SIZE(SP2529_reg_wb_sunny));
					break;
	}
	case MSM_CAMERA_WB_MODE_FLUORESCENT: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_wb_office[0],
			ARRAY_SIZE(SP2529_reg_wb_office));
					break;
	}
	case MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT: {
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_wb_cloudy[0],
			ARRAY_SIZE(SP2529_reg_wb_cloudy));
					break;
	}
	default:
		sp2529_i2c_write_table(s_ctrl, &SP2529_reg_wb_auto[0],
		ARRAY_SIZE(SP2529_reg_wb_auto));
	}
}
#endif
static int G_shutter = 0;
static int G_Gain = 0;
static int	setshutter = 0;
void SP2529_Set_Shutter(uint16_t iShutter)
{
	int temp_reg_L, temp_reg_H;
	temp_reg_L = iShutter & 0xff;
	temp_reg_H = (iShutter >>8) & 0xff;
	SP2529_write_cmos_sensor(0xfd,0x00); 
	SP2529_write_cmos_sensor(0x03,temp_reg_H);
	SP2529_write_cmos_sensor(0x04,temp_reg_L);
	CDBG(" SP2529_Set_Shutter\r\n");
} /* Set_SP2529_Shutter */
int  SP2529_Read_Shutter(void)
{
	int  temp_reg_L, temp_reg_H;
	int  shutter;
	SP2529_write_cmos_sensor(0xfd,0x00); 
	temp_reg_L = SP2529_read_cmos_sensor(0x04);
	temp_reg_H = SP2529_read_cmos_sensor(0x03);
	//add yyj
    SP2529_write_cmos_sensor(0xe7,0x03);
	SP2529_write_cmos_sensor(0xe7,0x00);
	shutter = (temp_reg_L & 0xFF) | (temp_reg_H << 8);

	CDBG(" SP2529_Read_Shutter %x \r\n",shutter);
	return shutter;
} /* SP2529_read_shutter */

void SP2529_AfterSnapshot(void)
{
    int  	AE_tmp;
	
    CDBG("SP2529_AfterSnapshot--capture--after--preview--yyj ");
	SP2529_write_cmos_sensor(0xfd,0x01);
	AE_tmp=SP2529_read_cmos_sensor(0x32);
	//AE_tmp=AE_tmp&0xfa;
	AE_tmp=AE_tmp&0xda;
	
	SP2529_write_cmos_sensor(0x32,AE_tmp);
	SP2529_write_cmos_sensor(0xfd,0x00);
	SP2529_write_cmos_sensor(0xe7,0x03);
	SP2529_write_cmos_sensor(0xe7,0x00);
	
	SP2529_Set_Shutter(G_shutter);
	
	SP2529_write_cmos_sensor(0xfd,0x00); 
	SP2529_write_cmos_sensor(0x24,G_Gain);
	
	SP2529_write_cmos_sensor(0xfd,0x00);
	SP2529_write_cmos_sensor(0xe7,0x03);
	SP2529_write_cmos_sensor(0xe7,0x00);

	SP2529_write_cmos_sensor(0xfd,0x01);
	AE_tmp=SP2529_read_cmos_sensor(0x32);
	AE_tmp=AE_tmp|0x05;
	SP2529_write_cmos_sensor(0x32,AE_tmp);
	SP2529_write_cmos_sensor(0xfd,0x00);
	SP2529_write_cmos_sensor(0xe7,0x03);
	SP2529_write_cmos_sensor(0xe7,0x00);

}  

void SP2529_BeforeSnapshot(void)
{	
	int Shutter_temp;
    int Shutter, HB;
    int HB_L,HB_H,Gain,AE_tmp;
	CDBG("SP2529_BeforeSnapshot--capture--yyj ");
	SP2529_write_cmos_sensor(0xfd,0x01);
	AE_tmp=SP2529_read_cmos_sensor(0x32);
	SP2529_write_cmos_sensor(0xfd,0x00); 
	Gain=SP2529_read_cmos_sensor(0x23);

	HB_H=SP2529_read_cmos_sensor(0x09);
	HB_L=SP2529_read_cmos_sensor(0x0a);

	G_Gain=Gain;
	HB = (HB_L & 0xFF) | (HB_H << 8);
	AE_tmp=(AE_tmp|0x20)&0xfa;
	Shutter = SP2529_Read_Shutter();
	G_shutter = Shutter;
	
	Shutter_temp = Shutter;
	//Shutter_temp = Shutter_temp*(517+HB)/2/(922+HB);          //// 1PLL preview and captrure
	//Shutter_temp = Shutter_temp*(517+HB)/2/(922+HB);	     ////1.5PLL preview and captrure
	Shutter_temp = Shutter_temp*3*(517+HB)/4/(922+HB);    ///2pll preview and 3pll captrure//  yyj 0218
	//Shutter_temp = Shutter_temp*3*(517+HB)/8/(922+HB);  ///2pll preview and 1.5pll captrure
	Shutter = Shutter_temp&0xffff;

	if(Shutter<1)
	{
		Shutter=1;
		usleep(20); //add yyj//lvcheng 0506 reduce delay
		
	}

	
	SP2529_write_cmos_sensor(0xfd,0x01);
	SP2529_write_cmos_sensor(0x32,AE_tmp);  
	SP2529_write_cmos_sensor(0xfd,0x00);
	SP2529_write_cmos_sensor(0xe7,0x03);
	SP2529_write_cmos_sensor(0xe7,0x00);
	
	SP2529_Set_Shutter(Shutter);	
	SP2529_write_cmos_sensor(0xfd,0x00); 
	SP2529_write_cmos_sensor(0x24,Gain);
	
	SP2529_write_cmos_sensor(0xfd,0x00);
	SP2529_write_cmos_sensor(0xe7,0x03);
	SP2529_write_cmos_sensor(0xe7,0x00);

} 

#if 0
static int SP2529_read_capture_flag(void)
{
	int flag = 0;
	//int value = 0;
	SP2529_write_cmos_sensor(0xfd,0x00);
	flag = SP2529_read_cmos_sensor(0x2f);
	return flag;
}
#endif
/*
  * Added for accurate  standby
  *
  * by ZTE_YCM_20140324 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
int32_t sp2529_sensor_init_reg(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	CDBG("%s:%s E\n",TAG,__func__);
#ifdef DEBUG_SENSOR_SP
	if(1 == SP_Initialize_from_T_Flash())
		rc = sp2529_i2c_write_table(s_ctrl,&SP_Init_Reg[0],reg_num);
	else{
		rc = sp2529_i2c_write_table(s_ctrl,&sp2529_recommend_settings[0],
				ARRAY_SIZE(sp2529_recommend_settings));
	}
#else
	rc = sp2529_i2c_write_table(s_ctrl,&sp2529_recommend_settings[0],
				ARRAY_SIZE(sp2529_recommend_settings));

#endif
	CDBG("%s:%s End\n",TAG,__func__);
	return rc;
}
#endif
// <---
int32_t sp2529_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		break;
	case CFG_SET_INIT_SETTING:
		CDBG("init setting start");
#ifndef SENSOR_USE_STANDBY
		#ifdef DEBUG_SENSOR_SP
		if(1 == SP_Initialize_from_T_Flash())
		{
			sp2529_i2c_write_table(s_ctrl,
				&SP_Init_Reg[0],
				reg_num);


		} 
		else
			{
				sp2529_i2c_write_table(s_ctrl,
				&sp2529_recommend_settings[0],
				ARRAY_SIZE(sp2529_recommend_settings));
			}
		#else
				sp2529_i2c_write_table(s_ctrl,
				&sp2529_recommend_settings[0],
				ARRAY_SIZE(sp2529_recommend_settings));

		#endif
#endif
		CDBG("init setting end");
		break;
	case CFG_SET_RESOLUTION: {
		int val = 0;
		if (copy_from_user(&val,
			(void *)cdata->cfg.setting, sizeof(int))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		printk("val=%d\n" , val);
		if (val == 0)
		{
			//if (0&&(SP2529_read_capture_flag() == 0x04))
			{
				SP2529_BeforeSnapshot();
				sp2529_i2c_write_table(s_ctrl, &sp2529_capture_uxga_settings[0],
				ARRAY_SIZE(sp2529_capture_uxga_settings));
            	CDBG("CFG_SET_RESOLUTION--capture ");
				setshutter=1;

			}
			/*
			else
			{
			sp2529_i2c_write_table(s_ctrl, &sp2529_uxga_settings[0],
				ARRAY_SIZE(sp2529_uxga_settings));
			}*/

			
		}
		else if (val == 1)
		{
			if(setshutter)
			{
				SP2529_AfterSnapshot();
				//CDBG(" SP2529_Read_setshutter %x \r\n",setshutter);
				CDBG("CFG_SET_RESOLUTION--capture--after--preview ");

			}
			setshutter=0;
			#ifdef DEBUG_SENSOR_SP
			if(1 == SP_Svga_Initialize_from_T_Flash())
			{
				sp2529_i2c_write_table(s_ctrl, &SP_SVGA_Reg[0],
					reg_sgva_num);

			}
			else 
			{
				sp2529_i2c_write_table(s_ctrl, &sp2529_svga_settings[0],
					ARRAY_SIZE(sp2529_svga_settings));
			}
			#else
			{
				sp2529_i2c_write_table(s_ctrl, &sp2529_svga_settings[0],
					ARRAY_SIZE(sp2529_svga_settings));
				CDBG("CFG_SET_RESOLUTION--normal--preview ");
			}
			#endif
		}
		break;
	}
	#if 1
	case CFG_SET_STOP_STREAM:
		sp2529_i2c_write_table(s_ctrl,
			&sp2529_stop_settings[0],
			ARRAY_SIZE(sp2529_stop_settings));
		break;
	case CFG_SET_START_STREAM:
		sp2529_i2c_write_table(s_ctrl,
			&sp2529_start_settings[0],
			ARRAY_SIZE(sp2529_start_settings));
		break;
	#endif 
	
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params =
			*s_ctrl->sensordata->sensor_init_params;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_sensor_power_setting_array *power_setting_array;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;
		}

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		s_ctrl->power_setting_array =
			sensor_slave_info.power_setting_array;
		power_setting_array = &s_ctrl->power_setting_array;
		power_setting_array->power_setting = kzalloc(
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting), GFP_KERNEL);
		if (!power_setting_array->power_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(power_setting_array->power_setting,
			(void *)
			sensor_slave_info.power_setting_array.power_setting,
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting))) {
			kfree(power_setting_array->power_setting);
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->free_power_setting = true;
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.slave_addr);
		CDBG("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		CDBG("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			power_setting_array->size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
			slave_index,
			power_setting_array->power_setting[slave_index].
			seq_type,
			power_setting_array->power_setting[slave_index].
			seq_val,
			power_setting_array->power_setting[slave_index].
			config_val,
			power_setting_array->power_setting[slave_index].
			delay);
		}
		kfree(power_setting_array->power_setting);
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
			(void *)reg_setting, stop_setting->size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}
	#if 1   //yyj214
	case CFG_SET_SATURATION: {
		int32_t sat_lev;
		if (copy_from_user(&sat_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: Saturation Value is %d", __func__, sat_lev);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_stauration(s_ctrl, sat_lev);
		#endif
		break;
	}
	case CFG_SET_CONTRAST: {
		int32_t con_lev;
		if (copy_from_user(&con_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Contrast Value is %d", __func__, con_lev);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_contrast(s_ctrl, con_lev);
		#endif
		break;
	}
	case CFG_SET_SHARPNESS: {
		int32_t shp_lev;
		if (copy_from_user(&shp_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: Sharpness Value is %d", __func__, shp_lev);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_sharpness(s_ctrl, shp_lev);
		#endif
		break;
	}
	#endif
	case CFG_SET_ISO: {
		int32_t iso_lev;
		if (copy_from_user(&iso_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: ISO Value is %d", __func__, iso_lev);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_iso(s_ctrl, iso_lev);
		#endif
		break;
	}
	case CFG_SET_EXPOSURE_COMPENSATION: {
		int32_t ec_lev;
		if (copy_from_user(&ec_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: Exposure compensation Value is %d",
			__func__, ec_lev);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_exposure_compensation(s_ctrl, ec_lev);
		#endif
		break;
	}
	
//modified by lvcheng for setting brigthness 20140307
	case CFG_SET_BRIGHTNESS: {
		int32_t bri_lev;
		if (copy_from_user(&bri_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: Brightness Value is %d",
			__func__, bri_lev);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_brightness(s_ctrl, bri_lev);
		#endif
		break;
	}
//modified by lvcheng for setting brigthness 20140307

	case CFG_SET_EFFECT: {
		int32_t effect_mode;
		if (copy_from_user(&effect_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Effect mode is %d", __func__, effect_mode);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_effect(s_ctrl, effect_mode);
		#endif
		break;
	}
	case CFG_SET_ANTIBANDING: {
		int32_t antibanding_mode;
		if (copy_from_user(&antibanding_mode,
			(void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: anti-banding mode is %d", __func__,
			antibanding_mode);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_antibanding(s_ctrl, antibanding_mode);
		#endif
		break;
	}
	case CFG_SET_BESTSHOT_MODE: {
		int32_t bs_mode;
		if (copy_from_user(&bs_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: best shot mode is %d", __func__, bs_mode);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_scene_mode(s_ctrl, bs_mode);
		#endif
		break;
	}
	case CFG_SET_WHITE_BALANCE: {
		int32_t wb_mode;
		if (copy_from_user(&wb_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: white balance is %d", __func__, wb_mode);
		#ifndef DEBUG_SENSOR_SP
		sp2529_set_white_balance_mode(s_ctrl, wb_mode);
		#endif
		break;
	}        
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	pr_err("%s:%d rc = %ld\n", __func__, __LINE__ , rc);

	rc =0;
  
	return rc;
}
  
static struct msm_sensor_fn_t sp2529_sensor_func_tbl = {
	.sensor_config = sp2529_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = sp2529_sensor_match_id,
};

static struct msm_sensor_ctrl_t sp2529_s_ctrl = {
	.sensor_i2c_client = &sp2529_sensor_i2c_client,
	.power_setting_array.power_setting = sp2529_power_setting,
	.power_setting_array.size = ARRAY_SIZE(sp2529_power_setting),
	.msm_sensor_mutex = &sp2529_mut,
	.sensor_v4l2_subdev_info = sp2529_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sp2529_subdev_info),
/*
  * Added for accurate  standby
  *
  * by ZTE_YCM_20140324 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
	.sensor_power_down = sp2529_sensor_power_down,
	.sensor_power_on = sp2529_sensor_power_on,
	.sensor_init_reg = sp2529_sensor_init_reg,
#endif
// <---
	.func_tbl = &sp2529_sensor_func_tbl,
};

module_init(sp2529_init_module);
module_exit(sp2529_exit_module);
MODULE_DESCRIPTION("Sp2529 2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
