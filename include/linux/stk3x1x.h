/*
 *
 * Id: stk3x1x.h
 *
 * Copyright (C) 2012 Lex Hsieh     <lex_hsieh@sitronix.com.tw>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */
#ifndef __STK3X1X_H__
#define __STK3X1X_H__

// ioctl numbers
#define STK_IOCTL_MAGIC        	0XCF
#define STK_IOCTL_ALS_ON       	_IO(STK_IOCTL_MAGIC, 1)
#define STK_IOCTL_ALS_OFF      	_IO(STK_IOCTL_MAGIC, 2)
#define STK_IOCTL_ALS_DATA     	_IOR(STK_IOCTL_MAGIC, 3, short)
#define STK_IOCTL_ALS_CALIBRATE	_IO(STK_IOCTL_MAGIC, 4)
#define STK_IOCTL_CONFIG_GET   	_IOR(STK_IOCTL_MAGIC, 5, struct stk_cfg)
#define STK_IOCTL_CONFIG_SET		_IOW(STK_IOCTL_MAGIC, 6, struct stk_cfg)
#define STK_IOCTL_PROX_ON		_IO(STK_IOCTL_MAGIC, 7)
#define STK_IOCTL_PROX_OFF		_IO(STK_IOCTL_MAGIC, 8)
#define STK_IOCTL_PROX_DATA		_IOR(STK_IOCTL_MAGIC, 9, struct stk_prox_info)
#define STK_IOCTL_PROX_EVENT           _IO(STK_IOCTL_MAGIC, 10)
#define STK_IOCTL_PROX_CALIBRATE	_IO(STK_IOCTL_MAGIC, 11)
#define STK_IOCTL_PROX_GET_ENABLED   	_IOR(STK_IOCTL_MAGIC, 12, int*)
#define STK_IOCTL_ALS_GET_ENABLED   	_IOR(STK_IOCTL_MAGIC, 13, int*)

struct stk_cfg {
	u32	calibrate_target;
	u16	als_time;
	u16	scale_factor;
	u16	gain_trim;
 	u8	filter_history;
	u8	filter_count;
	u8	gain;
	u16	prox_threshold_hi;
	u16	prox_threshold_lo;
	u8	prox_int_time;
	u8	prox_adc_time;
	u8	prox_wait_time;
	u8	prox_intr_filter;
	u8	prox_config;
	u8	prox_pulse_cnt;
	u8	prox_gain;
};

// proximity data
struct stk_prox_info {
        u16 prox_clear;
        u16 prox_data;
        int prox_event;
};

/* platform data */
struct stk3x1x_platform_data {
	uint8_t state_reg;
	uint8_t psctrl_reg;
	uint8_t alsctrl_reg;
	uint8_t ledctrl_reg;
	uint8_t wait_reg;
	uint16_t ps_thd_h;
	uint16_t ps_thd_l;
	int int_pin;
	uint32_t transmittance;
	uint32_t int_flags;
	bool use_fir;
};

#endif /* __STK3X1X_H__ */
