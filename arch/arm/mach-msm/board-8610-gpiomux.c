/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>

#define WLAN_CLK	27
#define WLAN_SET	26
#define WLAN_DATA0	25
#define WLAN_DATA1	24
#define WLAN_DATA2	23

static struct gpiomux_setting gpio_spi_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting gpio_spi_susp_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};
/*
static struct gpiomux_setting gpio_spi_susp_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
*/

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_cam_i2c_config = {
	.func = GPIOMUX_FUNC_1,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

#if !defined(CONFIG_ICE40_IRDA)
static struct gpiomux_setting gpio_nfc_config = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_nfc_sus_config = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif

 /*For EXTERNAL OVP control*/
#if defined(CONFIG_BOARD_APUS) || defined(CONFIG_BOARD_GIANT)
static struct gpiomux_setting gpio_ext_ovp_active = {
      .func = GPIOMUX_FUNC_GPIO,
      .drv = GPIOMUX_DRV_2MA,
      .pull = GPIOMUX_PULL_NONE,//ext pull down 100k
      .dir = GPIOMUX_OUT_LOW,
 };
 static struct msm_gpiomux_config msm_ext_ovp_configs[] __initdata = {
        {
                .gpio = 34,//ext_ovp_en
                .settings = {
                         [GPIOMUX_ACTIVE]    = &gpio_ext_ovp_active,
                         [GPIOMUX_SUSPENDED] = &gpio_ext_ovp_active,
               },
         },
 };
#endif

static struct gpiomux_setting atmel_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting atmel_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting focaltech_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting focaltech_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting focaltech_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting focaltech_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5wire_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5wire_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting wcnss_5gpio_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5gpio_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting lcd_en_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

#if defined(CONFIG_BOARD_APUS)
static struct gpiomux_setting lcd_en_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};
#else
static struct gpiomux_setting lcd_en_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};
#endif

static struct gpiomux_setting lcd_te_act_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting lcd_te_sus_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting gpio_keys_active = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting gpio_keys_suspend = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

/* define gpio used as interrupt input */
#if !defined(CONFIG_ICE40_IRDA)
static struct gpiomux_setting gpio_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting gpio_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};
#endif

static struct msm_gpiomux_config msm_gpio_int_configs[] __initdata = {
#if !defined(CONFIG_ICE40_IRDA)
	{
		.gpio = 84,
		.settings = {
			[GPIOMUX_ACTIVE]	= &gpio_int_act_cfg,
			[GPIOMUX_SUSPENDED]	= &gpio_int_sus_cfg,
		},
	},
#endif
};
static struct gpiomux_setting lcd_bl_active = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_HIGH,
};
#if 0
static struct gpiomux_setting lcd_id_activity = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
#endif
static struct gpiomux_setting lcd_id_suspend = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting lcd_5v_power_activity = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_HIGH,
};
#if 0
static struct gpiomux_setting lcd_5v_power_suspend = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_LOW,
};
#endif
static struct msm_gpiomux_config msm_lcd_configs[] __initdata = {
	{
		.gpio = 41,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_en_sus_cfg,
		},
	},
	{
		.gpio = 7,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_5v_power_activity,
			[GPIOMUX_SUSPENDED] = &lcd_5v_power_activity,
		},
	},
	{
		.gpio = 35,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_5v_power_activity,
			[GPIOMUX_SUSPENDED] = &lcd_5v_power_activity,
		},
	},
	{
		.gpio = 32,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_id_suspend,
			[GPIOMUX_SUSPENDED] = &lcd_id_suspend,
		},
	},
	{
		.gpio = 94,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_id_suspend,
			[GPIOMUX_SUSPENDED] = &lcd_id_suspend,
		},
	},
	{
		.gpio = 79,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_bl_active,
			[GPIOMUX_SUSPENDED] = &lcd_bl_active,
		},
	},
	{
		.gpio = 12,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_te_act_config,
			[GPIOMUX_SUSPENDED] = &lcd_te_sus_config,
		},
	},
};

// IRDA_20131223 ++++++
#if defined(CONFIG_ICE40_IRDA)
static struct gpiomux_setting out_irda_low_np_2ma = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};
static struct gpiomux_setting out_irda_high_np_2ma = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};
static struct gpiomux_setting in_irda_pd_2ma = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting gpio_irda_config = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting gpio_irda_sus_config = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif
// IRDA_20131223 ------

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
	{
		.gpio      = 2,		/* BLSP1 QUP1 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 3,		/* BLSP1 QUP1 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 10,	/* BLSP1 QUP3 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 11,	/* BLSP1 QUP3 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 16,	/* BLSP1 QUP6 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_cam_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_cam_i2c_config,
		},
	},
	{
		.gpio      = 17,	/* BLSP1 QUP6 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_cam_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_cam_i2c_config,
		},
	},
#if !defined(CONFIG_ICE40_IRDA)
	{
		.gpio      = 78,	/* NFC CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_nfc_config,
			[GPIOMUX_SUSPENDED] = &gpio_nfc_sus_config,
		},
	},
#else
	{
		.gpio      = 75,	/* IRDA RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &out_irda_high_np_2ma,
			[GPIOMUX_SUSPENDED] = &out_irda_high_np_2ma,
		},
	},
	{
		.gpio      = 78,	/* IRDA CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_irda_config,
			[GPIOMUX_SUSPENDED] = &gpio_irda_sus_config,
		},
	},
	{
		.gpio      = 83,	/* IRDA 3V3 EN */
		.settings = {
			[GPIOMUX_ACTIVE] = &out_irda_low_np_2ma,
			[GPIOMUX_SUSPENDED] = &out_irda_low_np_2ma,
		},
	},
	{
		.gpio      = 84,	/* IRDA CDONE */
		.settings = {
			[GPIOMUX_ACTIVE] = &in_irda_pd_2ma,
			[GPIOMUX_SUSPENDED] = &in_irda_pd_2ma,
		},
	},
#endif
};

static struct gpiomux_setting gpio_i2c_nfc_pvt_config = {
		.func = GPIOMUX_FUNC_5, /*active 1*/ /* 0 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	};

static struct msm_gpiomux_config msm_nfc_configs[] __initdata = {
	{
		.gpio   = 8,            /* BLSP1 QUP2 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_nfc_pvt_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_nfc_pvt_config,
		},
	},
	{
		.gpio   = 9,            /* BLSP1 QUP2 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_nfc_pvt_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_nfc_pvt_config,
		},
	},
};


static struct msm_gpiomux_config msm_atmel_configs[] __initdata = {
	{
		.gpio      = 0,		/* TOUCH RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &atmel_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_reset_sus_cfg,
		},
	},
	{
		.gpio      = 1,		/* TOUCH INT */
		.settings = {
			[GPIOMUX_ACTIVE] = &atmel_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,
		},
	},
	{
		.gpio      = 86,		/* BLSP1 QUP4 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 87,		/* BLSP1 QUP4 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
#if !defined(CONFIG_ICE40_IRDA)
	{
		.gpio      = 88,		/* BLSP1 QUP4 SPI_CS */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
#else
	{
		.gpio      = 88,		/* BLSP1 QUP4 SPI_CS */
		.settings = {
			[GPIOMUX_ACTIVE] = &out_irda_high_np_2ma,
			[GPIOMUX_SUSPENDED] = &out_irda_high_np_2ma,
		},
	},
#endif
};

static struct msm_gpiomux_config msm_focaltech_configs[] __initdata = {
	{
		.gpio      = 0,		/* TOUCH RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &focaltech_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &focaltech_reset_sus_cfg,
		},
	},
	{
		.gpio      = 1,		/* TOUCH INT */
		.settings = {
			[GPIOMUX_ACTIVE] = &focaltech_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &focaltech_int_sus_cfg,
		},
	},
	{
		.gpio      = 86,		/* BLSP1 QUP4 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 87,		/* BLSP1 QUP4 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},

	{
		.gpio      = 88,		/* BLSP1 QUP4 SPI_CS */
		.settings = {
			[GPIOMUX_ACTIVE] = &out_irda_high_np_2ma,
			[GPIOMUX_SUSPENDED] = &out_irda_high_np_2ma,
		},
	},
};


static struct msm_gpiomux_config wcnss_5wire_interface[] = {
	{
		.gpio = 23,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 26,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 27,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config wcnss_5gpio_interface[] = {
	{
		.gpio = 23,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
	{
		.gpio = 26,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
	{
		.gpio = 27,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5gpio_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5gpio_suspend_cfg,
		},
	},
};

#if 0 //weilanying add
static struct gpiomux_setting gpio_suspend_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,  /* IN-NP */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* O-LOW */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_LOW,
	},
};

static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_1, /*active 1*/ /* 0 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*suspend*/ /* 1 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*i2c suspend*/ /* 2 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 0*/ /* 3 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend 0*/ /* 4 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
};
#else 
static struct gpiomux_setting cam_active_settings[] = {
	{
		.func = GPIOMUX_FUNC_1, /*active 0*/ /* 0 */
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 1*/ /* 1 */
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 2*/ /* 2 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	
	{
		.func = GPIOMUX_FUNC_GPIO, /*active 3*/ /* 3 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO, /*active 4*/ /* 4 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_HIGH,
	},
	{
		.func = GPIOMUX_FUNC_GPIO, /*active 5*/ /* 5 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_UP,
		.dir = GPIOMUX_IN,
	},
	{
		.func = GPIOMUX_FUNC_1, /*active 6*/ /* 6 */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
};

static struct gpiomux_setting cam_suspend_settings[] = {
	{
		.func = GPIOMUX_FUNC_1, /*supend 0*/ /* 0 */
		.drv = GPIOMUX_DRV_8MA, 
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*supend 1*/ /* 1 */
		.drv = GPIOMUX_DRV_8MA, 
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*supend 2*/ /* 2*/
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_DOWN,
		.dir = GPIOMUX_OUT_HIGH,
	},
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend 3*/ /* 3 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_DOWN,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend 4*/ /* 4 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_HIGH,
	},
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend 5*/ /* 5 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend 6*/ /* 6 */
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.func = GPIOMUX_FUNC_1, /*supend 7*/ /* 7 */
		.drv = GPIOMUX_DRV_2MA, 
		.pull = GPIOMUX_PULL_NONE,
	},

};
#endif
static struct gpiomux_setting accel_interrupt_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_non_qrd_configs[] __initdata = {
	{
		.gpio = 8, /* CAM1_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[5],
		},
	},
	{
		.gpio = 81,	/*ACCEL_INT1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &accel_interrupt_config,
			[GPIOMUX_SUSPENDED] = &accel_interrupt_config,
		},
	},
};

static struct msm_gpiomux_config msm_sensor_configs[] __initdata = {
	{
		.gpio = 13, /* CAM_MCLK0 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[0],
		},
	},
	{
		.gpio = 14, /* CAM_MCLK1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[0],
		},
	},
	#if defined(CONFIG_BOARD_IRIS)||defined(CONFIG_BOARD_GIANT)
	{
		.gpio = 16, /* CCI_I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[6],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[7],
		},
	},
	{
		.gpio = 17, /* CCI_I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[6],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[7],
		},
	},
	#else
	{
		.gpio = 16, /* CCI_I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[1],
		},
	},
	{
		.gpio = 17, /* CCI_I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[1],
		},
	},
	#endif
	#if defined(CONFIG_BOARD_ADA)
	{
		.gpio = 18, /* FLASH_LED_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[5],
		},
	},
	#else
	{
		.gpio = 55, /* FLASH_LED_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[5],
		},
	},
	#endif
	#if defined(CONFIG_BOARD_FAUN)
	{
		.gpio = 75, 
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[4],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[4],
		},
	},
	{
		.gpio = 76, 
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[4],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[4],
		},
	},	
	{
		.gpio = 77, 
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[4],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[4],
		},
	},
	#endif
	{
		.gpio = 19, /* FLASH_LED_NOW */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[5],
		},
	},
	{
		.gpio = 85, /* CAM1_STANDBY_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[6],
		},
	},
	#if defined(CONFIG_BOARD_FAERIE) ||  defined(CONFIG_BOARD_EROS)
	{
		.gpio = 15, /* WEBCAM2_RESET_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[5],
			[GPIOMUX_SUSPENDED] = &cam_active_settings[5],//ZTE_YCM_20140215 lead current from 26 to 1.45
		},
	},
	#else
	{
		.gpio = 15, /* CAM1_RST_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[3],//ZTE_YCM_20140215 lead current from 26 to 1.45
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[6],//ZTE_YCM_20140215 lead current from 26 to 1.45
		},
	},
	#endif
	{
		.gpio = 20, /* WEBCAM1_STANDBY */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[6],//ZTE_YCM_20140215 lead current from 26 to 1.45
		},
	},
	#if defined(CONFIG_BOARD_FAERIE) ||  defined(CONFIG_BOARD_EROS)
	{
		.gpio = 21, /* WEBCAM2_RESET_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[5],
			[GPIOMUX_SUSPENDED] = &cam_active_settings[5],//ZTE_YCM_20140215 lead current from 26 to 1.45
		},
	},
	#else
	{
		.gpio = 21, /* WEBCAM2_RESET_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[6],//ZTE_YCM_20140215 lead current from 26 to 1.45
		},
	},
	#endif
	{
		.gpio = 82, /* CAM_IOVDD_EN  */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[4],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[4],
		},
	},
	{
		.gpio = 98, /* CAM_DVDD_EN  */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[4],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[4],
		},
	},
	{
		.gpio = 99, /* CAM_AVDD_EN  */	
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[4],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[4],
		},
	},
	{
		.gpio = 100, /* CAM_F_DVDD_EN  */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[4],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[4],
		},
	},
	{
		.gpio = 101, /* CAM_VAAM_EN  */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_suspend_settings[5],
		},
	},
};

static struct msm_gpiomux_config msm_keypad_configs[] __initdata = {
	{
		.gpio = 72,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_keys_active,
			[GPIOMUX_SUSPENDED] = &gpio_keys_suspend,
		},
	},
	{
		.gpio = 73,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_keys_active,
			[GPIOMUX_SUSPENDED] = &gpio_keys_suspend,
		},
	},
	{
		.gpio = 74,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_keys_active,
			[GPIOMUX_SUSPENDED] = &gpio_keys_suspend,
		},
	},
};

static struct gpiomux_setting sd_card_det_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting sd_card_det_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config sd_card_det[] __initdata = {
	{
		.gpio = 42,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sd_card_det_active_config,
			[GPIOMUX_SUSPENDED] = &sd_card_det_suspend_config,
		},
	},
};

static struct gpiomux_setting lcd_gpio_det_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};
static struct gpiomux_setting lcd_gpio_det_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config lcd_gpio_config[] __initdata = {
	{
		.gpio = 89,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_gpio_det_active_config,
			[GPIOMUX_SUSPENDED] = &lcd_gpio_det_suspend_config,
		},
	},
};

static struct gpiomux_setting interrupt_gpio_active = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting interrupt_gpio_suspend_pullup = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting interrupt_gpio_suspend_pulldown = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm_interrupt_configs[] __initdata = {
	{
		.gpio = 77,	/* NFC_IRQ */
		.settings = {
			[GPIOMUX_ACTIVE]    = &interrupt_gpio_active,
			[GPIOMUX_SUSPENDED] = &interrupt_gpio_suspend_pullup,
		},
	},
	{
		.gpio = 80,	/*ALSP_INT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &interrupt_gpio_active,
			[GPIOMUX_SUSPENDED] = &interrupt_gpio_suspend_pullup,
		},
	},
	{
		.gpio = 81,	/*ACCEL_INT1 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &interrupt_gpio_active,
			[GPIOMUX_SUSPENDED] = &interrupt_gpio_suspend_pulldown,
		},
	},
	#if 0 //weilanying delete
	{
		.gpio = 82,	/*ACCEL_INT2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &interrupt_gpio_active,
			[GPIOMUX_SUSPENDED] = &interrupt_gpio_suspend_pulldown,
		},
	},
	#endif
};

// chenjun:gpio 6 & 88 are used for speaker amplifier
static struct gpiomux_setting audio_spkr_boost = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.dir = GPIOMUX_OUT_LOW,
	.pull = GPIOMUX_PULL_NONE,
};

// chenjun:gpio 6 & 88 are used for speaker amplifier
static struct msm_gpiomux_config msm8610_audio_spkr_configs[] __initdata = {
	{
		.gpio = 6,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_spkr_boost,
		},
	},
#if defined(CONFIG_BOARD_APUS)
	{
		.gpio = 18,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_spkr_boost,
		},
	},
#else
	{
		.gpio = 88,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_spkr_boost,
		},
	},
#endif	
};
static struct gpiomux_setting gpio_cdc_dmic_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_NONE,
};


static struct msm_gpiomux_config msm_cdc_dmic_configs[] __initdata = {
	{
		.gpio = 100,	/* DMIC CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_cdc_dmic_cfg,
		},
	},
	{
		.gpio = 101,	/* DMIC DATA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_cdc_dmic_cfg,
		},
	},
};

void __init msm8610_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	msm_gpiomux_install(msm_blsp_configs, ARRAY_SIZE(msm_blsp_configs));
	if (of_board_is_qrd()) {
		msm_gpiomux_install(msm_focaltech_configs,
			ARRAY_SIZE(msm_focaltech_configs));
	} else {
		msm_gpiomux_install(msm_atmel_configs,
			ARRAY_SIZE(msm_atmel_configs));
	}
	msm_gpiomux_install(wcnss_5wire_interface,
			ARRAY_SIZE(wcnss_5wire_interface));
	msm_gpiomux_install_nowrite(msm_lcd_configs,
				ARRAY_SIZE(msm_lcd_configs));
	msm_gpiomux_install(msm_keypad_configs,
				ARRAY_SIZE(msm_keypad_configs));
	msm_gpiomux_install(sd_card_det, ARRAY_SIZE(sd_card_det));
	msm_gpiomux_install(msm_sensor_configs, ARRAY_SIZE(msm_sensor_configs));
	msm_gpiomux_install(msm_gpio_int_configs,
			ARRAY_SIZE(msm_gpio_int_configs));
	msm_gpiomux_install(lcd_gpio_config, ARRAY_SIZE(lcd_gpio_config));
	//chenjun:add install
	msm_gpiomux_install(msm8610_audio_spkr_configs,
			ARRAY_SIZE(msm8610_audio_spkr_configs));

	if (of_board_is_qrd()) {
		msm_gpiomux_install(msm_interrupt_configs,
			ARRAY_SIZE(msm_interrupt_configs));
		msm_gpiomux_install(msm_nfc_configs,
			ARRAY_SIZE(msm_nfc_configs));
	} else {
		msm_gpiomux_install(msm_non_qrd_configs,
			ARRAY_SIZE(msm_non_qrd_configs));
	}
	if (of_board_is_cdp())
		msm_gpiomux_install(msm_cdc_dmic_configs,
			ARRAY_SIZE(msm_cdc_dmic_configs));

#if defined(CONFIG_BOARD_APUS) || defined(CONFIG_BOARD_GIANT)
         msm_gpiomux_install(msm_ext_ovp_configs,
                         ARRAY_SIZE(msm_ext_ovp_configs));
#endif
}

static void wcnss_switch_to_gpio(void)
{
	/* Switch MUX to GPIO */
	msm_gpiomux_install(wcnss_5gpio_interface,
			ARRAY_SIZE(wcnss_5gpio_interface));

	/* Ensure GPIO config */
	gpio_direction_input(WLAN_DATA2);
	gpio_direction_input(WLAN_DATA1);
	gpio_direction_input(WLAN_DATA0);
	gpio_direction_output(WLAN_SET, 0);
	gpio_direction_output(WLAN_CLK, 0);
}

static void wcnss_switch_to_5wire(void)
{
	msm_gpiomux_install(wcnss_5wire_interface,
			ARRAY_SIZE(wcnss_5wire_interface));
}

u32 wcnss_rf_read_reg(u32 rf_reg_addr)
{
	int count = 0;
	u32 rf_cmd_and_addr = 0;
	u32 rf_data_received = 0;
	u32 rf_bit = 0;

	wcnss_switch_to_gpio();

	/* Reset the signal if it is already being used. */
	gpio_set_value(WLAN_SET, 0);
	gpio_set_value(WLAN_CLK, 0);

	/* We start with cmd_set high WLAN_SET = 1. */
	gpio_set_value(WLAN_SET, 1);

	gpio_direction_output(WLAN_DATA0, 1);
	gpio_direction_output(WLAN_DATA1, 1);
	gpio_direction_output(WLAN_DATA2, 1);

	gpio_set_value(WLAN_DATA0, 0);
	gpio_set_value(WLAN_DATA1, 0);
	gpio_set_value(WLAN_DATA2, 0);

	/* Prepare command and RF register address that need to sent out.
	 * Make sure that we send only 14 bits from LSB.
	 */
	rf_cmd_and_addr  = (((WLAN_RF_READ_REG_CMD) |
		(rf_reg_addr << WLAN_RF_REG_ADDR_START_OFFSET)) &
		WLAN_RF_READ_CMD_MASK);

	for (count = 0; count < 5; count++) {
		gpio_set_value(WLAN_CLK, 0);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(WLAN_DATA0, rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(WLAN_DATA1, rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		rf_bit = (rf_cmd_and_addr & 0x1);
		gpio_set_value(WLAN_DATA2, rf_bit ? 1 : 0);
		rf_cmd_and_addr = (rf_cmd_and_addr >> 1);

		/* Send the data out WLAN_CLK = 1 */
		gpio_set_value(WLAN_CLK, 1);
	}

	/* Pull down the clock signal */
	gpio_set_value(WLAN_CLK, 0);

	/* Configure data pins to input IO pins */
	gpio_direction_input(WLAN_DATA0);
	gpio_direction_input(WLAN_DATA1);
	gpio_direction_input(WLAN_DATA2);

	for (count = 0; count < 2; count++) {
		gpio_set_value(WLAN_CLK, 1);
		gpio_set_value(WLAN_CLK, 0);
	}

	rf_bit = 0;
	for (count = 0; count < 6; count++) {
		gpio_set_value(WLAN_CLK, 1);
		gpio_set_value(WLAN_CLK, 0);

		rf_bit = gpio_get_value(WLAN_DATA0);
		rf_data_received |= (rf_bit << (count * 3 + 0));

		if (count != 5) {
			rf_bit = gpio_get_value(WLAN_DATA1);
			rf_data_received |= (rf_bit << (count * 3 + 1));

			rf_bit = gpio_get_value(WLAN_DATA2);
			rf_data_received |= (rf_bit << (count * 3 + 2));
		}
	}

	gpio_set_value(WLAN_SET, 0);
	wcnss_switch_to_5wire();

	return rf_data_received;
}
