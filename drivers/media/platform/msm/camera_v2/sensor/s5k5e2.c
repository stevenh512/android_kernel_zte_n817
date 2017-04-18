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
#include "msm_camera_io_util.h"
#define S5K5E2_SENSOR_NAME "s5k5e2"
#define SENSOR_USE_STANDBY
DEFINE_MSM_MUTEX(s5k5e2_mut);

static struct msm_sensor_ctrl_t s5k5e2_s_ctrl;

static struct msm_sensor_power_setting s5k5e2_power_setting[] = {
  {
    .seq_type = SENSOR_VREG,
    .seq_val = CAM_VIO,
    .config_val = 0,
    .delay = 1,
  },
   {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_VIO,
    .config_val = GPIO_OUT_HIGH,
    .sleep_val = GPIO_OUT_HIGH,
    .delay = 0,
  },
  {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_VANA,
    .config_val = GPIO_OUT_HIGH,
    .sleep_val = GPIO_OUT_HIGH,
    .delay = 0,
  },
  {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_VDIG,
    .config_val = GPIO_OUT_HIGH,
    .sleep_val = GPIO_OUT_HIGH,
    .delay = 0,
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
    .sleep_val = GPIO_OUT_HIGH,
    .delay = 3,
  },
  {
    .seq_type = SENSOR_CLK,
    .seq_val = SENSOR_CAM_MCLK,
    #if defined(CONFIG_BOARD_IRIS)
    .config_val = 22500000,
    #else
    .config_val = 24000000,
    #endif
    .delay = 1,
  },
  {
    .seq_type = SENSOR_I2C_MUX,
    .seq_val = 0,
    .config_val = 0,
    .delay = 1,
  },
};
#if defined(SENSOR_USE_STANDBY)
static struct msm_camera_i2c_reg_conf init_reg_array[] = {
  {0x3000, 0x04}, //  ct_ld_start                                    
  {0x3002, 0x03}, //  ct_sl_start                                    
  {0x3003, 0x04}, //  ct_sl_margin                                   
  {0x3004, 0x02}, //  ct_rx_start                                    
  {0x3005, 0x00}, //  ct_rx_margin (MSB)                             
  {0x3006, 0x10}, //  ct_rx_margin (LSB)                             
  {0x3007, 0x03}, //  ct_tx_start                                    
  {0x3008, 0x55}, //  ct_tx_width                                    
  {0x3039, 0x00}, //  cintc1_margin (10 --> 00)                      
  {0x303A, 0x00}, //  cintc2_margin (10 --> 00)                      
  {0x303B, 0x00}, //  offs_sh                                        
  {0x3009, 0x05}, //  ct_srx_margin                                  
  {0x300A, 0x55}, //  ct_stx_width                                   
  {0x300B, 0x38}, //  ct_dstx_width                                  
  {0x300C, 0x10}, //  ct_stx2dstx                                    
  {0x3012, 0x05}, //  ct_cds_start                                   
  {0x3013, 0x00}, //  ct_s1s_start                                   
  {0x3014, 0x22}, //  ct_s1s_end                                     
  {0x300E, 0x79}, //  ct_s3_width                                    
  {0x3010, 0x68}, //  ct_s4_width                                    
  {0x3019, 0x03}, //  ct_s4d_start                                   
  {0x301A, 0x00}, //  ct_pbr_start                                   
  {0x301B, 0x06}, //  ct_pbr_width                                   
  {0x301C, 0x00}, //  ct_pbs_start                                   
  {0x301D, 0x22}, //  ct_pbs_width                                   
  {0x301E, 0x00}, //  ct_pbr_ob_start                                
  {0x301F, 0x10}, //  ct_pbr_ob_width                                
  {0x3020, 0x00}, //  ct_pbs_ob_start                                
  {0x3021, 0x00}, //  ct_pbs_ob_width                                
  {0x3022, 0x0A}, //  ct_cds_lim_start                               
  {0x3023, 0x1E}, //  ct_crs_start                                   
  {0x3024, 0x00}, //  ct_lp_hblk_cds_start (MSB)                     
  {0x3025, 0x00}, //  ct_lp_hblk_cds_start (LSB)                     
  {0x3026, 0x00}, //  ct_lp_hblk_cds_end (MSB)                       
  {0x3027, 0x00}, //  ct_lp_hblk_cds_end (LSB)                       
  {0x3028, 0x1A}, //  ct_rmp_off_start                               
  {0x3015, 0x00}, //  ct_rmp_rst_start (MSB)                         
  {0x3016, 0x84}, //  ct_rmp_rst_start (LSB)                         
  {0x3017, 0x00}, //  ct_rmp_sig_start (MSB)                         
  {0x3018, 0xA0}, //  ct_rmp_sig_start (LSB)                         
  {0x302B, 0x10}, //  ct_cnt_margin                                  
  {0x302C, 0x0A}, //  ct_rmp_per                                     
  {0x302D, 0x06}, //  ct_cnt_ms_margin1                              
  {0x302E, 0x05}, //  ct_cnt_ms_margin2                              
  {0x302F, 0x0E}, //  rst_mx                                         
  {0x3030, 0x2F}, //  sig_mx                                         
  {0x3031, 0x08}, //  ct_latch_start                                 
  {0x3032, 0x05}, //  ct_latch_width                                 
  {0x3033, 0x09}, //  ct_hold_start                                  
  {0x3034, 0x05}, //  ct_hold_width                                  
  {0x3035, 0x00}, //  ct_lp_hblk_dbs_start (MSB)                     
  {0x3036, 0x00}, //  ct_lp_hblk_dbs_start (LSB)                     
  {0x3037, 0x00}, //  ct_lp_hblk_dbs_end (MSB)                       
  {0x3038, 0x00}, //  ct_lp_hblk_dbs_end (LSB)                       
  {0x3088, 0x06}, //  ct_lat_lsb_offset_start1                       
  {0x308A, 0x08}, //  ct_lat_lsb_offset_end1                         
  {0x308C, 0x05}, //  ct_lat_lsb_offset_start2                       
  {0x308E, 0x07}, //  ct_lat_lsb_offset_end2                         
  {0x3090, 0x06}, //  ct_conv_en_offset_start1                       
  {0x3092, 0x08}, //  ct_conv_en_offset_end1                         
  {0x3094, 0x05}, //  ct_conv_en_offset_start2                       
  {0x3096, 0x21}, //  ct_conv_en_offset_end2                         
  {0x3099, 0x0E}, // cds_option ([3]:crs switch disable, s3,s4 streng
  {0x3070, 0x10}, // comp1_bias (default:77)                         
  {0x3085, 0x11}, // comp1_bias (gain1~4)                            
  {0x3086, 0x01}, // comp1_bias (gain4~8) modified 813                                                            
  {0x3064, 0x00}, // Multiple sampling(gainx8,x16)                   
  {0x3062, 0x08}, // off_rst                                         
  {0x3061, 0x11}, // dbr_tune_rd (default :08)                       
  {0x307B, 0x20}, // dbr_tune_rgsl (default :08)                     
  {0x3068, 0x00}, // RMP BP bias sampling                            
  {0x3074, 0x00}, // Pixel bias sampling [2]:Default L               
  {0x307D, 0x00}, // VREF sampling [1]                               
  {0x3045, 0x01}, // ct_opt_l1_start                                 
  {0x3046, 0x05}, // ct_opt_l1_width                                 
  {0x3047, 0x78}, //                                                 
  {0x307F, 0xB1}, //RDV_OPTION[5:4], RG default high                 
  {0x3098, 0x01}, //CDS_OPTION[16] SPLA-II enable                    
  {0x305C, 0xF6}, //lob_extension[6]                                 
  {0x306B, 0x10}, //                                                 
  {0x3063, 0x27}, // ADC_SAT 490mV --> 610mV                         
  {0x3400, 0x01}, // GAS bypass                                      
  {0x3235, 0x49}, // L/F-ADLC on                                     
  {0x3233, 0x00}, // D-pedestal L/F ADLC off (1FC0h)                 
  {0x3234, 0x00}, //                                                 
  {0x3300, 0x0C}, //BPC bypass                                                                           
  {0x3203, 0x45}, //ADC_OFFSET_EVEN                                  
  {0x3205, 0x4D}, //ADC_OFFSET_ODD                                   
  {0x320B, 0x40}, //ADC_DEFAULT                                      
  {0x320C, 0x06}, //ADC_MAX                                          
  {0x320D, 0xC0}, //   
  #if defined(CONFIG_BOARD_IRIS)
  {0x0305, 0x05}, //fxy for banding                                   
  #else
  {0x0305, 0x06}, //PLLP (def:5)                                      
  #endif
  {0x0306, 0x00}, //                                                  
  #if defined(CONFIG_BOARD_IRIS)
  {0x0307, 0xC7}, //PLLM (def:CCh 204d --> B3h 179d)                  
  #else
  {0x0307, 0xE0},
  #endif
  {0x3C1F, 0x00}, //PLLS                                              
  {0x0820, 0x03}, // requested link bit rate mbps : (def:3D3h 979d -->
  {0x0821, 0x80},                                                  
  {0x3C1C, 0x58},  //dbr_div                                           
  {0x0114, 0x01},  //MIPI Lane       
  {0x0340, 0x07}, // //frame_length_lines :2025      
  {0x0341, 0xE9}, //                                 
  {0x0342, 0x0B}, // //line_length_pck    :2950      
  {0x0343, 0x86}, //                                 
  {0x0344, 0x00}, // //x_addr_start             
  {0x0345, 0x08}, //                            
  {0x0346, 0x00}, // //y_addr_start             
  {0x0347, 0x08}, //                            
  {0x0348, 0x0A}, // //x_addr_end               
  {0x0349, 0x07}, //                            
  {0x034A, 0x07}, // //y_addr_end               
  {0x034B, 0x87}, //                                 
  {0x034C, 0x0A}, // //x_output size      :2560      
  {0x034D, 0x00}, //                                 
  {0x034E, 0x07}, // //y_output size      :1920      
  {0x034F, 0x80}, //
  {0x0900, 0x00},
  {0x0901, 0x20},
  {0x0381, 0x01},
  {0x0383, 0x01},
  {0x0385, 0x01},
  {0x0387, 0x01},   
  {0x0204, 0x00},
  {0x0205, 0x80},               
  {0x0202, 0x02},
  {0x0203, 0x00},
  {0x0200, 0x04},
  {0x0201, 0x98},         
  {0x340B, 0x00},
  {0x340C, 0x00},
  {0x340D, 0x00},
  {0x340E, 0x00},
  {0x3401, 0x50},
  {0x3402, 0x3C},
  {0x3403, 0x03},
  {0x3404, 0x33},
  {0x3405, 0x04},
  {0x3406, 0x44},
  {0x3458, 0x03},
  {0x3459, 0x33},
  {0x345A, 0x04},
  {0x345B, 0x44},
  {0x3400, 0x01},
};
static int32_t s5k5e2_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_camera_i2c_reg_conf *table, int num)
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

int32_t s5k5e2_sensor_init_reg_standby(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	printk("%s E\n",__func__);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x0103,0x01,MSM_CAMERA_I2C_BYTE_DATA);
	msleep(50);
	rc = s5k5e2_i2c_write_table(s_ctrl,
			&init_reg_array[0],
			ARRAY_SIZE(init_reg_array));
	msleep(50);
	if(rc < 0)
		goto error;

	msleep(100);

error:
	printk("%s E\n",__func__);
	return rc;		
}

 int32_t s5k5e2_sensor_standby_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	printk("s5k5e2_sensor_standby_down\n");
 	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x0100, 0,
    	MSM_CAMERA_I2C_BYTE_DATA);
	msleep(20);  
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_LOW);
	//lvcheng 20141216 from avp carl:giant pull down dvdd when standby
	#if defined( CONFIG_BOARD_GIANT) ||defined(CONFIG_BOARD_IRIS)
	msleep(10);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG],
				GPIO_OUT_LOW);
	msleep(10);
	#endif
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,0);
	return 0;
}
 int32_t s5k5e2_sensor_standby_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	//int rc =0;
	printk("s5k5e2_sensor_standby_on\n");
	//lvcheng 20141216 from avp carl:giant pull down dvdd when standby
	#if defined( CONFIG_BOARD_GIANT) ||defined(CONFIG_BOARD_IRIS)
	msleep(10);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG],
				GPIO_OUT_HIGH);
	msleep(10);
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
	msleep(10);
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,1);
	msleep(10);
	#else
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,1);
	msleep(100); 
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
	msleep(2);  
	#endif
	 	//s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,0x0100, 1,
    //	MSM_CAMERA_I2C_BYTE_DATA);
#if 0
	msleep(2);
	if(s_ctrl->sensor_init_reg){
	rc = s_ctrl->sensor_init_reg(s_ctrl);
		if (rc < 0) {
			pr_err("%s: s5k5e2 sensor_init_reg failed\n", __func__);
		}
	}
	#endif
	return 0;
}
 #endif
static struct v4l2_subdev_info s5k5e2_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id s5k5e2_i2c_id[] = {
	{S5K5E2_SENSOR_NAME, (kernel_ulong_t)&s5k5e2_s_ctrl},
	{ }
};

static int32_t msm_s5k5e2_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	printk("wly: msm_s5k5e2_i2c_probe\n");
	return msm_sensor_i2c_probe(client, id, &s5k5e2_s_ctrl);
}

static struct i2c_driver s5k5e2_i2c_driver = {
	.id_table = s5k5e2_i2c_id,
	.probe  = msm_s5k5e2_i2c_probe,
	.driver = {
		.name = S5K5E2_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k5e2_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id s5k5e2_dt_match[] = {
	{.compatible = "qcom,s5k5e2", .data = &s5k5e2_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, s5k5e2_dt_match);

static struct platform_driver s5k5e2_platform_driver = {
	.driver = {
		.name = "qcom,s5k5e2",
		.owner = THIS_MODULE,
		.of_match_table = s5k5e2_dt_match,
	},
};

static int32_t s5k5e2_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(s5k5e2_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init s5k5e2_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&s5k5e2_platform_driver,
		s5k5e2_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&s5k5e2_i2c_driver);
}

static void __exit s5k5e2_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (s5k5e2_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&s5k5e2_s_ctrl);
		platform_driver_unregister(&s5k5e2_platform_driver);
	} else
		i2c_del_driver(&s5k5e2_i2c_driver);
	return;
}
#ifdef SENSOR_USE_STANDBY
static struct msm_sensor_fn_t s5k5e2_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};
#endif
static struct msm_sensor_ctrl_t s5k5e2_s_ctrl = {
	.sensor_i2c_client = &s5k5e2_sensor_i2c_client,
	.power_setting_array.power_setting = s5k5e2_power_setting,
	.power_setting_array.size = ARRAY_SIZE(s5k5e2_power_setting),
	.msm_sensor_mutex = &s5k5e2_mut,
	.sensor_v4l2_subdev_info = s5k5e2_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k5e2_subdev_info),
	.msm_sensor_reg_default_data_type=MSM_CAMERA_I2C_BYTE_DATA,
	#ifdef SENSOR_USE_STANDBY
	.func_tbl = &s5k5e2_sensor_func_tbl,
	.sensor_power_down = s5k5e2_sensor_standby_down,
	.sensor_power_on = s5k5e2_sensor_standby_on,
	.sensor_init_reg = s5k5e2_sensor_init_reg_standby
	#endif
};

module_init(s5k5e2_init_module);
module_exit(s5k5e2_exit_module);
MODULE_DESCRIPTION("s5k5e2");
MODULE_LICENSE("GPL v2");
