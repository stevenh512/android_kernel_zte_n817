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
#define AT2250_SENSOR_NAME "at2250"
#define PLATFORM_DRIVER_NAME "msm_camera_at2250"

/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#define SENSOR_USE_STANDBY
#define TAG "ZTE_YCM"
// <---

//#define CLOSE_FUNCTION
//#define LOW_speed

DEFINE_MSM_MUTEX(at2250_mut);
static struct msm_sensor_ctrl_t at2250_s_ctrl;
/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#if 0
static struct msm_sensor_power_setting at2250_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
#if defined CONFIG_BOARD_FAUN
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
#else
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
#endif
        {
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VDIG,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};
#else/*power up****non power down*/
static struct msm_sensor_power_setting at2250_power_setting[] = {	
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
		.delay = 1,
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
		.delay = 0,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};
#endif
// <---
static struct msm_camera_i2c_reg_conf at2250_uxga_settings[] = {

	{0xb5, 0xf0},
	//{0x12, 0x40},
	{0x12,0x32},
	
#ifdef LOW_speed

	{0x0d,0xd0},
	{0x0e,0x10},
	{0x0f,0x66},
	{0x10,0x11},
	{0x11,0x04},

#else
/*
	{0x0d,0xd0},
	{0x0e,0x10},
	{0x0f,0x17},
	{0x10,0x14},
	{0x11,0x01},
	
*/	
#endif
	//{0X13,0X86},
	{0x36,0x7b},

	{0x1b,0x44},
	{0x1f,0x40},
	{0x20,0x8e},
	{0x21,0x07},
	{0x22,0xD8},
	{0x23,0x04},
	{0x24,0x40},
	{0x25,0xB4},
	{0x26,0x46},
	{0x27,0xE6}, //0xE4
	{0x28,0x08},
	{0x29,0x00},
	{0x2a,0xd4},
	{0x2b,0x10},
	{0x2c,0x00},
	{0x2d,0x00},
	{0x2e,0x30},
	{0x2f,0xa4},
	{0x38,0x40}, 
	{0x39,0xb0},
	{0x3a,0x46},
	{0x3b,0x40}, 
	{0x3c,0xb0},
	{0x3d,0x46},

	{0x40,0x02},

	{0xbd,0x80},
	{0xbe,0x0c},

#ifdef LOW_speed
	{0xb7,0x22},//6a
	{0xb8,0x23},//69
	{0xb9,0x27},//69
	{0xba,0x11},//33
	{0xbb,0x14},//0x16
	{0xbf,0x15},

#else
	/*{0xb7,0xb6},//6a
	{0xb8,0xd1},//69
	{0xb9,0xab},//69
	{0xba,0x55},//33
	{0xbb,0x14},//0x16
	{0xbf,0x15},*/
/*
       {0xb7,0x4a},//6a
	{0xb8,0x67},//69
	{0xb9,0x49},//69
	{0xba,0x32},//33*/
	{0xbb,0x14},//0x16  0x14
	{0xbf,0x1f},// 0x15
#endif
	{0x3e,0xa8},
	{0x3f,0x24},
	{0x60,0x0e},
	{0x61,0x11},
	{0x62,0x0e},
	{0x63,0x11},
	{0x64,0x50},
	{0x65,0xaa},	
//	{0x18,0xf8},//5d                                                     
//	{0x19,0x90},   

	{0x36,0x7b},
	{0x12,0x32},

	{0xb5,0x50},//0x50

};

static struct msm_camera_i2c_reg_conf at2250_start_settings[] = {
	{0xb5, 0x50},
};

static struct msm_camera_i2c_reg_conf at2250_stop_settings[] = {
	{0xb5, 0xf0},
};

static struct msm_camera_i2c_reg_conf at2250_recommend_settings[] = {

	{0x12,0x80},//	;reset, sleep, mirror, flip, fmt, downsample
	{0x12,0x42},//	;sleep

	{0x0c,0x04},//	;Bsun, ADCKDLY, PH_Gap, TM
	{0x30,0x96},//	;96 RST option, RST width
	{0x31,0x10},//	;16 TX option, PTX width
	{0x32,0x0c},//	;20 Bitline option, STX width
	{0x33,0x28},//	;28 Hold option, hold width
	{0xaa,0x28},//	;array iref,bias,blsw,ibit<3:0>
	{0xab,0x11},//	;cs gap,sa2ck delay,ianhlf,anbias
	{0xac,0x15},//	;Bsun 2x, vrSun<3:0>=1.5v 2x
	{0xad,0x25},//	;adbias, adrefc, adref, ad_r4
	{0xae,0x40},//	;adref, adph delay
	{0xaf,0xa0},//	;sa1-adc delay, agc vref, sa1-adc clock gap
	{0xb0,0x40},//	;60 pwc, byRSVDD,RSVDD<2:0>,ihlfary,pd_other,pd_bias,byRgu
	{0xb1,0x00},//	;00 pwc, vrBG<3:0>, vrBGlp<3:0>
	{0xb2,0x74},//	;74 pwc, byNpump, vrNvdd<2:0>, byPpump, vrHvdd<2:0>
	{0xb3,0x39},//	;41 pwc, vrFD<3:0>=0.4v, vrSun<3:0>=1.7v 1x ;10302012 from 38 to 39


	/*{0x0e,0x13},	
	{0x0f,0x03},//03  13
	{0x10,0x50},	
	{0x11,0x01},//01 02	
	{0x0d,0x80},*/

	/*
	{0x0d,0xd0},
	{0x0e,0x10},
	{0x0f,0x17},
	{0x10,0x18},
	{0x11,0x41},*/

#if 1
	{0x0e,0x10},	
	{0x0f,0x17},
	{0x10,0x1b},	
	{0x11,0x01},	
	{0x0d,0x90},

#endif

	{0x1d,0x00},//	;interface, pclk/href/vsync/dvp[9:0] off
	{0x1e,0x00},//

	{0xb5,0xF0},//	;;b5 mipi phy: pd, 2nd lane dis, 1st lane dis, data lane hs latch en, vcom, lpdrv

   //   {0xb7,0x6e},//6a
	//{0xb8,0x6a},//	69  ;;8a mipi: hs zero width<2:0>, ck zero width<4:0>
	//{0xb9,0x69},//69   	;;68 mipi: hs prep<2:0>, ck post<4:0>
	//{0xba,0x33},//33 	;;33 mipi: int pclk op, hs_trail<2:0>, ck_trail<3:0>
	{0xbb,0x14},//0x16//	;;0a mipi: pclk gate,pclk dly<1:0>,hs prep time manual sign,h full/v skip,man start cal,ck free,ckprep
	{0xbc,0x1e},//	;;2b mipi: output data type
	
	{0xbd,0x80},//	;;d0 mipi: data lane word count lsb
	{0xbe,0x0c},//	;;07 mipi: data lane work count msb
	{0xbf,0x15},//	;;14 mipi: manual prep time

#if 1
	{0xb7,0x6E},
	{0xb8,0x6a},
	{0xb9,0x69},
	{0xba,0x33},
#endif	
	{0x1b,0x44},//	;hvpading
	{0x1f,0x40},//	;cc656, data sequence, H full, H skip/bin, V bin/skip, group


			
	{0x20,0x8e},//	;20~23 frame width, height,   frame rate: 15.00375fps
	{0x21,0x07},//
	{0x22,0xD8},//
	{0x23,0x04},//
	{0x24,0x40},//	;24~26 H/V window
	{0x25,0xB4},//
	{0x26,0x46},//
	{0x27,0xE3},//	;27~29 H/V window start
	{0x28,0x0b},//
	{0x29,0x00},//
	{0x2a,0xd4},//	;2a~2b column shift start/end
	{0x2b,0x10},//
	{0x2c,0x00},//	;2c~2f H/V address shift start/end
	{0x2d,0x00},//
	{0x2e,0x30},//
	{0x2f,0xa4},//	;[7] AWB opt- 1:always do, 0 halt when ALC unstable, [3:2]-SenVEnd[9:8],[3:2]SenVst[9:8]
	{0x38,0x40},// 
	{0x39,0xb0},//
	{0x3a,0x46},//
	{0x3b,0x40},// 
	{0x3c,0xb0},//
	{0x3d,0x46},//
	{0xbd,0x80},//	;;mipi only: data lane word count lsb
	{0xbe,0x0c},//	;;mipi only: data lane work count msb*/

/*
	{0x1b,0x44},
	{0x1f,0x42},//42
	{0x20,0x52},
	{0x21,0x06},
	{0x22,0x6a},
	{0x23,0x02},
	{0x24,0x20},
	{0x25,0x58},
	{0x26,0x23},
	{0x27,0x8e}, //8d
	{0x28,0x08},
	{0x29,0x01},
	{0x2a,0x7E},
	{0x2b,0x11},
	{0x2c,0x00},
	{0x2d,0x00},
	{0x2e,0x31},
	{0x2f,0x24},

	{0x38,0x20},
	{0x39,0x58}, // 0x58
	{0x3a,0x23},
	{0x3b,0x80},
	{0x3c,0xE0},
	{0x3d,0x12},
	{0xbd,0x00},
	{0xbe,0x05},

	{0xb7,0x92},
	{0xb8,0xae},
	{0xb9,0x8b},
	{0xba,0x54},
	{0xbb,0x14},//0x06
	{0xbf,0x18},*/

	{0x13,0x86},//	;gain ceiling, auto frame rate, banding, partial line, aec/agc auto // 0x86
	{0x14,0x6C},//	;;14~17 aec/agc target, trigger range, threshold	// 0x4c
	{0x15,0x24},//	;
	{0x16,0xd9},//	;
	{0x17,0x36},//	;
	
	{0xa0,0x14},//	;avg weight
	{0xa1,0x7d},//	;avg weight
	{0xa2,0x7d},//	;avg weight
	{0xa3,0x14},//	;avg weight
	{0xa4,0x04},//	;a4 [2] 1 isp weight average/0 normal average, [1:0] 00 raw/01 RGB before gamma/10 RGB after gamma

	{0x36,0x7b},//	;hue/scalar off, isp:hue,sde,gamma,denoise,dpc,scalar,lenc,awb
	{0x37,0x00},//	;awb_stblen, isp:gbrg,bgrg,yrange,neg,nc,cip simple,awb_stblen,awb_greyen

	{0x40,0x00},//	;;40 [4]=0 raw data with AWB applied

	{0x49,0x02},//
	{0x4a,0x0f},//	;0d

	//AWB
//	{0x80,0xFA},		
	//{0x81,0x20},		
	{0x82,0x10}, //12
	{0x83,0x05}, //27
	{0x84,0x50}, // 0x50
	{0x85,0x1E},
	{0x86,0x64},
	{0x87,0x80},
	{0x88,0x92},
	{0x89,0x9b},
	{0x8A,0x94},
	{0x8B,0xad},
	{0x8C,0x8e},
	{0x8D,0x62},
	{0x8E,0x5b},
	{0x8F,0x52}, // 0x52
	{0x90,0xC8},

	//SDE
	{0x75,0x00},
	{0x76,0x40},
	{0x79,0x00},
	{0x7a,0x00},
	{0x7b,0x00},
	{0x7c,0x00},
	{0x7d,0x00},
	{0x7e,0x00},
	{0x7f,0x80},//80
	{0x77,0x0b},
	{0x78,0x09},

	//color matrix
	{0x91,0x11},
	{0x92,0x2e},
	{0x93,0x00},
	{0x94,0x11},
	{0x95,0x4a},
	{0x96,0x5b},
	{0x97,0x5f},
	{0x98,0x63},
	{0x99,0x03},
	{0x9a,0x00},
	{0x9b,0x80},
	{0x9c,0x80},
	{0x9d,0x4C},
	{0x9e,0x08},

	//gamma     
	{0x50,0x04}, // 0x04
	{0x51,0x06}, // 0x06
	{0x52,0x0c},
	{0x53,0x1a},
	{0x54,0x28},
	{0x55,0x38},
	{0x56,0x48},
	{0x57,0x59},
	{0x58,0x70},
	{0x59,0x86},
	{0x5a,0x9a},
	{0x5b,0xa8},
	{0x5c,0xb4},
	{0x5d,0xcc},
	{0x5e,0xe0},
	{0x5f,0xff},

	//ed,denoise
	{0x3e,0xa8},
	{0x3f,0x24},
	{0x60,0x0e},
	{0x61,0x11},
	{0x62,0x0e},
	{0x63,0x11},
	{0x64,0x50},
	{0x65,0xaa},


	//Lens
	{0x66,0xc4},          //RH
	{0x67,0xc8},          //RV
	{0x68,0xae},       //  ;GH
	{0x69,0xa7},       //  ;GV
	{0x6a,0xa0},       // ;BH
	{0x6b,0xa3},       //  ;BV

	{0x6c,0x23},          //RH LSB
	{0x6d,0x70},         //RV LSB
	{0x6e,0xBB},         //[7:0]GV,GH,RV, RH 
	{0x6f,0x2e},           //GH LSB
	{0x70,0x78},          //GV LSB
	{0x71,0x1f},          //BH LSB
	{0x72,0x7f},          //BV LSB
	{0x73,0x0B},          //[3:0] BV, BH MSB

	{0x05,0x60},
	{0x06,0x00},
	{0x07,0xab},
	{0x08,0x54},
	{0x09,0x05},

//	{0x18,0xf8},
//	{0x19,0x90},
	{0x36,0x7b},
	{0x12,0x32},

	{0xb5,0x50},

};
static struct v4l2_subdev_info at2250_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt	= 1,
		.order	= 0,
	},
};

static struct msm_camera_i2c_reg_conf at2250_vga_settings[] = {

{0xb5,0xF0},
		{0x12,0x43},
/*			
	{0x0e,0x10},	
	{0x0f,0x17},
		{0x10,0x1b},	
		{0x11,0x01},	
		{0x0d,0x90},
*/
	//	{0x36,0x7f},
	
		{0x1b,0x44},
		{0x1f,0x42},
		{0x20,0x52},
		{0x21,0x06},
		{0x22,0x6a},
		{0x23,0x02},
		{0x24,0x20},
		{0x25,0x58},
		{0x26,0x23},
		{0x27,0x8e}, //8d
		{0x28,0x08}, // 0x07
		{0x29,0x01},
		{0x2a,0x7E},
		{0x2b,0x11},
		{0x2c,0x02},
		{0x2d,0x03},
		{0x2e,0x31},
		{0x2f,0x24},
		//scaler
		{0x38,0x00},//0x10
		{0x39,0x40},//0x4c
		{0x3a,0x23},

		{0x3b,0x80},
		{0x3c,0xE0},
		{0x3d,0x12},
		{0xbd,0x00},
		{0xbe,0x05},

/*		{0xb7,0x6E},
		{0xb8,0x6a},
		{0xb9,0x69},
		{0xba,0x33},
	*/
		{0xbb,0x14},// 0X04
		{0xbf,0x0f},//0x0a

		{0x3e,0x92},
		{0x3f,0x24},
		{0x60,0x10},
		{0x61,0x14},
		{0x62,0x10},
		{0x63,0x14},
		{0x64,0x48},
		{0x65,0xaa},
        


		{0x18,0x77},//fa
		{0x19,0x91},//90
		
//		{0x13,0x86},
		{0x36,0x7f},
		{0x12,0x33}, //03

		{0xb5,0x50},

};
/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
static struct msm_camera_i2c_reg_conf at2250_wake_settings[] = {
	{0xb0,0x40},  
	{0x12,0x32},
	{0xb5,0x50},
};
#endif
// <---
static struct msm_camera_i2c_reg_conf at2250_sleep_settings[] = {
/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
#else
	{0x12,0x80},
#endif
// <---
	{0x1d,0x00},
	{0x1e,0x00},
	{0xb0,0x41},  // fcs test
	{0xb5,0xf0},
	{0x12,0x40},//	;sleep
};

static struct msm_camera_i2c_reg_conf AT2250_reg_saturation[11][13] = {
	{//0.5
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0x9 },//
		{0xc3,0x24},//
		{0xc5,0x2D},//
		{0xc7,0x2F},//
		{0xc9,0x2E},//
		{0xcb,0x1 },//

		{0x1f,0x41},

	},
	{//0.6
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0xA },//
		{0xc3,0x2B},//
		{0xc5,0x36},//
		{0xc7,0x39},//
		{0xc9,0x37},//
		{0xcb,0x1 },//

		{0x1f,0x41},

	},
	{ // 0.7
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},
		
		{0xc1,0xC },//
		{0xc3,0x33},//
		{0xc5,0x3F},//
		{0xc7,0x42},//
		{0xc9,0x40},//
		{0xcb,0x1 },//

		{0x1f,0x41},
	},
	{ // 0.8
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0x0E},//
		{0xc3,0x3A},//
		{0xc5,0x48},//
		{0xc7,0x4C},//
		{0xc9,0x49},//
		{0xcb,0x01},//

		{0x1f,0x41},
	},
	{ // 0.9
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0x10},//
		{0xc3,0x41},//
		{0xc5,0x51},//
		{0xc7,0x55},//
		{0xc9,0x52},//
		{0xcb,0x01},//

		{0x1f,0x41},
	},
	{
		#if 1 //131126_lj
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		/*
		{0xc1,0x12},//
		{0xc3,0x40},//0x45
		{0xc5,0x55},//
		{0xc7,0x5c},//
		{0xc9,0x5c},//
		{0xcb,0x02},//*/
		{0xc1,0x11},//
		{0xc3,0x4a},//0x45
		{0xc5,0x5b},//
		{0xc7,0x5f},//0x54 0x57
		{0xc9,0x63},//
		{0xcb,0x03},//

		{0x1f,0x41},

		#else
		{0x94,0x12,1,0},//
		{0x95,0x49,1,0},//
		{0x96,0x5a,1,0},//
		{0x97,0x5f,1,0},//
		{0x98,0x5c,1,0},//
		{0x99,0x02,1,0},//
		#endif
	},
	{// 1.1
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0x13},//
		{0xc3,0x50},//
		{0xc5,0x63},//
		{0xc7,0x68},//
		{0xc9,0x65},//
		{0xcb,0x2 },//

		{0x1f,0x41},

	},
	{ // 1.2       
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0x15},//
		{0xc3,0x57},//
		{0xc5,0x6C},//
		{0xc7,0x72},//
		{0xc9,0x6E},//
		{0xcb,0x02},//

		{0x1f,0x41},

  
	},
	{ // 1.3            
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0x17},//
		{0xc3,0x5E},//
		{0xc5,0x75},//
		{0xc7,0x7B},//
		{0xc9,0x77},//
		{0xcb,0x2 },//
		
	  	{0x1f,0x41},
	},
	{// 1.4
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0x19},//
		{0xc3,0x66},//
		{0xc5,0x7E},//
		{0xc7,0x85},//
		{0xc9,0x80},//
		{0xcb,0x2 },//

	  	{0x1f,0x41},
	},
	{// 1.5
		{0xc0,0x94},
		{0xc2,0x95},
		{0xc4,0x96},
		{0xc6,0x97},
		{0xc8,0x98},
		{0xca,0x99},

		{0xc1,0x1B},//
		{0xc3,0x6D},//
		{0xc5,0x87},//
		{0xc7,0x8E},//
		{0xc9,0x8A},//
		{0xcb,0x3 },//

	  	{0x1f,0x41},
	},
};

#define AT2250_contrast_0x7f_val 0x80


static struct msm_camera_i2c_reg_conf AT2250_reg_contrast[11][1] = {
	{
	//Contrast -5
		{0x7f, AT2250_contrast_0x7f_val-40},		

	},
	{
	//Contrast -4
		{0x7f, AT2250_contrast_0x7f_val-32},		

	},
	{
	//Contrast -3
		{0x7f, AT2250_contrast_0x7f_val-24},		

	},
	{
	//Contrast -2
		{0x7f, AT2250_contrast_0x7f_val-16},		

	},
	{
	//Contrast -1
		{0x7f, AT2250_contrast_0x7f_val-8},		

	},
	{
	//Contrast (Default)
		{0x7f, AT2250_contrast_0x7f_val},		

	},
	{
	//Contrast +1
		{0x7f, AT2250_contrast_0x7f_val+8},		


	},
	{
	//Contrast +2
		{0x7f, AT2250_contrast_0x7f_val+16},		


	},
	{
	//Contrast +3
		{0x7f, AT2250_contrast_0x7f_val+24},		


	},
	{
	//Contrast +4
		{0x7f, AT2250_contrast_0x7f_val+32},		


	},
	{
	//Contrast +5
		{0x7f, AT2250_contrast_0x7f_val+40},		


	},
};

//modified by lvcheng for setting brigthness 20140311
static struct msm_camera_i2c_reg_conf AT2250_reg_brightness_compensation[5][5] = {
	/* -2 */	
	{		

	  	{0xc0,0x91},
		{0xc1,0x09},
		{0xc2,0x92},
		{0xc3,0x26},
		{0x1f,0x41},
	},	
	/* -1 */	
	{		

	    {0xc0,0x91},
		{0xc1,0x0d},
		{0xc2,0x92},
		{0xc3,0x2a},
		{0x1f,0x41},

		
	},	
	/* 0 */	
	{       
	    {0xc0,0x91},
		{0xc1,0x11},
		{0xc2,0x92},
		{0xc3,0x2e},
		{0x1f,0x41},
	},	
	/* 1 */	
	{

	  	{0xc0,0x91},
		{0xc1,0x15},
		{0xc2,0x92},
		{0xc3,0x32},
		{0x1f,0x41},
		
		
	},	
	/* 2 */	
	{	
		{0xc0,0x91},
		{0xc1,0x19},
		{0xc2,0x92},
		{0xc3,0x36},
		{0x1f,0x41},
	},
};
//modified by lvcheng for setting brigthness 20140311		

#define AT2250_sharpness_0x3e_val  0xac 

static struct msm_camera_i2c_reg_conf AT2250_reg_sharpness[7][1] = {
	{
		{0x3e,AT2250_sharpness_0x3e_val-24},

	}, /* SHARPNESS LEVEL 0*/
	{
		{0x3e,AT2250_sharpness_0x3e_val-16},
	}, /* SHARPNESS LEVEL 1*/
	{
		{0x3e,AT2250_sharpness_0x3e_val-8},

	}, /* SHARPNESS LEVEL 2*/
	{

		{0x3e,AT2250_sharpness_0x3e_val},

	}, /* SHARPNESS LEVEL 3*/
	{
		{0x3e,AT2250_sharpness_0x3e_val+8},

	}, /* SHARPNESS LEVEL 4*/
	{
		{0x3e,AT2250_sharpness_0x3e_val+16},
	}, /* SHARPNESS LEVEL 5*/
	{
		{0x3e,AT2250_sharpness_0x3e_val+24},
	}, /* SHARPNESS LEVEL 6*/
};
#define at2250_reg_0x14_val 0x6c
static struct msm_camera_i2c_reg_conf AT2250_reg_iso[7][4] = {
	/* auto */
	{
		//{0x14,at2250_reg_0x14},		
			
				{0x7e,0x00},
				{0x7d,0x00},	
				
	},
	/* auto hjt */
	{
				{0x7e,0x00},
				{0x7d,0x00},	
	},
	/* iso 100 */
	{
				{0x7e,0x04},
				{0x7d,0x10},	
	},
	/* iso 200 */
	{
				{0x7e,0x04},
				{0x7d,0x08},	
	},
	/* iso 400 */
	{
				{0x7e,0x00},
				{0x7d,0x08},	
					
	},
	/* iso 800 */
	{
				{0x7e,0x00},
				{0x7d,0x10},	
	},
	/* iso 1600 */
	{
				{0x7e,0x00},
				{0x7d,0x18},	
	},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_exposure_compensation[5][4] = {
	/* -2 */
	{
		{0x14,at2250_reg_0x14_val-0x30},				  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},

	},
	/* -1 */
	{
		{0x14,at2250_reg_0x14_val-0x18},				  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},
	},
	/* 0 */
	{
		{0x14,at2250_reg_0x14_val},                  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},

	},
	/* 1 */
	{
		{0x14,at2250_reg_0x14_val+0x18},				  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},

	},
	/* 2 */
	{
		{0x14,at2250_reg_0x14_val+0x30},				  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},

	},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_antibanding[][2] = {
	/* OFF */
	{
		{0x18, 0x77}, // fa
		{0x19, 0x91},
	},
	/* 50Hz */
	{
		{0x18, 0x77}, // fa
		{0x19, 0x91},
	},
	/* 60Hz */
	{
		{0x18, 0x38}, //d0
		{0x19, 0x91},
	},
	/* AUTO */
	{
		{0x18, 0x77}, // fa
		{0x19, 0x91},
	},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_effect_normal[] = {
	/* normal: */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_effect_black_white[] = {
	/* B&W: */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_effect_negative[] = {
	/* Negative: */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_effect_old_movie[] = {
	/* Sepia(antique): */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_effect_solarize[] = {
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_scene_auto[] = {
	/* <SCENE_auto> */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_scene_portrait[] = {
	/* <CAMTUNING_SCENE_PORTRAIT> */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_scene_landscape[] = {
	/* <CAMTUNING_SCENE_LANDSCAPE> */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_scene_night[] = {
	/* <SCENE_NIGHT> */
	{0xff,0xff},

};

static struct msm_camera_i2c_reg_conf AT2250_reg_wb_auto[] = {
	/* Auto: */
	{0x05,0xcc},
	{0x06,0x00},
	{0x07,0x60},
	{0x08,0x54},
	{0x09,0x04},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_wb_sunny[] = {
	/* Sunny: */
	{0x05, 0xa0},
	{0x06, 0x10},
	{0x07, 0x00},
	{0x08, 0x43},
	{0x09, 0x03},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_wb_cloudy[] = {
	/* Cloudy: */
	{0x05, 0x60},
	{0x06, 0x00},
	{0x07, 0xd6},
	{0x08, 0x64},
	{0x09, 0x03},
};

static struct msm_camera_i2c_reg_conf AT2250_reg_wb_office[] = {
	/* Office: */
	{0x05, 0x60},
	{0x06, 0x00},
	{0x07, 0xab},
	{0x08, 0x54},
	{0x09, 0x05},

};

static struct msm_camera_i2c_reg_conf AT2250_reg_wb_home[] = {
	/* Home: */
	{0x05, 0x32}, 
	{0x06, 0x00}, 
	{0x07, 0xfd}, 
	{0x08, 0x44}, 
	{0x09, 0x05}, 
};


static const struct i2c_device_id at2250_i2c_id[] = {
	{AT2250_SENSOR_NAME, (kernel_ulong_t)&at2250_s_ctrl},
	{ }
};

static int32_t msm_at2250_i2c_probe(struct i2c_client *client,
	   const struct i2c_device_id *id)
{
	   return msm_sensor_i2c_probe(client, id, &at2250_s_ctrl);
}

static struct i2c_driver at2250_i2c_driver = {
	.id_table = at2250_i2c_id,
	.probe  = msm_at2250_i2c_probe,
	.driver = {
		.name = AT2250_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client at2250_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static const struct of_device_id at2250_dt_match[] = {
	{.compatible = "qcom,at2250", .data = &at2250_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, at2250_dt_match);

static struct platform_driver at2250_platform_driver = {
	.driver = {
		.name = "qcom,at2250",
		.owner = THIS_MODULE,
		.of_match_table = at2250_dt_match,
	},
};
/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
static int32_t at2250_i2c_write(struct msm_sensor_ctrl_t *s_ctrl,
		uint8_t reg_addr, uint8_t reg_data)
// <---
{
	//int i = 0;
	int rc = 0;
	//for (i = 0; i < num; ++i) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
			s_ctrl->sensor_i2c_client, reg_addr,
			reg_data,
			MSM_CAMERA_I2C_BYTE_DATA);

	//	table++;
	//}
/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
	return rc;
// <---
}

static uint16_t at2250_i2c_read(struct msm_sensor_ctrl_t *s_ctrl,
		uint32_t reg_addr)
{
	//int i = 0;
	int rc = 0;
	uint16_t reg_data=0 ;
	//for (i = 0; i < num; ++i) {
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(
			s_ctrl->sensor_i2c_client,reg_addr,
			&reg_data, MSM_CAMERA_I2C_BYTE_DATA);

	//	table++;
	//}
	return reg_data;
}
/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
static int32_t at2250_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_camera_i2c_reg_conf *table,
		int num)
// <---
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
/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
	return rc;
// <---
}

static int32_t at2250_sensor_power_on(struct msm_sensor_ctrl_t *s_ctrl)  // fcs test 
{
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;
	int index; //ZTE_YCM_2014_02_17
	//struct msm_camera_sensor_board_info *data = s_ctrl->sensordata;
	//printk("wly: at2250_sensor_power_on\n");
	power_setting_array = &s_ctrl->power_setting_array;
	for (index = 0; index < power_setting_array->size; index++) {
		power_setting = &power_setting_array->power_setting[index];
		if(power_setting->seq_type == SENSOR_CLK)
			break;
	}
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_LOW);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET],
						GPIO_OUT_LOW);
	msleep(2);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET],
						GPIO_OUT_HIGH);
			msm_cam_clk_enable(s_ctrl->dev,
				&s_ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				s_ctrl->clk_info_size,
				1);
	return 0;

}


static int32_t at2250_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	#if 0
	at2250_i2c_write_table(s_ctrl, &at2250_sleep_settings[0],
		ARRAY_SIZE(at2250_sleep_settings));
	return msm_sensor_power_down(s_ctrl);
	#endif
	
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;
	int index; //ZTE_YCM_2014_02_17
	//struct msm_camera_sensor_board_info *data = s_ctrl->sensordata;
	power_setting_array = &s_ctrl->power_setting_array;
#if 0 
	power_setting = &power_setting_array->power_setting[8];
#else ////ZTE_YCM_2014_02_17 
	for (index = 0; index < power_setting_array->size; index++) {
		power_setting = &power_setting_array->power_setting[index];
		if(power_setting->seq_type == SENSOR_CLK)
			break;
	}
#endif
	at2250_i2c_write_table(s_ctrl, &at2250_sleep_settings[0],
		ARRAY_SIZE(at2250_sleep_settings));
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
			msm_cam_clk_enable(s_ctrl->dev,
				&s_ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				s_ctrl->clk_info_size,
				0);
	//return msm_sensor_power_down(s_ctrl);
	return 0;

}

static int32_t at2250_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(at2250_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init at2250_init_module(void)
{
	int32_t rc;
	
	rc = platform_driver_probe(&at2250_platform_driver,
		at2250_platform_probe);
	pr_err("%s: rc = %d\n", __func__, rc);
	if (!rc)
		return rc;
	return i2c_add_driver(&at2250_i2c_driver);
}

static void __exit at2250_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (at2250_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&at2250_s_ctrl);
		platform_driver_unregister(&at2250_platform_driver);
	} else
		i2c_del_driver(&at2250_i2c_driver);
	return;
}

static int32_t at2250_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
		s_ctrl->sensor_i2c_client,
		s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
		&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: %s: at2250 read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);
	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

static void at2250_set_stauration(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	int i;
#ifdef CLOSE_FUNCTION
	return;
#endif
	pr_debug("%s %d", __func__, value);
	for(i=0xc0;i<=0xff;i++)
	{
		at2250_i2c_write(s_ctrl,i,0xff);
	}
	at2250_i2c_write_table(s_ctrl, &AT2250_reg_saturation[value][0],
		ARRAY_SIZE(AT2250_reg_saturation[value]));
}

static void at2250_set_contrast(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif
	pr_debug("%s %d", __func__, value);
	at2250_i2c_write_table(s_ctrl, &AT2250_reg_contrast[value][0],
		ARRAY_SIZE(AT2250_reg_contrast[value]));
}

//modified by lvcheng for setting brigthness 20140311
static void at2250_set_brightness_compensation(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{	
	int val,i;
#ifdef CLOSE_FUNCTION		
	return;
#endif	
	val = value*5/7; 
	pr_debug("%s %d", __func__, val);
	for(i=0xc0;i<=0xff;i++)
	{		
		at2250_i2c_write(s_ctrl,i,0xff);	
	}
	at2250_i2c_write_table(s_ctrl, &AT2250_reg_brightness_compensation[val][0],
			ARRAY_SIZE(AT2250_reg_brightness_compensation[val]));
}

//modified by lvcheng for setting brigthness 20140311

static void at2250_set_sharpness(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	int val;
#ifdef CLOSE_FUNCTION
		return;
#endif

	val = value / 6;
	pr_debug("%s %d", __func__, value);
	at2250_i2c_write_table(s_ctrl, &AT2250_reg_sharpness[val][0],
		ARRAY_SIZE(AT2250_reg_sharpness[val]));
}


static void at2250_set_iso(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif
	pr_debug("%s %d", __func__, value);
	at2250_i2c_write_table(s_ctrl, &AT2250_reg_iso[value][0],
		ARRAY_SIZE(AT2250_reg_iso[value]));
}

static void at2250_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	int val;
#ifdef CLOSE_FUNCTION
		return;
#endif
 	val = (value + 12) / 6;
	pr_debug("%s %d", __func__, val);
	at2250_i2c_write_table(s_ctrl, &AT2250_reg_exposure_compensation[val][0],
		ARRAY_SIZE(AT2250_reg_exposure_compensation[val]));
}

static void at2250_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int value)
{


	uint16_t reg0x37;
	uint16_t reg0x7e;
	uint16_t reg0x45;
	uint16_t reg0x47;
	uint16_t reg0x36;
	reg0x37 = at2250_i2c_read(s_ctrl, 0x37);
	reg0x7e = at2250_i2c_read(s_ctrl, 0x7e);
	reg0x45 = at2250_i2c_read(s_ctrl, 0x45);
	reg0x47 = at2250_i2c_read(s_ctrl, 0x47);
	reg0x36 = at2250_i2c_read(s_ctrl, 0x36);

	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_EFFECT_MODE_OFF: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_effect_normal[0],
			ARRAY_SIZE(AT2250_reg_effect_normal));
		at2250_i2c_write(s_ctrl,0x37, reg0x37 & 0xef);
		at2250_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		at2250_i2c_write(s_ctrl,0x45, reg0x45 & 0x7f);
		at2250_i2c_write(s_ctrl,0x47, reg0x47 & 0xfa);
		at2250_i2c_write(s_ctrl,0x36, reg0x36 & 0x7f);
		at2250_i2c_write(s_ctrl,0x7b, 0x00);
		at2250_i2c_write(s_ctrl,0x7c, 0x00);
		at2250_i2c_write(s_ctrl,0x79, 0x00);
		at2250_i2c_write(s_ctrl,0x7a, 0x00);
		at2250_i2c_write(s_ctrl,0x7d, 0x00);
		at2250_i2c_write(s_ctrl,0x7f, 0x80);

		
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_MONO: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_effect_black_white[0],
			ARRAY_SIZE(AT2250_reg_effect_black_white));
		at2250_i2c_write(s_ctrl,0x37, reg0x37 & 0xef);
		at2250_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		at2250_i2c_write(s_ctrl,0x7e, reg0x7e | 0x08);
		at2250_i2c_write(s_ctrl,0x45, reg0x45 & 0x7f);
		at2250_i2c_write(s_ctrl,0x47, reg0x47 & 0xfa);
		at2250_i2c_write(s_ctrl,0x36, reg0x36 & 0x7f);
		at2250_i2c_write(s_ctrl,0x7b, 0x00);
		at2250_i2c_write(s_ctrl,0x7c, 0x00);
		at2250_i2c_write(s_ctrl,0x7d, 0x20);
		at2250_i2c_write(s_ctrl,0x79, 0xff);
		at2250_i2c_write(s_ctrl,0x7a, 0xff);
		at2250_i2c_write(s_ctrl,0x7f, 0xb0);

		
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_NEGATIVE: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_effect_negative[0],
			ARRAY_SIZE(AT2250_reg_effect_negative));
		at2250_i2c_write(s_ctrl,0x37, reg0x37 | 0x10);
		at2250_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		at2250_i2c_write(s_ctrl,0x45, reg0x45 | 0x80);
		at2250_i2c_write(s_ctrl,0x47, reg0x47 | 0x05);
		at2250_i2c_write(s_ctrl,0x36, reg0x36 | 0x80);
		at2250_i2c_write(s_ctrl,0x7b, 0x00);
		at2250_i2c_write(s_ctrl,0x7c, 0x00);
		at2250_i2c_write(s_ctrl,0x79, 0x00);
		at2250_i2c_write(s_ctrl,0x7a, 0x00);
		at2250_i2c_write(s_ctrl,0x7d, 0x00);
		at2250_i2c_write(s_ctrl,0x7f, 0x80);

		
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_SEPIA: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_effect_old_movie[0],
			ARRAY_SIZE(AT2250_reg_effect_old_movie));
		at2250_i2c_write(s_ctrl,0x37, reg0x37 & 0xef);
		at2250_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		at2250_i2c_write(s_ctrl,0x7e, reg0x7e | 0x01);
		at2250_i2c_write(s_ctrl,0x45, reg0x45 & 0x7f);
		at2250_i2c_write(s_ctrl,0x47, reg0x47 & 0xfa);
		at2250_i2c_write(s_ctrl,0x36, reg0x36 & 0x7f);
		at2250_i2c_write(s_ctrl,0x7b, 0x20);
		at2250_i2c_write(s_ctrl,0x7c, 0x20);
		at2250_i2c_write(s_ctrl,0x79, 0x00);
		at2250_i2c_write(s_ctrl,0x7a, 0x00);
		at2250_i2c_write(s_ctrl,0x7f, 0x80);

		
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_SOLARIZE: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_effect_solarize[0],
			ARRAY_SIZE(AT2250_reg_effect_solarize));

		
		break;
	}
	default:
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_effect_normal[0],
			ARRAY_SIZE(AT2250_reg_effect_normal));
		at2250_i2c_write(s_ctrl,0x37, reg0x37 & 0xef);
		at2250_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		at2250_i2c_write(s_ctrl,0x45, reg0x45 & 0x7f);
		at2250_i2c_write(s_ctrl,0x47, reg0x47 & 0xfa);
		at2250_i2c_write(s_ctrl,0x36, reg0x36 & 0x7f);
		at2250_i2c_write(s_ctrl,0x7b, 0x00);
		at2250_i2c_write(s_ctrl,0x7c, 0x00);
		at2250_i2c_write(s_ctrl,0x79, 0x00);
		at2250_i2c_write(s_ctrl,0x7a, 0x00);
		at2250_i2c_write(s_ctrl,0x7d, 0x00);
		at2250_i2c_write(s_ctrl,0x7f, 0x80);

		
	}

}

static void at2250_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif

	pr_debug("%s %d", __func__, value);
	at2250_i2c_write_table(s_ctrl, &AT2250_reg_antibanding[value][0],
		ARRAY_SIZE(AT2250_reg_antibanding[value]));
}

static void at2250_set_scene_mode(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif

	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_SCENE_MODE_OFF: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_scene_auto[0],
			ARRAY_SIZE(AT2250_reg_scene_auto));
		
		at2250_i2c_write(s_ctrl,0x13, 0x86);
		break;
	}
	case MSM_CAMERA_SCENE_MODE_NIGHT: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_scene_night[0],
			ARRAY_SIZE(AT2250_reg_scene_night));

		
		at2250_i2c_write(s_ctrl,0xe0, 0x13);
		at2250_i2c_write(s_ctrl,0xe1, 0x8e);
		
		at2250_i2c_write(s_ctrl,0x1f, 0x41);
		
		
					break;
	}
	case MSM_CAMERA_SCENE_MODE_LANDSCAPE: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_scene_landscape[0],
			ARRAY_SIZE(AT2250_reg_scene_landscape));
		break;
	}
	case MSM_CAMERA_SCENE_MODE_PORTRAIT: {
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_scene_portrait[0],
			ARRAY_SIZE(AT2250_reg_scene_portrait));
		break;
	}
	default:
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_scene_auto[0],
			ARRAY_SIZE(AT2250_reg_scene_auto));
	}
}

static void at2250_set_white_balance_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif
uint8_t reg0x36;
   reg0x36 = at2250_i2c_read(s_ctrl,0x36);  
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_WB_MODE_AUTO: {
	   if ((reg0x36&0x01)==0x01)
	   	{
                break;
	       }
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_wb_auto[0],
			ARRAY_SIZE(AT2250_reg_wb_auto));
		at2250_i2c_write(s_ctrl,0x36, reg0x36|0x01);	
		break;
	}
	case MSM_CAMERA_WB_MODE_INCANDESCENT: {
	at2250_i2c_write(s_ctrl,0x36, reg0x36&0xfe);
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_wb_home[0],
			ARRAY_SIZE(AT2250_reg_wb_home));
		break;
	}
	case MSM_CAMERA_WB_MODE_DAYLIGHT: {
	at2250_i2c_write(s_ctrl,0x36, reg0x36&0xfe);
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_wb_sunny[0],
			ARRAY_SIZE(AT2250_reg_wb_sunny));
					break;
	}
	case MSM_CAMERA_WB_MODE_FLUORESCENT: {
	at2250_i2c_write(s_ctrl,0x36, reg0x36&0xfe);
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_wb_office[0],
			ARRAY_SIZE(AT2250_reg_wb_office));
					break;
	}
	case MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT: {
	at2250_i2c_write(s_ctrl,0x36, reg0x36&0xfe);
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_wb_cloudy[0],
			ARRAY_SIZE(AT2250_reg_wb_cloudy));
					break;
	}
	default:
		at2250_i2c_write_table(s_ctrl, &AT2250_reg_wb_auto[0],
		ARRAY_SIZE(AT2250_reg_wb_auto));
		at2250_i2c_write(s_ctrl,0x36, reg0x36|0x01);
	}
}

uint16_t  AT2250_shutter_tmp = 0;

static void AT2250_BeforeSnapshot(struct msm_sensor_ctrl_t *s_ctrl )
{

		
	uint8_t   	reg0x00, reg0x01, reg0x02, reg0x13, reg0x18, reg0x19, reg0x22, reg0x23, bandstep,i;
	uint8_t   	reg0x36, reg0x05, reg0x06, reg0x07, reg0x08, reg0x09;
	uint16_t  	shutter, bandvalue_pre, bandvalue_cap, Capture_Shutter;
	uint16_t  	VTS;
	// uint16_t  	VTS_new;
	reg0x13 = at2250_i2c_read(s_ctrl,0x13);  
	at2250_i2c_write(s_ctrl,0x13, reg0x13 | 0x01);							//disable AEC/AGC 0x13[0] write 1

	reg0x36 = at2250_i2c_read(s_ctrl, 0x36);  
//	at2250_i2c_write(s_ctrl,0x36, reg0x36 &(~0x01));							//disable AWB 0x36[0] write 0     // fcs test

	reg0x00 				= at2250_i2c_read(s_ctrl, 0x00);    
	reg0x01 				= at2250_i2c_read(s_ctrl, 0x01);
	reg0x02 				= at2250_i2c_read(s_ctrl,0x02);
	shutter 				= ((reg0x02 << 8) | reg0x01);				//read preview exposure time
	AT2250_shutter_tmp = shutter;

	reg0x18 				= at2250_i2c_read(s_ctrl,0x18);							
	reg0x19 				= at2250_i2c_read(s_ctrl,0x19);	
	bandvalue_pre 	= ((reg0x19 & 0x03) << 8)| reg0x18; //read preview bandvalue
	reg0x05 				= at2250_i2c_read(s_ctrl,0x05);
	reg0x06 				= at2250_i2c_read(s_ctrl,0x06);
	reg0x07 				= at2250_i2c_read(s_ctrl,0x07);
	reg0x08 				= at2250_i2c_read(s_ctrl,0x08);
	reg0x09 				= at2250_i2c_read(s_ctrl,0x09);
	at2250_i2c_write_table(s_ctrl, &at2250_uxga_settings[0],
					ARRAY_SIZE(at2250_uxga_settings));
	for(i=0;i<ARRAY_SIZE(at2250_uxga_settings);i++)
	{
		CDBG("	AT2250 %x = %x \n", at2250_uxga_settings[i].reg_addr,at2250_i2c_read(s_ctrl,at2250_uxga_settings[i].reg_addr));
	}
	//Sensor_SetMode(sensor_snapshot_mode);	
	//into capture mode
 	msleep(30);
	at2250_i2c_write(s_ctrl,0x05, reg0x05);
	at2250_i2c_write(s_ctrl,0x06, reg0x06);
	at2250_i2c_write(s_ctrl,0x07, reg0x07);
	at2250_i2c_write(s_ctrl,0x08, reg0x08);
	at2250_i2c_write(s_ctrl,0x09, reg0x09); 
	bandstep 		= (shutter / bandvalue_pre)* 2;			
	reg0x18 = at2250_i2c_read(s_ctrl,0x18);
	reg0x19 = at2250_i2c_read(s_ctrl,0x19);
	bandvalue_cap = ((reg0x19 & 0x03) << 8)| reg0x18 ;	//read capture bandvalue
		
	reg0x22 = at2250_i2c_read(s_ctrl,0x22);
	reg0x23 = at2250_i2c_read(s_ctrl,0x23);
	VTS = (reg0x23 << 8)| reg0x22 ;	//read capture VTS
	
	Capture_Shutter = shutter * 2*0x652/0x78e;

	/* if (shutter < bandvalue_pre)													// shutter less than 1 band value
	{	
		Capture_Shutter = shutter * 2*0x652/0x78e;
	}
	else
	{
		Capture_Shutter = bandvalue_cap * bandstep;
	}	 		
	*/	
	/*

	if(reg0x00 > 0x37)
	{
		at2250_i2c_write(s_ctrl,0x00, reg0x00 & 0x37);
		Capture_Shutter = Capture_Shutter * 2 ;								
		msleep(50);

	}
	*/
	//	if(Capture_Shutter  <= VTS)
	{
		at2250_i2c_write(s_ctrl,0x02, (Capture_Shutter >> 8) & 0xff);
		at2250_i2c_write(s_ctrl,0x01, Capture_Shutter & 0xff);
		msleep(100);
	}
	/*				else	//enter night mode
	{
		VTS_new = Capture_Shutter + 2; 
		at2250_i2c_write(s_ctrl,0x23, (VTS_new >> 8) & 0xff);
		at2250_i2c_write(s_ctrl,0x22, VTS_new & 0xff); 
		msleep(50);								
		at2250_i2c_write(s_ctrl,0x02, (Capture_Shutter >> 8) & 0xff);
		at2250_i2c_write(s_ctrl,0x01, Capture_Shutter & 0xff);
		msleep(150);
		//SENSOR_PRINT("===========bandvalue_cap=%x, bandvalue_pre=%x, VTS=%x, VTS_new=%x, Capture Shutter=%x.\n" , bandvalue_cap, bandvalue_pre, VTS, VTS_new, Capture_Shutter);
	}
	*/

	}


int32_t at2250_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifndef SENSOR_USE_STANDBY
		CDBG("init setting");
		at2250_i2c_write_table(s_ctrl,
				&at2250_recommend_settings[0],
				ARRAY_SIZE(at2250_recommend_settings));

		for(i=0xc0;i<=0xff;i++)
		{
			at2250_i2c_write(s_ctrl,i,0xff);
		}

		for(i=0;i<ARRAY_SIZE(at2250_recommend_settings);i++)
		{
			CDBG("AT2250 %x = %x  \n", at2250_recommend_settings[i].reg_addr,at2250_i2c_read(s_ctrl,at2250_recommend_settings[i].reg_addr));
		}

		msleep(100);

		
		CDBG("init setting X");
#endif
// <---
		break;
	case CFG_SET_RESOLUTION: 
	{
		int val = 0;
		if (copy_from_user(&val,
		(void *)cdata->cfg.setting, sizeof(int))) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		rc = -EFAULT;
		break;
		}
		CDBG("	AT2250 resolution val = %d\n",val);

	if (val == 0)
	{
		AT2250_BeforeSnapshot(s_ctrl );
	}


	else if (val == 1)
	{
		at2250_i2c_write_table(s_ctrl, &at2250_vga_settings[0],
		ARRAY_SIZE(at2250_vga_settings));
		//    reg0x13 = at2250_i2c_read(s_ctrl,0x13);  
//		at2250_i2c_write(s_ctrl,0x13, at2250_i2c_read(s_ctrl,0x13) | 0x01);	
		at2250_i2c_write(s_ctrl,0x02, (AT2250_shutter_tmp >> 8) & 0xff);
		at2250_i2c_write(s_ctrl,0x01, AT2250_shutter_tmp & 0xff);
		//	AT2250_shutter_tmp;
	//	msleep(100);
		at2250_i2c_write(s_ctrl,0x13, 0x86);
		msleep(200);
		/*		for(i=0;i<ARRAY_SIZE(at2250_vga_settings);i++)
		{
		CDBG("	AT2250 %x = %x \n", at2250_vga_settings[i].reg_addr,at2250_i2c_read(s_ctrl,at2250_vga_settings[i].reg_addr));
		}
		*/
		}
		break;
	}
	case CFG_SET_STOP_STREAM:
		CDBG(" at2250 stop stream \n ");
		at2250_i2c_write_table(s_ctrl,
			&at2250_stop_settings[0],
			ARRAY_SIZE(at2250_stop_settings));
		break;
	case CFG_SET_START_STREAM:

		CDBG(" at2250 start stream \n ");

		at2250_i2c_write_table(s_ctrl,
			&at2250_start_settings[0],
			ARRAY_SIZE(at2250_start_settings));
		break;
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
		at2250_set_stauration(s_ctrl, sat_lev);
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
		at2250_set_contrast(s_ctrl, con_lev);
		break;
	}
//modified by lvcheng for setting brigthness 20140311
	case CFG_SET_BRIGHTNESS: {
		int32_t br_lev;
		if (copy_from_user(&br_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {			
			pr_err("%s:%d failed\n", __func__, __LINE__);			rc = -EFAULT;			break;		}		pr_debug("%s: brightness compensation Value is %d",
			__func__, br_lev);
			at2250_set_brightness_compensation(s_ctrl, br_lev);
			break;	
	}
//modified by lvcheng for setting brigthness 20140311
	case CFG_SET_SHARPNESS: {
		int32_t shp_lev;
		if (copy_from_user(&shp_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Sharpness Value is %d", __func__, shp_lev);
		at2250_set_sharpness(s_ctrl, shp_lev);
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
		at2250_set_iso(s_ctrl, iso_lev);
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
		at2250_set_exposure_compensation(s_ctrl, ec_lev);
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
		at2250_set_effect(s_ctrl, effect_mode);
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
		at2250_set_antibanding(s_ctrl, antibanding_mode);
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
		at2250_set_scene_mode(s_ctrl, bs_mode);
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
		at2250_set_white_balance_mode(s_ctrl, wb_mode);
		break;
	}
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
int32_t at2250_sensor_init_reg_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int i = 0;
	printk("%s:%s E\n",TAG,__func__);
		rc = at2250_i2c_write_table(s_ctrl,
				&at2250_recommend_settings[0],
				ARRAY_SIZE(at2250_recommend_settings));
		if(rc < 0)
			goto error;

		for(i=0xc0;i<=0xff;i++)
		{
			rc = at2250_i2c_write(s_ctrl,i,0xff);
			if(rc < 0)
				goto error;
		}

		for(i=0;i<ARRAY_SIZE(at2250_recommend_settings);i++)
		{
			CDBG("AT2250 %x = %x  \n", at2250_recommend_settings[i].reg_addr,at2250_i2c_read(s_ctrl,at2250_recommend_settings[i].reg_addr));
		}

		msleep(100);
error:
	printk("%s:%s E\n",TAG,__func__);
	return rc;		
}
static int32_t at2250_sensor_power_down_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	at2250_i2c_write_table(s_ctrl, &at2250_sleep_settings[0],
		ARRAY_SIZE(at2250_sleep_settings));
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
	msleep(1);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,0);
	printk("%s:%s X\n",TAG,__func__);
	return 0;
}
static int32_t at2250_sensor_power_on_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_LOW);
	msleep(1);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,1);
	printk("%s:%s X\n",TAG,__func__);
	at2250_i2c_write_table(s_ctrl, &at2250_wake_settings[0],
		ARRAY_SIZE(at2250_wake_settings));
	return 0;
}
#endif
// <---

static struct msm_sensor_fn_t at2250_sensor_func_tbl = {
	.sensor_config = at2250_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = at2250_sensor_power_down,
	.sensor_match_id = at2250_sensor_match_id,
};

static struct msm_sensor_ctrl_t at2250_s_ctrl = {
	.sensor_i2c_client = &at2250_sensor_i2c_client,
	.power_setting_array.power_setting = at2250_power_setting,
	.power_setting_array.size = ARRAY_SIZE(at2250_power_setting),
	.msm_sensor_mutex = &at2250_mut,
	.sensor_v4l2_subdev_info = at2250_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(at2250_subdev_info),
	.func_tbl = &at2250_sensor_func_tbl,
	.msm_sensor_reg_default_data_type=MSM_CAMERA_I2C_BYTE_DATA,
	.sensor_power_on = at2250_sensor_power_on,
/*
  * Added for accurate  standby at2250
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
	.sensor_power_down = at2250_sensor_power_down_standby,
	.sensor_power_on = at2250_sensor_power_on_standby,
	.sensor_init_reg = at2250_sensor_init_reg_standby,
#endif
// <---
};

module_init(at2250_init_module);
module_exit(at2250_exit_module);
MODULE_DESCRIPTION("At2250 2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
