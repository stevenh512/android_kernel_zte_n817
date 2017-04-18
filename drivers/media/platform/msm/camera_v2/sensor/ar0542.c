/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

/*
 * Added for camera sensor by weilanying 20130927
 */

#include "msm_sensor.h"
#include "msm_camera_io_util.h"
#define AR0542_SENSOR_NAME "ar0542"

/*
  * Added log info
  *
  * by ZTE_YCM_20140325 yi.changming
  */
// --->
#define TAG "ZTE_YCM"
// <---

DEFINE_MSM_MUTEX(ar0542_mut);
/*#define CONFIG_MSMB_CAMERA_OTP*/
#undef CDBG
#if 0
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif
#define 	SENSOR_INFO_MODULE_ID_QETCH 0X0001
#define 	SENSOR_INFO_MODULE_ID_SUNNY 0X000a
#define 	SENSOR_INFO_MODULE_ID_MCNEX 0X00a0
#define 	SENSOR_INFO_MODULE_ID_TRULY 0X0010
#define 	SENSOR_INFO_MODULE_ID_SAMSUNG 0X0033
#define 	SENSOR_INFO_MODULE_ID_KARR 0X0002
#if 1 //distinguish actuator maker id, weilanying add
#define 	ACTUTOR_MODULE_ID_JCT 0X01
#endif
//extern struct msm_sensor_module_info_t module_info[2];

/*add for ar0542 eeprom by caidezun 20140326*/
uint8_t ar0542_eeprom_buffer[12] = {0};

struct ar0542_otp_struct {
  uint16_t otp_check_flag;   /* otp_check_flag*/
  uint16_t module_integrator_id;   /* module id */
  uint16_t vcm_maker_info;   /* Bit[15:8]:vcm maker  ||  Bit[7:0]:vcm  number*/
  uint16_t vcm_ic_info;    /* Bit[15:8]:vcm IC maker  ||  Bit[7:0]:vcm IC  number*/
  uint16_t lens_info;       /* Bit[15:8]:lens maker  ||  Bit[7:0]:lens  number*/
  uint16_t LSC_data[106];   /* LSC Data                           */
} st_ar0542_otp = {
  0x0000,                   /* otp_check_flag*/
  0x0000,                   /* module id */
  0x0000,                   /* Bit[15:8]:vcm maker  ||  Bit[7:0]:vcm  number*/
  0x0000,                   /* Bit[15:8]:vcm IC maker  ||  Bit[7:0]:vcm IC  number*/
  0x0000,                   /* Bit[15:8]:lens maker  ||  Bit[7:0]:lens  number*/
  {			 	  /* LSC Data*/
    0x20b0,0x6809,0x72b0,0xf7ad,0xfb31,0x2190,0x9e0d,0x28d0,0x0f2f,0xbfb1,
    0x2090,0xcbca,0x774d,0xc52e,0xc0d0,0x26b0,0x8c2e,0x10f1,0x0c4f,0x9f12,
    0xcb0b,0xabcd,0xc4cd,0x828c,0x0e10,0x8bc9,0x334e,0x4dce,0xd68e,0xc20e,
    0x0a4c,0x294e,0x418e,0xa74f,0xca4f,0x040d,0xa06f,0x162f,0x2750,0xbdf0,
    0x16d1,0x162f,0xe6d3,0x8cd1,0x0ff4,0x2811,0x196f,0xdb73,0xfc11,0x0f14,
    0x3e50,0x1f0f,0xa2b3,0xee4e,0x78d3,0x1891,0x77af,0x8714,0xff11,0x44d4,
    0x76ee,0x83ee,0x6730,0x2391,0x9153,0x4eaf,0xb72e,0x9cb0,0x1c6f,0xecb1,
    0x24ac,0xf90e,0x99ee,0x4130,0xeeaf,0xc16b,0x4b70,0x8e50,0xc571,0x3ff0,
    0xbed2,0xf8d0,0x3513,0x0192,0x7b34,0xcb72,0xebcf,0x4613,0x54b1,0x4a54,
    0xa7b2,0xda90,0x6a53,0x1cee,0x1693,0xc932,0x8a90,0x7ef3,0x0511,0x27b4,
    0x051c,0x03a0,0x206c,0x080c,0x272c,0x6f2b
  }
};
/*******************************************************************************
* otp_type: otp_type should be 0x30,0x31,0x32,0x33
* return value:
*     0, OTPM have no valid data
*     1, OTPM have valid data
*******************************************************************************/
int ar0542_check_otp_status(struct msm_sensor_ctrl_t *s_ctrl, int otp_type)
{
  uint16_t i = 0;
  uint16_t temp = 0;
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x301A,0x0610,
    MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3134,0xCD95,
    MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x304C,(otp_type&0xff)<<8,
    MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x304A,0x0200,
    MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x304A,0x0010,
    MSM_CAMERA_I2C_WORD_DATA);

  do{
    s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x304A,&temp,
      MSM_CAMERA_I2C_WORD_DATA);
    if(0x60 == (temp & 0x60)){
      pr_err("%s: read success\n", __func__);
      break;
    }
    usleep_range(5000, 5100);
    i++;
  }while(i < 10);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3800,&temp,
		MSM_CAMERA_I2C_WORD_DATA);
	pr_err("%s: 0x3800   = 0x%d\n",__func__,temp);

  if(0==temp){
    //OTPM have no valid data
    return 0;
  }else{
    //OTPM have valid data
    return 1;
  }
}
/*
  * camera sensor module compatile
  *
  * by ZTE_YCM_20140509 yi.changming
  */
// --->
#if 0
void ar0542_read_module_id(struct msm_sensor_ctrl_t *s_ctrl)
{

	uint16_t model_id;
	char buf[32];
	model_id = st_ar0542_otp.module_integrator_id;
	switch ( model_id) {
	case SENSOR_INFO_MODULE_ID_QETCH:
		sprintf(buf, "qtech_ar0542");
		break;
	case SENSOR_INFO_MODULE_ID_SUNNY:
		sprintf(buf, "sunny_ar0542");
		break;
	case SENSOR_INFO_MODULE_ID_MCNEX:
		sprintf(buf, "mcnex_ar0542");
		break;
	case SENSOR_INFO_MODULE_ID_TRULY:
		sprintf(buf, "truly_ar0542");
		break;
	case SENSOR_INFO_MODULE_ID_KARR:
		sprintf(buf, "karr_ar0542");
		break;
	default:
		sprintf(buf, "ar0542");
		break;
	}
	pr_err("%s: module_name   = %s\n",__func__,buf);
	memcpy(s_ctrl->module_name, buf, 32);
 
}
#else
void ar0542_read_module_id(struct msm_sensor_ctrl_t *s_ctrl)
{

	uint16_t model_id;
	model_id = st_ar0542_otp.module_integrator_id;
	switch ( model_id) {
	case SENSOR_INFO_MODULE_ID_QETCH:
		#if defined(CONFIG_BOARD_BETTY) || defined(CONFIG_BOARD_GLAUCUS) //A32 and A32_bell are the same chromatix lib
		s_ctrl->module_name = "betty_qtech_ar0542";
		s_ctrl->sensordata->module_name = "betty_qtech_ar0542";
		#else
		s_ctrl->module_name = "qtech_ar0542";
		s_ctrl->sensordata->module_name = "qtech_ar0542";
		#endif
		break;
	case SENSOR_INFO_MODULE_ID_SUNNY:
		s_ctrl->module_name = "sunny_ar0542";
		s_ctrl->sensordata->module_name = "sunny_ar0542";
		break;
	case SENSOR_INFO_MODULE_ID_MCNEX:
		#if defined (CONFIG_BOARD_BETTY)|| defined(CONFIG_BOARD_GLAUCUS) //A32 and A32_bell are the same chromatix lib
		s_ctrl->module_name = "betty_mcnex_ar0542";
		s_ctrl->sensordata->module_name = "betty_mcnex_ar0542";
		#else
		s_ctrl->module_name = "mcnex_ar0542";
		s_ctrl->sensordata->module_name = "mcnex_ar0542";
		#endif
		break;
	case SENSOR_INFO_MODULE_ID_TRULY:
		s_ctrl->module_name = "truly_ar0542";
		s_ctrl->sensordata->module_name = "truly_ar0542";
		break;
	case SENSOR_INFO_MODULE_ID_SAMSUNG:
		s_ctrl->module_name = "samsung_ar0542";
		s_ctrl->sensordata->module_name = "samsung_ar0542";
		break;
	case SENSOR_INFO_MODULE_ID_KARR:
		s_ctrl->module_name = "karr_ar0542";
		s_ctrl->sensordata->module_name = "karr_ar0542";
		break;	
	default:
		s_ctrl->module_name = "qtech_ar0542";
		s_ctrl->sensordata->module_name = "qtech_ar0542";
		break;
	}
	pr_err("%s: module_name(project command)   = %s \n",__func__,s_ctrl->module_name);
	pr_err("%s: module_name(relate lib_name)   = %s \n",__func__,s_ctrl->sensordata->module_name);
}
#endif
// <---
/*******************************************************************************
* return value:
*     0, OTPM have no valid data
*     1, OTPM have valid data
*******************************************************************************/
int ar0542_read_otp(struct msm_sensor_ctrl_t *s_ctrl, struct ar0542_otp_struct *p_otp)
{
  uint16_t i = 0;
  int otp_status = 0;//initial OTPM have no valid data status

/*
  * modified for otp read sequence by caidezun 20140410
  * read from the second block to first block
  *
  */
	if(ar0542_check_otp_status(s_ctrl,0x31)){
		
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x3802,
			 &p_otp->module_integrator_id, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x3804,
			 &p_otp->vcm_maker_info, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x3806,
			 &p_otp->vcm_ic_info, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x3808,
			 &p_otp->lens_info, MSM_CAMERA_I2C_WORD_DATA);

		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(s_ctrl->sensor_i2c_client, 0x3802,
			&ar0542_eeprom_buffer[0], 8);

	}else if(ar0542_check_otp_status(s_ctrl,0x30)){

		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x3802,
			 &p_otp->module_integrator_id, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x3804,
			 &p_otp->vcm_maker_info, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x3806,
			 &p_otp->vcm_ic_info, MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x3808,
			 &p_otp->lens_info, MSM_CAMERA_I2C_WORD_DATA);

		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(s_ctrl->sensor_i2c_client, 0x3802,
			&ar0542_eeprom_buffer[0], 8);

    }

	if(ar0542_check_otp_status(s_ctrl,0x33)){

		for (i = 0; i < 106; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, (0x3802+i*2),
		&p_otp->LSC_data[i], MSM_CAMERA_I2C_WORD_DATA);
  }

		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(s_ctrl->sensor_i2c_client, 0x38d6,
			&ar0542_eeprom_buffer[8], 4);

	}else if(ar0542_check_otp_status(s_ctrl,0x32)){

		for (i = 0; i < 106; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, (0x3802+i*2),
		&p_otp->LSC_data[i], MSM_CAMERA_I2C_WORD_DATA);
	  }

		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(s_ctrl->sensor_i2c_client, 0x38d6,
			&ar0542_eeprom_buffer[8], 4);

	}

	CDBG("%s: otp_check_flag   = 0x%04x\n",__func__,p_otp->otp_check_flag);
	CDBG("%s: module id  = 0x%04x\n",__func__,p_otp->module_integrator_id);
	CDBG("%s: vcm_maker_info   =0x%04x\n",__func__,p_otp->vcm_maker_info);
	CDBG("%s: vcm_ic_info   = 0x%04x\n",__func__,p_otp->vcm_ic_info);
	CDBG("%s: lens_info       = 0x%04x\n",__func__,p_otp->lens_info);

	for (i = 0; i < 106; i++) {
		CDBG("%s: LSC_data[%d]     = 0x%04x \n",__func__,i,p_otp->LSC_data[i]);
    }

  return otp_status;
}

void ar0542_write_otp(struct msm_sensor_ctrl_t *s_ctrl, struct ar0542_otp_struct *p_otp)
{
  uint16_t i = 0;
  uint16_t temp = 0;
	CDBG("%s:E \n",__func__);
  //0 - 20 words
  for (i = 0; i < 20; i++) {
    s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, (0x3600+i*2),
      p_otp->LSC_data[i], MSM_CAMERA_I2C_WORD_DATA);
  }
  //20 - 40 words
  for (i = 0; i < 20; i++) {
    s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, (0x3640+i*2),
      p_otp->LSC_data[20+i], MSM_CAMERA_I2C_WORD_DATA);
  }
  //40 - 60 words
  for (i = 0; i < 20; i++) {
    s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, (0x3680+i*2),
      p_otp->LSC_data[40+i], MSM_CAMERA_I2C_WORD_DATA);
  }
  //60 - 80 words
  for (i = 0; i < 20; i++) {
    s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, (0x36c0+i*2),
      p_otp->LSC_data[60+i], MSM_CAMERA_I2C_WORD_DATA);
  }
  //80 - 100 words
  for (i = 0; i < 20; i++) {
    s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, (0x3700+i*2),
      p_otp->LSC_data[80+i], MSM_CAMERA_I2C_WORD_DATA);
  }

  //last 6 words
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3782,
    p_otp->LSC_data[100], MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3784,
    p_otp->LSC_data[101], MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x37C0,
    p_otp->LSC_data[102], MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x37C2,
    p_otp->LSC_data[103], MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x37C4,
    p_otp->LSC_data[104], MSM_CAMERA_I2C_WORD_DATA);
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x37C6,
    p_otp->LSC_data[105], MSM_CAMERA_I2C_WORD_DATA);

  //enable poly_sc_enable Bit
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,0x3780,&temp,
    MSM_CAMERA_I2C_WORD_DATA);
  temp = temp | 0x8000;
  s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x3780,temp,
    MSM_CAMERA_I2C_WORD_DATA);
}


void ar0542_load_otp_parm(struct msm_sensor_ctrl_t *s_ctrl)
{
  ar0542_read_otp(s_ctrl, &st_ar0542_otp);
	ar0542_read_module_id(s_ctrl);
}

void  ar0542_msm_sensor_otp_func(struct msm_sensor_ctrl_t *s_ctrl)
{
	ar0542_write_otp(s_ctrl, &st_ar0542_otp);
}


static struct msm_sensor_ctrl_t ar0542_s_ctrl;

static struct msm_sensor_power_setting ar0542_power_setting[] = {
#if 0
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
		{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 0,
	},
		{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
#endif
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
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
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.sleep_val = GPIO_OUT_LOW,
		.delay = 1,
	},
		{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_LOW,
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
		.delay = 4,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VAF,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_LOW,
		.delay = 0,
	},

	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

/*
function: get_actuator_cam_name, distinguish actuator maker id, weilanying add
vcm_ic , 0x01 -->  dw9714 IC;
vcm_maker_id, 0x01-->sikao; 0x10-->mitsumi; ox11-->jct;

*/

 int32_t get_actuator_cam_name( int *cam_name)
{
	int8_t vcm_maker_id;
	int8_t vcm_ic_id;
	char buf[32];
	vcm_ic_id = st_ar0542_otp.vcm_ic_info & 0xff;
	vcm_maker_id = (st_ar0542_otp.vcm_maker_info>>8) & 0xff;//vcm_maker_info>>8 means vcm_maker_id
	pr_err("wly: %s:  actuator_name=%s, cam_name=%d, vcm_maker_id=%d\n",__func__,buf, *cam_name, vcm_maker_id);
	if (0x01== vcm_ic_id){//dw9714 ic
		switch(vcm_maker_id){
		case ACTUTOR_MODULE_ID_JCT:
			sprintf(buf, "dw9714_jct");
			*cam_name = 1;
			break;
		default:
			sprintf(buf, "dw9714");
			*cam_name = 1;
			break;	
		}
		pr_err("wly: %s:  actuator_name=%s, cam_name=%d, vcm_maker_id=%d\n",__func__,buf, *cam_name, vcm_maker_id);
	}
	return 0;
}


static int32_t ar0542_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
/*
  * Added log info
  *
  * by ZTE_YCM_20140325 yi.changming
  */
// --->
	printk("%s:%s E\n",TAG,__func__);
 	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x0100, 0,
    	MSM_CAMERA_I2C_BYTE_DATA);

//	msleep(2);  //modify no preview   by ZTE_YCM_20140402 yi.changming    before = 1
	usleep_range(2 * 1000,(2 * 1000) + 1000);

	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_LOW);
	
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET],
				GPIO_OUT_HIGH);
//	msleep(1);
	usleep_range(1 * 1000,(1 * 1000) + 1000);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_VAF],
				GPIO_OUT_LOW);

	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,0);
/*
  * Added log info
  *
  * by ZTE_YCM_20140325 yi.changming
  */
// --->
	printk("%s:%s X\n",TAG,__func__);
	return 0;
}
static int32_t ar0542_sensor_power_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
/*
  * Added log info
  *
  * by ZTE_YCM_20140325 yi.changming
  */
// --->
	printk("%s:%s E\n",TAG,__func__);
// <---
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,1);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET],
				GPIO_OUT_LOW);

//	msleep(2);  //modify no preview   by ZTE_YCM_20140402 yi.changming    before = 1
	usleep_range(2 * 1000,(2 * 1000) + 1000);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);

//	msleep(2);  //modify no preview   by ZTE_YCM_20140402 yi.changming    before = 1
	usleep_range(1 * 1000,(1 * 1000) + 1000);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET],
				GPIO_OUT_HIGH);
/*
  * modify no preview
  *
  * by ZTE_YCM_20140402 yi.changming
  */
// --->	
#if 0
	msleep(1);
	 s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x0100, 1,
    	MSM_CAMERA_I2C_BYTE_DATA);
#else
	msleep(2);  
#endif
	 gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_VAF],
				GPIO_OUT_HIGH);
/*
  * Added log info
  *
  * by ZTE_YCM_20140325 yi.changming
  */
// --->
	 printk("%s:%s X\n",TAG,__func__);
// <---
	return 0;
}
static struct v4l2_subdev_info ar0542_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id ar0542_i2c_id[] = {
	{AR0542_SENSOR_NAME, (kernel_ulong_t)&ar0542_s_ctrl},
	{ }
};

static int32_t msm_ar0542_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ar0542_s_ctrl);
}

static struct i2c_driver ar0542_i2c_driver = {
	.id_table = ar0542_i2c_id,
	.probe  = msm_ar0542_i2c_probe,
	.driver = {
		.name = AR0542_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ar0542_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ar0542_dt_match[] = {
	{.compatible = "qcom,ar0542", .data = &ar0542_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ar0542_dt_match);

static struct platform_driver ar0542_platform_driver = {
	.driver = {
		.name = "qcom,ar0542",
		.owner = THIS_MODULE,
		.of_match_table = ar0542_dt_match,
	},
};

static int32_t ar0542_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(ar0542_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init ar0542_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&ar0542_platform_driver,
		ar0542_platform_probe);
	if (!rc)
		return rc;
	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver_async(&ar0542_i2c_driver);
}

static void __exit ar0542_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ar0542_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ar0542_s_ctrl);
		platform_driver_unregister(&ar0542_platform_driver);
	} else
		i2c_del_driver(&ar0542_i2c_driver);
	return;
}
static struct msm_sensor_fn_t ar0542_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};
static struct msm_sensor_ctrl_t ar0542_s_ctrl = {
	.sensor_i2c_client = &ar0542_sensor_i2c_client,
	.power_setting_array.power_setting = ar0542_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ar0542_power_setting),
	.msm_sensor_mutex = &ar0542_mut,
	.sensor_v4l2_subdev_info = ar0542_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ar0542_subdev_info),
	//Added by weilanying on 20131024 begin
	.sensor_otp_func = ar0542_msm_sensor_otp_func,
	.load_otp_parm =ar0542_load_otp_parm,
	//Added by weilanying on 20131024 end
	.msm_sensor_reg_default_data_type=MSM_CAMERA_I2C_WORD_DATA,
	.sensor_power_down = ar0542_sensor_power_down,
	.sensor_power_on = ar0542_sensor_power_on,
	.func_tbl = &ar0542_sensor_func_tbl,
};

module_init(ar0542_init_module);
module_exit(ar0542_exit_module);
MODULE_DESCRIPTION("ar0542");
MODULE_LICENSE("GPL v2");
