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
#define OV9740_SENSOR_NAME "ov9740"
#define PLATFORM_DRIVER_NAME "msm_camera_ov9740"

#define CONFIG_MSMB_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

/*
  * Added for accurate  standby ov9740
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#define SENSOR_USE_STANDBY
#define TAG "ZTE_YCM"
// <---

//#define CLOSE_FUNCTION
//#define LOW_speed

DEFINE_MSM_MUTEX(ov9740_mut);
static struct msm_sensor_ctrl_t ov9740_s_ctrl;

static struct msm_sensor_power_setting ov9740_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VDIG,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
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
		.delay = 1,
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
		.delay = 1,
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

static struct msm_camera_i2c_reg_conf ov9740_uxga_settings[] = {
{0x0101,0x02},  
{0x3104,0x20},  
{0x0305,0x03},  
{0x0307,0x4C}, // 0x5f  0x4c 
{0x0303,0x01}, // 0x02  0x01
{0x0301,0x08}, // 0x0a  0x08
{0x3010,0x01},  
{0x300e,0x01},  
{0x0340,0x03},  
{0x0341,0x07},  
{0x0342,0x06},  
{0x0343,0x62},  
{0x0344,0x00},  
{0x0345,0x08},  
{0x0346,0x00},  
{0x0347,0x05},  
{0x0348,0x05},  
{0x0349,0x0c},  
{0x034a,0x02},  
{0x034b,0xd9},  
{0x034c,0x05},  
{0x034d,0x00},  
{0x034e,0x02},  
{0x034f,0xd0},  
{0x3707,0x11},  
{0x3833,0x04},  
{0x3835,0x04},  
{0x3819,0x6e},  
{0x3817,0x94},  
{0x3831,0x40},  
{0x381a,0x00},  
{0x4608,0x00},  
{0x4609,0x04},  
{0x5001,0xff},  
{0x5003,0xff},  
{0x501e,0x05},  
{0x501f,0x00},  
{0x5020,0x02},  
{0x5021,0xd0},  
{0x530d,0x06},  
//{0x0100,0x01},  

};

static struct msm_camera_i2c_reg_conf ov9740_start_settings[] = {
	{0x0100, 0x01},
	
};

static struct msm_camera_i2c_reg_conf ov9740_stop_settings[] = {
  {0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf ov9740_recommend_settings[] = {
{0x0101, 0x02}, 
{0x3104, 0x20}, 
{0x0305, 0x03}, 
{0x0307, 0x4c}, 
{0x0303, 0x01}, 
{0x0301, 0x08}, 
{0x3010, 0x01}, 
{0x0340, 0x03}, 
{0x0341, 0x07}, 
{0x0342, 0x06}, 
{0x0343, 0x62}, 
{0x0344, 0x00}, 
{0x0345, 0x08}, 
{0x0346, 0x00}, 
{0x0347, 0x05}, 
{0x0348, 0x05}, 
{0x0349, 0x0c}, 
{0x034a, 0x02}, 
{0x034b, 0xd9}, 
{0x034c, 0x05}, 
{0x034d, 0x00}, 
{0x034e, 0x02}, 
{0x034f, 0xd0}, 
{0x3002, 0x00}, 
{0x3004, 0x00}, 
{0x3005, 0x00}, 
{0x3012, 0x70}, 
{0x3013, 0x60}, 
{0x3014, 0x01}, 
{0x301f, 0x03}, 
{0x3026, 0x00}, 
{0x3027, 0x00}, 
{0x3601, 0x40}, 
{0x3602, 0x16}, 
{0x3603, 0xaa}, 
{0x3604, 0x0c}, 
{0x3610, 0xa1}, 
{0x3612, 0x24}, 
{0x3620, 0x66}, 
{0x3621, 0xc0}, 
{0x3622, 0x9f}, 
{0x3630, 0xd2}, 
{0x3631, 0x5e}, 
{0x3632, 0x27}, 
{0x3633, 0x50}, 
{0x3703, 0x42}, 
{0x3704, 0x10}, 
{0x3705, 0x45}, 
{0x3707, 0x11}, 
{0x3817, 0x94}, 
{0x3819, 0x6e}, 
{0x3831, 0x40}, 
{0x3833, 0x04}, 
{0x3835, 0x04}, 
{0x3837, 0x01},
{0x3503, 0x10}, 
{0x3a18, 0x01}, 
{0x3a19, 0xB5}, 
{0x3a1a, 0x05}, 
{0x3a11, 0x90}, 
{0x3a1b, 0x4a}, 
{0x3a0f, 0x48}, 
{0x3a10, 0x44}, 
{0x3a1e, 0x42}, 
{0x3a1f, 0x22}, 
{0x3a08, 0x00}, 
{0x3a09, 0xe8}, 
{0x3a0e, 0x03}, 
{0x3a14, 0x15}, 
{0x3a15, 0xc6}, 
{0x3a0a, 0x00}, 
{0x3a0b, 0xc0}, 
{0x3a0d, 0x04}, 
{0x3a02, 0x18}, 
{0x3a03, 0x20}, 
{0x3c0a, 0x9c}, 
{0x3c0b, 0x3f}, 
{0x4002, 0x45}, 
{0x4005, 0x18}, 
{0x4601, 0x16}, 
{0x460e, 0x82},
{0x4702, 0x04}, 
{0x4704, 0x00}, 
{0x4706, 0x08}, 
{0x4800, 0x44}, 
{0x4801, 0x0f},
{0x4803, 0x05},
{0x4805, 0x10},
{0x4837, 0x20},// 0x1a 0x20
{0x5000, 0xff}, 
{0x5001, 0xff}, 
{0x5003, 0xff}, 
{0x5180, 0xf0}, 
{0x5181, 0x00}, 
{0x5182, 0x41}, 
{0x5183, 0x42}, 
{0x5184, 0x80}, 
{0x5185, 0x68}, 
{0x5186, 0x93}, 
{0x5187, 0xa8}, 
{0x5188, 0x17}, 
{0x5189, 0x45}, 
{0x518a, 0x27}, 
{0x518b, 0x41}, 
{0x518c, 0x2d}, 
{0x518d, 0xf0}, 
{0x518e, 0x10}, 
{0x518f, 0xff}, 
{0x5190, 0x0 },
{0x5191, 0xff}, 
{0x5192, 0x00}, 
{0x5193, 0xff}, 
{0x5194, 0x00}, 
{0x529a, 0x02}, 
{0x529b, 0x08}, 
{0x529c, 0x0a}, 
{0x529d, 0x10}, 
{0x529e, 0x10}, 
{0x529f, 0x28}, 
{0x52a0, 0x32}, 
{0x52a2, 0x00}, 
{0x52a3, 0x02}, 
{0x52a4, 0x00}, 
{0x52a5, 0x04}, 
{0x52a6, 0x00}, 
{0x52a7, 0x08}, 
{0x52a8, 0x00}, 
{0x52a9, 0x10}, 
{0x52aa, 0x00}, 
{0x52ab, 0x38}, 
{0x52ac, 0x00}, 
{0x52ad, 0x3c}, 
{0x52ae, 0x00}, 
{0x52af, 0x4c}, 
{0x530d, 0x06}, 
{0x5380, 0x01}, 
{0x5381, 0x00}, 
{0x5382, 0x00}, 
{0x5383, 0x0d}, 
{0x5384, 0x00}, 
{0x5385, 0x2f}, 
{0x5386, 0x00}, 
{0x5387, 0x00}, 
{0x5388, 0x00}, 
{0x5389, 0xd3}, 
{0x538a, 0x00}, 
{0x538b, 0x0f}, 
{0x538c, 0x00}, 
{0x538d, 0x00}, 
{0x538e, 0x00}, 
{0x538f, 0x32}, 
{0x5390, 0x00}, 
{0x5391, 0x94}, 
{0x5392, 0x00}, 
{0x5393, 0xa4}, 
{0x5394, 0x18}, 
{0x5401, 0x2c}, 
{0x5403, 0x28}, 
{0x5404, 0x06}, 
{0x5405, 0xe0}, 
{0x5480, 0x04}, 
{0x5481, 0x12}, 
{0x5482, 0x27}, 
{0x5483, 0x49}, 
{0x5484, 0x57}, 
{0x5485, 0x66}, 
{0x5486, 0x75}, 
{0x5487, 0x81}, 
{0x5488, 0x8c}, 
{0x5489, 0x95}, 
{0x548a, 0xa5}, 
{0x548b, 0xb2}, 
{0x548c, 0xc8}, 
{0x548d, 0xd9}, 
{0x548e, 0xec}, 
{0x5490, 0x01}, 
{0x5491, 0xc0}, 
{0x5492, 0x03}, 
{0x5493, 0x00}, 
{0x5494, 0x03}, 
{0x5495, 0xe0}, 
{0x5496, 0x03}, 
{0x5497, 0x10}, 
{0x5498, 0x02}, 
{0x5499, 0xac}, 
{0x549a, 0x02}, 
{0x549b, 0x75}, 
{0x549c, 0x02}, 
{0x549d, 0x44}, 
{0x549e, 0x02}, 
{0x549f, 0x20}, 
{0x54a0, 0x02}, 
{0x54a1, 0x07}, 
{0x54a2, 0x01}, 
{0x54a3, 0xec}, 
{0x54a4, 0x01}, 
{0x54a5, 0xc0}, 
{0x54a6, 0x01}, 
{0x54a7, 0x9b}, 
{0x54a8, 0x01}, 
{0x54a9, 0x63}, 
{0x54aa, 0x01}, 
{0x54ab, 0x2b}, 
{0x54ac, 0x01}, 
{0x54ad, 0x22}, 
{0x5501, 0x1c}, 
{0x5502, 0x00}, 
{0x5503, 0x40}, 
{0x5504, 0x00}, 
{0x5505, 0x80}, 
{0x5800, 0x1c}, 
{0x5801, 0x16}, 
{0x5802, 0x15}, 
{0x5803, 0x16}, 
{0x5804, 0x18}, 
{0x5805, 0x1a}, 
{0x5806, 0x0c}, 
{0x5807, 0x0a}, 
{0x5808, 0x08}, 
{0x5809, 0x08}, 
{0x580a, 0x0a}, 
{0x580b, 0x0b}, 
{0x580c, 0x05}, 
{0x580d, 0x02}, 
{0x580e, 0x00}, 
{0x580f, 0x00}, 
{0x5810, 0x02}, 
{0x5811, 0x05}, 
{0x5812, 0x04}, 
{0x5813, 0x01}, 
{0x5814, 0x00}, 
{0x5815, 0x00}, 
{0x5816, 0x02}, 
{0x5817, 0x03}, 
{0x5818, 0x0a}, 
{0x5819, 0x07}, 
{0x581a, 0x05}, 
{0x581b, 0x05}, 
{0x581c, 0x08}, 
{0x581d, 0x0b}, 
{0x581e, 0x15}, 
{0x581f, 0x14}, 
{0x5820, 0x14}, 
{0x5821, 0x13}, 
{0x5822, 0x17}, 
{0x5823, 0x16}, 
{0x5824, 0x46}, 
{0x5825, 0x4c}, 
{0x5826, 0x6c}, 
{0x5827, 0x4c}, 
{0x5828, 0x80}, 
{0x5829, 0x2e}, 
{0x582a, 0x48}, 
{0x582b, 0x46}, 
{0x582c, 0x2a}, 
{0x582d, 0x68}, 
{0x582e, 0x08}, 
{0x582f, 0x26}, 
{0x5830, 0x44}, 
{0x5831, 0x46}, 
{0x5832, 0x62}, 
{0x5833, 0x0c}, 
{0x5834, 0x28}, 
{0x5835, 0x46}, 
{0x5836, 0x28}, 
{0x5837, 0x88}, 
{0x5838, 0x0e}, 
{0x5839, 0x0e}, 
{0x583a, 0x2c}, 
{0x583b, 0x2e}, 
{0x583c, 0x46}, 
{0x583d, 0xca}, 
{0x583e, 0xf0}, 
{0x5842, 0x02}, 
{0x5843, 0x5e}, 
{0x5844, 0x04}, 
{0x5845, 0x32}, 
{0x5846, 0x03}, 
{0x5847, 0x29}, 
{0x5848, 0x02}, 
{0x5849, 0xcc}, 
{0x0601, 0x10}, 
};

static struct v4l2_subdev_info ov9740_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt	= 1,
		.order	= 0,
	},
};

static struct msm_camera_i2c_reg_conf ov9740_svga_settings[] = {
#if 0
{0x0101,0x02},
{0x3104,0x20},
{0x0305,0x03},
{0x0307,0x4c}, 
{0x0303,0x01}, 
{0x0301,0x08}, 
{0x3010,0x01},
{0x300e,0x12},
{0x0340,0x03},
{0x0341,0x07},
{0x0342,0x06},
{0x0343,0x62},
{0x0344,0x00},
{0x0345,0xa8},
{0x0346,0x00},
{0x0347,0x05},
{0x0348,0x04},
{0x0349,0x67},
{0x034a,0x02},
{0x034b,0xd9},
{0x034c,0x03},
{0x034d,0x20},
{0x034e,0x02},
{0x034f,0x58},
{0x3707,0x11},
{0x3833,0x04},
{0x3835,0x04},
{0x3819,0x6e},
{0x3817,0x94},
{0x3831,0x40},
{0x381a,0x00},
{0x4608,0x02},
{0x4609,0x70},
{0x5001,0xff},
{0x5003,0xff},
{0x501e,0x03},
{0x501f,0xc0},
{0x5020,0x02},
{0x5021,0xd0},
{0x530d,0x06},
{0x0100,0x01},
#endif
};
#if 0
static struct msm_camera_i2c_reg_conf ov9740_sleep_settings[] = {
};
#endif

static struct msm_camera_i2c_reg_conf OV9740_reg_saturation[9][30] = {
	{//0.6
		{0x5490, 0x01}, 
    {0x5491, 0x1a}, 
    {0x5492, 0x01}, 
    {0x5493, 0xe3}, 
    {0x5494, 0x02}, 
    {0x5495, 0x70}, 
    {0x5496, 0x01}, 
    {0x5497, 0xed}, 
    {0x5498, 0x01}, 
    {0x5499, 0xae}, 
    {0x549a, 0x01}, 
    {0x549b, 0x8c}, 
    {0x549c, 0x01}, 
    {0x549d, 0x6d}, 
    {0x549e, 0x01}, 
    {0x549f, 0x46}, 
    {0x54a0, 0x01}, 
    {0x54a1, 0x35}, 
    {0x54a2, 0x01}, 
    {0x54a3, 0x35}, 
    {0x54a4, 0x01}, 
    {0x54a5, 0x1a}, 
    {0x54a6, 0x01}, 
    {0x54a7, 0x02}, 
    {0x54a8, 0x00}, 
    {0x54a9, 0xdf}, 
    {0x54aa, 0x00}, 
    {0x54ab, 0xbc}, 
    {0x54ac, 0x00}, 
    {0x54ad, 0xb6},

	},
	{ // 0.7
    {0x5490, 0x01}, 
    {0x5491, 0x3c}, 
    {0x5492, 0x02}, 
    {0x5493, 0x1f}, 
    {0x5494, 0x02}, 
    {0x5495, 0xbd}, 
    {0x5496, 0x02}, 
    {0x5497, 0x2a}, 
    {0x5498, 0x01}, 
    {0x5499, 0xe3}, 
    {0x549a, 0x01}, 
    {0x549b, 0xbc}, 
    {0x549c, 0x01}, 
    {0x549d, 0x9a}, 
    {0x549e, 0x01}, 
    {0x549f, 0x80}, 
    {0x54a0, 0x01}, 
    {0x54a1, 0x6e}, 
    {0x54a2, 0x01}, 
    {0x54a3, 0x5b}, 
    {0x54a4, 0x01}, 
    {0x54a5, 0x3c}, 
    {0x54a6, 0x01}, 
    {0x54a7, 0x22}, 
    {0x54a8, 0x00}, 
    {0x54a9, 0xfb}, 
    {0x54aa, 0x00}, 
    {0x54ab, 0xd3}, 
    {0x54ac, 0x00}, 
    {0x54ad, 0xcd},

	},
	{ // 0.8

	  {0x5490, 0x01}, 
    {0x5491, 0x63}, 
    {0x5492, 0x02}, 
    {0x5493, 0x61}, 
    {0x5494, 0x03}, 
    {0x5495, 0x13}, 
    {0x5496, 0x02}, 
    {0x5497, 0x6e}, 
    {0x5498, 0x02}, 
    {0x5499, 0x1e}, 
    {0x549a, 0x01}, 
    {0x549b, 0xf3}, 
    {0x549c, 0x01}, 
    {0x549d, 0xcc}, 
    {0x549e, 0x01}, 
    {0x549f, 0xaf}, 
    {0x54a0, 0x01}, 
    {0x54a1, 0x9b}, 
    {0x54a2, 0x01}, 
    {0x54a3, 0x86}, 
    {0x54a4, 0x01}, 
    {0x54a5, 0x63}, 
    {0x54a6, 0x01}, 
    {0x54a7, 0x46}, 
    {0x54a8, 0x01}, 
    {0x54a9, 0x19}, 
    {0x54aa, 0x00}, 
    {0x54ab, 0xed}, 
    {0x54ac, 0x00}, 
    {0x54ad, 0xe6},
	},
	{ // 0.9
    {0x5490, 0x01}, 
    {0x5491, 0x8f}, 
    {0x5492, 0x02}, 
    {0x5493, 0xac}, 
    {0x5494, 0x03}, 
    {0x5495, 0x73}, 
    {0x5496, 0x02}, 
    {0x5497, 0xba}, 
    {0x5498, 0x02}, 
    {0x5499, 0x61}, 
    {0x549a, 0x02}, 
    {0x549b, 0x30}, 
    {0x549c, 0x02}, 
    {0x549d, 0x04}, 
    {0x549e, 0x01}, 
    {0x549f, 0xe4}, 
    {0x54a0, 0x01}, 
    {0x54a1, 0xce}, 
    {0x54a2, 0x01}, 
    {0x54a3, 0xb6}, 
    {0x54a4, 0x01}, 
    {0x54a5, 0x8f}, 
    {0x54a6, 0x01}, 
    {0x54a7, 0x6e}, 
    {0x54a8, 0x01}, 
    {0x54a9, 0x3c}, 
    {0x54aa, 0x01}, 
    {0x54ab, 0x0a}, 
    {0x54ac, 0x01}, 
    {0x54ad, 0x02},		
	},

	{//1.0
    {0x5490, 0x01}, 
    {0x5491, 0xc0}, 
    {0x5492, 0x03}, 
    {0x5493, 0x00}, 
    {0x5494, 0x03}, 
    {0x5495, 0xe0}, 
    {0x5496, 0x03}, 
    {0x5497, 0x10}, 
    {0x5498, 0x02}, 
    {0x5499, 0xac}, 
    {0x549a, 0x02}, 
    {0x549b, 0x75}, 
    {0x549c, 0x02}, 
    {0x549d, 0x44}, 
    {0x549e, 0x02}, 
    {0x549f, 0x20}, 
    {0x54a0, 0x02}, 
    {0x54a1, 0x07}, 
    {0x54a2, 0x01}, 
    {0x54a3, 0xec}, 
    {0x54a4, 0x01}, 
    {0x54a5, 0xc0}, 
    {0x54a6, 0x01}, 
    {0x54a7, 0x9b}, 
    {0x54a8, 0x01}, 
    {0x54a9, 0x63}, 
    {0x54aa, 0x01}, 
    {0x54ab, 0x2b}, 
    {0x54ac, 0x01}, 
    {0x54ad, 0x22},
	},
	{// 1.1
    {0x5490, 0x01}, 
    {0x5491, 0xf6}, 
    {0x5492, 0x03}, 
    {0x5493, 0x5e}, 
    {0x5494, 0x04}, 
    {0x5495, 0x59}, 
    {0x5496, 0x03}, 
    {0x5497, 0x70}, 
    {0x5498, 0x02}, 
    {0x5499, 0xff}, 
    {0x549a, 0x02}, 
    {0x549b, 0xc2}, 
    {0x549c, 0x02}, 
    {0x549d, 0x8b}, 
    {0x549e, 0x02}, 
    {0x549f, 0x62}, 
    {0x54a0, 0x02}, 
    {0x54a1, 0x46}, 
    {0x54a2, 0x02}, 
    {0x54a3, 0x28}, 
    {0x54a4, 0x01}, 
    {0x54a5, 0xf6}, 
    {0x54a6, 0x01}, 
    {0x54a7, 0xcd}, 
    {0x54a8, 0x01}, 
    {0x54a9, 0x8e}, 
    {0x54aa, 0x01}, 
    {0x54ab, 0x4f}, 
    {0x54ac, 0x01}, 
    {0x54ad, 0x45},

	},
	{ // 1.2       
    {0x5490, 0x02}, 
    {0x5491, 0x34}, 
    {0x5492, 0x03}, 
    {0x5493, 0xc7}, 
    {0x5494, 0x04}, 
    {0x5495, 0xe1}, 
    {0x5496, 0x03}, 
    {0x5497, 0xdb}, 
    {0x5498, 0x03}, 
    {0x5499, 0x5d}, 
    {0x549a, 0x03}, 
    {0x549b, 0x18}, 
    {0x549c, 0x02}, 
    {0x549d, 0xda}, 
    {0x549e, 0x02}, 
    {0x549f, 0xad}, 
    {0x54a0, 0x02}, 
    {0x54a1, 0x8d}, 
    {0x54a2, 0x02}, 
    {0x54a3, 0x6b}, 
    {0x54a4, 0x02}, 
    {0x54a5, 0x34}, 
    {0x54a6, 0x02}, 
    {0x54a7, 0x05}, 
    {0x54a8, 0x01}, 
    {0x54a9, 0xbf}, 
    {0x54aa, 0x01}, 
    {0x54ab, 0x78}, 
    {0x54ac, 0x01}, 
    {0x54ad, 0x6d},
  
	},
	{ // 1.3            
    {0x5490, 0x02}, 
    {0x5491, 0x79}, 
    {0x5492, 0x04}, 
    {0x5493, 0x3e}, 
    {0x5494, 0x05}, 
    {0x5495, 0x7a}, 
    {0x5496, 0x04}, 
    {0x5497, 0x54}, 
    {0x5498, 0x03}, 
    {0x5499, 0xc7}, 
    {0x549a, 0x03}, 
    {0x549b, 0x79}, 
    {0x549c, 0x03}, 
    {0x549d, 0x34}, 
    {0x549e, 0x03}, 
    {0x549f, 0x01}, 
    {0x54a0, 0x02}, 
    {0x54a1, 0xdd}, 
    {0x54a2, 0x02}, 
    {0x54a3, 0xb7}, 
    {0x54a4, 0x02}, 
    {0x54a5, 0x79}, 
    {0x54a6, 0x02}, 
    {0x54a7, 0x45}, 
    {0x54a8, 0x01}, 
    {0x54a9, 0xf6}, 
    {0x54aa, 0x01}, 
    {0x54ab, 0xa6}, 
    {0x54ac, 0x01}, 
    {0x54ad, 0x9a},
	},
	{// 1.4
    {0x5490, 0x02}, 
    {0x5491, 0xc7}, 
    {0x5492, 0x04}, 
    {0x5493, 0xc3}, 
    {0x5494, 0x06}, 
    {0x5495, 0x26}, 
    {0x5496, 0x04}, 
    {0x5497, 0xdc}, 
    {0x5498, 0x04}, 
    {0x5499, 0x3d}, 
    {0x549a, 0x03}, 
    {0x549b, 0xe6}, 
    {0x549c, 0x03}, 
    {0x549d, 0x98}, 
    {0x549e, 0x03}, 
    {0x549f, 0x5f}, 
    {0x54a0, 0x03}, 
    {0x54a1, 0x37}, 
    {0x54a2, 0x03}, 
    {0x54a3, 0x0d}, 
    {0x54a4, 0x02}, 
    {0x54a5, 0xc7}, 
    {0x54a6, 0x02}, 
    {0x54a7, 0x8c}, 
    {0x54a8, 0x02}, 
    {0x54a9, 0x33}, 
    {0x54aa, 0x01}, 
    {0x54ab, 0xda}, 
    {0x54ac, 0x01}, 
    {0x54ad, 0xcc},
	},

};

static struct msm_camera_i2c_reg_conf OV9740_reg_contrast[9][1] = {
	{
	//Contrast -4
		{0x5586, 0x0d},		

	},
	{
	//Contrast -3
		{0x5586, 0x10},		

	},
	{
	//Contrast -2
		{0x5586, 0x14},		

	},
	{
	//Contrast -1
		{0x5586, 0x19},		

	},
	{
	//Contrast (Default)
		{0x5586, 0x20},		

	},
	{
	//Contrast +1
		{0x5586, 0x28},		


	},
	{
	//Contrast +2
		{0x5586, 0x30},		


	},
	{
	//Contrast +3
		{0x5586, 0x20},		


	},
	{
	//Contrast +4
		{0x5586, 0x40},		


	},
};

#define OV9740_sharpness_0x3e_val  0xac 

static struct msm_camera_i2c_reg_conf OV9740_reg_sharpness[7][1] = {
	{
		{0x3e,OV9740_sharpness_0x3e_val-24},

	}, /* SHARPNESS LEVEL 0*/
	{
		{0x3e,OV9740_sharpness_0x3e_val-16},
	}, /* SHARPNESS LEVEL 1*/
	{
		{0x3e,OV9740_sharpness_0x3e_val-8},

	}, /* SHARPNESS LEVEL 2*/
	{

		{0x3e,OV9740_sharpness_0x3e_val},

	}, /* SHARPNESS LEVEL 3*/
	{
		{0x3e,OV9740_sharpness_0x3e_val+8},

	}, /* SHARPNESS LEVEL 4*/
	{
		{0x3e,OV9740_sharpness_0x3e_val+16},
	}, /* SHARPNESS LEVEL 5*/
	{
		{0x3e,OV9740_sharpness_0x3e_val+24},
	}, /* SHARPNESS LEVEL 6*/
};
#define ov9740_reg_0x14_val 0x6c
static struct msm_camera_i2c_reg_conf OV9740_reg_iso[7][4] = {
	/* auto */
	{
		//{0x14,ov9740_reg_0x14},		
			
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

static struct msm_camera_i2c_reg_conf OV9740_reg_exposure_compensation[5][4] = {
	/* -2 */
	{
		{0x14,ov9740_reg_0x14_val-0x30},				  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},

	},
	/* -1 */
	{
		{0x14,ov9740_reg_0x14_val-0x18},				  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},
	},
	/* 0 */
	{
		{0x14,ov9740_reg_0x14_val},                  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},

	},
	/* 1 */
	{
		{0x14,ov9740_reg_0x14_val+0x18},				  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},

	},
	/* 2 */
	{
		{0x14,ov9740_reg_0x14_val+0x30},				  
		{0x15,0x24},
		{0x16,0xd9},
		{0x17,0x36},

	},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_antibanding[][2] = {
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

static struct msm_camera_i2c_reg_conf OV9740_reg_effect_normal[] = {
	/* normal: */
	{0x5580,0x06},
	{0x5583,0x40},
	{0x5584,0x40},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_effect_black_white[] = {
	/* B&W: */
	{0x5580,0x26},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_effect_negative[] = {
	/* Negative: */
  {0x5580,0x46},
	{0x5583,0x40},
	{0x5584,0x40},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_effect_old_movie[] = {
	/* Sepia(antique): */
	{0x5580,0x1e},
	{0x5583,0x40},
	{0x5584,0xa0},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_effect_solarize[] = {
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_scene_auto[] = {
	/* <SCENE_auto> */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_scene_portrait[] = {
	/* <CAMTUNING_SCENE_PORTRAIT> */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_scene_landscape[] = {
	/* <CAMTUNING_SCENE_LANDSCAPE> */
	{0xff,0xff},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_scene_night[] = {
	/* <SCENE_NIGHT> */
	{0xff,0xff},

};

static struct msm_camera_i2c_reg_conf OV9740_reg_wb_auto[] = {
	/* Auto: */
	{0x3406,0x00},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_wb_sunny[] = {
	/* Sunny: */
	{0x3406, 0x01},
	{0x3400, 0x07},
	{0x3401, 0x5c},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x74},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_wb_cloudy[] = {
	/* Cloudy: */
	{0x3406, 0x01},
	{0x3400, 0x08},
	{0x3401, 0x26},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x4d},
};

static struct msm_camera_i2c_reg_conf OV9740_reg_wb_office[] = {
	/* Office: */
	{0x3406, 0x01},
	{0x3400, 0x06},
	{0x3401, 0xcf},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x08},
	{0x3405, 0x0b},

};

static struct msm_camera_i2c_reg_conf OV9740_reg_wb_home[] = {
	/* Home: */
	{0x3406, 0x01},
	{0x3400, 0x04},
	{0x3401, 0xa9},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x09},
	{0x3405, 0x0e}, 
};


static const struct i2c_device_id ov9740_i2c_id[] = {
	{OV9740_SENSOR_NAME, (kernel_ulong_t)&ov9740_s_ctrl},
	{ }
};

static int32_t msm_ov9740_i2c_probe(struct i2c_client *client,
	   const struct i2c_device_id *id)
{
	   return msm_sensor_i2c_probe(client, id, &ov9740_s_ctrl);
}

static struct i2c_driver ov9740_i2c_driver = {
	.id_table = ov9740_i2c_id,
	.probe  = msm_ov9740_i2c_probe,
	.driver = {
		.name = OV9740_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov9740_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov9740_dt_match[] = {
	{.compatible = "qcom,ov9740", .data = &ov9740_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov9740_dt_match);

static struct platform_driver ov9740_platform_driver = {
	.driver = {
		.name = "qcom,ov9740",
		.owner = THIS_MODULE,
		.of_match_table = ov9740_dt_match,
	},
};
static void ov9740_i2c_write(struct msm_sensor_ctrl_t *s_ctrl,
		uint8_t reg_addr, uint8_t reg_data)
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
}

static uint16_t ov9740_i2c_read(struct msm_sensor_ctrl_t *s_ctrl,
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

static int32_t ov9740_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
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

static int32_t ov9740_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	#if 0
	ov9740_i2c_write_table(s_ctrl, &ov9740_sleep_settings[0],
		ARRAY_SIZE(ov9740_sleep_settings));
#endif
	return msm_sensor_power_down(s_ctrl);
}

static int32_t ov9740_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(ov9740_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init ov9740_init_module(void)
{
	int32_t rc;
	
	rc = platform_driver_probe(&ov9740_platform_driver,
		ov9740_platform_probe);
	pr_err("%s: rc = %d\n", __func__, rc);
	if (!rc)
		return rc;
	return i2c_add_driver(&ov9740_i2c_driver);
}

static void __exit ov9740_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ov9740_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov9740_s_ctrl);
		platform_driver_unregister(&ov9740_platform_driver);
	} else
		i2c_del_driver(&ov9740_i2c_driver);
	return;
}

static int32_t ov9740_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
		s_ctrl->sensor_i2c_client,
		s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
		&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: ov9740 read id failed\n", __func__,
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

static void ov9740_set_stauration(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	int i;
#ifdef CLOSE_FUNCTION
	return;
#endif
	pr_debug("%s %d", __func__, value);
	for(i=0xc0;i<=0xff;i++)
	{
		ov9740_i2c_write(s_ctrl,i,0xff);
	}
	ov9740_i2c_write_table(s_ctrl, &OV9740_reg_saturation[value][0],
		ARRAY_SIZE(OV9740_reg_saturation[value]));
}

static void ov9740_set_contrast(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif
	pr_debug("%s %d", __func__, value);
	ov9740_i2c_write_table(s_ctrl, &OV9740_reg_contrast[value][0],
		ARRAY_SIZE(OV9740_reg_contrast[value]));
}

static void ov9740_set_sharpness(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	int val;
#ifdef CLOSE_FUNCTION
		return;
#endif

	val = value / 6;
	pr_debug("%s %d", __func__, value);
	ov9740_i2c_write_table(s_ctrl, &OV9740_reg_sharpness[val][0],
		ARRAY_SIZE(OV9740_reg_sharpness[val]));
}


static void ov9740_set_iso(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif
	pr_debug("%s %d", __func__, value);
	ov9740_i2c_write_table(s_ctrl, &OV9740_reg_iso[value][0],
		ARRAY_SIZE(OV9740_reg_iso[value]));
}

static void ov9740_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	int val;
#ifdef CLOSE_FUNCTION
		return;
#endif
 	val = (value + 12) / 6;
	pr_debug("%s %d", __func__, val);
	ov9740_i2c_write_table(s_ctrl, &OV9740_reg_exposure_compensation[val][0],
		ARRAY_SIZE(OV9740_reg_exposure_compensation[val]));
}

static void ov9740_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	uint16_t reg0x37;
	uint16_t reg0x7e;
	uint16_t reg0x45;
	uint16_t reg0x47;
	uint16_t reg0x36;
	reg0x37 = ov9740_i2c_read(s_ctrl, 0x37);
	reg0x7e = ov9740_i2c_read(s_ctrl, 0x7e);
	reg0x45 = ov9740_i2c_read(s_ctrl, 0x45);
	reg0x47 = ov9740_i2c_read(s_ctrl, 0x47);
	reg0x36 = ov9740_i2c_read(s_ctrl, 0x36);

	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_EFFECT_MODE_OFF: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_effect_normal[0],
			ARRAY_SIZE(OV9740_reg_effect_normal));
		ov9740_i2c_write(s_ctrl,0x37, reg0x37 & 0xef);
		ov9740_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		ov9740_i2c_write(s_ctrl,0x45, reg0x45 & 0x7f);
		ov9740_i2c_write(s_ctrl,0x47, reg0x47 & 0xfa);
		ov9740_i2c_write(s_ctrl,0x36, reg0x36 & 0x7f);
		ov9740_i2c_write(s_ctrl,0x7b, 0x00);
		ov9740_i2c_write(s_ctrl,0x7c, 0x00);
		ov9740_i2c_write(s_ctrl,0x79, 0x00);
		ov9740_i2c_write(s_ctrl,0x7a, 0x00);
		ov9740_i2c_write(s_ctrl,0x7d, 0x00);
		ov9740_i2c_write(s_ctrl,0x7f, 0x80);

		
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_MONO: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_effect_black_white[0],
			ARRAY_SIZE(OV9740_reg_effect_black_white));
		ov9740_i2c_write(s_ctrl,0x37, reg0x37 & 0xef);
		ov9740_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		ov9740_i2c_write(s_ctrl,0x7e, reg0x7e | 0x08);
		ov9740_i2c_write(s_ctrl,0x45, reg0x45 & 0x7f);
		ov9740_i2c_write(s_ctrl,0x47, reg0x47 & 0xfa);
		ov9740_i2c_write(s_ctrl,0x36, reg0x36 & 0x7f);
		ov9740_i2c_write(s_ctrl,0x7b, 0x00);
		ov9740_i2c_write(s_ctrl,0x7c, 0x00);
		ov9740_i2c_write(s_ctrl,0x7d, 0x20);
		ov9740_i2c_write(s_ctrl,0x79, 0xff);
		ov9740_i2c_write(s_ctrl,0x7a, 0xff);
		ov9740_i2c_write(s_ctrl,0x7f, 0xb0);

		
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_NEGATIVE: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_effect_negative[0],
			ARRAY_SIZE(OV9740_reg_effect_negative));
		ov9740_i2c_write(s_ctrl,0x37, reg0x37 | 0x10);
		ov9740_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		ov9740_i2c_write(s_ctrl,0x45, reg0x45 | 0x80);
		ov9740_i2c_write(s_ctrl,0x47, reg0x47 | 0x05);
		ov9740_i2c_write(s_ctrl,0x36, reg0x36 | 0x80);
		ov9740_i2c_write(s_ctrl,0x7b, 0x00);
		ov9740_i2c_write(s_ctrl,0x7c, 0x00);
		ov9740_i2c_write(s_ctrl,0x79, 0x00);
		ov9740_i2c_write(s_ctrl,0x7a, 0x00);
		ov9740_i2c_write(s_ctrl,0x7d, 0x00);
		ov9740_i2c_write(s_ctrl,0x7f, 0x80);

		
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_SEPIA: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_effect_old_movie[0],
			ARRAY_SIZE(OV9740_reg_effect_old_movie));
		ov9740_i2c_write(s_ctrl,0x37, reg0x37 & 0xef);
		ov9740_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		ov9740_i2c_write(s_ctrl,0x7e, reg0x7e | 0x01);
		ov9740_i2c_write(s_ctrl,0x45, reg0x45 & 0x7f);
		ov9740_i2c_write(s_ctrl,0x47, reg0x47 & 0xfa);
		ov9740_i2c_write(s_ctrl,0x36, reg0x36 & 0x7f);
		ov9740_i2c_write(s_ctrl,0x7b, 0x20);
		ov9740_i2c_write(s_ctrl,0x7c, 0x20);
		ov9740_i2c_write(s_ctrl,0x79, 0x00);
		ov9740_i2c_write(s_ctrl,0x7a, 0x00);
		ov9740_i2c_write(s_ctrl,0x7f, 0x80);

		
		break;
	}
	case MSM_CAMERA_EFFECT_MODE_SOLARIZE: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_effect_solarize[0],
			ARRAY_SIZE(OV9740_reg_effect_solarize));

		
		break;
	}
	default:
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_effect_normal[0],
			ARRAY_SIZE(OV9740_reg_effect_normal));
		ov9740_i2c_write(s_ctrl,0x37, reg0x37 & 0xef);
		ov9740_i2c_write(s_ctrl,0x7e, (reg0x7e &= 0xf4));
		ov9740_i2c_write(s_ctrl,0x45, reg0x45 & 0x7f);
		ov9740_i2c_write(s_ctrl,0x47, reg0x47 & 0xfa);
		ov9740_i2c_write(s_ctrl,0x36, reg0x36 & 0x7f);
		ov9740_i2c_write(s_ctrl,0x7b, 0x00);
		ov9740_i2c_write(s_ctrl,0x7c, 0x00);
		ov9740_i2c_write(s_ctrl,0x79, 0x00);
		ov9740_i2c_write(s_ctrl,0x7a, 0x00);
		ov9740_i2c_write(s_ctrl,0x7d, 0x00);
		ov9740_i2c_write(s_ctrl,0x7f, 0x80);

		
	}

}

static void ov9740_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif
/*
  uint16_t reg0x3a00;
	uint16_t reg0x3c00;
	uint16_t reg0x3c01;
	reg0x3a00 = ov9740_i2c_read(s_ctrl, 0x3a00);
	reg0x3c00 = ov9740_i2c_read(s_ctrl, 0x3c00);
	reg0x3c01 = ov9740_i2c_read(s_ctrl, 0x3c01);
*/
	pr_debug("%s %d", __func__, value);
	ov9740_i2c_write_table(s_ctrl, &OV9740_reg_antibanding[value][0],
		ARRAY_SIZE(OV9740_reg_antibanding[value]));
/*		
		switch (value) {
	  case MSM_CAMERA_BANDING_OFF: {
		  reg0x3a00 = reg0x3a00 & 0xdf ;
		  ov9740_i2c_write(s_ctrl,0x3a00, reg0x3a00);
		break;
		case MSM_CAMERA_BANDING_50HZ: {
		  reg0x3c00 = reg0x3c00 | 0x04 ;
		  ov9740_i2c_write(s_ctrl,0x3c00, reg0x3c00);
		  reg0x3c01 = reg0x3c01 | 0x80 ;
		  ov9740_i2c_write(s_ctrl,0x3c01, reg0x3c01);
		  reg0x3a00 = reg0x3a00 | 0x20 ;
		  ov9740_i2c_write(s_ctrl,0x3a00, reg0x3a00);
		break;
		case MSM_CAMERA_SCENE_MODE_OFF: {
		  reg0x3c00 = reg0x3c00 & 0xfb ;
		  ov9740_i2c_write(s_ctrl,0x3c00, reg0x3c00);
		  reg0x3c01 = reg0x3c01 | 0x80 ;
		  ov9740_i2c_write(s_ctrl,0x3c01, reg0x3c01);
		  reg0x3a00 = reg0x3a00 | 0x20 ;
		  ov9740_i2c_write(s_ctrl,0x3a00, reg0x3a00);
		break;
		default:
			reg0x3c01 = reg0x3c01 & 0x7f ;
		  ov9740_i2c_write(s_ctrl,0x3c01, reg0x3c01);
		  reg0x3a00 = reg0x3a00 | 0x20 ;
		  ov9740_i2c_write(s_ctrl,0x3a00, reg0x3a00);
		  
	}*/
}

static void ov9740_set_scene_mode(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif

	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_SCENE_MODE_OFF: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_scene_auto[0],
			ARRAY_SIZE(OV9740_reg_scene_auto));
		
		ov9740_i2c_write(s_ctrl,0x13, 0x86);
		break;
	}
	case MSM_CAMERA_SCENE_MODE_NIGHT: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_scene_night[0],
			ARRAY_SIZE(OV9740_reg_scene_night));

		
		ov9740_i2c_write(s_ctrl,0xe0, 0x13);
		ov9740_i2c_write(s_ctrl,0xe1, 0x8e);
		
		ov9740_i2c_write(s_ctrl,0x1f, 0x41);
		
		
					break;
	}
	case MSM_CAMERA_SCENE_MODE_LANDSCAPE: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_scene_landscape[0],
			ARRAY_SIZE(OV9740_reg_scene_landscape));
		break;
	}
	case MSM_CAMERA_SCENE_MODE_PORTRAIT: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_scene_portrait[0],
			ARRAY_SIZE(OV9740_reg_scene_portrait));
		break;
	}
	default:
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_scene_auto[0],
			ARRAY_SIZE(OV9740_reg_scene_auto));
	}
}

static void ov9740_set_white_balance_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
#ifdef CLOSE_FUNCTION
		return;
#endif
 
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_WB_MODE_AUTO: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_wb_auto[0],
			ARRAY_SIZE(OV9740_reg_wb_auto));
		break;
	}
	case MSM_CAMERA_WB_MODE_INCANDESCENT: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_wb_home[0],
			ARRAY_SIZE(OV9740_reg_wb_home));
		break;
	}
	case MSM_CAMERA_WB_MODE_DAYLIGHT: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_wb_sunny[0],
			ARRAY_SIZE(OV9740_reg_wb_sunny));
					break;
	}
	case MSM_CAMERA_WB_MODE_FLUORESCENT: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_wb_office[0],
			ARRAY_SIZE(OV9740_reg_wb_office));
					break;
	}
	case MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT: {
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_wb_cloudy[0],
			ARRAY_SIZE(OV9740_reg_wb_cloudy));
					break;
	}
	default:
		ov9740_i2c_write_table(s_ctrl, &OV9740_reg_wb_auto[0],
		ARRAY_SIZE(OV9740_reg_wb_auto));
	}
}

uint16_t  OV9740_shutter_tmp = 0;
#if 0
static void OV9740_BeforeSnapshot(struct msm_sensor_ctrl_t *s_ctrl )
{

		
	uint8_t   	reg0x00, reg0x01, reg0x02, reg0x13, reg0x18, reg0x19, reg0x22, reg0x23, bandstep,i;
	uint8_t   	reg0x36, reg0x05, reg0x06, reg0x07, reg0x08, reg0x09;
	uint16_t  	shutter, bandvalue_pre, bandvalue_cap, Capture_Shutter;
	uint16_t  	VTS;
	// uint16_t  	VTS_new;
	reg0x13 = ov9740_i2c_read(s_ctrl,0x13);  
	ov9740_i2c_write(s_ctrl,0x13, reg0x13 | 0x01);							//disable AEC/AGC 0x13[0] write 1

	reg0x36 = ov9740_i2c_read(s_ctrl, 0x36);  
	ov9740_i2c_write(s_ctrl,0x36, reg0x36 &(~0x01));							//disable AWB 0x36[0] write 0

	reg0x00 				= ov9740_i2c_read(s_ctrl, 0x00);    
	reg0x01 				= ov9740_i2c_read(s_ctrl, 0x01);
	reg0x02 				= ov9740_i2c_read(s_ctrl,0x02);
	shutter 				= ((reg0x02 << 8) | reg0x01);				//read preview exposure time
	OV9740_shutter_tmp = shutter;

	reg0x18 				= ov9740_i2c_read(s_ctrl,0x18);							
	reg0x19 				= ov9740_i2c_read(s_ctrl,0x19);	
	bandvalue_pre 	= ((reg0x19 & 0x03) << 8)| reg0x18; //read preview bandvalue
	reg0x05 				= ov9740_i2c_read(s_ctrl,0x05);
	reg0x06 				= ov9740_i2c_read(s_ctrl,0x06);
	reg0x07 				= ov9740_i2c_read(s_ctrl,0x07);
	reg0x08 				= ov9740_i2c_read(s_ctrl,0x08);
	reg0x09 				= ov9740_i2c_read(s_ctrl,0x09);
	ov9740_i2c_write_table(s_ctrl, &ov9740_uxga_settings[0],
					ARRAY_SIZE(ov9740_uxga_settings));
	for(i=0;i<ARRAY_SIZE(ov9740_uxga_settings);i++)
	{
		CDBG("	OV9740 %x = %x \n", ov9740_uxga_settings[i].reg_addr,ov9740_i2c_read(s_ctrl,ov9740_uxga_settings[i].reg_addr));
	}
	//Sensor_SetMode(sensor_snapshot_mode);	
	//into capture mode
 	msleep(30);
	ov9740_i2c_write(s_ctrl,0x05, reg0x05);
	ov9740_i2c_write(s_ctrl,0x06, reg0x06);
	ov9740_i2c_write(s_ctrl,0x07, reg0x07);
	ov9740_i2c_write(s_ctrl,0x08, reg0x08);
	ov9740_i2c_write(s_ctrl,0x09, reg0x09); 
	bandstep 		= (shutter / bandvalue_pre)* 2;			
	reg0x18 = ov9740_i2c_read(s_ctrl,0x18);
	reg0x19 = ov9740_i2c_read(s_ctrl,0x19);
	bandvalue_cap = ((reg0x19 & 0x03) << 8)| reg0x18 ;	//read capture bandvalue
		
	reg0x22 = ov9740_i2c_read(s_ctrl,0x22);
	reg0x23 = ov9740_i2c_read(s_ctrl,0x23);
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
		ov9740_i2c_write(s_ctrl,0x00, reg0x00 & 0x37);
		Capture_Shutter = Capture_Shutter * 2 ;								
		msleep(50);

	}
	*/
	//	if(Capture_Shutter  <= VTS)
	{
		ov9740_i2c_write(s_ctrl,0x02, (Capture_Shutter >> 8) & 0xff);
		ov9740_i2c_write(s_ctrl,0x01, Capture_Shutter & 0xff);
		msleep(100);
	}
	/*				else	//enter night mode
	{
		VTS_new = Capture_Shutter + 2; 
		ov9740_i2c_write(s_ctrl,0x23, (VTS_new >> 8) & 0xff);
		ov9740_i2c_write(s_ctrl,0x22, VTS_new & 0xff); 
		msleep(50);								
		ov9740_i2c_write(s_ctrl,0x02, (Capture_Shutter >> 8) & 0xff);
		ov9740_i2c_write(s_ctrl,0x01, Capture_Shutter & 0xff);
		msleep(150);
		//SENSOR_PRINT("===========bandvalue_cap=%x, bandvalue_pre=%x, VTS=%x, VTS_new=%x, Capture Shutter=%x.\n" , bandvalue_cap, bandvalue_pre, VTS, VTS_new, Capture_Shutter);
	}
	*/

	}

#endif
int32_t ov9740_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
  * Added for accurate  standby ov9740
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifndef SENSOR_USE_STANDBY
		CDBG("init setting");
		//ov9740_i2c_write(s_ctrl,
		//0x0103, 0x01);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
			s_ctrl->sensor_i2c_client, 0x0103,
			0x01,
			MSM_CAMERA_I2C_BYTE_DATA);
		msleep(10);
		ov9740_i2c_write_table(s_ctrl,
				&ov9740_recommend_settings[0],
				ARRAY_SIZE(ov9740_recommend_settings));


		for(i=0;i<ARRAY_SIZE(ov9740_recommend_settings);i++)
		{
			CDBG("OV9740 %x = %x  \n", ov9740_recommend_settings[i].reg_addr,ov9740_i2c_read(s_ctrl,ov9740_recommend_settings[i].reg_addr));
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
		CDBG("	OV9740 resolution val = %d\n",val);

		if (val == 0)
		{
		ov9740_i2c_write_table(s_ctrl,
		&ov9740_uxga_settings[0],
		ARRAY_SIZE(ov9740_uxga_settings));
		}


		else if (val == 1)
		{
		ov9740_i2c_write_table(s_ctrl, &ov9740_svga_settings[0],
		ARRAY_SIZE(ov9740_svga_settings));
		break;
		}
	}
	case CFG_SET_STOP_STREAM:
		CDBG(" ov9740 stop stream \n ");
		ov9740_i2c_write_table(s_ctrl,
			&ov9740_stop_settings[0],
			ARRAY_SIZE(ov9740_stop_settings));
		break;
	case CFG_SET_START_STREAM:

		CDBG(" ov9740 start stream \n ");

		ov9740_i2c_write_table(s_ctrl,
			&ov9740_start_settings[0],
			ARRAY_SIZE(ov9740_start_settings));
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
		ov9740_set_stauration(s_ctrl, sat_lev);
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
		ov9740_set_contrast(s_ctrl, con_lev);
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
		ov9740_set_sharpness(s_ctrl, shp_lev);
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
		ov9740_set_iso(s_ctrl, iso_lev);
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
		ov9740_set_exposure_compensation(s_ctrl, ec_lev);
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
		ov9740_set_effect(s_ctrl, effect_mode);
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
		ov9740_set_antibanding(s_ctrl, antibanding_mode);
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
		ov9740_set_scene_mode(s_ctrl, bs_mode);
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
		ov9740_set_white_balance_mode(s_ctrl, wb_mode);
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
  * Added for accurate  standby ov9740
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
int32_t ov9740_sensor_init_reg_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int i = 0;
	printk("%s:%s E\n",TAG,__func__);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x0103,0x01,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(10);	
	rc = ov9740_i2c_write_table(s_ctrl,
			&ov9740_recommend_settings[0],
			ARRAY_SIZE(ov9740_recommend_settings));
	if(rc < 0)
		goto error;

	for(i=0;i<ARRAY_SIZE(ov9740_recommend_settings);i++)
		CDBG("AT2250 %x = %x  \n", ov9740_recommend_settings[i].reg_addr,ov9740_i2c_read(s_ctrl,ov9740_recommend_settings[i].reg_addr));

	msleep(100);

error:
	printk("%s:%s E\n",TAG,__func__);
	return rc;		
}
static int32_t ov9740_sensor_power_down_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x0100, 0,
    		MSM_CAMERA_I2C_BYTE_DATA);
	msleep(2);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,0);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
	printk("%s:%s X\n",TAG,__func__);
	return 0;
}
static int32_t ov9740_sensor_power_on_standby(struct msm_sensor_ctrl_t *s_ctrl)
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
static struct msm_sensor_fn_t ov9740_sensor_func_tbl = {
	.sensor_config = ov9740_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = ov9740_sensor_power_down,
	.sensor_match_id = ov9740_sensor_match_id,
};

static struct msm_sensor_ctrl_t ov9740_s_ctrl = {
	.sensor_i2c_client = &ov9740_sensor_i2c_client,
	.power_setting_array.power_setting = ov9740_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov9740_power_setting),
	.msm_sensor_mutex = &ov9740_mut,
	.sensor_v4l2_subdev_info = ov9740_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov9740_subdev_info),
	.func_tbl = &ov9740_sensor_func_tbl,
	.msm_sensor_reg_default_data_type=MSM_CAMERA_I2C_BYTE_DATA,
/*
  * Added for accurate  standby ov9740
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
	.sensor_power_down = ov9740_sensor_power_down_standby,
	.sensor_power_on = ov9740_sensor_power_on_standby,
	.sensor_init_reg = ov9740_sensor_init_reg_standby,
#endif
// <---
};

module_init(ov9740_init_module);
module_exit(ov9740_exit_module);
MODULE_DESCRIPTION("ov9740 2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
