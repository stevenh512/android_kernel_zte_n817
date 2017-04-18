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
 */

#include <linux/module.h> //add by ZTE_BOOT
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <linux/msm_tsens.h>
#include <asm/mach/map.h>
#include <asm/arch_timer.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/restart.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <linux/regulator/qpnp-regulator.h>
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/msm_smem.h>
#include <linux/msm_thermal.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"
#include "spm.h"
#include "pm.h"
#include "modem_notifier.h"

//ZTE_RIL_RIL_20130615 begin
#include <linux/memblock.h>
#include <mach/boot_shared_imem_cookie.h>
//ZTE_RIL_RIL_20130615 end
#include <linux/export.h>

//ZTE_RIL_RIL_20130615 begin

//physical address to reserve for sdlog
#define MSM_SDLOG_PHYS      0x1C000000
#define MSM_SDLOG_SIZE      (SZ_1M * 16)
#define SDLOG_MEM_RESERVED_COOKIE    0x20130615

//ZTE_RIL_RIL_20130615 end
/*
 * Add RAM console support by ZTE_BOOT_JIA_20130121 jia.jia
 */
#ifdef CONFIG_ZTE_RAM_CONSOLE
/*
 * Set to the last 1MB region of the first 'System RAM'
 * and the region allocated by '___alloc_bootmem' should be considered
 */
#define MSM_RAM_CONSOLE_PHYS         (0x1EF00000) /* Refer to 'debug.c' in aboot */

/* add RAM console part below */
#define MSM_RAM_CONSOLE_PHYS_PART_0  (MSM_RAM_CONSOLE_PHYS)
#define MSM_RAM_CONSOLE_SIZE_PART_0  (SZ_1M - 0x4000) //Top 16K is used for RPMB secure channel
#define MSM_RAM_CONSOLE_SIZE_TOTAL   (MSM_RAM_CONSOLE_SIZE_PART_0)
#define MSM_RAM_CONSOLE_NUM          (1)
/* add RAM console part above */

#endif /* CONFIG_ZTE_RAM_CONSOLE */

/*
 * Support for FTM & RECOVERY mode by ZTE_BOOT
 */
#ifdef CONFIG_ZTE_BOOT_MODE
#define SOCINFO_CMDLINE_BOOTMODE          "androidboot.mode="
#define SOCINFO_CMDLINE_BOOTMODE_NORMAL   "normal"
#define SOCINFO_CMDLINE_BOOTMODE_FTM      "ftm"
#define SOCINFO_CMDLINE_BOOTMODE_RECOVERY "recovery"
#define SOCINFO_CMDLINE_BOOTMODE_FFBM     "ffbm"

static int __init bootmode_init(char *mode)
{
	int boot_mode = 0;

	if (!strncmp(mode, SOCINFO_CMDLINE_BOOTMODE_NORMAL, strlen(SOCINFO_CMDLINE_BOOTMODE_NORMAL)))
	{
		boot_mode = BOOT_MODE_NORMAL;
	}
	else if (!strncmp(mode, SOCINFO_CMDLINE_BOOTMODE_FTM, strlen(SOCINFO_CMDLINE_BOOTMODE_FTM)))
	{
		boot_mode = BOOT_MODE_FTM;
	}
	else if (!strncmp(mode, SOCINFO_CMDLINE_BOOTMODE_RECOVERY, strlen(SOCINFO_CMDLINE_BOOTMODE_RECOVERY)))
	{
		boot_mode = BOOT_MODE_RECOVERY;
	}
        else if (!strncmp(mode, SOCINFO_CMDLINE_BOOTMODE_FFBM, strlen(SOCINFO_CMDLINE_BOOTMODE_FFBM)))
        {
                boot_mode = BOOT_MODE_FFBM;
        }

	else
	{
		boot_mode = BOOT_MODE_NORMAL;
	}

       pr_err("rxz,%s: boot_mode: %d\n", __func__, boot_mode);
	socinfo_set_boot_mode(boot_mode);

	return 1;
}

#if 0 // To fix compiling error
__setup(SOCINFO_CMDLINE_BOOTMODE, bootmode_init);
#else
static const char __setup_str_bootmode_init[] __initconst __aligned(1) = SOCINFO_CMDLINE_BOOTMODE;
static struct obs_kernel_param __setup_bootmode_init __used __section(.init.setup) __attribute__((aligned((sizeof(long))))) =
	{ SOCINFO_CMDLINE_BOOTMODE, bootmode_init, 0 };
#endif
#endif /* CONFIG_ZTE_BOOT_MODE */

static struct memtype_reserve msm8610_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm8610_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct of_dev_auxdata msm8610_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	{}
};

static struct reserve_info msm8610_reserve_info __initdata = {
	.memtype_reserve_table = msm8610_reserve_table,
	.paddr_to_memtype = msm8610_paddr_to_memtype,
};

static void __init msm8610_early_memory(void)
{
	reserve_info = &msm8610_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8610_reserve_table);
}

static void __init msm8610_reserve(void)
{
	reserve_info = &msm8610_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8610_reserve_table);
	msm_reserve();
}

/*
 * FTM device driver support by ZTE_BOOT_JIA_20130107 jia.jia
 */
#ifdef CONFIG_ZTE_FTM
static struct platform_device zte_ftm_device = {
	.name = "zte_ftm",
	.id = 0,
};
#endif

/*
 * Add RAM console support by ZTE_BOOT_JIA_20130121 jia.jia
 */
#ifdef CONFIG_ZTE_RAM_CONSOLE
static struct resource ram_console_resource[MSM_RAM_CONSOLE_NUM] = {
	/* add RAM console part below */
	{
		.start  = MSM_RAM_CONSOLE_PHYS_PART_0,
		.end    = MSM_RAM_CONSOLE_PHYS_PART_0 + MSM_RAM_CONSOLE_SIZE_PART_0 - 1,
		.flags	= IORESOURCE_MEM,
	},
	/* add RAM console part above */
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources  = MSM_RAM_CONSOLE_NUM,
	.resource       = ram_console_resource,
};
#endif /* CONFIG_ZTE_RAM_CONSOLE */

static struct platform_device *zte_msm8610_common_devices[] = {
/*
 * Add RAM console support by ZTE_BOOT_JIA_20130121 jia.jia
*/
#ifdef CONFIG_ZTE_RAM_CONSOLE
	&ram_console_device,
#endif
/*
 * FTM device driver support by ZTE_BOOT_JIA_20130107 jia.jia
 */
#ifdef CONFIG_ZTE_FTM
	&zte_ftm_device,
#endif
/*
 * add other devices
 */
};
#if 0
static ssize_t syna_vkeys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(
		buf,__stringify(EV_KEY) ":" __stringify(KEY_BACK) ":85:846:100:70"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":240:856:100:70"
		":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":390:846:100:70"
		"\n");	
}
static struct kobj_attribute syna_vkeys_attr = {
	.attr = {
		.mode = S_IRUGO,
	},
	.show = &syna_vkeys_show,
};

static struct attribute *syna_properties_attrs[] = {
	&syna_vkeys_attr.attr,
	NULL
};

static struct attribute_group syna_properties_attr_group = {
	.attrs = syna_properties_attrs,
};
#if defined CONFIG_GT9XX_TOUCHPANEL_DRIVER_ZTE

static ssize_t goodix_vkeys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(
		buf,__stringify(EV_KEY) ":" __stringify(KEY_BACK) ":50:520:90:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":160:520:90:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":270:520:90:60"
		"\n");	
}

static struct kobj_attribute goodix_vkeys_attr = {
	.attr = {
		.mode = S_IRUGO,
	},
	.show = &goodix_vkeys_show,
};

static struct attribute *goodix_properties_attrs[] = {
	&goodix_vkeys_attr.attr,
	NULL
};

static struct attribute_group goodix_properties_attr_group = {
	.attrs = goodix_properties_attrs,
};
#endif
static void syna_init_vkeys_8610(void)
{
	int rc = 0;
	static struct kobject *syna_properties_kobj;

	syna_properties_kobj = kobject_create_and_add("board_properties",
								NULL);	
	syna_vkeys_attr.attr.name = "virtualkeys.syna-touchscreen";

	if (syna_properties_kobj)
		rc = sysfs_create_group(syna_properties_kobj,
					&syna_properties_attr_group);
	if (!syna_properties_kobj || rc)
		pr_err("%s: failed to create board_properties\n",
				__func__);
#ifdef CONFIG_GT9XX_TOUCHPANEL_DRIVER_ZTE	
	goodix_vkeys_attr.attr.name = "virtualkeys.goodix-touchscreen";

	if (syna_properties_kobj)
		rc = sysfs_create_group(syna_properties_kobj,
					&goodix_properties_attr_group);
	if (!syna_properties_kobj || rc)
		pr_err("%s: failed to create board_properties\n",
				__func__);	
#endif

	return;
}
#endif
#if 1 //added by ZTE_BOOT
static void __init msm8610_init_buses(void)
{
	platform_add_devices(zte_msm8610_common_devices,
				ARRAY_SIZE(zte_msm8610_common_devices));
};
#endif

//ZTE_RIL_RIL_20130615 begin
static void msm_sdlog_init(void)
{
    //reserve app memory for sdlog
    int reserver = 0;
    
    struct boot_shared_imem_cookie_type *boot_shared_imem_ptr = (struct boot_shared_imem_cookie_type *)MSM_IMEM_BASE;
    
    if (boot_shared_imem_ptr->app_mem_reserved == SDLOG_MEM_RESERVED_COOKIE)
    {
        pr_err("sdlog is enable, resever %d buffer in 0x%x \n", MSM_SDLOG_SIZE, MSM_SDLOG_PHYS);
        reserver = memblock_reserve(MSM_SDLOG_PHYS, MSM_SDLOG_SIZE);

        if (reserver != 0)
        {
            pr_err("sdlog reserve memory failed, disable sdlog \n");
            boot_shared_imem_ptr->app_mem_reserved = 0;
        }
        
    }
    else
    {
        pr_err("sdlog is disabled \n");
    }

}
//ZTE_RIL_RIL_20130615 end

/*
 * Support for reading board ID by ZTE_BOOT_RXZ_20140418, ruan.xianzhang
 */
#ifdef CONFIG_ZTE_BOARD_ID
#if defined (CONFIG_BOARD_WELLINGTON)
typedef enum {
    HW_VER_01      = 0, /* [GPIO46,GPIO75,GPIO48]=[0,0,0]*/
    HW_VER_02      = 1, /* [GPIO46,GPIO75,GPIO48]=[0,0,1]*/
    HW_VER_03      = 2, /* [GPIO46,GPIO75,GPIO48]=[0,1,0]*/
    HW_VER_04      = 3, /* [GPIO46,GPIO75,GPIO48]=[0,1,1]*/
    HW_VER_05      = 4, /* [GPIO46,GPIO75,GPIO48]=[1,0,0]*/
    HW_VER_06      = 5, /* [GPIO46,GPIO75,GPIO48]=[1,0,1]*/
    HW_VER_07      = 6, /* [GPIO46,GPIO75,GPIO48]=[1,1,0]*/
    HW_VER_08      = 7, /* [GPIO46,GPIO75,GPIO48]=[1,1,1]*/
    HW_VER_INVALID = 8,
    HW_VER_MAX
} zte_hw_ver_type;
#else
typedef enum {
    HW_VER_01      = 0, /* [GPIO95,GPIO96]=[0,0]*/
    HW_VER_02      = 1, /* [GPIO95,GPIO96]=[0,1]*/
    HW_VER_03      = 2, /* [GPIO95,GPIO96]=[1,0]*/
    HW_VER_04      = 3, /* [GPIO95,GPIO96]=[1,1]*/
    HW_VER_INVALID = 4,
    HW_VER_MAX
} zte_hw_ver_type;
#endif

#if defined(CONFIG_BOARD_WELLINGTON)
    /*N817*/
static const char *zte_hw_ver_str[HW_VER_MAX] = {
    [HW_VER_01]     = "INVALID",
    [HW_VER_02]     = "CQ5110V2.0",
    [HW_VER_03]     = "CQ5110V3.0",
    [HW_VER_04]     = "CQ5110V1.0",
    [HW_VER_05]     = "INVALID",
    [HW_VER_06]     = "INVALID",
    [HW_VER_07]     = "INVALID",
    [HW_VER_08]     = "INVALID",
    [HW_VER_INVALID]= "INVALID"
};
#elif defined (CONFIG_BOARD_DEMI)
    /*P821A16*/
static const char *zte_hw_ver_str[HW_VER_MAX] = {
    [HW_VER_01]     = "wxxA",
    [HW_VER_02]     = "wxxB",
    [HW_VER_03]     = "wxxC",
    [HW_VER_04]     = "wxxD",
    [HW_VER_INVALID]= "INVALID"
};
#elif defined (CONFIG_BOARD_GIANT)
    /*P821T05/P821T05_TF*/
static const char *zte_hw_ver_str[HW_VER_MAX] = {
    [HW_VER_01]     = "wqpA",
    [HW_VER_02]     = "wqpB",
    [HW_VER_03]     = "wqpC",
    [HW_VER_04]     = "wqpD",
    [HW_VER_INVALID]= "INVALID"
};
#else
static const char *zte_hw_ver_str[HW_VER_MAX] = {
    [HW_VER_01]     = "wyuA",
    [HW_VER_02]     = "wyuB",
    [HW_VER_03]     = "wyuC",
    [HW_VER_04]     = "wyuD",
    [HW_VER_INVALID]= "INVALID"
};
#endif

uint8_t hw_version=0;

/*only get global variable hw_version,without readding gipo*/
uint8_t get_zte_hw_ver_byte(void)
{
    if (hw_version >= HW_VER_MAX)
    {
        pr_err("%s: invalid HW version: %d\n", __func__, hw_version);
        hw_version = HW_VER_INVALID;
    }
    pr_err("%s:rxz,hw_version_byte=%d\n", __func__,hw_version);
    return hw_version;
}

EXPORT_SYMBOL_GPL(get_zte_hw_ver_byte);

/*only get global variable zte_hw_ver_str,without readding gipo*/
const char* get_zte_hw_ver(void)
{

    if (hw_version >= HW_VER_MAX)
    {
        pr_err("%s: invalid HW version: %d\n", __func__, hw_version);
        hw_version = HW_VER_INVALID;
    }
    pr_err("%s:rxz,hw_version=%s\n", __func__,zte_hw_ver_str[hw_version]);
    return zte_hw_ver_str[hw_version];
}

EXPORT_SYMBOL_GPL(get_zte_hw_ver);

/*really read gpio to get hw id*/
#if defined(CONFIG_BOARD_WELLINGTON)
const char* read_zte_hw_ver(void)
{
    zte_hw_ver_type hw_ver = HW_VER_INVALID;
    int32_t val_gpio_46 = 0,val_gpio_75 = 0, val_gpio_48 = 0;
    int32_t rc = 0;

    rc = gpio_request(46, "gpio46_zte_hw_ver");
    if (!rc) {
        gpio_tlmm_config(GPIO_CFG(46, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        rc = gpio_direction_input(46);
        if (!rc) {
            val_gpio_46 = gpio_get_value_cansleep(46);
        }
        gpio_free(46);
    }

    rc = gpio_request(75, "gpio75_zte_hw_ver");
    if (!rc) {
        gpio_tlmm_config(GPIO_CFG(75, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        rc = gpio_direction_input(75);
        if (!rc) {
            val_gpio_75 = gpio_get_value_cansleep(75);
        }
        gpio_free(75);
    }

    rc = gpio_request(48, "gpio48_zte_hw_ver");
    if (!rc) {
        gpio_tlmm_config(GPIO_CFG(48, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        rc = gpio_direction_input(48);
        if (!rc) {
            val_gpio_48 = gpio_get_value_cansleep(48);
        }
        gpio_free(48);
    }

    pr_err("%s:zte val_gpio_46=%d\n", __func__,val_gpio_46);
    pr_err("%s:zte val_gpio_75=%d\n", __func__,val_gpio_75);
    pr_err("%s:zte val_gpio_48=%d\n", __func__,val_gpio_48);

    hw_ver = (zte_hw_ver_type)((val_gpio_46 << 2)|(val_gpio_75 << 1) | val_gpio_48);
    pr_err("%s:zte hw_ver=%d\n",  __func__,hw_ver);
    if (hw_ver >= HW_VER_MAX) {
        pr_err("%s: invalid HW version: %d\n", __func__, hw_ver);
        hw_ver = HW_VER_INVALID;
    }
    hw_version = hw_ver;
    return zte_hw_ver_str[hw_ver];
}

#else
const char* read_zte_hw_ver(void)
{
    zte_hw_ver_type hw_ver = HW_VER_INVALID;
    int32_t val_gpio_95 = 0, val_gpio_96 = 0;
    int32_t rc = 0;

    rc = gpio_request(95, "gpio95_zte_hw_ver");
    if (!rc) {
        gpio_tlmm_config(GPIO_CFG(95, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        rc = gpio_direction_input(95);
        if (!rc) {
            val_gpio_95 = gpio_get_value_cansleep(95);
        }
        gpio_free(95);
    }

    rc = gpio_request(96, "gpio96_zte_hw_ver");
    if (!rc) {
        gpio_tlmm_config(GPIO_CFG(96, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        rc = gpio_direction_input(96);
        if (!rc) {
            val_gpio_96 = gpio_get_value_cansleep(96);
        }
        gpio_free(96);
    }
    pr_err("%s:rxz,val_gpio_96=%d\n", __func__,val_gpio_96);
    pr_err("%s:rxz,val_gpio_95=%d\n", __func__,val_gpio_95);
    hw_ver = (zte_hw_ver_type)((val_gpio_95 << 1) | val_gpio_96);
    pr_err("%s:rxz,hw_ver=%d\n",  __func__,hw_ver);
    if (hw_ver >= HW_VER_MAX) {
        pr_err("%s: invalid HW version: %d\n", __func__, hw_ver);
        hw_ver = HW_VER_INVALID;
    }
    hw_version = hw_ver;
    return zte_hw_ver_str[hw_ver];
}
#endif
EXPORT_SYMBOL_GPL(read_zte_hw_ver);

//#endif
#endif

void __init msm8610_add_drivers(void)
{
	msm_smem_init();
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	rpm_regulator_smd_driver_init();
	qpnp_regulator_init();
	tsens_tm_init_driver();
	msm_thermal_device_init();

	if (of_board_is_rumi())
		msm_clock_init(&msm8610_rumi_clock_init_data);
	else
		msm_clock_init(&msm8610_clock_init_data);
#if 1 //added by ZTE_BOOT
        msm8610_init_buses();
#endif
	//syna_init_vkeys_8610();		
}

#if defined (CONFIG_ZTE_SUPPORT_RPM_CMD_FOR_RFCLK45_COMBINED)
extern unsigned char rfclk45_request_noirq(unsigned int value);	// used by board-8610.c for power on rpm feature mode set
#endif

void __init msm8610_init(void)
{
	//zte jiangfeng add
	unsigned smem_size;
	unsigned long long	zte_boot_reason	=	0;
	//zte jiangfeng add, end
	struct of_dev_auxdata *adata = msm8610_auxdata_lookup;
#ifdef CONFIG_ZTE_BOARD_ID
      const char *hw_ver = NULL;
#endif

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm8610_init_gpiomux();
	board_dt_populate(adata);
	msm8610_add_drivers();
#ifdef CONFIG_ZTE_BOARD_ID
    hw_ver = read_zte_hw_ver();
    /*read board id test*/
    pr_err("%s: rxz,hw_ver=%s\n", __func__,hw_ver);
    socinfo_sync_sysfs_zte_hw_ver(hw_ver);
#endif

#if defined (CONFIG_ZTE_SUPPORT_RPM_CMD_FOR_RFCLK45_COMBINED)
	// power up config the rpm zte specific feature to ON MODE
	rfclk45_request_noirq(1);	// 1 = INNER_ENABLE_CMD_VALUE
#endif

//zte jiangfeng add
	zte_boot_reason = *(unsigned int *)
		(smem_get_entry(SMEM_POWER_ON_STATUS_INFO, &smem_size));

	boot_reason = zte_boot_reason & 0xffffffff;
	boot_reason_1 = zte_boot_reason >> 32;
	printk(KERN_NOTICE "Boot Reason = 0x%02x\n", boot_reason);
//zte jiangfeng add, end
}

static const char *msm8610_dt_match[] __initconst = {
	"qcom,msm8610",
	NULL
};

static void __init msm8610_map_io(void)
{
	msm_map_msm8610_io();
    msm_sdlog_init();
}

DT_MACHINE_START(MSM8610_DT, "Qualcomm MSM 8610 (Flattened Device Tree)")
	.map_io = msm8610_map_io,//modify by ruijiagui for sdlog
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8610_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8610_dt_match,
	.restart = msm_restart,
	.reserve = msm8610_reserve,
	.init_very_early = msm8610_early_memory,
	.smp = &arm_smp_ops,
MACHINE_END
