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
/*
  * modfiy code  for ov2680 standby mode
  *
  * by ZTE_YCM_20140506 yi.changming
  */
// --->
#include "msm_camera_io_util.h"
#include "msm_cci.h"
// <---
#define CONFIG_ZTE_CAMERA_SENSOR_STANDBY_POWER_CONTROL
#define OV2680_SENSOR_NAME "ov2680"
DEFINE_MSM_MUTEX(ov2680_mut);

static struct msm_sensor_ctrl_t ov2680_s_ctrl;
/*
  * modfiy code  for ov2680 standby mode
  *
  * by ZTE_YCM_20140506 yi.changming
  */
// --->
#ifdef CONFIG_ZTE_CAMERA_SENSOR_STANDBY_POWER_CONTROL
#if defined(CONFIG_BOARD_WELLINGTON)

/* 20170717 ERIC - modified for power config */
static struct msm_sensor_power_setting ov2680_power_setting[] = {
  {
    .seq_type = SENSOR_VREG,
    .seq_val = CAM_VDIG,
    .config_val = 0,
    .delay = 5,
  },
  {
    .seq_type = SENSOR_VREG,
    .seq_val = CAM_VANA,
    .config_val = 0,
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
    .delay = 10,
  },
  {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_RESET,
    .config_val = GPIO_OUT_LOW,
    .delay = 5,
  },
  {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_RESET,
    .config_val = GPIO_OUT_HIGH,
    .delay = 10,
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
#else
static struct msm_sensor_power_setting ov2680_power_setting[] = {
  {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_STANDBY,
    .config_val = GPIO_OUT_LOW,
    .sleep_val = GPIO_OUT_LOW,
    .delay = 0,
  },
   /*
  * modify ov2680 power sequence
  * 
  * by ZTE_YCM_20140718 yi.changming 000017
  */
// --->
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
    .sleep_val = GPIO_OUT_HIGH,
    .config_val = GPIO_OUT_HIGH,
    .delay = 0,
  },
#if 0
   {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_VDIG,
    .config_val = GPIO_OUT_HIGH,
    .delay = 5,
  },
#endif
  // <---
  {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_STANDBY,
    .config_val = GPIO_OUT_HIGH,
    .sleep_val = GPIO_OUT_LOW,
    .delay = 10,
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
#endif
static int32_t ov2680_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];

	printk("%s:E\n",__func__);
	
	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}
	
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,0);

	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_LOW);
	
	usleep_range(1 * 1000,(1 * 1000) + 1000); 
	
	printk("%s: X\n",__func__);
	return 0;
}
static int32_t ov2680_sensor_power_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	static void *data[10];
	int32_t  rc = 0;
	
	printk("%s:E\n",__func__);
	
	msm_cam_clk_enable(s_ctrl->dev,&s_ctrl->clk_info[0],(struct clk **)&data[0],s_ctrl->clk_info_size,1);

	usleep_range(1 * 1000,(1 * 1000) + 1000); 
	
	gpio_set_value_cansleep(s_ctrl->sensordata->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY],
				GPIO_OUT_HIGH);
	
	usleep_range(1 * 1000,(1 * 1000) + 1000); 

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
		}
	}
	
	printk("%s: X\n",__func__);
	
	return rc;
}
#else


static struct msm_sensor_power_setting ov2680_power_setting[] = {
	  {
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.sleep_val = GPIO_OUT_LOW,
		.delay = 0,
	  },
	   /*
	  * modify ov2680 power sequence
	  * 
	  * by ZTE_YCM_20140718 yi.changming 000017
	  */
	// --->
	#if 0// iovdd is L14 support
	  {
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_HIGH,
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
	  // <---
	  {
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.sleep_val = GPIO_OUT_LOW,
		.delay = 10,
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
#endif
// <---
static struct v4l2_subdev_info ov2680_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id ov2680_i2c_id[] = {
	{OV2680_SENSOR_NAME, (kernel_ulong_t)&ov2680_s_ctrl},
	{ }
};

static int32_t msm_ov2680_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov2680_s_ctrl);
}

static struct i2c_driver ov2680_i2c_driver = {
	.id_table = ov2680_i2c_id,
	.probe  = msm_ov2680_i2c_probe,
	.driver = {
		.name = OV2680_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov2680_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov2680_dt_match[] = {
	{.compatible = "qcom,ov2680", .data = &ov2680_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov2680_dt_match);

static struct platform_driver ov2680_platform_driver = {
	.driver = {
		.name = "qcom,ov2680",
		.owner = THIS_MODULE,
		.of_match_table = ov2680_dt_match,
	},
};

static int32_t ov2680_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	printk("%s: *******************  %d\n", __func__, __LINE__);	
	match = of_match_device(ov2680_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	printk("%s: *******************  %d\n", __func__, __LINE__);	
	return rc;
}

static int __init ov2680_init_module(void)
{
	int32_t rc = 0;
	pr_err("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&ov2680_platform_driver,
		ov2680_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&ov2680_i2c_driver);
}

static void __exit ov2680_exit_module(void)
{
	pr_err("%s:%d\n", __func__, __LINE__);
	if (ov2680_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov2680_s_ctrl);
		platform_driver_unregister(&ov2680_platform_driver);
	} else
		i2c_del_driver(&ov2680_i2c_driver);
	return;
}
/*
  * modfiy code  for ov2680 standby mode
  *
  * by ZTE_YCM_20140506 yi.changming
  */
// --->
#ifdef CONFIG_ZTE_CAMERA_SENSOR_STANDBY_POWER_CONTROL

int32_t ov2680_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		s_ctrl->sensor_i2c_client->client->addr = 0x20;//karr module i2c address = 0x20,sanyingxing module i2c address = 0x6c
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0) {
			pr_err("%s: %s: read id failed\n", __func__,
				s_ctrl->sensordata->sensor_name);
			return rc;
		}
	}
	pr_err("%s: read id: %x expected id %x\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);
	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}


static struct msm_sensor_fn_t ov2680_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = ov2680_sensor_match_id,
};
#endif
// <---
static struct msm_sensor_ctrl_t ov2680_s_ctrl = {
	.sensor_i2c_client = &ov2680_sensor_i2c_client,
	.power_setting_array.power_setting = ov2680_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov2680_power_setting),
	.msm_sensor_mutex = &ov2680_mut,
	.sensor_v4l2_subdev_info = ov2680_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov2680_subdev_info),
	.msm_sensor_reg_default_data_type=MSM_CAMERA_I2C_BYTE_DATA,
/*
  * modfiy code  for ov2680 standby mode
  *
  * by ZTE_YCM_20140506 yi.changming
  */
// --->
#ifdef CONFIG_ZTE_CAMERA_SENSOR_STANDBY_POWER_CONTROL
	.func_tbl = &ov2680_sensor_func_tbl,
	.sensor_power_down = ov2680_sensor_power_down,
	.sensor_power_on = ov2680_sensor_power_on,
#endif
// <---
};

module_init(ov2680_init_module);
module_exit(ov2680_exit_module);
MODULE_DESCRIPTION("ov2680");
MODULE_LICENSE("GPL v2");
