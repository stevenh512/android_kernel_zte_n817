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
#define SP1628_SENSOR_NAME "sp1628"
#define PLATFORM_DRIVER_NAME "msm_camera_sp1628"
//#define   DEBUG_SENSOR_SP   //yyj224

/*
  * Added for accurate  standby sp1628
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#define SENSOR_USE_STANDBY
#define TAG "ZTE_YCM"
// <---

#ifdef DEBUG_SENSOR_SP
#define SP_OP_CODE_INI		0x00		/* Initial value. */
#define SP_OP_CODE_REG		0x01		/* Register */
#define SP_OP_CODE_DLY		0x02		/* Delay */
#define SP_OP_CODE_END		0x03		/* End of initial setting. */
uint16_t fromsd;

struct msm_camera_i2c_reg_conf SP_Init_Reg[1500];


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
	static u8 data_buff[20*1024] ;

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


#endif

#define CONFIG_MSMB_CAMERA_DEBUG

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


DEFINE_MSM_MUTEX(sp1628_mut);
static struct msm_sensor_ctrl_t sp1628_s_ctrl;

static struct msm_sensor_power_setting sp1628_power_setting[] = {
#if 1
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
	.seq_val = SENSOR_GPIO_RESET,
	.config_val = GPIO_OUT_LOW,
	.delay = 5,
	},

	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_STANDBY,
	.config_val = GPIO_OUT_LOW,
	.delay = 5,
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
	.delay = 10,
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
#else
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_STANDBY,
	.config_val = GPIO_OUT_LOW,
	.delay = 20,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_STANDBY,
	.config_val = GPIO_OUT_HIGH,
	.delay = 0,
	},
	{
	.seq_type = SENSOR_VREG,
	.seq_val = CAM_VIO,
	.config_val = 0,
	.delay = 0,
	},
	{
	.seq_type = SENSOR_VREG,
	.seq_val = CAM_VANA,
	.config_val = 0,
	.delay = 0,
	},
	{
	.seq_type = SENSOR_VREG,
	.seq_val = CAM_VDIG,
	.config_val = 0,
	.delay = 0,
	},
	{
	.seq_type = SENSOR_CLK,
	.seq_val = SENSOR_CAM_MCLK,
	.config_val = 24000000,
	.delay = 10,
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
	.config_val = GPIO_OUT_LOW,
	.delay = 10,
	},
	{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_RESET,
	.config_val = GPIO_OUT_HIGH,
	.delay = 30,
	},
	{
	.seq_type = SENSOR_I2C_MUX,
	.seq_val = 0,
	.config_val = 0,
	.delay = 0,
	},
#endif
};
#if 0
static struct msm_camera_i2c_reg_conf sp1628_capture_uxga_settings[] = {
	//uxga
 {0xfd,0x00},
 {0x19,0x00},
 {0x30,0x00},//00  
 {0x31,0x00},
 {0x33,0x00},
//MIPI      
 {0x95,0x06},
 {0x94,0x40},
 {0x97,0x04},
 {0x96,0xb0},
 
{0xfd,0x00},
{0x2f,0x09},///1.5 倍频  	 
};
#endif
#if 0
#define UXGA 

static struct msm_camera_i2c_reg_conf sp1628_uxga_settings[] = {
{0xfd,0x00},
{0x2f,0x04},
{0x30,0x00},
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
static struct msm_camera_i2c_reg_conf sp1628_start_settings[] = {
	{0xfd,0x00},	
	{0x92,0x81}, 
};

static struct msm_camera_i2c_reg_conf sp1628_stop_settings[] = {
	{0xfd,0x00},	
	{0x92,0x00},
};
#endif
static struct msm_camera_i2c_reg_conf sp1628_recommend_settings[] = {
	{0xfd,0x00},
	{0x91,0x00},
	//{0x92,0x81}, //29 20140121wxc
	{0x98,0x3a},//29 20140121wxc
	{0x96,0xd0},
	{0x97,0x02},

	{0x2f,0x4d},	//[6:4] [1:0] 24M*2.5=60M  20140121wxc
	{0x0b,0x46},//analog 20140121wxc
	{0x30,0x80}, //00
	{0x0c,0x33}, //analog 66 20140213wxc
	{0x0d,0x12},
	{0x13,0x1d},//10 20140121wxc
	{0x14,0x00},
	{0x12,0x00},
	{0x6b,0x1e},//11 20140121wxc
	{0x6c,0x00},
	{0x6d,0x00},
	{0x6e,0x00},
	{0x6f,0x1e},//11 20140121wxc
	{0x73,0x1f},//12 20140121wxc
	{0x7a,0x2e},//11 20140121wxc
	{0x15,0x10},//18 20140121wxc
	{0x71,0x11},//19 20140121wxc
	{0x76,0x11},//19 20140121wxc
	{0x29,0x08},
	{0x18,0x01},
	{0x19,0x10},
	{0x1a,0xc3},//c1
	{0x1b,0x6f},
	{0x1d,0x11},//01
	{0x1e,0x00},//e
	{0x1f,0x80},
	{0x20,0x7f},
	{0x22,0x14},////1b 20140121wxc
	{0x25,0xff},
	{0x2b,0x88},
	{0x2c,0x85},
	{0x2d,0x00},
	{0x2e,0x80},
	{0x27,0x38},
	{0x28,0x03},
	{0x70,0x16},//  20140121wxc
	{0x72,0x15},//a  20140121wxc
	{0x74,0x15},// 20140121wxc
	{0x75,0x15},// 20140121wxc
	{0x77,0x13},//18
	{0x7f,0x16}, // 20140121wxc
	{0xfd,0x01}, 
    {0x5d,0x11},//lvcheng 20140409:	fix mirror problem from sbk yyj
	{0x5f,0x00},		//延长
	{0x36,0x08},		//延长
	{0x2f,0xff},		//延长
	{0xfb,0x25},		//blacklevl
	{0x48,0x00},		//dp
	{0x49,0x99}, 
	{0xf2,0x0a},		//同SP2518 0xf4 
	{0xfd,0x02},//AE
	{0x52,0x34},
	{0x53,0x02},		//测试是否ae抖
	{0x54,0x0c},
	{0x55,0x08}, 
	{0x86,0x0c},		//其中满足条件帧数
	{0x87,0x10},		//检测总帧数
	{0x8b,0x10}, 
	
#if 1
	//60HZ pll 2.5 60M 12-19.974fps 
	{0xfd,0x00},//ae setting    50HZ，60Hz哪里设置？
	{0x03,0x03}, 
	{0x04,0x00}, 
	{0x05,0x00},
	{0x06,0x00},  
	{0x09,0x00},
	{0x0a,0xc2},
	{0xfd,0x01},
	{0xf0,0x00},		//base 5msb
	{0xf7,0x80},
	{0xf8,0x80}, 
	{0x02,0x0a},    // 60hz =0x0a 50hz =0x08
	{0x03,0x01},		//exp_min_indr
	{0x06,0x80}, 
	{0x07,0x00},		//50Hz exp_max_outdr[12:8]
	{0x08,0x01},		//50Hz exp_min_outdr[7:0]
	{0x09,0x00},		//50Hz exp_min_outdr[12:8]
	{0xfd,0x02},		
	{0x40,0x0a},		//60Hz exp_max_indr
	{0x41,0x80},		//60Hz exp_max_outdr[7:0]   
	{0x42,0x00},		//60Hz exp_max_outdr[12:8]  
	{0x88,0x00},		//(2/Regf7)*256*256
	{0x89,0x00},		//(2/Regf8)*256*256
	{0x8a,0x44},		//
	{0xfd,0x02},//Status
	{0xbe,0x00},		//DP_exp_8lsm
	{0xbf,0x05},		//DP_exp_5hsm
	{0xd0,0x00},		
	{0xd1,0x05},		//exp_heq_dummy_5hsm
	{0xfd,0x01},		//exp_heq_dummy_8lsm
	{0x5b,0x05},		//exp_heq_low_5hsm
	{0x5c,0x00},		//exp_heq_low_8lsm  
	{0xfd,0x00},  
#endif
#if 0
	//   50Hz  pll 2.5 60M 12~19.896fps   1280x720
	{0xfd,0x00},//ae setting    50HZ,60Hz?????		
	{0x03,0x03}, 		
	{0x04,0x90}, 		
	{0x05,0x00},		
	{0x06,0x00},  		
	{0x09,0x00},		
	{0x0a,0xcc},		
	{0xfd,0x01},		
	{0xf0,0x00},		//base 5msb		
	{0xf7,0x98},		
	{0xf8,0x80}, 		
	{0x02,0x08},    // 60hz =0x0a 50hz =0x08		
	{0x03,0x01},		//exp_min_indr		
	{0x06,0x98}, 		
	{0x07,0x00},		//50Hz exp_max_outdr[12:8]		
	{0x08,0x01},		//50Hz exp_min_outdr[7:0]		
	{0x09,0x00},		//50Hz exp_min_outdr[12:8]		
	{0xfd,0x02},				
	{0x40,0x0a},		//60Hz exp_max_indr		
	{0x41,0x80},		//60Hz exp_max_outdr[7:0]   		
	{0x42,0x00},		//60Hz exp_max_outdr[12:8]  		
	{0x88,0x5e},		//(2/Regf7)*256*256		
	{0x89,0x00},		//(2/Regf8)*256*256		
	{0x8a,0x43},		//		
	{0xfd,0x02},//Status		
	{0xbe,0xc0},		//DP_exp_8lsm		
	{0xbf,0x04},		//DP_exp_5hsm		
	{0xd0,0xc0},				
	{0xd1,0x04},		//exp_heq_dummy_5hsm		
	{0xfd,0x01},		//exp_heq_dummy_8lsm		
	{0x5b,0x04},		//exp_heq_low_5hsm		
	{0x5c,0xc0},		//exp_heq_low_8lsm  		
	{0xfd,0x00},
#endif

	{0xfd,0x01},//fix status
	{0x5a,0x38},		//DP_gain
	{0xfd,0x02},
	{0xba,0x30},		//mean_dummy_low
	{0xbb,0x50},		//mean_low_dummy
	{0xbc,0xc0},		//rpc_heq_low
	{0xbd,0xa0},		//rpc_heq_dummy
	{0xb8,0x80},		//mean_nr_dummy
	{0xb9,0x90},		//mean_dummy_nr
	{0xfd,0x01},//rpc
	{0xe0,0x54},//6c 
	{0xe1,0x40},//54 
	{0xe2,0x38},//48 
	{0xe3,0x34},//40
	{0xe4,0x34},//40
	{0xe5,0x30},//e
	{0xe6,0x30},//e
	{0xe7,0x2e},//a
	{0xe8,0x2e},//a
	{0xe9,0x2e},//a
	{0xea,0x2c},//38
	{0xf3,0x2c},//38
	{0xf4,0x2c},//38
	{0xfd,0x01},//ae min gain 
	{0x04,0xc0},		//rpc_max_indr
	{0x05,0x2c},//38		//1e//rpc_min_indr 
	{0x0a,0xc0},		//rpc_max_outdr
	{0x0b,0x2c},//38		//rpc_min_outdr 
	{0xfd,0x01},//ae target
	{0xeb,0x78},		 
	{0xec,0x78},		
	{0xed,0x05},
	{0xee,0x08},
	{0xfd,0x01},		//lsc
	{0x26,0x30},
	{0x27,0xdc},
	{0x28,0x05},
	{0x29,0x08},
	{0x2a,0x00},
	{0x2b,0x03},
	{0x2c,0x00},
	{0x2d,0x2f},
	{0xfd,0x01},		//RGain
	{0xa1,0x60},//52//46//0x3c//48		//left
	{0xa2,0x5c},//c//50//0x3f//58		//right
	{0xa3,0x46},//30//43//0x30//58		//up
	{0xa4,0x46},//30//44//0x38//50		//down
	{0xad,0x08},//06//08//0x08//08		//lu
	{0xae,0x03},//03//0a//0x0a//10		//ru
	{0xaf,0x03},//0a//0a//0x0a//10		//ld
	{0xb0,0x08},//06//06//0x06//10		//rd
	{0x18,0x00},//40		//left
	{0x19,0x00},//50		//right 
	{0x1a,0x00},//32		//up
	{0x1b,0x00},//30		//down
	{0xbf,0x00},//a5		//lu
	{0xc0,0x00},//a0		//ru
	{0xc1,0x00},//08		//ld
	{0xfa,0x00},//00		//rd 

	{0xa5,0x5a},//a//34//0x34//38	//GGain
	{0xa6,0x50},//40//3a//0x3a//48
	{0xa7,0x40},//30//30//0x30//48
	{0xa8,0x40},//30//38//0x38//40
	{0xb1,0x00},//00//00
	{0xb2,0x00},//00//00
	{0xb3,0x00},//00//00
	{0xb4,0x00},//00//00
	{0x1c,0x00},//28
	{0x1d,0x00},//40
	{0x1e,0x00},//c
	{0xb9,0x00},//25 
	{0x21,0x00},//b0
	{0x22,0x00},//a0
	{0x23,0x00},//50
	{0x24,0x00},//0d

	{0xa9,0x50},//40//30//0x38//38		//BGain
	{0xaa,0x50},//40//33//0x3c//48
	{0xab,0x40},//30//2d//0x30//46
	{0xac,0x40},//30//30//0x38//46
	{0xb5,0x00},//00//08
	{0xb6,0x00},//00//08
	{0xb7,0x00},//04//08
	{0xb8,0x00},//02//08
	{0xba,0x00},//12
	{0xbc,0x00},//30
	{0xbd,0x00},//31
	{0xbe,0x00},//e
	{0x25,0x00},//a0
	{0x45,0x00},//a0
	{0x46,0x00},//12
	{0x47,0x00},//09

	{0xfd,0x02},
	{0x26,0xc9},
	{0x27,0x8b},
	{0x08,0x05},//add
	{0x09,0x06},//add
	{0x11,0x09},//add
	{0x1b,0x80},
	{0x1a,0x80},
	{0x18,0x27},
	{0x19,0x26},
	{0x2a,0x00},//01
	{0x2b,0x00},//10
	{0x28,0xf8},		//0xa0
	{0x29,0x08},
	//d65 88 ce 73
	{0x66,0x38},//48//37//42//38//35		//0x48
	{0x67,0x65},//68//57//6a//54//60		//0x69
	{0x68,0xb8},//c0//bc//f8//a7//ba//b0		//c8//0xb5//0xaa
	{0x69,0xea},//da//13//d2//ce//e0		//f4//0xda//0xed
	{0x6a,0xa5},//a6//
	//indoor 89
	{0x7c,0x25},//44//
	{0x7d,0x53},//60//
	{0x7e,0xe3},//d5// e8
	{0x7f,0x10},//08// 18
	{0x80,0xa6},
	//cwf 8a  
	{0x70,0x18},//24//15//2f		//0x3b
	{0x71,0x3d},//4     //4a		//0x55
	{0x72,0x00},	//0x28
	{0x73,0x25},//24		//0x45
	{0x74,0xaa},
	//tl84 8b 96 9b
	{0x6b,0x00},// 0c//08//18 20140121
	{0x6c,0x18},//28//2c//24//34		20140121
	{0x6d,0x10},//15//12//17		//0x35
	{0x6e,0x30},////35//32		//0x52
	{0x6f,0xaa},
	//f 8c b4 8e
	{0x61,0xe8},//f2 20140213wxc    
	{0x62,0x08},//22 20140213wxc    
	{0x63,0x38},//28 20140213wxc    
	{0x64,0x58},//50 20140213wxc    
	{0x65,0x6a},		//0x6a

	{0x75,0x80},
	{0x76,0x09},
	{0x77,0x02},
	{0x24,0x25},
	{0x0e,0x16},
	{0x3b,0x09},

	{0xfd,0x02},		// sharp
	{0xde,0x0f},

	{0xd2,0x02},//0x04		//控制黑白边；0-边粗，f-变细
	{0xd3,0x06},//lvcheng 20140331:	fix redder problem,04//0x06
	{0xd4,0x08},
	{0xd5,0x08},

	{0xd7,0x18},		//轮廓判断
	{0xd8,0x25},
	{0xd9,0x32},
	{0xda,0x40},
	{0xdb,0x08},

	{0xe8,0x48},//0x40		//轮廓强度
	{0xe9,0x48},//0x38
	{0xea,0x30},
	{0xeb,0x20},

	{0xec,0x48},//0x40
	{0xed,0x48},//0x40
	{0xee,0x30},
	{0xef,0x20},

	{0xf3,0x00},		//平坦区域锐化力度
	{0xf4,0x00},
	{0xf5,0x00},
	{0xf6,0x00},

	{0xfd,0x02},		//skin sharpen
	{0xdc,0x04},		//肤色降锐化
	{0x05,0x6f},		//排除肤色降锐化对分辨率卡引起的干扰
	{0x09,0x10},		//肤色排除白点区域
	{0xfd,0x01},		//dns
	{0x64,0x53},//lvcheng 20140331:	fix redder problem//0x44//22		//沿方向边缘平滑力度//0-最强，8-最弱
	{0x65,0x22},		
	{0x86,0x20},		//沿方向边缘平滑阈值，越小越弱
	{0x87,0x20},		
	{0x88,0x20},		
	{0x89,0x20},		
	//lvcheng 20140331:	fix redder problem	
	{0x6d,0x04},//0x04//0f		//强平滑（平坦）区域平滑阈值
	{0x6e,0x06},//0x06//0f		
	{0x6f,0x0a},		
	{0x70,0x10},		
	//lvcheng 20140331:	fix redder problem	
	{0x71,0x08},//04//0x08//0d		//弱轮廓（非平坦）区域平滑阈值	
	{0x72,0x12},//08//0x12//23	 	
	{0x73,0x1c},//a		
	{0x74,0x24},//f	
	{0x75,0x44},//46		//[7:4]平坦区域强度，[3:0]非平坦区域强度；0-最强，8-最弱；
	//lvcheng 20140331:	fix redder problem
	{0x76,0x01},//36		
	{0x77,0x00},//25		
	{0x78,0x00},//12		
	{0x81,0x10},//		//2x//根据增益判定区域阈值
	{0x82,0x20},////4x
	{0x83,0x30},//ff		//8x
	{0x84,0x48},//ff		//16x
	{0x85,0x0a},		// 12/8+reg0x81 第二阈值，在平坦和非平坦区域做连接
	{0xfd,0x01},		//gamma
	{0x8b,0x00},//00////00//00//00//00//00// 
	{0x8c,0x07},//02////02//0b//0b//10//07// 
	{0x8d,0x10},//0a////0a//19//17//20//11// 
	{0x8e,0x1b},//13////13//2a//27//31//1d// 
	{0x8f,0x27},//d////1d//37//35//3f//29// 
	{0x90,0x3b},//30////30//4b//51//53//3a// 
	{0x91,0x4b},//40////40//5e//64//64//4a// 
	{0x92,0x5b},//4e////4e//6c//74//74//5a// 
	{0x93,0x6a},//5a////5a//78//80//80//69// 
	{0x94,0x83},//71////71//92//92//92//81// 
	{0x95,0x95},//85////85//a6//a2//a2//95// 
	{0x96,0xa6},//96////96//b5//af//af//a6// 
	{0x97,0xb5},//a6////a6//bf//bb//bb//b6// 
	{0x98,0xc1},//b3////b3//ca//c6//c6//c3// 
	{0x99,0xca},//c0////c0//d2//d0//d0//cd// 
	{0x9a,0xd2},//cb////cb//d9//d9//d9//d5// 
	{0x9b,0xdb},//d5////d5//e1//e0//e0//dd// 
	{0x9c,0xe3},//df////df//e8//e8//e8//e3// 
	{0x9d,0xeb},//e9////e9//ee//ee//ee//e8// 
	{0x9e,0xf3},//f2////f2//f4//f4//f4//ed// 
	{0x9f,0xfa},//fa//fa//fa//fa//fa// f2//
	{0xa0,0xff},//ff//ff//ff//ff//ff// f7//
	//ccm   
	{0xfd,0x02},		//CCM
	{0x15,0xb0},//0xab		//b>tha0 a4a6 95 c3
	{0x16,0x95},//0x90	//r<th88 878f ae 7d

	{0xa0,0x80},//0x80//99//0x99//a6//a6//8c//80// 非F 86
	{0xa1,0x00},//0x00//0c//0x0c//da//da//da//fa//00// fa
	{0xa2,0x00},//0x00//da//0xda//00//00//00//fa//00// 00
//lvcheng 20140331:	fix redder problem
	{0xa3,0xf7},//0xf4//00//0x00//e7//e7//da//da//e7//
	{0xa4,0x82},//0xa6//8c//0x99//c0//c0//c0//c0//a6//
	{0xa5,0x07},//0xe7//f4//0xe7//da//da//e7//e7//f4//
	{0xa6,0x01},//0xed//00//0x00//00//00//00//00//00//
	{0xa7,0xe7},//0x06//e7//0xe7//b4//b4//a7//cd//da//
	{0xa8,0x97},//lvcheng 20140331:	fix redder problem,0x8c//99//0x99//cc//d9//b3//a6//
	{0xa9,0x00},//0x00//30//0x30//0c//0c//0c//3c//00// 0c   
	{0xaa,0x03},//0x33//30//0x30//33//33//33//33//33//  
	{0xab,0x0c},//lvcheng 20140331:	fix redder problem,0x03//0c//0x0c//0c//0c//0c//0c//0c//

	{0xac,0x99},//0x99//0x99//80//a2//b3//8c//F
	{0xad,0x26},//0x26//0x26//00//04//0c//0c//
	{0xae,0xc0},//0xc0//0xc0//00//da//c0//e7//
	{0xaf,0x00},//0x00//0xed//e7//cd//cd//b4//
	{0xb0,0x93},//0xa6//0xcc//c0//d9//e6//e6//
	{0xb1,0xed},//0xda//0xcd//da//da//cd//e7//
	{0xb2,0xed},//0xed//0xed//e7//f6//e7//e7//
	{0xb3,0xda},//0xda//0xda//b4//98//9a//9a//
	{0xb4,0xb9},//0xb9//0xb9//e6//f3//00//00// 
	{0xb5,0x30},//0x30//0x30//00//30//30//30//
	{0xb6,0x30},//0x30//0x33//33//33//33//33//
	{0xb7,0x0f},//0x0f//0x0f//0f//0f//1f//1f// 

	{0xfd,0x01},		//sat u 
	//lvcheng 20140331:	fix redder problem
	{0xd3,0x78},//8e//0x86	//过标准118% 0x8a    
	{0xd4,0x78},//8e//0x86		0x8a
	{0xd5,0x70},//82//0x82	0x86
	{0xd6,0x68},//7a//0x7a	0x7e
	{0xd7,0x78},//8e//0x86	//sat v 0x8a 
	{0xd8,0x78},//8e//0x86		0x8a
	{0xd9,0x70},//82//0x82	0x86	
	{0xda,0x68},//7a//0x7a	0x7e	
	{0xfd,0x01},		//auto_sat
	{0xd2,0x00},		//autosa_en
	{0xfd,0x01},		//uv_th	
	{0xc2,0xee}, //白色物体表面有彩色噪声降低此值
	{0xc3,0xee},
	{0xc4,0xdd},
	{0xc5,0xbb},
	{0xfd,0x01},		//low_lum_offset
	{0xcd,0x10},
	{0xce,0x1f},
	{0xfd,0x02},		//gw
	{0x35,0x6f},
	{0x37,0x13},
	{0xfd,0x01},		//heq
	{0xdb,0x00},
	{0x10,0x00}, 
	{0x11,0x00}, 
	{0x12,0x00}, 
	{0x13,0x00}, 
	{0x14,0x10},//15
	{0x15,0x06},//0a
	{0x16,0x06},//0a
	{0x17,0x02},//05 
	{0xfd,0x02},		//cnr
	{0x8e,0x10}, 
	{0x90,0x20},
	{0x91,0x20},
	{0x92,0x60},
	{0x93,0x80},
	{0xfd,0x02},		//auto 
	{0x85,0x00},	//12 enable 50Hz/60Hz function
	{0xfd,0x01}, 
	{0x00,0x00}, 	//fix mode 
	{0x32,0x15},//		//ae en
	{0x33,0xef},		//lsc\bpc en
	{0x34,0xc7},		 //ynr\cnr\gamma\color en
	{0x35,0x40},		//YUYV     00//yyj224          
	{0xfd,0x00},	  		
	{0x31,0x71},       //0x71   //lvcheng 20140409:	fix mirror problem from sbk yyj
	{0x19,0x10},//720p
	{0x30,0x00},
/*
  * Added for accurate  standby sp1628
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
//	{0x92,0x81}, //20140121 wxc
};

static struct v4l2_subdev_info sp1628_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt	= 1,
		.order	= 0,
	},
};
/*
//lvcheng20140411 has problem,revert,lvcheng 20140409:	reduce reg setting from sbk yyj --->
static struct msm_camera_i2c_reg_conf sp1628_capture_settings[] ={
    //60HZ pll 2.5 60M 12-19.974fps 
	{0xfd,0x00},//ae setting    50HZ，60Hz哪里设置？
	{0x03,0x03}, 
	{0x04,0x00}, 
	{0x05,0x00},
	{0x06,0x00},  
	{0x09,0x00},
	{0x0a,0xc2},
	{0xfd,0x01},
	{0xf0,0x00},		//base 5msb
	{0xf7,0x80},
	{0xf8,0x80}, 
	{0x02,0x0a},    // 60hz =0x0a 50hz =0x08
	{0x03,0x01},		//exp_min_indr
	{0x06,0x80}, 
	{0x07,0x00},		//50Hz exp_max_outdr[12:8]
	{0x08,0x01},		//50Hz exp_min_outdr[7:0]
	{0x09,0x00},		//50Hz exp_min_outdr[12:8]
	{0xfd,0x02},		
	{0x40,0x0a},		//60Hz exp_max_indr
	{0x41,0x80},		//60Hz exp_max_outdr[7:0]   
	{0x42,0x00},		//60Hz exp_max_outdr[12:8]  
	{0x88,0x00},		//(2/Regf7)*256*256
	{0x89,0x00},		//(2/Regf8)*256*256
	{0x8a,0x44},		//
	{0xfd,0x02},//Status
	{0xbe,0x00},		//DP_exp_8lsm
	{0xbf,0x05},		//DP_exp_5hsm
	{0xd0,0x00},		
	{0xd1,0x05},		//exp_heq_dummy_5hsm
	{0xfd,0x01},		//exp_heq_dummy_8lsm
	{0x5b,0x05},		//exp_heq_low_5hsm
	{0x5c,0x00},		//exp_heq_low_8lsm  
	
	{0xfd,0x00},  
    {0x2f,0x4d},
    {0x31,0x71},
    {0x19,0x10},//720p
	{0x30,0x00},
	
    {0xfd,0x01},  
    {0x5d,0x11},
};
// <---
*/
#if 0
static struct msm_camera_i2c_reg_conf sp1628_svga_settings[] = {
//bininig svga
 {0xfd,0x00},
 {0x19,0x03},
 {0x30,0x00},//01 00
 {0x31,0x04},
 {0x33,0x01},
 
 {0x3f,0x00},   //mirror/flip	//test-luo-0730																  

 //MIPI     
 {0x95,0x03},
 {0x94,0x20},
 {0x97,0x02},
 {0x96,0x58},


//FPS2
//sp1628 24M 2倍频  17.0574-17fps ABinning+DBinning AE_Parameters_20130925170449
{0xfd,0x00},
{0x2f,0x04},//04
//ae setting
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
{0x02,0x05},
{0x03,0x01},
{0x06,0xd3},
{0x07,0x00},
{0x08,0x01},
{0x09,0x00},
{0xfd,0x02},
{0x3d,0x07},
{0x3e,0xb0},
{0x3f,0x00},
{0x88,0x6d},
{0x89,0xe8},
{0x8a,0x22},
{0xfd,0x02},
{0xbe,0x1f},
{0xbf,0x04},
{0xd0,0x1f},
{0xd1,0x04},
{0xc9,0x1f},
{0xca,0x04}, 
 };
#endif
#if 0
static struct msm_camera_i2c_reg_conf SP1628_reg_saturation[11][9] = {
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

	{
		//sat u                                                                                            
		 {0xfd,0x01},                                                                                     
		 {0xd3,0x8C},                                                                                     
		 {0xd4,0x8C},                                                                                     
		 {0xd5,0x8C},                                                                                     
		 {0xd6,0x8C},                                                                                     
		//sat v                                                                                            
		 {0xd7,0x8C},                                                                                     
		 {0xd8,0x8C},                                                                                     
		 {0xd9,0x8C},                                                                                     
		 {0xda,0x8C},  
	}, /* SATURATION LEVEL5*/

	{
		{0xfd,0x01},	//sat u
		{0xd3,0xbb},
		{0xd4,0xbb},
		{0xd5,0xa0},
		{0xd6,0x80},
		{0xd7,0xbb},	//sat v
		{0xd8,0xbb},
		{0xd9,0xa0},
		{0xda,0x80},
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

static struct msm_camera_i2c_reg_conf SP1628_reg_contrast[11][9] = {
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

static struct msm_camera_i2c_reg_conf SP1628_reg_sharpness[7][9] = {
	{
		{0xfd,0x02},
		{0xe8,0x00},		//轮廓强度
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
		{0xe8,0x10},		//轮廓强度
		{0xe9,0x10},
		{0xea,0x10},
		{0xeb,0x10},
		{0xec,0x10},
		{0xed,0x10},
		{0xee,0x10},
		{0xef,0x10},
	}, /* SHARPNESS LEVEL 1*/
	{
		{0xfd,0x02},
		{0xe8,0x18},		//轮廓强度
		{0xe9,0x18},
		{0xea,0x20},
		{0xeb,0x20},
		{0xec,0x50},
		{0xed,0x40},
		{0xee,0x30},
		{0xef,0x20},
	}, /* SHARPNESS LEVEL 2*/
	{
		{0xfd,0x02},                                                                                     
		{0xe8,0x30},   //sharpness gain for increasing pixel’s Y, in outdoor                             
		{0xec,0x40},   //sharpness gain for decreasing pixel’s Y, in outdoor                             
		{0xe9,0x38},//38   //sharpness gain for increasing pixel’s Y, in normal                           
		{0xed,0x40},//40   //sharpness gain for decreasing pixel’s Y, in normal                           
		{0xea,0x28},   //sharpness gain for increasing pixel’s Y,in dummy                                
		{0xee,0x38},   //sharpness gain for decreasing pixel’s Y, in dummy                               
		{0xeb,0x20},   //sharpness gain for increasing pixel’s Y,in lowlight                             
		{0xef,0x30},   //sharpness gain for decreasing pixel’s Y, in low light                           
	}, /* SHARPNESS LEVEL 3*/    
	{       
		{0xfd,0x02},
		{0xe8,0x78},		//轮廓强度
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
		{0xe8,0xa8},		//轮廓强度
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
		{0xe8,0xe0},		//轮廓强度
		{0xe9,0xe0},
		{0xea,0xe0},
		{0xeb,0xe0},
		{0xec,0xff},
		{0xed,0xff},
		{0xee,0xff},
		{0xef,0xff},
	}, /* SHARPNESS LEVEL 6*/
};
//yyj224
#endif
static struct msm_camera_i2c_reg_conf SP1628_reg_iso[7][3] = {
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

static struct msm_camera_i2c_reg_conf SP1628_reg_exposure_compensation[5][2] = {
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

static struct msm_camera_i2c_reg_conf SP1628_reg_antibanding[][33] = {
	
	/* OFF */
	{ 
#if 1//def  UXGA	
//60HZ pll 2.5 60M 12-19.974fps 
{0xfd,0x00},//ae setting    50HZ，60Hz哪里设置？
{0x03,0x03}, 
{0x04,0x00}, 
{0x05,0x00},
{0x06,0x00},  
{0x09,0x00},
{0x0a,0xc2},
{0xfd,0x01},
{0xf0,0x00},		//base 5msb
{0xf7,0x80},
{0xf8,0x80}, 
{0x02,0x0a},    // 60hz =0x0a 50hz =0x08
{0x03,0x01},		//exp_min_indr
{0x06,0x80}, 
{0x07,0x00},		//50Hz exp_max_outdr[12:8]
{0x08,0x01},		//50Hz exp_min_outdr[7:0]
{0x09,0x00},		//50Hz exp_min_outdr[12:8]
{0xfd,0x02},		
{0x40,0x0a},		//60Hz exp_max_indr
{0x41,0x80},		//60Hz exp_max_outdr[7:0]   
{0x42,0x00},		//60Hz exp_max_outdr[12:8]  
{0x88,0x00},		//(2/Regf7)*256*256
{0x89,0x00},		//(2/Regf8)*256*256
{0x8a,0x44},		//
{0xfd,0x02},//Status
{0xbe,0x00},		//DP_exp_8lsm
{0xbf,0x05},		//DP_exp_5hsm
{0xd0,0x00},		
{0xd1,0x05},		//exp_heq_dummy_5hsm
{0xfd,0x01},		//exp_heq_dummy_8lsm
{0x5b,0x05},		//exp_heq_low_5hsm
{0x5c,0x00},		//exp_heq_low_8lsm  
{0xfd,0x00},  
#endif
	},
	/* 60Hz */
	{
		#if 1//def  UXGA
	//60HZ pll 2.5 60M 12-19.974fps 
{0xfd,0x00},//ae setting    50HZ，60Hz哪里设置？
{0x03,0x03}, 
{0x04,0x00}, 
{0x05,0x00},
{0x06,0x00},  
{0x09,0x00},
{0x0a,0xc2},
{0xfd,0x01},
{0xf0,0x00},		//base 5msb
{0xf7,0x80},
{0xf8,0x80}, 
{0x02,0x0a},    // 60hz =0x0a 50hz =0x08
{0x03,0x01},		//exp_min_indr
{0x06,0x80}, 
{0x07,0x00},		//50Hz exp_max_outdr[12:8]
{0x08,0x01},		//50Hz exp_min_outdr[7:0]
{0x09,0x00},		//50Hz exp_min_outdr[12:8]
{0xfd,0x02},		
{0x40,0x0a},		//60Hz exp_max_indr
{0x41,0x80},		//60Hz exp_max_outdr[7:0]   
{0x42,0x00},		//60Hz exp_max_outdr[12:8]  
{0x88,0x00},		//(2/Regf7)*256*256
{0x89,0x00},		//(2/Regf8)*256*256
{0x8a,0x44},		//
{0xfd,0x02},//Status
{0xbe,0x00},		//DP_exp_8lsm
{0xbf,0x05},		//DP_exp_5hsm
{0xd0,0x00},		
{0xd1,0x05},		//exp_heq_dummy_5hsm
{0xfd,0x01},		//exp_heq_dummy_8lsm
{0x5b,0x05},		//exp_heq_low_5hsm
{0x5c,0x00},		//exp_heq_low_8lsm  
{0xfd,0x00},  
#endif

	},
	/* 50Hz */
	{
#if 1//def  UXGA
	//   50Hz  pll 2.5 60M 12~19.896fps   1280x720
{0xfd,0x00},//ae setting    50HZ,60Hz?????		
{0x03,0x03}, 		
{0x04,0x90}, 		
{0x05,0x00},		
{0x06,0x00},  		
{0x09,0x00},		
{0x0a,0xcc},		
{0xfd,0x01},		
{0xf0,0x00},		//base 5msb		
{0xf7,0x98},		
{0xf8,0x80}, 		
{0x02,0x08},    // 60hz =0x0a 50hz =0x08		
{0x03,0x01},		//exp_min_indr		
{0x06,0x98}, 		
{0x07,0x00},		//50Hz exp_max_outdr[12:8]		
{0x08,0x01},		//50Hz exp_min_outdr[7:0]		
{0x09,0x00},		//50Hz exp_min_outdr[12:8]		
{0xfd,0x02},				
{0x40,0x0a},		//60Hz exp_max_indr		
{0x41,0x80},		//60Hz exp_max_outdr[7:0]   		
{0x42,0x00},		//60Hz exp_max_outdr[12:8]  		
{0x88,0x5e},		//(2/Regf7)*256*256		
{0x89,0x00},		//(2/Regf8)*256*256		
{0x8a,0x43},		//		
{0xfd,0x02},//Status		
{0xbe,0xc0},		//DP_exp_8lsm		
{0xbf,0x04},		//DP_exp_5hsm		
{0xd0,0xc0},				
{0xd1,0x04},		//exp_heq_dummy_5hsm		
{0xfd,0x01},		//exp_heq_dummy_8lsm		
{0x5b,0x04},		//exp_heq_low_5hsm		
{0x5c,0xc0},		//exp_heq_low_8lsm  		
{0xfd,0x00},
#endif
	},
	/* AUTO */
	{
#if 1//def  UXGA
//60HZ pll 2.5 60M 12-19.974fps 
{0xfd,0x00},//ae setting    50HZ，60Hz哪里设置？
{0x03,0x03}, 
{0x04,0x00}, 
{0x05,0x00},
{0x06,0x00},  
{0x09,0x00},
{0x0a,0xc2},
{0xfd,0x01},
{0xf0,0x00},		//base 5msb
{0xf7,0x80},
{0xf8,0x80}, 
{0x02,0x0a},    // 60hz =0x0a 50hz =0x08
{0x03,0x01},		//exp_min_indr
{0x06,0x80}, 
{0x07,0x00},		//50Hz exp_max_outdr[12:8]
{0x08,0x01},		//50Hz exp_min_outdr[7:0]
{0x09,0x00},		//50Hz exp_min_outdr[12:8]
{0xfd,0x02},		
{0x40,0x0a},		//60Hz exp_max_indr
{0x41,0x80},		//60Hz exp_max_outdr[7:0]   
{0x42,0x00},		//60Hz exp_max_outdr[12:8]  
{0x88,0x00},		//(2/Regf7)*256*256
{0x89,0x00},		//(2/Regf8)*256*256
{0x8a,0x44},		//
{0xfd,0x02},//Status
{0xbe,0x00},		//DP_exp_8lsm
{0xbf,0x05},		//DP_exp_5hsm
{0xd0,0x00},		
{0xd1,0x05},		//exp_heq_dummy_5hsm
{0xfd,0x01},		//exp_heq_dummy_8lsm
{0x5b,0x05},		//exp_heq_low_5hsm
{0x5c,0x00},		//exp_heq_low_8lsm  
{0xfd,0x00},  
#endif
 },
};

static struct msm_camera_i2c_reg_conf SP1628_reg_effect_normal[] = {
	{0xfd,0x01},
	{0x66,0x00},
	{0x67,0x80},
	{0x68,0x80},
	//{0xdb,0x00},
	//{0x34,0xc7},

};

static struct msm_camera_i2c_reg_conf SP1628_reg_effect_black_white[] = {
	/* B&W: */
	{0xfd,0x01},
	{0x66,0x20},
	{0x67,0x80},
	{0x68,0x80},
	//{0xdb,0x00},
	//{0x34,0xc7},

};

static struct msm_camera_i2c_reg_conf SP1628_reg_effect_negative[] = {
	/* Negative: */
	{0xfd,0x01},
	{0x66,0x08},
	{0x67,0x80},
	{0x68,0x80},
	//{0xdb,0x00},
	//{0x34,0xc7},

};

static struct msm_camera_i2c_reg_conf SP1628_reg_effect_old_movie[] = {
	/* Sepia(antique): */
	{0xfd,0x01},
	{0x66,0x10},
	{0x67,0x98},
	{0x68,0x58},
	//{0xdb,0x00},
	//{0x34,0xc7},

};

static struct msm_camera_i2c_reg_conf SP1628_reg_effect_sepiablue[] = {
	/* Sepiabule: */
	{0xfd,0x01},
	{0x66,0x10},
	{0x67,0x80},
	{0x68,0xb0},
	//{0xdb,0x00},
	//{0x34,0xc7},

};
static struct msm_camera_i2c_reg_conf SP1628_reg_effect_solarize[] = {
	{0xfd,0x01},
	{0x66,0x80},
	{0x67,0x80},
	{0x68,0x80}, 
	//{0xdb,0x00},
	{0xdf,0x80},
	//{0x34,0xc7},

};
static struct msm_camera_i2c_reg_conf SP1628_reg_effect_emboss[] = {
	{0xfd,0x01},
	{0x66,0x01},
	{0x67,0x80},
	{0x68,0x80},
	//{0xdb,0x00},
	//{0x34,0xc7},



};
#if 0
static struct msm_camera_i2c_reg_conf SP1628_reg_scene_auto[] = {
	/* <SCENE_auto> */
	#if 0
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

static struct msm_camera_i2c_reg_conf SP1628_reg_scene_portrait[] = {
	/* <CAMTUNING_SCENE_PORTRAIT> */

};

static struct msm_camera_i2c_reg_conf SP1628_reg_scene_landscape[] = {
	/* <CAMTUNING_SCENE_LANDSCAPE> */

};

static struct msm_camera_i2c_reg_conf SP1628_reg_scene_night[] = {
	/* <SCENE_NIGHT> */
	#if 0
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
static struct msm_camera_i2c_reg_conf SP1628_reg_wb_auto[] = {
	/* Auto: */
	{0xfd,0x00},
	{0xe7,0x03},
 
	{0xfd,0x02},
	{0x26,0xc9},//0xac
	{0x27,0x8b},//0x91     

	{0xfd,0x01},
	{0x32,0x15},

	{0xfd,0x00},
	{0xe7,0x00},
};

static struct msm_camera_i2c_reg_conf SP1628_reg_wb_sunny[] = {
	/*DAYLIGHT*/
		{0xfd,0x01},
		{0x32,0x05},

		{0xfd,0x00},
		{0xe7,0x03},

		{0xfd,0x02},
		{0x26,0xca},
		{0x27,0x73},

		{0xfd,0x00},
		{0xe7,0x00},
	};

static struct msm_camera_i2c_reg_conf SP1628_reg_wb_cloudy[] = {
	/*CLOUDY*/
		{0xfd,0x01},
		{0x32,0x05},

		{0xfd,0x00},
		{0xe7,0x03},

		{0xfd,0x02},
		{0x26,0xdb},
		{0x27,0x63},

		{0xfd,0x00},
		{0xe7,0x00},
	};

static struct msm_camera_i2c_reg_conf SP1628_reg_wb_office[] = {
/* Office: *//*INCANDISCENT*/
		{0xfd,0x01},
		{0x32,0x05},

		{0xfd,0x00},
		{0xe7,0x03},

		{0xfd,0x02},
		{0x26,0x8c},
		{0x27,0xb3},

		{0xfd,0x00},
		{0xe7,0x00},
	};

static struct msm_camera_i2c_reg_conf SP1628_reg_wb_home[] = {
	/* Home: */
		{0xfd,0x01},
		{0x32,0x05},

		{0xfd,0x00},
		{0xe7,0x03},

		{0xfd,0x02},
		{0x26,0x90},
		{0x27,0xa5},

		{0xfd,0x00},
		{0xe7,0x00},
};

static const struct i2c_device_id sp1628_i2c_id[] = {
	{SP1628_SENSOR_NAME, (kernel_ulong_t)&sp1628_s_ctrl},
	{ }
};

static int32_t msm_sp1628_i2c_probe(struct i2c_client *client,
	   const struct i2c_device_id *id)
{
	CDBG("%s, E.", __func__);

	return msm_sensor_i2c_probe(client, id, &sp1628_s_ctrl);
}

static struct i2c_driver sp1628_i2c_driver = {
	.id_table = sp1628_i2c_id,
	.probe  = msm_sp1628_i2c_probe,
	.driver = {
		.name = SP1628_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client sp1628_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static const struct of_device_id sp1628_dt_match[] = {
	{.compatible = "qcom,sp1628", .data = &sp1628_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, sp1628_dt_match);

static struct platform_driver sp1628_platform_driver = {
	.driver = {
		.name = "qcom,sp1628",
		.owner = THIS_MODULE,
		.of_match_table = sp1628_dt_match,
	},
};

static int32_t sp1628_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
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

#if 0
 static int SP1628_read_cmos_sensor(int reg_addr)
{
	int rc = 0;
	int result = 0;
	rc = (&sp1628_s_ctrl)->sensor_i2c_client->i2c_func_tbl->
		i2c_read(
		(&sp1628_s_ctrl)->sensor_i2c_client, reg_addr,
		(uint16_t*)&result,MSM_CAMERA_I2C_BYTE_DATA);
	return result;
}

  static void SP1628_write_cmos_sensor(int reg_addr , int reg_data)
{
	(&sp1628_s_ctrl)->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
			(&sp1628_s_ctrl)->sensor_i2c_client, reg_addr,
			reg_data,
			MSM_CAMERA_I2C_BYTE_DATA);
}
#endif


static int32_t sp1628_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	CDBG("%s, E.", __func__);
	match = of_match_device(sp1628_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init sp1628_init_module(void)
{
	int32_t rc;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&sp1628_platform_driver,
		sp1628_platform_probe);
	if (!rc)
		return rc;
	return i2c_add_driver(&sp1628_i2c_driver);
}

static void __exit sp1628_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (sp1628_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&sp1628_s_ctrl);
		platform_driver_unregister(&sp1628_platform_driver);
	} else
		i2c_del_driver(&sp1628_i2c_driver);
	return;
}

static int32_t sp1628_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	CDBG("%s, E. calling i2c_read:, i2c_addr:%d, id_reg_addr:%d",
		__func__,
		s_ctrl->sensordata->slave_info->sensor_slave_addr,
		s_ctrl->sensordata->slave_info->sensor_id_reg_addr);

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			0x02,
			&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id 0x16:\n", __func__, chipid);
	if (chipid != 0x16) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}

	chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			0xa0,
			&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id 0x28:\n", __func__, chipid);
	if (chipid != 0x28) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}

	return rc;
}

#if 0
static void sp1628_set_stauration(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	sp1628_i2c_write_table(s_ctrl, &SP1628_reg_saturation[value][0],
		ARRAY_SIZE(SP1628_reg_saturation[value]));
}

static void sp1628_set_contrast(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	sp1628_i2c_write_table(s_ctrl, &SP1628_reg_contrast[value][0],
		ARRAY_SIZE(SP1628_reg_contrast[value]));
}

static void sp1628_set_sharpness(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	int val = value / 6;
	pr_debug("%s %d", __func__, value);
	sp1628_i2c_write_table(s_ctrl, &SP1628_reg_sharpness[val][0],
		ARRAY_SIZE(SP1628_reg_sharpness[val]));
}
#endif

static void sp1628_set_iso(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	sp1628_i2c_write_table(s_ctrl, &SP1628_reg_iso[value][0],
		ARRAY_SIZE(SP1628_reg_iso[value]));
}

static void sp1628_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	int val = (value + 12) / 6;
	pr_debug("%s %d", __func__, val);
	sp1628_i2c_write_table(s_ctrl, &SP1628_reg_exposure_compensation[val][0],
		ARRAY_SIZE(SP1628_reg_exposure_compensation[val]));
}

static void sp1628_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_EFFECT_MODE_OFF: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_effect_normal[0],
			ARRAY_SIZE(SP1628_reg_effect_normal));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_MONO: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_effect_black_white[0],
			ARRAY_SIZE(SP1628_reg_effect_black_white));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_NEGATIVE: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_effect_negative[0],
			ARRAY_SIZE(SP1628_reg_effect_negative));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_SEPIA: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_effect_old_movie[0],
			ARRAY_SIZE(SP1628_reg_effect_old_movie));
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_POSTERIZE:
	{
		
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_effect_emboss[0],
			ARRAY_SIZE(SP1628_reg_effect_emboss));
		break;
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_AQUA: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_effect_sepiablue[0],
			ARRAY_SIZE(SP1628_reg_effect_sepiablue));
		break;
	}  
	case MSM_CAMERA_EFFECT_MODE_SOLARIZE: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_effect_solarize[0],
			ARRAY_SIZE(SP1628_reg_effect_solarize));
		break;
	}
	default:
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_effect_normal[0],
			ARRAY_SIZE(SP1628_reg_effect_normal));
	}
}

static void sp1628_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	sp1628_i2c_write_table(s_ctrl, &SP1628_reg_antibanding[value][0],
		ARRAY_SIZE(SP1628_reg_antibanding[value]));
}
#if 0
static void sp1628_set_scene_mode(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_SCENE_MODE_OFF: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_scene_auto[0],
			ARRAY_SIZE(SP1628_reg_scene_auto));
					break;
	}
	case MSM_CAMERA_SCENE_MODE_NIGHT: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_scene_night[0],
			ARRAY_SIZE(SP1628_reg_scene_night));
					break;
	}
	case MSM_CAMERA_SCENE_MODE_LANDSCAPE: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_scene_landscape[0],
			ARRAY_SIZE(SP1628_reg_scene_landscape));
					break;
	}
	case MSM_CAMERA_SCENE_MODE_PORTRAIT: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_scene_portrait[0],
			ARRAY_SIZE(SP1628_reg_scene_portrait));
					break;
	}
	default:
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_scene_auto[0],
			ARRAY_SIZE(SP1628_reg_scene_auto));
	}
}
#endif
static void sp1628_set_white_balance_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_WB_MODE_AUTO: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_wb_auto[0],
			ARRAY_SIZE(SP1628_reg_wb_auto));
		break;
	}
	case MSM_CAMERA_WB_MODE_INCANDESCENT: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_wb_home[0],
			ARRAY_SIZE(SP1628_reg_wb_home));
		break;
	}
	case MSM_CAMERA_WB_MODE_DAYLIGHT: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_wb_sunny[0],
			ARRAY_SIZE(SP1628_reg_wb_sunny));
					break;
	}
	case MSM_CAMERA_WB_MODE_FLUORESCENT: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_wb_office[0],
			ARRAY_SIZE(SP1628_reg_wb_office));
					break;
	}
	case MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT: {
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_wb_cloudy[0],
			ARRAY_SIZE(SP1628_reg_wb_cloudy));
					break;
	}
	default:
		sp1628_i2c_write_table(s_ctrl, &SP1628_reg_wb_auto[0],
		ARRAY_SIZE(SP1628_reg_wb_auto));
	}
}


int32_t sp1628_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
/*
  * Added for accurate  standby sp1628
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifndef SENSOR_USE_STANDBY
		CDBG("init setting");
		#if 0
		sp1628_i2c_write_table(s_ctrl,
				&sp1628_recommend_settings[0],
				ARRAY_SIZE(sp1628_recommend_settings));
		#endif
	#ifdef DEBUG_SENSOR_SP
		if(1 == SP_Initialize_from_T_Flash())
		{
			sp1628_i2c_write_table(s_ctrl,
				&SP_Init_Reg[0],
				reg_num);


		} 
		else
			{
				sp1628_i2c_write_table(s_ctrl,
				&sp1628_recommend_settings[0],
				ARRAY_SIZE(sp1628_recommend_settings));
			}
		#else
				sp1628_i2c_write_table(s_ctrl,
				&sp1628_recommend_settings[0],
				ARRAY_SIZE(sp1628_recommend_settings));

		#endif

		CDBG("init setting X");
#endif
// <---
		break;
	case CFG_SET_RESOLUTION: 
	//lvcheng20140411 has problem,revert,lvcheng 20140409:	reduce reg setting from sbk yyj --->     
		sp1628_i2c_write_table(s_ctrl, &sp1628_recommend_settings[0],
		ARRAY_SIZE(sp1628_recommend_settings));
		CDBG("capture and preview setting X");
	// <---
		break;
	#if 1
	case CFG_SET_STOP_STREAM:
		sp1628_i2c_write_table(s_ctrl,
			&sp1628_stop_settings[0],
			ARRAY_SIZE(sp1628_stop_settings));
		break;
	case CFG_SET_START_STREAM:
		sp1628_i2c_write_table(s_ctrl,
			&sp1628_start_settings[0],
			ARRAY_SIZE(sp1628_start_settings));
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
	case CFG_SET_SATURATION: {
		int32_t sat_lev;
		if (copy_from_user(&sat_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Saturation Value is %d", __func__, sat_lev);
		//sp1628_set_stauration(s_ctrl, sat_lev);
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
		//sp1628_set_contrast(s_ctrl, con_lev);
		break;
	}
//modified by lvcheng for setting brigthness 20140307
	case CFG_SET_BRIGHTNESS: {

		CDBG("%s: Brightness Value is,need to add",
			__func__);

		break;
	}
//modified by lvcheng for setting brigthness 20140307
	case CFG_SET_SHARPNESS: {
		int32_t shp_lev;
		if (copy_from_user(&shp_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Sharpness Value is %d", __func__, shp_lev);
		//sp1628_set_sharpness(s_ctrl, shp_lev);
		break;
	}
	case CFG_SET_ISO: {
		int32_t iso_lev;
		if (copy_from_user(&iso_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: ISO Value is %d", __func__, iso_lev);
		sp1628_set_iso(s_ctrl, iso_lev);
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
		pr_debug("%s: Exposure compensation Value is %d",
			__func__, ec_lev);
		sp1628_set_exposure_compensation(s_ctrl, ec_lev);
		break;
	}
	case CFG_SET_EFFECT: {
		int32_t effect_mode;
		if (copy_from_user(&effect_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Effect mode is %d", __func__, effect_mode);
		sp1628_set_effect(s_ctrl, effect_mode);
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
		sp1628_set_antibanding(s_ctrl, antibanding_mode);
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
		//sp1628_set_scene_mode(s_ctrl, bs_mode);
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
		sp1628_set_white_balance_mode(s_ctrl, wb_mode);
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
/*
  * Added for accurate  standby sp1628
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
int32_t sp1628_sensor_init_reg_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	printk("%s:%s E\n",TAG,__func__);
  
	rc = sp1628_i2c_write_table(s_ctrl,
			&sp1628_recommend_settings[0],
			ARRAY_SIZE(sp1628_recommend_settings));
	if(rc < 0)
		goto error;
	
error:
	printk("%s:%s E\n",TAG,__func__);
	return rc;		
}
static int32_t sp1628_sensor_power_down_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	msleep(1);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,0);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
	printk("%s:%s X\n",TAG,__func__);
	return 0;
}
static int32_t sp1628_sensor_power_on_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_LOW);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,1);
	msleep(1);
	printk("%s:%s X\n",TAG,__func__);
	return 0;
}
#endif
// <---  
static struct msm_sensor_fn_t sp1628_sensor_func_tbl = {
	.sensor_config = sp1628_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = sp1628_sensor_match_id,
};

static struct msm_sensor_ctrl_t sp1628_s_ctrl = {
	.sensor_i2c_client = &sp1628_sensor_i2c_client,
	.power_setting_array.power_setting = sp1628_power_setting,
	.power_setting_array.size = ARRAY_SIZE(sp1628_power_setting),
	.msm_sensor_mutex = &sp1628_mut,
	.sensor_v4l2_subdev_info = sp1628_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sp1628_subdev_info),
	.func_tbl = &sp1628_sensor_func_tbl,
/*
  * Added for accurate  standby sp1628
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
	.sensor_power_down = sp1628_sensor_power_down_standby,
	.sensor_power_on = sp1628_sensor_power_on_standby,
	.sensor_init_reg = sp1628_sensor_init_reg_standby,
#endif
// <---
};

module_init(sp1628_init_module);
module_exit(sp1628_exit_module);
MODULE_DESCRIPTION("Sp1628 2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");


