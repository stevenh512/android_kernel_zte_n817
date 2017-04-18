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
#define OV8835_SENSOR_NAME "ov8835"
DEFINE_MSM_MUTEX(ov8835_mut);
/*
  * Added for accurate  standby ov8835
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#include "msm_camera_io_util.h"
#define SENSOR_USE_STANDBY
#define TAG "ZTE_YCM"
// <---
#undef CDBG
#if 0
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif
#define 	OV8835_SENSOR_INFO_MODULE_ID_SUNNY 0X01
#define 	OV8835_SENSOR_INFO_MODULE_ID_TRULY 0X02
#define 	OV8835_SENSOR_INFO_MODULE_ID_QETCH 0X06
#define 	OV8835_SENSOR_INFO_MODULE_ID_LITEON 0X15
#define 	OV8835_SENSOR_INFO_MODULE_ID_MCNEX 0X31

struct ov8835_otp_struct {
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int user_data[5];
	int lenc[62];
	int VCM_start;
	int VCM_end;
	uint32_t final_R_gain;
	uint32_t final_G_gain;
	uint32_t final_B_gain;
	int wbwritten;
	int lencwritten;
};
struct ov8835_otp_struct st_ov8835_otp;
static struct msm_sensor_ctrl_t ov8835_s_ctrl;

static struct msm_sensor_power_setting ov8835_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
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
		.seq_val = SENSOR_GPIO_VDIG,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 2,
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
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VAF,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
};

static struct v4l2_subdev_info ov8835_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id ov8835_i2c_id[] = {
	{OV8835_SENSOR_NAME, (kernel_ulong_t)&ov8835_s_ctrl},
	{ }
};
#if 1 //weilanying add actuator calibration on 20140714
void get_otp_af_infor(int * vcm_start, int *vcm_end)
{
	if(st_ov8835_otp.VCM_start <= st_ov8835_otp.VCM_end){
		*vcm_start = st_ov8835_otp.VCM_start;
		*vcm_end = st_ov8835_otp.VCM_end;
	}else if (st_ov8835_otp.VCM_start > st_ov8835_otp.VCM_end)
	{
		*vcm_start = st_ov8835_otp.VCM_end;
		*vcm_end = st_ov8835_otp.VCM_start;
	}
}
#endif
uint16_t read_sccb16(uint16_t address, struct msm_sensor_ctrl_t *s_ctrl) 
{ 
	uint16_t temp, flag; 

	temp= s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read( 
	                        s_ctrl->sensor_i2c_client, 
	                        address, &flag, 
	                        MSM_CAMERA_I2C_BYTE_DATA); 
	return flag; 
} 


uint16_t write_sccb16(uint16_t address, uint16_t value, struct msm_sensor_ctrl_t *s_ctrl) 
{ 
	int rc=0; 
	 
	rc= s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write( 
	                        s_ctrl->sensor_i2c_client, 
	                        address, value, 
	                        MSM_CAMERA_I2C_BYTE_DATA); 
	return rc; 
}      

void clear_otp_buffer(struct msm_sensor_ctrl_t *s_ctrl) 
{ 
        int i; 
        // clear otp buffer 
        for (i=0;i<16;i++) { 
        write_sccb16(0x3d00 + i, 0x00,s_ctrl); 
        } 
} 

// index: index of otp group. ( 1, 2,3) 
// return: 0, group index is empty 
// 1, group index has invalid data 
// 2, group index has valid data 
int check_otp_wb(uint index, struct msm_sensor_ctrl_t *s_ctrl) 
{ 
        uint  flag; 
        int bank, address; 
        // select bank index 
        bank = 0xc0 | index; 
        write_sccb16(0x3d84, bank,s_ctrl); 
        // read otp into buffer 
        write_sccb16(0x3d81, 0x01,s_ctrl); 
        msleep(5); 
        // read flag 
        address = 0x3d00; 
        flag = read_sccb16(address,s_ctrl); 
	pr_err("otpwb %s  flag=0x%x",__func__,flag); 

        flag = flag & 0xc0; 
        // disable otp read 
        write_sccb16(0x3d81, 0x00,s_ctrl); 
        // clear otp buffer 
        clear_otp_buffer(s_ctrl); 
        if (flag==0) { 
        	return 0; 
        } 
        else if (flag & 0x80) { 
        	return 1; 
        } 
        else { 
        	return 2; 
        } 
 
}

// index: index of otp group. ( 1, 2,3) 
// return: 0, 
int read_otp_wb(uint index, struct ov8835_otp_struct *otp_ptr, struct msm_sensor_ctrl_t *s_ctrl) 
{ 
	int  bank,address,temp; 

	// select bank index 
	bank = 0xc0 | index; 
	write_sccb16(0x3d84, bank,s_ctrl); 
	// read otp into buffer 
	write_sccb16(0x3d81, 0x01,s_ctrl); 
	msleep(5); 
	address = 0x3d00; 
	otp_ptr->module_integrator_id = read_sccb16(address + 1,s_ctrl); 
	otp_ptr->lens_id = read_sccb16(address + 2,s_ctrl); 
	otp_ptr->production_year = read_sccb16(address + 3,s_ctrl);
	otp_ptr->production_month = read_sccb16(address + 4,s_ctrl);
	otp_ptr->production_day = read_sccb16(address + 5,s_ctrl);

	temp=read_sccb16(address + 10,s_ctrl); 

	otp_ptr->rg_ratio = (read_sccb16(address + 6,s_ctrl)<<2)+((temp>>6)&0x03); 
	otp_ptr->bg_ratio =(read_sccb16(address + 7,s_ctrl)<<2)+((temp>>4)&0x03); 
	otp_ptr->light_rg = (read_sccb16(address + 8,s_ctrl)<<2)+((temp>>2)&0x03); 
	otp_ptr->light_bg = (read_sccb16(address + 9,s_ctrl)<<2)+(temp&0x03); 

	otp_ptr->user_data[0] = read_sccb16(address + 11,s_ctrl); 
	otp_ptr->user_data[1] = read_sccb16(address + 12,s_ctrl); 
	otp_ptr->user_data[2] = read_sccb16(address + 13,s_ctrl); 
	otp_ptr->user_data[3] = read_sccb16(address + 14,s_ctrl); 
	otp_ptr->user_data[4] = read_sccb16(address + 15,s_ctrl); 
	// disable otp read 
	write_sccb16(0x3d81, 0x00,s_ctrl); 
	// clear otp buffer 
	clear_otp_buffer(s_ctrl); 

	return 0; 
}

// index: index of otp group. ( 1, 2,3) 
// return: 0, 
int load_otp_wb(struct msm_sensor_ctrl_t *s_ctrl,struct ov8835_otp_struct *p_otp) 
{ 
	uint i, temp, otp_index; 
	uint32_t  R_gain,G_gain,B_gain,G_gain_R, G_gain_B; 
	int rg,bg; 
	uint RG_Ratio_typical=0x102; 
	uint BG_Ratio_typical=0x116;

	// R/G and B/G of current camera module is read out from sensor OTP 
	// check first wb OTP with valid data 
	for(i=1;i<=3;i++) {                             //shawn 20130408
		temp = check_otp_wb(i,s_ctrl); 
		pr_err("otpwb %s  temp =%d  i=%d",__func__,temp,i); 
		if (temp == 2) { 
			otp_index = i; 
			break; 
		} 
	}               
	if (i>3) {                                     //shawn 20130408
		pr_err("otpwb %s  i=%d   no valid wb OTP data ",__func__,i); 
		return 1; 
	} 
	
	read_otp_wb(otp_index, p_otp,s_ctrl); 
	
	if(p_otp->light_rg==0) { 
		// no light source information in OTP, light factor = 1 
		rg = p_otp->rg_ratio; 
	} else { 
		rg = p_otp->rg_ratio * ((p_otp->light_rg +512) / 1024); 
	} 
	if(p_otp->light_bg==0) { 
	// not light source information in OTP, light factor = 1 
		bg = p_otp->bg_ratio; 
	} else { 
		bg = p_otp->bg_ratio * ((p_otp->light_bg +512) / 1024); 
	} 
 
	if(rg==0||bg==0) return 0; 
 
	//calculate gain 
	//0x400 = 1x gain 
	if(bg < BG_Ratio_typical) 
	{ 
	        if (rg < RG_Ratio_typical){ 
	                // current_ov8835_otp.bg_ratio < BG_Ratio_typical && 
	                // current_ov8835_otp.rg_ratio < RG_Ratio_typical 
	                G_gain = 0x400; 
	                B_gain = 0x400 * BG_Ratio_typical / bg; 
	                R_gain = 0x400 * RG_Ratio_typical / rg; 
	       } else{ 
	                // current_ov8835_otp.bg_ratio < BG_Ratio_typical && 
	                // current_ov8835_otp.rg_ratio >= RG_Ratio_typical 
	                R_gain = 0x400; 
	                G_gain = 0x400 * rg / RG_Ratio_typical; 
	                B_gain = G_gain * BG_Ratio_typical / bg; 
                } 
	} else{ 
        
                if (rg < RG_Ratio_typical){ 
	                // current_ov8835_otp.bg_ratio >= BG_Ratio_typical && 
	                // current_ov8835_otp.rg_ratio < RG_Ratio_typical 
	                B_gain = 0x400; 
	                G_gain = 0x400 * bg / BG_Ratio_typical; 
	                R_gain = G_gain * RG_Ratio_typical / rg; 
                } else{ 
		                // current_ov8835_otp.bg_ratio >= BG_Ratio_typical && 
		                // current_ov8835_otp.rg_ratio >= RG_Ratio_typical 
		                G_gain_B = 0x400 * bg / BG_Ratio_typical; 
		                G_gain_R = 0x400 * rg / RG_Ratio_typical; 
		                if(G_gain_B > G_gain_R ){ 
			                B_gain = 0x400; 
			                G_gain = G_gain_B; 
			                R_gain = G_gain * RG_Ratio_typical / rg; 
		                }else{ 
			                R_gain = 0x400; 
			                G_gain = G_gain_R; 
			                B_gain = G_gain * BG_Ratio_typical / bg; 
		                } 
		} 
	} 

	p_otp->final_R_gain=R_gain;
	p_otp->final_G_gain=G_gain;
	p_otp->final_B_gain=B_gain;
	p_otp->wbwritten=1;

	return 0; 
}

// index: index of otp group. (1, 2, 3) 
// return: 0, group index is empty 
// 1, group index has invalid data 
// 2, group index has valid data 
int check_otp_lenc(uint index, struct msm_sensor_ctrl_t *s_ctrl) 
{ 
        int flag,  bank; 
        int address; 
        // select bank: index*4 =4,8,12
        bank = 0xc0 | (index*4); 
        write_sccb16(0x3d84, bank,s_ctrl); 
        // read otp into buffer 
        write_sccb16(0x3d81, 0x01,s_ctrl); 
        msleep(5); 
        // read flag 
        address = 0x3d00; 
        flag = read_sccb16(address,s_ctrl); 
	pr_err("otplenc %s  flag=0x%x",__func__,flag); 
        flag = flag & 0xc0; 
        // disable otp read 
        write_sccb16(0x3d81, 0x00,s_ctrl); 
        // clear otp buffer 
        clear_otp_buffer(s_ctrl); 
        if (flag==0) { 
        return 0; 
        } 
        else if (flag & 0x80) { 
        return 1; 
        } 
        else { 
        return 2; 
        } 
 
}

// index: index of otp group. (1, 2, 3) 
// return: 0 
int read_otp_lenc(uint index, struct ov8835_otp_struct *otp_ptr, struct msm_sensor_ctrl_t *s_ctrl) 
{ 
	int bank, i; 
	int address; 
	// select bank: index*4 
	bank = 0xc0 | (index*4); 
	write_sccb16(0x3d84, bank,s_ctrl); 
	// read otp into buffer 
	write_sccb16(0x3d81, 0x01,s_ctrl); 
	msleep(5); 
	address = 0x3d01; 
	for(i=0;i<15;i++) { 
		otp_ptr->lenc[i]=read_sccb16(address,s_ctrl); 
		pr_err("%s  lenc[%d]=0x%x",__func__,i,otp_ptr->lenc[i]); 
		address++; 
	} 
	// disable otp read 
	write_sccb16(0x3d81, 0x00,s_ctrl); 
	// clear otp buffer 
	clear_otp_buffer(s_ctrl); 
	// select 2nd bank 
	bank++; 
	write_sccb16(0x3d84, bank ,s_ctrl); 
	// read otp 
	write_sccb16(0x3d81, 0x01,s_ctrl); 
	msleep(5); 
	address = 0x3d00; 
	for(i=15;i<31;i++) { 
		otp_ptr->lenc[i]=read_sccb16(address,s_ctrl); 
		pr_err("%s  lenc[%d]=0x%x",__func__,i,otp_ptr->lenc[i]); 
		address++; 
	} 
	// disable otp read 
	write_sccb16(0x3d81, 0x00,s_ctrl); 
	// clear otp buffer 
	clear_otp_buffer(s_ctrl); 
	// select 3rd bank 
	bank++; 
	write_sccb16(0x3d84, bank ,s_ctrl); 
	// read otp 
	write_sccb16(0x3d81, 0x01,s_ctrl); 
	msleep(5); 
	address = 0x3d00; 
	for(i=31;i<47;i++) { 
		otp_ptr->lenc[i]=read_sccb16(address,s_ctrl); 
		pr_err("%s  lenc[%d]=0x%x",__func__,i,otp_ptr->lenc[i]); 
		address++; 
	} 
	// disable otp read 
	write_sccb16(0x3d81, 0x00,s_ctrl); 
	// clear otp buffer 
	clear_otp_buffer(s_ctrl); 
	// select 4th bank 
	bank++; 
	write_sccb16(0x3d84, bank ,s_ctrl); 
	// read otp 
	write_sccb16(0x3d81, 0x01,s_ctrl); 
	msleep(5); 
	address = 0x3d00; 
	for(i=47;i<62;i++) { 
		otp_ptr->lenc[i]=read_sccb16(address,s_ctrl); 
		pr_err("%s  lenc[%d]=0x%x",__func__,i,otp_ptr->lenc[i]); 
		address++; 
	} 
	// disable otp read 
	write_sccb16(0x3d81, 0x00,s_ctrl); 
	// clear otp buffer 
	clear_otp_buffer(s_ctrl); 
	return 0; 
}

// call this function after OV8820 initialization 
// return value: 0 update success 
// 1, no OTP 
int load_otp_lenc(struct msm_sensor_ctrl_t *s_ctrl,struct ov8835_otp_struct *p_otp) 
{ 
 
	uint i, temp, otp_index; 
 
	// check first lens correction OTP with valid data 
 	for(i=1;i<=3;i++) {         //shawn 20130408
		temp = check_otp_lenc(i,s_ctrl); 
		if (temp == 2) { 
			pr_err("otplenc %s  temp =%d  i=%d",__func__,temp,i); 
			otp_index = i; 
			break; 
		} 
	} 
	if (i>3) {     //shawn 20130408
		// no lens correction data 
		pr_err("otplenc %s  i=%d   no valid lenc OTP data ",__func__,i); 
		return 1; 
	} 
	read_otp_lenc(otp_index, p_otp,s_ctrl); 
	p_otp->lencwritten=1;
	return 0; 
} 

// index: index of otp group. (1, 2, 3) 
//code:0 start code,1 stop code
// return: 0, group index is empty 
// 1, group index has invalid data 
// 2, group index has valid data 
int check_otp_VCM(int index,int code, struct msm_sensor_ctrl_t *a_ctrl) 
{ 
            int flag,  bank; 
            int address; 
            // select bank: 16 
            bank = 0xc0 + 16; 
            write_sccb16(0x3d84, bank,a_ctrl); 
            // read otp into buffer 
            write_sccb16(0x3d81, 0x01,a_ctrl); 
            msleep(5); 
            // read flag 
            address = 0x3d00 + (index-1)*4+code*2; 
            flag = read_sccb16(address,a_ctrl); 
		pr_err("otpVCM %s index =%d , code=%d ,flag=0x%x",__func__,index,code,flag); 
            flag = flag & 0xc0; 
            // disable otp read 
            write_sccb16(0x3d81, 0x00,a_ctrl); 
            // clear otp buffer 
            clear_otp_buffer(a_ctrl); 
            if (flag==0) { 
            	return 0; 
            } 
            else if (flag & 0x80) { 
            	return 1; 
            } 
            else { 
            	return 2; 
            } 
 
}

// index: index of otp group. (1, 2, 3) 
//code:0 start code,1 stop code
// return: 0, 
int read_otp_VCM(int index,int code,struct msm_sensor_ctrl_t *a_ctrl,struct ov8835_otp_struct *p_otp) 
{ 
        int vcm, bank; 
        int address; 
        // select bank: 16 
        bank = 0xc0 + 16; 
        write_sccb16(0x3d84, bank,a_ctrl); 
        // read otp into buffer 
        write_sccb16(0x3d81, 0x01,a_ctrl); 
        msleep(5); 
        // read flag 
        address = 0x3d00 + (index-1)*4+code*2; 
        vcm = read_sccb16(address,a_ctrl); 
        vcm = (vcm & 0x03) + (read_sccb16(address+1,a_ctrl)<<2); 

	  if(code==1){
		p_otp->VCM_end=vcm;
	  }else{
		p_otp->VCM_start=vcm;
	  }
	  
        // disable otp read 
        write_sccb16(0x3d81, 0x00,a_ctrl); 
        // clear otp buffer 
        clear_otp_buffer(a_ctrl); 
        return 0; 
 
}

int load_otp_VCM(struct msm_sensor_ctrl_t *a_ctrl,struct ov8835_otp_struct *p_otp) 
{ 
	int i, code,temp, otp_index; 
	for(code=0;code<2;code++) { 

		for(i=1;i<=3;i++) { 
			temp = check_otp_VCM(i,code,a_ctrl); 
			pr_err("otpaf %s  temp =%d , i=%d,code=%d",__func__,temp,i,code); 
			if (temp == 2) { 
				otp_index = i; 
				break; 
			} 
		} 
		if (i>3) { 
			pr_err("otpaf %s  i=%d   no valid af OTP data ",__func__,i); 
			return 1; 
		} 

		read_otp_VCM(otp_index,code, a_ctrl,p_otp); 

	}
	return 0; 
} 

int32_t ov8835_read_otp(struct msm_sensor_ctrl_t *s_ctrl,struct ov8835_otp_struct *p_otp)
{
       pr_err("%s: %d\n", __func__, __LINE__);

	load_otp_wb(s_ctrl,p_otp);
	
	load_otp_lenc(s_ctrl,p_otp);

	load_otp_VCM(s_ctrl,p_otp);
	
	if(p_otp->module_integrator_id){
		pr_err("%s  ov8835_otp: module id =0x%x\n",__func__,p_otp->module_integrator_id);
		pr_err("%s  ov8835_otp: lens_id =0x%x\n",__func__,p_otp->lens_id);
		pr_err("%s  ov8835_otp: production_year =%d\n",__func__,p_otp->production_year);
		pr_err("%s  ov8835_otp: production_month =%d\n",__func__,p_otp->production_month);
		pr_err("%s  ov8835_otp: production_day =%d\n",__func__,p_otp->production_day);
		pr_err("%s  ov8835_otp: rg_ratio =0x%x\n",__func__,p_otp->rg_ratio);
		pr_err("%s  ov8835_otp: bg_ratio =0x%x\n",__func__,p_otp->bg_ratio);
		pr_err("%s  ov8835_otp: light_rg =0x%x\n",__func__,p_otp->light_rg);
		pr_err("%s  ov8835_otp: light_bg =0x%x\n",__func__,p_otp->light_bg);
		pr_err("%s  ov8835_otp: user_data[0] =0x%x\n",__func__,p_otp->user_data[0]);
		pr_err("%s  ov8835_otp: user_data[1] =0x%x\n",__func__,p_otp->user_data[1]);
		pr_err("%s  ov8835_otp: user_data[2] =0x%x\n",__func__,p_otp->user_data[2]);
		pr_err("%s  ov8835_otp: user_data[3] =0x%x\n",__func__,p_otp->user_data[3]);
		pr_err("%s  ov8835_otp: user_data[4] =0x%x\n",__func__,p_otp->user_data[4]);		
		pr_err("%s  ov8835_otp: VCM_start =0x%x\n",__func__,p_otp->VCM_start);
		pr_err("%s  ov8835_otp: VCM_end =0x%x\n",__func__,p_otp->VCM_end);	
	}else{
		pr_err("%s  no  otp  module \n",__func__);
	}
	
	return 0;
}

void ov8835_read_module_id(struct msm_sensor_ctrl_t *s_ctrl)
{

	uint16_t model_id;
	char buf[32];
	model_id = st_ov8835_otp.module_integrator_id;
	switch (model_id) {
	case OV8835_SENSOR_INFO_MODULE_ID_SUNNY:
		sprintf(buf, "sunny_ov8835");
		break;
	case OV8835_SENSOR_INFO_MODULE_ID_TRULY:
		sprintf(buf, "truly_ov8835");
		break;
	case OV8835_SENSOR_INFO_MODULE_ID_QETCH:
		sprintf(buf, "qtech_ov8835");
		break;
	case OV8835_SENSOR_INFO_MODULE_ID_LITEON:
		sprintf(buf, "liteon_ov8835");
		break;
	case OV8835_SENSOR_INFO_MODULE_ID_MCNEX:
		sprintf(buf, "mcnex_ov8835");
		break;
	default:
		sprintf(buf, "liteon_ov8835");
		break;
	}
	pr_err("%s: module_name   = %s\n",__func__,buf);
	memcpy(s_ctrl->module_name, buf, 32);
 
}

void ov8835_load_otp_parm(struct msm_sensor_ctrl_t *s_ctrl)
{
	 write_sccb16(0x0100, 0x01,s_ctrl); 
	ov8835_read_otp(s_ctrl, &st_ov8835_otp);
	ov8835_read_module_id(s_ctrl);
	write_sccb16(0x100, 0x00,s_ctrl); 
}
 
//R_gain: red gain of sensor AWB, 0x400 = 1 
// G_gain: green gain of sensor AWB, 0x400 = 1 
// B_gain: blue gain of sensor AWB, 0x400 = 1 
// return 0 
int update_awb_gain(uint32_t R_gain, uint32_t G_gain, uint32_t B_gain, struct msm_sensor_ctrl_t *s_ctrl) 
{ 
	CDBG("otpwb %s  R_gain =%x  0x3400=%x",__func__,R_gain,R_gain>>8); 
	CDBG("otpwb %s  R_gain =%x  0x3401=%x",__func__,R_gain,R_gain & 0x00ff); 
	CDBG("otpwb %s  G_gain =%x  0x3402=%x",__func__,G_gain,G_gain>>8); 
	CDBG("otpwb %s  G_gain =%x  0x3403=%x",__func__,G_gain,G_gain & 0x00ff); 
	CDBG("otpwb %s  B_gain =%x  0x3404=%x",__func__,B_gain,B_gain>>8); 
	CDBG("otpwb %s  B_gain =%x  0x3405=%x",__func__,B_gain,B_gain & 0x00ff); 
	 
	if (R_gain>0x400) { 
		write_sccb16(0x3400, R_gain>>8,s_ctrl); 
		write_sccb16(0x3401, R_gain & 0x00ff,s_ctrl); 
	} 
	if (G_gain>0x400) { 
		write_sccb16(0x3402, G_gain>>8,s_ctrl); 
		write_sccb16(0x3403, G_gain & 0x00ff,s_ctrl); 
	} 
	if (B_gain>0x400) { 
		write_sccb16(0x3404, B_gain>>8,s_ctrl); 
		write_sccb16(0x3405, B_gain & 0x00ff,s_ctrl); 
	} 
	return 0; 
}

int update_lenc(struct ov8835_otp_struct *otp_ptr, struct msm_sensor_ctrl_t *s_ctrl) 
{ 
	int i, temp; 
	temp= read_sccb16(0x5000,s_ctrl); 
	temp = 0x80 | temp; 
	write_sccb16(0x5000, temp,s_ctrl); 

	for(i=0;i<62;i++) { 
		write_sccb16(0x5800 + i, otp_ptr->lenc[i],s_ctrl); 
	} 
	return 0; 
} 

// return: 0 ¨Cuse module DCBLC, 
// 1 ¨Cuse sensor DCBLC 
// 2 ¨Cuse defualt DCBLC 
int update_blc_ratio(struct msm_sensor_ctrl_t *a_ctrl) 
{ 
	int K,temp; 

	// select bank 31 
	write_sccb16(0x3d84, 0xdf,a_ctrl); 

	// read otp into buffer 
	write_sccb16(0x3d81, 0x01,a_ctrl); 
	msleep(5); 

	K  = read_sccb16(0x3d0b,a_ctrl); 
	if(K!=0){
		if ((K>=0x15) && (K<=0x40)){ 
			//auto load mode
			pr_err("%s  auto load mode ",__func__);
			temp = read_sccb16(0x4008,a_ctrl); 
			temp &= 0xfb;
			write_sccb16(0x4008, temp,a_ctrl); 

			temp = read_sccb16(0x4000,a_ctrl); 
			temp &= 0xf7;
			write_sccb16(0x4000, temp,a_ctrl); 
			return 2; 
		}
	}

	K  = read_sccb16(0x3d0a,a_ctrl); 
	if ((K>=0x10) && (K<=0x40)){ 
		//manual load mode
		pr_err("%s  manual load mode ",__func__);
		write_sccb16(0x4006, K,a_ctrl); 
		
		temp = read_sccb16(0x4008,a_ctrl); 
		temp &= 0xfb;
		write_sccb16(0x4008, temp,a_ctrl); 

		temp = read_sccb16(0x4000,a_ctrl); 
		temp |= 0x08;
		write_sccb16(0x4000, temp,a_ctrl); 
		return 1; 
	}else{ 
		//set to default
		pr_err("%s  set to default ",__func__);
		write_sccb16(0x4006, 0x20,a_ctrl); 
		
		temp = read_sccb16(0x4008,a_ctrl); 
		temp &= 0xfb;
		write_sccb16(0x4008, temp,a_ctrl); 

		temp = read_sccb16(0x4000,a_ctrl); 
		temp |= 0x08;
		write_sccb16(0x4000, temp,a_ctrl); 
		return 0; 
	}

}

void ov8835_write_otp(struct msm_sensor_ctrl_t *s_ctrl, struct ov8835_otp_struct *p_otp)
{
	if(p_otp->wbwritten){
		pr_err("%s: wb otp is writing\n", __func__);
		update_awb_gain(p_otp->final_R_gain, p_otp->final_G_gain, p_otp->final_B_gain,s_ctrl); 
	}else{
		pr_err("%s: no wb otp \n", __func__);
	}

	if(p_otp->lencwritten){
		pr_err("%s: lenc otp is writing\n", __func__);
		update_lenc(p_otp,s_ctrl); 
	}else{
		pr_err("%s: no lenc otp \n", __func__);
	}

	update_blc_ratio(s_ctrl);

}

void  ov8835_msm_sensor_otp_func(struct msm_sensor_ctrl_t *s_ctrl)
{
	ov8835_write_otp(s_ctrl, &st_ov8835_otp);
}
static int32_t msm_ov8835_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov8835_s_ctrl);
}

static struct i2c_driver ov8835_i2c_driver = {
	.id_table = ov8835_i2c_id,
	.probe  = msm_ov8835_i2c_probe,
	.driver = {
		.name = OV8835_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov8835_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov8835_dt_match[] = {
	{.compatible = "qcom,ov8835", .data = &ov8835_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov8835_dt_match);

static struct platform_driver ov8835_platform_driver = {
	.driver = {
		.name = "qcom,ov8835",
		.owner = THIS_MODULE,
		.of_match_table = ov8835_dt_match,
	},
};

static int32_t ov8835_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(ov8835_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init ov8835_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&ov8835_platform_driver,
		ov8835_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver_async(&ov8835_i2c_driver);
}

static void __exit ov8835_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ov8835_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov8835_s_ctrl);
		platform_driver_unregister(&ov8835_platform_driver);
	} else
		i2c_del_driver(&ov8835_i2c_driver);
	return;
}
/*
  * Added for accurate  standby ov8835
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
static struct msm_camera_i2c_reg_conf  init_reg_array[] = { 
{0x0100, 0x00},  
{0x0102, 0x01},  
{0x3001, 0x2a},  
{0x3002, 0x88},  
{0x3005, 0x00},  
{0x3011, 0x21}, 
{0x3015, 0xc8}, 
{0x301b, 0xb4},  
{0x301d, 0x02},  
{0x3021, 0x00},  
{0x3022, 0x02},  
{0x3081, 0x02},  
{0x3083, 0x01},  
{0x3090, 0x03},  
{0x3091, 0x23},  
{0x3094, 0x00},  
{0x3092, 0x01},  
{0x3093, 0x00},  
{0x3098, 0x04},  
{0x3099, 0x10},  
{0x309a, 0x00},  
{0x309b, 0x00},  
{0x30a2, 0x01},  
{0x30b0, 0x05},  
{0x30b2, 0x00},  
{0x30b3, 0x64},  
{0x30b4, 0x03},  
{0x30b5, 0x04},  
{0x30b6, 0x01},  
{0x3104, 0xa1},  
{0x3106, 0x01},  
{0x3400, 0x04},  
{0x3401, 0x00},  
{0x3402, 0x04},  
{0x3403, 0x00},  
{0x3404, 0x04},  
{0x3405, 0x00},  
{0x3406, 0x01},  
{0x3500, 0x00},  
{0x3501, 0x50},  
{0x3502, 0x00},  
{0x3503, 0x07},  
{0x3504, 0x00},  
{0x3505, 0x30},  
{0x3506, 0x00},  
{0x3507, 0x08},  
{0x3508, 0x80},  
{0x3509, 0x10},  
{0x350a, 0x00},  
{0x350b, 0x38},  
{0x3600, 0x98},  
{0x3601, 0x02},  
{0x3602, 0x7c},  
{0x3604, 0x38},  
{0x3612, 0x80},  
{0x3620, 0x41},  
{0x3621, 0xa4},  
{0x3622, 0x0f},  
{0x3625, 0x44},  
{0x3630, 0x55},  
{0x3631, 0xf2},  
{0x3632, 0x00},  
{0x3633, 0x34},  
{0x3634, 0x03},  
{0x364d, 0x0d},  
{0x364f, 0xcf},   //{0x364f, 0x60},  
{0x3660, 0x80},  
{0x3662, 0x10},  
{0x3665, 0x00},  
{0x3666, 0x00},  
{0x3667, 0x00},  
{0x366a, 0x80},  
{0x366c, 0x00},  
{0x366d, 0x00},  
{0x366e, 0x00},  
{0x366f, 0x20},  
{0x3680, 0xe5},  //{0x3680, 0xb5},
{0x3681, 0x00},  
{0x3701, 0x14},  
{0x3702, 0x50},  
{0x3703, 0x8c},  
{0x3704, 0x78},  //{0x3704, 0x68},
{0x3705, 0x02},  
{0x3708, 0xe6},  
{0x3709, 0x43},  
{0x370a, 0x00},  
{0x370b, 0x20},  
{0x370c, 0x0c},  
{0x370d, 0x11},  
{0x370e, 0x00},  
{0x370f, 0x00},  
{0x3710, 0x00},  
{0x371c, 0x01},  
{0x371f, 0x0c},  
{0x3721, 0x00},  
{0x3724, 0x10},  
{0x3726, 0x00},  
{0x372a, 0x01},  
{0x3730, 0x18},  
{0x3738, 0x22},  
{0x3739, 0x08},  //{0x3739, 0xd0},
{0x373a, 0x51},  //{0x373a, 0x50}
{0x373b, 0x02},  
{0x373c, 0x20},  
{0x373f, 0x02},  
{0x3740, 0x42},  
{0x3741, 0x02},  
{0x3742, 0x18},  
{0x3743, 0x01},  
{0x3744, 0x02},  
{0x3747, 0x10},  
{0x374c, 0x04},  
{0x3751, 0xf0},  
{0x3752, 0x00},  
{0x3753, 0x00},  
{0x3754, 0xc0},  
{0x3755, 0x00},  
{0x3756, 0x1a},  
{0x3758, 0x00},  
{0x3759, 0x0f},  
{0x375c, 0x04},  
{0x3767, 0x01},  
{0x376b, 0x44},  
{0x3774, 0x10},  
{0x3776, 0x00},  
{0x377f, 0x08},  
{0x3780, 0x22},  
{0x3781, 0x0c},  //{0x3781, 0xcc},
{0x3784, 0x2c},  
{0x3785, 0x1e},  //{0x3785, 0x08},
{0x3786, 0x16},  
{0x378f, 0xf5},  
{0x3791, 0xb0},  
{0x3795, 0x00},  
{0x3796, 0x64},  //{0x3796, 0x94}, 
{0x3797, 0x11},  
{0x3798, 0x30},  
{0x3799, 0x41},  
{0x379a, 0x07},  
{0x379b, 0xb0},  
{0x379c, 0x0c},  
{0x37c5, 0x00},  
{0x37c6, 0xa0},  
{0x37c7, 0x00},  
{0x37c9, 0x00},  
{0x37ca, 0x00},  
{0x37cb, 0x00},  
{0x37cc, 0x00},  
{0x37cd, 0x00},  
{0x37ce, 0x01},  
{0x37cf, 0x00},  
{0x37d1, 0x01},  
{0x37de, 0x00},  
{0x37df, 0x00},  
{0x3800, 0x00},  
{0x3801, 0x08},  
{0x3802, 0x00},  
{0x3803, 0x08},  
{0x3804, 0x0c},  
{0x3805, 0xd7},  
{0x3806, 0x09},  
{0x3807, 0xa7},  
{0x3808, 0x06},  
{0x3809, 0x60},  
{0x380a, 0x04},  
{0x380b, 0xc8},  
{0x380c, 0x0d},  
{0x380d, 0xbc},  
{0x380e, 0x04},  
{0x380f, 0xf0},  
{0x3810, 0x00},  
{0x3811, 0x04},  
{0x3812, 0x00},  
{0x3813, 0x04},  
{0x3814, 0x31},  
{0x3815, 0x31},  
{0x3820, 0x11},  
{0x3821, 0x0f},  
{0x3823, 0x00},  
{0x3824, 0x00},  
{0x3825, 0x00},  
{0x3826, 0x00},  
{0x3827, 0x00},  
{0x382a, 0x04},  
{0x3a04, 0x04},  
{0x3a05, 0xc9},  
{0x3a06, 0x00},  
{0x3a07, 0xf8},  
{0x3b00, 0x00},  
{0x3b02, 0x00},  
{0x3b03, 0x00},  
{0x3b04, 0x00},  
{0x3b05, 0x00},  
{0x3d00, 0x00},  
{0x3d01, 0x00},  
{0x3d02, 0x00},  
{0x3d03, 0x00},  
{0x3d04, 0x00},  
{0x3d05, 0x00},  
{0x3d06, 0x00},  
{0x3d07, 0x00},  
{0x3d08, 0x00},  
{0x3d09, 0x00},  
{0x3d0a, 0x00},  
{0x3d0b, 0x00},  
{0x3d0c, 0x00},  
{0x3d0d, 0x00},  
{0x3d0e, 0x00},  
{0x3d0f, 0x00},  
{0x3d80, 0x00},  
{0x3d81, 0x00},  
{0x3d84, 0x00},  
{0x4000, 0x18},  
{0x4001, 0x04},  
{0x4002, 0x45},  
{0x4004, 0x02},  
{0x4005, 0x18},  
{0x4006, 0x20},  
{0x4008, 0x20},  //{0x4008, 0x24},
{0x4009, 0x10},  
{0x404F, 0xA0},  
{0x4100, 0x10},  //{0x4100, 0x17},
{0x4101, 0x12},  //{0x4101, 0x03}
{0x4102, 0x24},  //{0x4102, 0x04},
{0x4103, 0x00},  //{0x4103, 0x03}
{0x4104, 0x5b},  //{0x4104, 0x5a}
{0x4307, 0x30},  
{0x4315, 0x00},  
{0x4511, 0x05},  
{0x4512, 0x00},  
{0x4805, 0x21},  
{0x4806, 0x00},  
{0x481f, 0x36},  
{0x4831, 0x6c},  
{0x4837, 0x0a},  
{0x4a00, 0xaa},  
{0x4a03, 0x01},  
{0x4a05, 0x08},  
{0x4a0a, 0x88},  
{0x4d03, 0xbb},  
{0x5000, 0x06},  
{0x5001, 0x01},  
{0x5002, 0x80},  
{0x5003, 0x20},  
{0x5013, 0x00},  
{0x5046, 0x4a},  
{0x5780, 0x1c},  
{0x5786, 0x20},  
{0x5787, 0x10},  
{0x5788, 0x18},  
{0x578a, 0x04},  
{0x578b, 0x02},  
{0x578c, 0x02},  
{0x578e, 0x06},  
{0x578f, 0x02},  
{0x5790, 0x02},  
{0x5791, 0xff},  
{0x5a08, 0x02},  
{0x5e00, 0x00},  
{0x5e10, 0x0c},  
};
static int32_t ov8835_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
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

int32_t ov8835_sensor_init_reg_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	printk("%s:%s E\n",TAG,__func__);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x0103,0x01,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(10);
	rc = ov8835_i2c_write_table(s_ctrl,
			&init_reg_array[0],
			ARRAY_SIZE(init_reg_array));
	if(rc < 0)
		goto error;

	msleep(100);

error:
	printk("%s:%s E\n",TAG,__func__);
	return rc;		
}
static int32_t ov8835_sensor_power_down_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	 s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x0100, 0,
    		MSM_CAMERA_I2C_BYTE_DATA);
	msleep(2);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,0);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_VAF],
				GPIO_OUT_LOW);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_LOW);
	
	printk("%s:%s X\n",TAG,__func__);
	return 0;
}
static int32_t ov8835_sensor_power_on_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("%s:%s E\n",TAG,__func__);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,1);
	msleep(1);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_VAF],
				GPIO_OUT_HIGH);
	printk("%s:%s X\n",TAG,__func__);
	return 0;
	
}
static struct msm_sensor_fn_t ov8835_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};
#endif
// <---
static struct msm_sensor_ctrl_t ov8835_s_ctrl = {
	.sensor_i2c_client = &ov8835_sensor_i2c_client,
	.power_setting_array.power_setting = ov8835_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov8835_power_setting),
	.msm_sensor_mutex = &ov8835_mut,
	.sensor_v4l2_subdev_info = ov8835_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov8835_subdev_info),
	.sensor_otp_func = ov8835_msm_sensor_otp_func,
	.load_otp_parm =ov8835_load_otp_parm,
	.msm_sensor_reg_default_data_type=MSM_CAMERA_I2C_BYTE_DATA,
/*
  * Added for accurate  standby ov8835
  *
  * by ZTE_YCM_20140408 yi.changming
  */
// --->
#ifdef SENSOR_USE_STANDBY
	.func_tbl = &ov8835_sensor_func_tbl,
	.sensor_power_down = ov8835_sensor_power_down_standby,
	.sensor_power_on = ov8835_sensor_power_on_standby,
	.sensor_init_reg = ov8835_sensor_init_reg_standby,
#endif
// <---
};

module_init(ov8835_init_module);
module_exit(ov8835_exit_module);
MODULE_DESCRIPTION("ov8835");
MODULE_LICENSE("GPL v2");
