/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/smp.h>
#include <linux/tick.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/cpu_pm.h>
#include <linux/remote_spinlock.h>
#include <asm/uaccess.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <mach/remote_spinlock.h>
#include <mach/scm.h>
#include <mach/msm_bus.h>
#include <mach/jtag.h>
#include <mach/socinfo.h>
#include "acpuclock.h"
#include "avs.h"
#include "idle.h"
#include "pm.h"
#include "scm-boot.h"
#include "spm.h"
#include "pm-boot.h"
#include "clock.h"

#define CREATE_TRACE_POINTS
#include <mach/trace_msm_low_power.h>

#define SCM_CMD_TERMINATE_PC	(0x2)
#define SCM_CMD_CORE_HOTPLUGGED (0x10)

#define GET_CPU_OF_ATTR(attr) \
	(container_of(attr, struct msm_pm_kobj_attribute, ka)->cpu)

#define SCLK_HZ (32768)

#define MAX_BUF_SIZE  512

static int msm_pm_debug_mask = 1;
module_param_named(
	debug_mask, msm_pm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static bool use_acpuclk_apis;

#ifndef ZTE_GPIO_DEBUG
#define ZTE_GPIO_DEBUG
#endif

enum {
	MSM_PM_DEBUG_SUSPEND = BIT(0),
	MSM_PM_DEBUG_POWER_COLLAPSE = BIT(1),
	MSM_PM_DEBUG_SUSPEND_LIMITS = BIT(2),
	MSM_PM_DEBUG_CLOCK = BIT(3),
	MSM_PM_DEBUG_RESET_VECTOR = BIT(4),
	MSM_PM_DEBUG_IDLE_CLK = BIT(5),
	MSM_PM_DEBUG_IDLE = BIT(6),
	MSM_PM_DEBUG_IDLE_LIMITS = BIT(7),
	MSM_PM_DEBUG_HOTPLUG = BIT(8),
#ifdef ZTE_GPIO_DEBUG
	MSM_PM_DEBUG_ZTE_LOGS = BIT(9),//zte's customize log,default not open
#endif
};

enum msm_pc_count_offsets {
	MSM_PC_ENTRY_COUNTER,
	MSM_PC_EXIT_COUNTER,
	MSM_PC_FALLTHRU_COUNTER,
	MSM_PC_NUM_COUNTERS,
};

enum {
	MSM_PM_MODE_ATTR_SUSPEND,
	MSM_PM_MODE_ATTR_IDLE,
	MSM_PM_MODE_ATTR_NR,
};

static char *msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_NR] = {
	[MSM_PM_MODE_ATTR_SUSPEND] = "suspend_enabled",
	[MSM_PM_MODE_ATTR_IDLE] = "idle_enabled",
};

struct msm_pm_kobj_attribute {
	unsigned int cpu;
	struct kobj_attribute ka;
};

struct msm_pm_sysfs_sleep_mode {
	struct kobject *kobj;
	struct attribute_group attr_group;
	struct attribute *attrs[MSM_PM_MODE_ATTR_NR + 1];
	struct msm_pm_kobj_attribute kas[MSM_PM_MODE_ATTR_NR];
};

static char *msm_pm_sleep_mode_labels[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = "power_collapse",
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = "wfi",
	[MSM_PM_SLEEP_MODE_RETENTION] = "retention",
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] =
		"standalone_power_collapse",
};

static bool msm_pm_ldo_retention_enabled = true;
static bool msm_no_ramp_down_pc;
static struct msm_pm_sleep_status_data *msm_pm_slp_sts;
DEFINE_PER_CPU(struct clk *, cpu_clks);
static struct clk *l2_clk;

static int cpu_count;
static DEFINE_SPINLOCK(cpu_cnt_lock);
#define SCM_HANDOFF_LOCK_ID "S:7"
static bool need_scm_handoff_lock;
static remote_spinlock_t scm_handoff_lock;

static void (*msm_pm_disable_l2_fn)(void);
static void (*msm_pm_enable_l2_fn)(void);
static void (*msm_pm_flush_l2_fn)(void);
static void __iomem *msm_pc_debug_counters;

/*
 * Default the l2 flush flag to OFF so the caches are flushed during power
 * collapse unless the explicitly voted by lpm driver.
 */
static enum msm_pm_l2_scm_flag msm_pm_flush_l2_flag = MSM_SCM_L2_OFF;

void msm_pm_set_l2_flush_flag(enum msm_pm_l2_scm_flag flag)
{
	msm_pm_flush_l2_flag = flag;
}
EXPORT_SYMBOL(msm_pm_set_l2_flush_flag);

static enum msm_pm_l2_scm_flag msm_pm_get_l2_flush_flag(void)
{
	return msm_pm_flush_l2_flag;
}

static cpumask_t retention_cpus;
static DEFINE_SPINLOCK(retention_lock);

static int msm_pm_get_pc_mode(struct device_node *node,
		const char *key, uint32_t *pc_mode_val)
{
	struct pc_mode_of {
		uint32_t mode;
		char *mode_name;
	};
	int i;
	struct pc_mode_of pc_modes[] = {
				{MSM_PM_PC_TZ_L2_INT, "tz_l2_int"},
				{MSM_PM_PC_NOTZ_L2_EXT, "no_tz_l2_ext"},
				{MSM_PM_PC_TZ_L2_EXT , "tz_l2_ext"} };
	int ret;
	const char *pc_mode_str;
	*pc_mode_val = MSM_PM_PC_TZ_L2_INT;

	ret = of_property_read_string(node, key, &pc_mode_str);
	if (!ret) {
		ret = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(pc_modes); i++) {
			if (!strncmp(pc_mode_str, pc_modes[i].mode_name,
				strlen(pc_modes[i].mode_name))) {
				*pc_mode_val = pc_modes[i].mode;
				ret = 0;
				break;
			}
		}
	} else {
		pr_debug("%s: Cannot read %s,defaulting to 0", __func__, key);
		ret = 0;
	}
	return ret;
}

/*
 * Write out the attribute.
 */
static ssize_t msm_pm_mode_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct kernel_param kp;
		unsigned int cpu;
		struct msm_pm_platform_data *mode;

		if (msm_pm_sleep_mode_labels[i] == NULL)
			continue;

		if (strcmp(kobj->name, msm_pm_sleep_mode_labels[i]))
			continue;

		cpu = GET_CPU_OF_ATTR(attr);
		mode = &msm_pm_sleep_modes[MSM_PM_MODE(cpu, i)];

		if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_SUSPEND])) {
			u32 arg = mode->suspend_enabled;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_IDLE])) {
			u32 arg = mode->idle_enabled;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		}

		break;
	}

	if (ret > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		ret++;
	}

	return ret;
}

static ssize_t msm_pm_mode_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct kernel_param kp;
		unsigned int cpu;
		struct msm_pm_platform_data *mode;

		if (msm_pm_sleep_mode_labels[i] == NULL)
			continue;

		if (strcmp(kobj->name, msm_pm_sleep_mode_labels[i]))
			continue;

		cpu = GET_CPU_OF_ATTR(attr);
		mode = &msm_pm_sleep_modes[MSM_PM_MODE(cpu, i)];

		if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_SUSPEND])) {
			kp.arg = &mode->suspend_enabled;
			ret = param_set_byte(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_IDLE])) {
			kp.arg = &mode->idle_enabled;
			ret = param_set_byte(buf, &kp);
		}

		break;
	}

	return ret ? ret : count;
}

static int msm_pm_mode_sysfs_add_cpu(
	unsigned int cpu, struct kobject *modes_kobj)
{
	char cpu_name[8];
	struct kobject *cpu_kobj;
	struct msm_pm_sysfs_sleep_mode *mode = NULL;
	int i, j, k;
	int ret;

	snprintf(cpu_name, sizeof(cpu_name), "cpu%u", cpu);
	cpu_kobj = kobject_create_and_add(cpu_name, modes_kobj);
	if (!cpu_kobj) {
		pr_err("%s: cannot create %s kobject\n", __func__, cpu_name);
		ret = -ENOMEM;
		goto mode_sysfs_add_cpu_exit;
	}

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		int idx = MSM_PM_MODE(cpu, i);

		if ((!msm_pm_sleep_modes[idx].suspend_supported)
			&& (!msm_pm_sleep_modes[idx].idle_supported))
			continue;

		if (!msm_pm_sleep_mode_labels[i] ||
				!msm_pm_sleep_mode_labels[i][0])
			continue;

		mode = kzalloc(sizeof(*mode), GFP_KERNEL);
		if (!mode) {
			pr_err("%s: cannot allocate memory for attributes\n",
				__func__);
			ret = -ENOMEM;
			goto mode_sysfs_add_cpu_exit;
		}

		mode->kobj = kobject_create_and_add(
				msm_pm_sleep_mode_labels[i], cpu_kobj);
		if (!mode->kobj) {
			pr_err("%s: cannot create kobject\n", __func__);
			ret = -ENOMEM;
			goto mode_sysfs_add_cpu_exit;
		}

		for (k = 0, j = 0; k < MSM_PM_MODE_ATTR_NR; k++) {
			if ((k == MSM_PM_MODE_ATTR_IDLE) &&
				!msm_pm_sleep_modes[idx].idle_supported)
				continue;
			if ((k == MSM_PM_MODE_ATTR_SUSPEND) &&
			     !msm_pm_sleep_modes[idx].suspend_supported)
				continue;
			sysfs_attr_init(&mode->kas[j].ka.attr);
			mode->kas[j].cpu = cpu;
			mode->kas[j].ka.attr.mode = 0644;
			mode->kas[j].ka.show = msm_pm_mode_attr_show;
			mode->kas[j].ka.store = msm_pm_mode_attr_store;
			mode->kas[j].ka.attr.name = msm_pm_mode_attr_labels[k];
			mode->attrs[j] = &mode->kas[j].ka.attr;
			j++;
		}
		mode->attrs[j] = NULL;

		mode->attr_group.attrs = mode->attrs;
		ret = sysfs_create_group(mode->kobj, &mode->attr_group);
		if (ret) {
			pr_err("%s: cannot create kobject attribute group\n",
				__func__);
			goto mode_sysfs_add_cpu_exit;
		}
	}

	ret = 0;

mode_sysfs_add_cpu_exit:
	if (ret) {
		if (mode && mode->kobj)
			kobject_del(mode->kobj);
		kfree(mode);
	}

	return ret;
}

int msm_pm_mode_sysfs_add(void)
{
	struct kobject *module_kobj;
	struct kobject *modes_kobj;
	unsigned int cpu;
	int ret;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		ret = -ENOENT;
		goto mode_sysfs_add_exit;
	}

	modes_kobj = kobject_create_and_add("modes", module_kobj);
	if (!modes_kobj) {
		pr_err("%s: cannot create modes kobject\n", __func__);
		ret = -ENOMEM;
		goto mode_sysfs_add_exit;
	}

	for_each_possible_cpu(cpu) {
		ret = msm_pm_mode_sysfs_add_cpu(cpu, modes_kobj);
		if (ret)
			goto mode_sysfs_add_exit;
	}

	ret = 0;

mode_sysfs_add_exit:
	return ret;
}

static inline void msm_arch_idle(void)
{
	mb();
	wfi();
}

static bool msm_pm_is_L1_writeback(void)
{
	u32 sel = 0, cache_id;

	asm volatile ("mcr p15, 2, %[ccselr], c0, c0, 0\n\t"
		      "isb\n\t"
		      "mrc p15, 1, %[ccsidr], c0, c0, 0\n\t"
		      :[ccsidr]"=r" (cache_id)
		      :[ccselr]"r" (sel)
		     );
	return cache_id & BIT(31);
}

static enum msm_pm_time_stats_id msm_pm_swfi(bool from_idle)
{
	msm_arch_idle();
	return MSM_PM_STAT_IDLE_WFI;
}

static enum msm_pm_time_stats_id msm_pm_retention(bool from_idle)
{
	int ret = 0;
	int cpu = smp_processor_id();
	int saved_rate = 0;
	struct clk *cpu_clk = per_cpu(cpu_clks, cpu);

	spin_lock(&retention_lock);

	if (!msm_pm_ldo_retention_enabled)
		goto bailout;

	cpumask_set_cpu(cpu, &retention_cpus);
	spin_unlock(&retention_lock);

	if (use_acpuclk_apis)
		saved_rate = acpuclk_power_collapse();
	else
		clk_disable(cpu_clk);

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_POWER_RETENTION, false);
	WARN_ON(ret);

	msm_arch_idle();

	if (use_acpuclk_apis) {
		if (acpuclk_set_rate(cpu, saved_rate, SETRATE_PC))
			pr_err("%s(): Error setting acpuclk_set_rate\n",
					__func__);
	} else {
		if (clk_enable(cpu_clk))
			pr_err("%s(): Error restoring cpu clk\n", __func__);
	}

	spin_lock(&retention_lock);
	cpumask_clear_cpu(cpu, &retention_cpus);
bailout:
	spin_unlock(&retention_lock);

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);
	WARN_ON(ret);

	return MSM_PM_STAT_RETENTION;
}

static inline void msm_pc_inc_debug_count(uint32_t cpu,
		enum msm_pc_count_offsets offset)
{
	uint32_t cnt;

	if (!msm_pc_debug_counters)
		return;

	cnt = readl_relaxed(msm_pc_debug_counters + cpu * 4 + offset * 4);
	writel_relaxed(++cnt, msm_pc_debug_counters + cpu * 4 + offset * 4);
	mb();
}

static bool msm_pm_pc_hotplug(void)
{
	uint32_t cpu = smp_processor_id();

	if (msm_pm_is_L1_writeback())
		flush_cache_louis();

	msm_pc_inc_debug_count(cpu, MSM_PC_ENTRY_COUNTER);

	scm_call_atomic1(SCM_SVC_BOOT, SCM_CMD_TERMINATE_PC,
			SCM_CMD_CORE_HOTPLUGGED);

	/* Should not return here */
	msm_pc_inc_debug_count(cpu, MSM_PC_FALLTHRU_COUNTER);
	return 0;
}

static int msm_pm_collapse(unsigned long unused)
{
	uint32_t cpu = smp_processor_id();
	enum msm_pm_l2_scm_flag flag = MSM_SCM_L2_ON;

	spin_lock(&cpu_cnt_lock);
	cpu_count++;
	if (cpu_count == num_online_cpus())
		flag = msm_pm_get_l2_flush_flag();

	pr_debug("cpu:%d cores_in_pc:%d L2 flag: %d\n",
			cpu, cpu_count, flag);

	/*
	 * The scm_handoff_lock will be release by the secure monitor.
	 * It is used to serialize power-collapses from this point on,
	 * so that both Linux and the secure context have a consistent
	 * view regarding the number of running cpus (cpu_count).
	 *
	 * It must be acquired before releasing cpu_cnt_lock.
	 */
	if (need_scm_handoff_lock)
		remote_spin_lock_rlock_id(&scm_handoff_lock,
					  REMOTE_SPINLOCK_TID_START + cpu);
	spin_unlock(&cpu_cnt_lock);

	if (flag == MSM_SCM_L2_OFF) {
		flush_cache_all();
		if (msm_pm_flush_l2_fn)
			msm_pm_flush_l2_fn();
	} else if (msm_pm_is_L1_writeback())
		flush_cache_louis();

	if (msm_pm_disable_l2_fn)
		msm_pm_disable_l2_fn();

	msm_pc_inc_debug_count(cpu, MSM_PC_ENTRY_COUNTER);

	scm_call_atomic1(SCM_SVC_BOOT, SCM_CMD_TERMINATE_PC, flag);

	msm_pc_inc_debug_count(cpu, MSM_PC_FALLTHRU_COUNTER);

	if (msm_pm_enable_l2_fn)
		msm_pm_enable_l2_fn();

	return 0;
}

static bool __ref msm_pm_spm_power_collapse(
	unsigned int cpu, bool from_idle, bool notify_rpm)
{
	void *entry;
	bool collapsed = 0;
	int ret;
	bool save_cpu_regs = !cpu || from_idle;

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: notify_rpm %d\n",
			cpu, __func__, (int) notify_rpm);

	if (from_idle)
		cpu_pm_enter();

	ret = msm_spm_set_low_power_mode(
			MSM_SPM_MODE_POWER_COLLAPSE, notify_rpm);
	WARN_ON(ret);

	entry = save_cpu_regs ?  cpu_resume : msm_secondary_startup;

	msm_pm_boot_config_before_pc(cpu, virt_to_phys(entry));

	if (MSM_PM_DEBUG_RESET_VECTOR & msm_pm_debug_mask)
		pr_info("CPU%u: %s: program vector to %p\n",
			cpu, __func__, entry);

	msm_jtag_save_state();

	collapsed = save_cpu_regs ?
		!cpu_suspend(0, msm_pm_collapse) : msm_pm_pc_hotplug();

	if (save_cpu_regs) {
		spin_lock(&cpu_cnt_lock);
		cpu_count--;
		BUG_ON(cpu_count > num_online_cpus());
		spin_unlock(&cpu_cnt_lock);
	}
	msm_jtag_restore_state();

	if (collapsed) {
		cpu_init();
		local_fiq_enable();
	}

	msm_pm_boot_config_after_pc(cpu);

	if (from_idle)
		cpu_pm_exit();

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: msm_pm_collapse returned, collapsed %d\n",
			cpu, __func__, collapsed);

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);
	WARN_ON(ret);
	return collapsed;
}

static enum msm_pm_time_stats_id msm_pm_power_collapse_standalone(
		bool from_idle)
{
	unsigned int cpu = smp_processor_id();
	unsigned int avsdscr;
	unsigned int avscsr;
	bool collapsed;

	avsdscr = avs_get_avsdscr();
	avscsr = avs_get_avscsr();
	avs_set_avscsr(0); /* Disable AVS */

	collapsed = msm_pm_spm_power_collapse(cpu, from_idle, false);

	avs_set_avsdscr(avsdscr);
	avs_set_avscsr(avscsr);
	return collapsed ? MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE :
			MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE;
}

static int ramp_down_last_cpu(int cpu)
{
	struct clk *cpu_clk = per_cpu(cpu_clks, cpu);
	int ret = 0;

	if (use_acpuclk_apis) {
		ret = acpuclk_power_collapse();
		if (MSM_PM_DEBUG_CLOCK & msm_pm_debug_mask)
			pr_info("CPU%u: %s: change clk rate(old rate = %d)\n",
					cpu, __func__, ret);
	} else {
		clk_disable(cpu_clk);
		clk_disable(l2_clk);
	}
	return ret;
}

static int ramp_up_first_cpu(int cpu, int saved_rate)
{
	struct clk *cpu_clk = per_cpu(cpu_clks, cpu);
	int rc = 0;

	if (MSM_PM_DEBUG_CLOCK & msm_pm_debug_mask)
		pr_info("CPU%u: %s: restore clock rate\n",
				cpu, __func__);

	if (use_acpuclk_apis) {
		rc = acpuclk_set_rate(cpu, saved_rate, SETRATE_PC);
		if (rc)
			pr_err("CPU:%u: Error restoring cpu clk\n", cpu);
	} else {
		if (l2_clk) {
			rc = clk_enable(l2_clk);
			if (rc)
				pr_err("%s(): Error restoring l2 clk\n",
						__func__);
		}

		if (cpu_clk) {
			int ret = clk_enable(cpu_clk);

			if (ret) {
				pr_err("%s(): Error restoring cpu clk\n",
						__func__);
				return ret;
			}
		}
	}

	return rc;
}

static enum msm_pm_time_stats_id msm_pm_power_collapse(bool from_idle)
{
	unsigned int cpu = smp_processor_id();
	unsigned long saved_acpuclk_rate = 0;
	unsigned int avsdscr;
	unsigned int avscsr;
	bool collapsed;

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: idle %d\n",
			cpu, __func__, (int)from_idle);

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: pre power down\n", cpu, __func__);

	/* This spews a lot of messages when a core is hotplugged. This
	 * information is most useful from last core going down during
	 * power collapse
	 */
	if ((!from_idle && cpu_online(cpu))
			|| (MSM_PM_DEBUG_IDLE_CLK & msm_pm_debug_mask))
		clock_debug_print_enabled();

	avsdscr = avs_get_avsdscr();
	avscsr = avs_get_avscsr();
	avs_set_avscsr(0); /* Disable AVS */

	if (cpu_online(cpu) && !msm_no_ramp_down_pc)
		saved_acpuclk_rate = ramp_down_last_cpu(cpu);

	collapsed = msm_pm_spm_power_collapse(cpu, from_idle, true);

	if (cpu_online(cpu) && !msm_no_ramp_down_pc)
		ramp_up_first_cpu(cpu, saved_acpuclk_rate);

	avs_set_avsdscr(avsdscr);
	avs_set_avscsr(avscsr);

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: post power up\n", cpu, __func__);

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: return\n", cpu, __func__);
	return collapsed ? MSM_PM_STAT_IDLE_POWER_COLLAPSE :
			MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE;
}
/******************************************************************************
 * External Idle/Suspend Functions
 *****************************************************************************/

void arch_idle(void)
{
	return;
}

static inline void msm_pm_ftrace_lpm_enter(unsigned int cpu,
		uint32_t latency, uint32_t sleep_us,
		uint32_t wake_up,
		enum msm_pm_sleep_mode mode)
{
	switch (mode) {
	case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
		trace_msm_pm_enter_wfi(cpu, latency, sleep_us, wake_up);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
		trace_msm_pm_enter_spc(cpu, latency, sleep_us, wake_up);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		trace_msm_pm_enter_pc(cpu, latency, sleep_us, wake_up);
		break;
	case MSM_PM_SLEEP_MODE_RETENTION:
		trace_msm_pm_enter_ret(cpu, latency, sleep_us, wake_up);
		break;
	default:
		break;
	}
}

static inline void msm_pm_ftrace_lpm_exit(unsigned int cpu,
		enum msm_pm_sleep_mode mode,
		bool success)
{
	switch (mode) {
	case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
		trace_msm_pm_exit_wfi(cpu, success);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
		trace_msm_pm_exit_spc(cpu, success);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		trace_msm_pm_exit_pc(cpu, success);
		break;
	case MSM_PM_SLEEP_MODE_RETENTION:
		trace_msm_pm_exit_ret(cpu, success);
		break;
	default:
		break;
	}
}

static enum msm_pm_time_stats_id (*execute[MSM_PM_SLEEP_MODE_NR])(bool idle) = {
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = msm_pm_swfi,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] =
		msm_pm_power_collapse_standalone,
	[MSM_PM_SLEEP_MODE_RETENTION] = msm_pm_retention,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = msm_pm_power_collapse,
};

bool msm_cpu_pm_check_mode(unsigned int cpu, enum msm_pm_sleep_mode mode,
		bool from_idle)
{
	int idx = MSM_PM_MODE(cpu, mode);
	struct msm_pm_platform_data *d = &msm_pm_sleep_modes[idx];

	if ((mode == MSM_PM_SLEEP_MODE_RETENTION)
			&& !msm_pm_ldo_retention_enabled)
		return false;

	if (from_idle)
		return d->idle_enabled && d->idle_supported;
	else
		return d->suspend_enabled && d->suspend_supported;
}

#ifdef ZTE_GPIO_DEBUG
extern int msm_dump_gpios(struct seq_file *m, int curr_len, char *gpio_buffer);
extern int pmic_dump_pins(struct seq_file *m, int curr_len, char *gpio_buffer);
static char *gpio_sleep_status_info;

int print_gpio_buffer(struct seq_file *m)
{
	if (gpio_sleep_status_info)
		seq_printf(m, gpio_sleep_status_info);
	else
		seq_printf(m, "Device haven't suspended yet!\n");

	return 0;
}
EXPORT_SYMBOL(print_gpio_buffer);

int free_gpio_buffer(void)
{
	kfree(gpio_sleep_status_info);
	gpio_sleep_status_info = NULL;

	return 0;
}
EXPORT_SYMBOL(free_gpio_buffer);
#endif



#ifndef ZTE_PM_NOTIFY_MODEM_APP_SUSPENDED
//#define ZTE_PM_NOTIFY_MODEM_APP_SUSPENDED
#endif

#ifdef ZTE_PM_NOTIFY_MODEM_APP_SUSPENDED
static zte_smem_global*    zte_global  =   NULL;   //zte jiangfeng
#endif


#ifndef CONFIG_ZTE_PLATFORM_RECORD_APP_AWAKE_SUSPEND_TIME 
#define CONFIG_ZTE_PLATFORM_RECORD_APP_AWAKE_SUSPEND_TIME
#endif

#ifdef CONFIG_ZTE_PLATFORM_RECORD_APP_AWAKE_SUSPEND_TIME

#define MSM_PM_DPRINTK(mask, level, message, ...) \
	do { \
		if ((mask) & msm_pm_debug_mask) \
			printk(level message, ## __VA_ARGS__); \
	} while (0)


long lateresume_2_earlysuspend_time_s = 0;		//LHX_PM_20110411_01 time to record how long it takes to earlysuspend after last resume. namely,to record how long the LCD keeps on.
void zte_update_lateresume_2_earlysuspend_time(bool resume_or_earlysuspend)	// LHX_PM_20110411_01 resume_or_earlysuspend? lateresume : earlysuspend
{
	if(resume_or_earlysuspend)//lateresume,need to record when the lcd is turned on
	{
		lateresume_2_earlysuspend_time_s = current_kernel_time().tv_sec;
	}
	else	//earlysuspend,need to record when the lcd is turned off
	{
		lateresume_2_earlysuspend_time_s = current_kernel_time().tv_sec - lateresume_2_earlysuspend_time_s;	//record how long the lcd keeps on
	}
}

#include <linux/io.h>
#include <mach/boot_shared_imem_cookie.h>
#include <mach/msm_iomap.h>
#include "clock.h"

static struct boot_shared_imem_cookie_type *zte_imem_ptr = 
    (struct boot_shared_imem_cookie_type *)MSM_IMEM_BASE;

#if 0
static ssize_t pm_monitor_amss_sleep_time_show(struct device *devp, struct device_attribute *attr, char *buf)
{
	char * echo = buf;	
	int msleeptimeinsecs=zte_imem_ptr->modemsleeptime;
	if(msleeptimeinsecs==-1)
		msleeptimeinsecs=0;
	
	echo += sprintf(echo, "%d", (int)(msleeptimeinsecs/100));
	
	return echo-buf;
}
static DEVICE_ATTR(amss_sleep_time, S_IRUGO, pm_monitor_amss_sleep_time_show, NULL);
#endif
unsigned pm_modem_sleep_time_get(void)
{
	pr_info("ZTE_PM get modemsleeptime %d \n",zte_imem_ptr->modemsleeptime);
	return zte_imem_ptr->modemsleeptime;
}

unsigned pm_modem_phys_link_time_get(void)/////liukejing add 20130606 
{
	pr_info("ZTE_PM get physlinktime %d\n",zte_imem_ptr->physlinktime);
	return zte_imem_ptr->physlinktime;
}

unsigned pm_modem_awake_time_get(int *current_sleep)
{
	*current_sleep =  zte_imem_ptr->modemsleep_or_awake;
	pr_info("ZTE_PM get modemawaketime %d,current_sleep=%d \n",zte_imem_ptr->modemawaketime,*current_sleep);
	return zte_imem_ptr->modemawaketime;
}



/*BEGIN LHX_PM_20110324_01 add code to record how long the APP sleeps or keeps awake*/
#define AMSS_NEVER_ENTER_SLEEP 0x4//lianghouxing 20121119 add to indicate modem is sleep or awake,0 sleep, 1 awake,4 means never enter sleep yet
#define AMSS_NOW_SLEEP 0x0
#define AMSS_NOW_AWAKE 0x1
#define THRESOLD_FOR_OFFLINE_AWAKE_TIME 100 //ms,modem awake time is less than this,conside modem is set as offline.
#define THRESOLD_FOR_OFFLINE_TIME 5000 //5s

static int zte_enable2record_flag = 0;
module_param_named(zte_enable2record,
	zte_enable2record_flag, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define RECORED_TOTAL_TIME
struct timespec time_updated_when_sleep_awake;
void record_sleep_awake_time(bool record_sleep_awake)
{
	//record_sleep_awake?: true?record awake time, else record  sleep time
	struct timespec ts;
	unsigned time_updated_when_sleep_awake_s;
	#ifdef RECORED_TOTAL_TIME
	static bool record_firsttime = true;
	static bool record_firsttime_modem = true;
	static	unsigned time_modem_firsttime_awake_s=0;
	static	unsigned time_modem_firsttime_sleep_s =0;
	static	unsigned time_app_total_awake_s=0;
	static	unsigned time_app_total_sleep_s =0;
	static unsigned time_lcdon_total_s=0;
	#endif
	unsigned time_updated_when_sleep_awake_ms;
	unsigned time_updated_when_sleep_awake_ms_temp;
	static unsigned amss_sleep_time_ms = 0;
	static unsigned amss_physlink_current_total_time_s = 0;/////liukejing 20130606 add
	static unsigned amss_physlink_last_total_time_s = 0;/////liukejing 20130606 add
	unsigned amss_sleep_time_ms_temp = 0;
	unsigned deta_sleep_ms = 0;
	unsigned deta_awake_ms = 0;
	unsigned deta_physlink_s = 0;/////liukejing 20130606 add
//	bool offlinemode = false;
	unsigned amss_awake_last =0;
	
	unsigned amss_current_sleep_or_awake=0;//indicate modem is sleep or awake,1 means never enter sleep yet,2 sleep, 3 awake
	static unsigned  amss_current_sleep_or_awake_previous=0;
	
	static unsigned amss_awake_time_ms = 0;
	unsigned amss_awake_time_ms_temp = 0;
	bool get_amss_awake_ok = false;// true:get amss_awake time,false get amss_sleep_time

	unsigned percentage_amss_not_sleep_while_app_suspend = 0;	//the percentage of modem awake while app suspend in %o;
	static bool sleep_success_flag = false;  //set true while msm_pm_collapse returned 1 by passing record_sleep_awake as true;

//	if (!((MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE) & msm_pm_debug_mask) )
	//	return ;

if(0 ==zte_enable2record_flag)
{
	pr_info("[ZTE_PM]: not enable to record when app enter to suspend or resume yet! \n");
	return;
}
	ts = current_kernel_time();
	
	time_updated_when_sleep_awake_ms_temp =(unsigned) ((ts.tv_sec - time_updated_when_sleep_awake.tv_sec) * MSEC_PER_SEC + ((ts.tv_nsec / NSEC_PER_MSEC) - (time_updated_when_sleep_awake.tv_nsec / NSEC_PER_MSEC)));
	time_updated_when_sleep_awake_s = (time_updated_when_sleep_awake_ms_temp/MSEC_PER_SEC);
	time_updated_when_sleep_awake_ms = (time_updated_when_sleep_awake_ms_temp - time_updated_when_sleep_awake_s * MSEC_PER_SEC);

	if(record_sleep_awake)//record app awake time
	{
		sleep_success_flag = true;

		amss_sleep_time_ms_temp = amss_sleep_time_ms;	//backup previous total sleep time
		amss_sleep_time_ms  = pm_modem_sleep_time_get();	//get new total sleep time
		deta_sleep_ms = amss_sleep_time_ms - amss_sleep_time_ms_temp;
		//printk("  during this %s: modem sleep pre: %d  new %d s,modem sleep this time: %d ms\n",record_sleep_awake?"awake":"sleep",(int)amss_sleep_time_ms_temp ,amss_sleep_time_ms,deta_sleep_ms);

		amss_awake_time_ms_temp = amss_awake_time_ms;	//backup previous total sleep time
		amss_awake_time_ms  = pm_modem_awake_time_get(&amss_current_sleep_or_awake);	//get new total sleep time
		deta_awake_ms = amss_awake_time_ms - amss_awake_time_ms_temp;
		
		amss_physlink_current_total_time_s=pm_modem_phys_link_time_get();///liukejing 20130606 add to get total phys link time
		deta_physlink_s=amss_physlink_current_total_time_s - amss_physlink_last_total_time_s;
		//pr_info("print phys link time------awake------:amss_physlink_current_total_time_s %10d s amss_physlink_last_total_time_s %10d s\n", 
						//amss_physlink_current_total_time_s,amss_physlink_last_total_time_s);
		amss_physlink_last_total_time_s = amss_physlink_current_total_time_s;
//		printk("  during this %s: modem awake pre: %d  new %d s,modem awake this time: %d ms \n",record_sleep_awake?"awake":"sleep",(int)amss_awake_time_ms_temp ,amss_awake_time_ms,deta_awake_ms);
	//	printk(" modem total sleep: %d  ms,modem total awake: %d ms \n",amss_sleep_time_ms,amss_awake_time_ms);
/*
amss_current_sleep_or_awake_previous  amss_current_sleep_or_awake 
X 4 ---modem not enter sleep yet
0 0 ---previous is sleep,curret is sleep, modem awake time is updated,get awake deta directly.
otherwise get modem sleep time.
if modem is set to offline,print offline in the log
*/
		if((AMSS_NOW_SLEEP ==amss_current_sleep_or_awake_previous)&&(AMSS_NOW_SLEEP ==amss_current_sleep_or_awake))//00,get modem awake time
		{
			if((deta_awake_ms < THRESOLD_FOR_OFFLINE_AWAKE_TIME ) && (THRESOLD_FOR_OFFLINE_TIME < time_updated_when_sleep_awake_ms_temp))//if sleep time is 0 and awake is 0,offline mode
			{
				pr_info("[ZTE_PM] offline mode \n");
			}
			get_amss_awake_ok = true;
			amss_awake_last = deta_awake_ms;
		}
		else if(AMSS_NEVER_ENTER_SLEEP == amss_current_sleep_or_awake)
		{
			pr_info("[ZTE_PM] modem not enter sleep yet \n");
		}
		
		if(!get_amss_awake_ok)
		{
			amss_awake_last = time_updated_when_sleep_awake_ms_temp - deta_sleep_ms;
		}
		//printk("during this  %s: amss_awake_last: %d ms,deta_awake_ms %d ms,deta_sleep_ms this time %d ms\n",record_sleep_awake?"awake":"sleep",amss_awake_last,deta_awake_ms,deta_sleep_ms);
		percentage_amss_not_sleep_while_app_suspend = (amss_awake_last * 1000/(time_updated_when_sleep_awake_ms_temp + 1));

#ifdef RECORED_TOTAL_TIME
		if(!record_firsttime)
			{
				time_app_total_awake_s += time_updated_when_sleep_awake_s;
				time_lcdon_total_s += lateresume_2_earlysuspend_time_s;
			}
				record_firsttime = false;		
#endif
		pr_info("[ZTE_PM] APP wake for %6d.%03d s, lcd on for %6d s %3d %%,modem wake for %10d ms(%s) %4d %%o,modem sleep for %10d ----%d%d\n", 
			time_updated_when_sleep_awake_s,time_updated_when_sleep_awake_ms,(int)lateresume_2_earlysuspend_time_s,(int)(lateresume_2_earlysuspend_time_s*100/(time_updated_when_sleep_awake_s + 1))
			,amss_awake_last,get_amss_awake_ok?"get_directly ":"from sleep_time",percentage_amss_not_sleep_while_app_suspend,deta_sleep_ms,amss_current_sleep_or_awake_previous,amss_current_sleep_or_awake);//in case Division by zero, +1
		pr_info("[ZTE_PM] modem_phys_link_total_time %4d min %4d s,deta_physlink_s %4d min %4d s during app wake\n", 
				amss_physlink_current_total_time_s/60,amss_physlink_current_total_time_s%60,deta_physlink_s/60,deta_physlink_s%60);//liukejing 20130606 add to count phys link time

		time_updated_when_sleep_awake = ts; 
		lateresume_2_earlysuspend_time_s = 0;	//LHX_PM_20110411_01 clear how long the lcd keeps on

	}
	else	//record app sleep time
	{
		if(!sleep_success_flag) //only record sleep time while really resume after successfully suspend/sleep;
		{
			printk("[ZTE_PM] app resume due to fail to suspend\n");
			return;
		}
		sleep_success_flag = false;

		amss_sleep_time_ms_temp = amss_sleep_time_ms;	//backup previous total sleep time
//		amss_sleep_time_ms  = pm_modem_sleep_time_get() / MSEC_PER_SEC;	//get new total sleep time
		amss_sleep_time_ms  = pm_modem_sleep_time_get();	//get new total sleep time
		deta_sleep_ms = amss_sleep_time_ms - amss_sleep_time_ms_temp;
		//printk("  during this %s: modem sleep pre: %d  new %d s,modem sleep this time: %d ms\n",record_sleep_awake?"awake":"sleep",(int)amss_sleep_time_ms_temp ,amss_sleep_time_ms,deta_sleep_ms);
		amss_awake_time_ms_temp = amss_awake_time_ms;	//backup previous total sleep time
	//	amss_awake_time_ms  = pm_modem_awake_time_get(&amss_current_sleep_or_awake) / MSEC_PER_SEC;	//get new total sleep time
		amss_awake_time_ms  = pm_modem_awake_time_get(&amss_current_sleep_or_awake);	//get new total sleep time
		deta_awake_ms = amss_awake_time_ms - amss_awake_time_ms_temp;
		
		amss_physlink_current_total_time_s=pm_modem_phys_link_time_get();///liukejing 20130606 add to get total phys link time
		deta_physlink_s=amss_physlink_current_total_time_s - amss_physlink_last_total_time_s;
				//pr_info("print phys link time------sleep------:amss_physlink_current_total_time_s %10d s amss_physlink_last_total_time_s %10d s\n", 
						//amss_physlink_current_total_time_s,amss_physlink_last_total_time_s);
		amss_physlink_last_total_time_s = amss_physlink_current_total_time_s;
		//printk("  during this %s: modem awake pre: %d  new %d s,modem awake this time: %d ms \n",record_sleep_awake?"awake":"sleep",(int)amss_awake_time_ms_temp ,amss_awake_time_ms,deta_awake_ms);
		if((AMSS_NOW_SLEEP ==amss_current_sleep_or_awake_previous)&&(AMSS_NOW_SLEEP ==amss_current_sleep_or_awake))//00,get modem awake time
		{
			if((deta_awake_ms < THRESOLD_FOR_OFFLINE_AWAKE_TIME ) && (THRESOLD_FOR_OFFLINE_TIME < time_updated_when_sleep_awake_ms_temp))//if sleep time is 0 and awake is 0,offline mode
			{
				pr_info("[ZTE_PM] offline mode \n");
			}
			get_amss_awake_ok = true;
			amss_awake_last = deta_awake_ms;
		}
		else if(AMSS_NEVER_ENTER_SLEEP == amss_current_sleep_or_awake)
		{
			pr_info("[ZTE_PM] modem not enter sleep yet \n");
		}
		
		if(!get_amss_awake_ok)
		{
			amss_awake_last = time_updated_when_sleep_awake_ms_temp - deta_sleep_ms;
		}
#ifdef RECORED_TOTAL_TIME
		time_app_total_sleep_s += time_updated_when_sleep_awake_s;
		if(record_firsttime_modem)
		{
			time_modem_firsttime_awake_s =		amss_awake_last/1000;	
			time_modem_firsttime_sleep_s =		amss_sleep_time_ms/1000;	
			record_firsttime_modem= false;		
		}
		pr_info("[ZTE_PM] modem total sleep: %d	s,modem total awake: %d s app total sleep: %d	s,app total awake: %d s,lcdon total: %d s\n",(amss_sleep_time_ms/1000 - time_modem_firsttime_sleep_s),(amss_awake_time_ms/1000-time_modem_firsttime_awake_s),time_app_total_sleep_s,time_app_total_awake_s,time_lcdon_total_s);
#endif
		percentage_amss_not_sleep_while_app_suspend = (amss_awake_last * 1000/(time_updated_when_sleep_awake_ms_temp + 1));
		pr_info("[ZTE_PM] APP sleep for %10d.%03d s, modem wake for %10d ms(%s) %4d %%o,modem_sleep for %10d ----%d%d\n",time_updated_when_sleep_awake_s,time_updated_when_sleep_awake_ms,amss_awake_last,get_amss_awake_ok?"get_directly ":"from sleep_time",percentage_amss_not_sleep_while_app_suspend,deta_sleep_ms,amss_current_sleep_or_awake_previous,amss_current_sleep_or_awake);
		pr_info("[ZTE_PM] modem_phys_link_total_time %4d min %4d s, deta_physlink_s %4d min %4d s during app sleep\n", 
				amss_physlink_current_total_time_s/60,amss_physlink_current_total_time_s%60,deta_physlink_s/60,deta_physlink_s%60);//liukejing 20130606 add to count phys link time
		time_updated_when_sleep_awake = ts; 
	}
	
	amss_current_sleep_or_awake_previous = amss_current_sleep_or_awake;
}
/*End LHX_PM_20110324_01 add code to record how long the APP sleeps or keeps awake*/
#endif


/*
zte_pm_before_powercollapse called before enter PowerCollapse from suspend,which will record app state ,and do gpio dump.
*/
void zte_pm_before_powercollapse(void)
{
#ifdef ZTE_GPIO_DEBUG
	int curr_len = 0;
	/*
	 * Default not open the gpio dump,too much logs...
	*/

	do{
		if (MSM_PM_DEBUG_ZTE_LOGS & msm_pm_debug_mask) {
			if (gpio_sleep_status_info) {
				memset(gpio_sleep_status_info, 0,
					sizeof(gpio_sleep_status_info));
			} else {
				gpio_sleep_status_info = kmalloc(25000, GFP_KERNEL);
				if (!gpio_sleep_status_info) {
					pr_err("[PM] kmalloc memory failed in %s\n",
						__func__);
					break;
				}
			}

			curr_len = msm_dump_gpios(NULL, curr_len,	gpio_sleep_status_info);
			curr_len = pmic_dump_pins(NULL, curr_len,
							gpio_sleep_status_info);
		}
	}
	while(0);

	if (MSM_PM_DEBUG_ZTE_LOGS & msm_pm_debug_mask) {
		if (gpio_sleep_status_info) {
			memset(gpio_sleep_status_info, 0,
				sizeof(gpio_sleep_status_info));
		} else {
			gpio_sleep_status_info = kmalloc(25000, GFP_KERNEL);
			if (!gpio_sleep_status_info) {
				pr_err("[PM] kmalloc memory failed in %s\n",
					__func__);
				goto skip_dump;
			}
		}

		curr_len = msm_dump_gpios(NULL, curr_len,	gpio_sleep_status_info);
		curr_len = pmic_dump_pins(NULL, curr_len,
						gpio_sleep_status_info);
	}
	skip_dump:

#endif

#ifdef ZTE_PM_NOTIFY_MODEM_APP_SUSPENDED
		//zte jiangfeng
		if(zte_global	==	NULL)
			zte_global= ioremap(ZTE_SMEM_LOG_GLOBAL_BASE, 
				sizeof(zte_smem_global));
		if(zte_global)
			zte_global->app_suspend_state	=	0xAA;
		//zte jiangfeng, end
#endif

#ifdef CONFIG_ZTE_PLATFORM_RECORD_APP_AWAKE_SUSPEND_TIME
		//clock_debug_print_enabled();
		record_sleep_awake_time(true);//LHX_PM_20110324_01 add code to record how long the APP sleeps or keeps awake 
#endif		
}

/*
zte_pm_before_powercollapse called after exit PowerCollapse from suspend,which will inform modem app has exit suspend.
*/
void zte_pm_after_powercollapse(void)
{
#ifdef ZTE_PM_NOTIFY_MODEM_APP_SUSPENDED
	if(zte_global)
		zte_global->app_suspend_state	=	0;		//zte jiangfeng
#endif
}

int msm_cpu_pm_enter_sleep(enum msm_pm_sleep_mode mode, bool from_idle)
{
	int64_t time;
	bool collapsed = 1;
	int exit_stat = -1;

	if (MSM_PM_DEBUG_IDLE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: mode %d\n",
			smp_processor_id(), __func__, mode);
	if (!from_idle)
		pr_info("CPU%u: %s mode:%d\n",
			smp_processor_id(), __func__, mode);

	time = sched_clock();
	if((MSM_PM_SLEEP_MODE_POWER_COLLAPSE ==mode) && (!from_idle)) //suspend -> PC,need to record
	{
		zte_pm_before_powercollapse();
	}

	if (execute[mode])
		exit_stat = execute[mode](from_idle);
	if((MSM_PM_SLEEP_MODE_POWER_COLLAPSE ==mode) && (!from_idle)) //exit PC from suspend ,need to record
	{
		zte_pm_after_powercollapse();
	}
	if (from_idle) {
		time = sched_clock() - time;
		msm_pm_ftrace_lpm_exit(smp_processor_id(), mode, collapsed);
		if (exit_stat >= 0)
			msm_pm_add_stat(exit_stat, time);
	}


	return collapsed;
}

int msm_pm_wait_cpu_shutdown(unsigned int cpu)
{
	int timeout = 10;

	if (!msm_pm_slp_sts)
		return 0;
	if (!msm_pm_slp_sts[cpu].base_addr)
		return 0;
	while (1) {
		/*
		 * Check for the SPM of the core being hotplugged to set
		 * its sleep state.The SPM sleep state indicates that the
		 * core has been power collapsed.
		 */
		int acc_sts = __raw_readl(msm_pm_slp_sts[cpu].base_addr);

		if (acc_sts & msm_pm_slp_sts[cpu].mask)
			return 0;
		udelay(100);
		WARN(++timeout == 20, "CPU%u didn't collapse in 2 ms\n", cpu);
	}

	return -EBUSY;
}

void msm_pm_cpu_enter_lowpower(unsigned int cpu)
{
	int i;
	bool allow[MSM_PM_SLEEP_MODE_NR];

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct msm_pm_platform_data *mode;

		mode = &msm_pm_sleep_modes[MSM_PM_MODE(cpu, i)];
		allow[i] = mode->suspend_supported && mode->suspend_enabled;
	}

	if (MSM_PM_DEBUG_HOTPLUG & msm_pm_debug_mask)
		pr_notice("CPU%u: %s: shutting down cpu\n", cpu, __func__);

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE])
		msm_pm_power_collapse(false);
	else if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE])
		msm_pm_power_collapse_standalone(false);
	else if (allow[MSM_PM_SLEEP_MODE_RETENTION])
		msm_pm_retention(false);
	else
		msm_pm_swfi(false);
}

static void msm_pm_ack_retention_disable(void *data)
{
	/*
	 * This is a NULL function to ensure that the core has woken up
	 * and is safe to disable retention.
	 */
}
/**
 * msm_pm_enable_retention() - Disable/Enable retention on all cores
 * @enable: Enable/Disable retention
 *
 */
void msm_pm_enable_retention(bool enable)
{
	if (enable == msm_pm_ldo_retention_enabled)
		return;

	msm_pm_ldo_retention_enabled = enable;

	/*
	 * If retention is being disabled, wakeup all online core to ensure
	 * that it isn't executing retention. Offlined cores need not be woken
	 * up as they enter the deepest sleep mode, namely RPM assited power
	 * collapse
	 */
	if (!enable) {
		preempt_disable();
		smp_call_function_many(&retention_cpus,
				msm_pm_ack_retention_disable,
				NULL, true);
		preempt_enable();
	}
}
EXPORT_SYMBOL(msm_pm_enable_retention);

static int msm_pm_snoc_client_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct msm_bus_scale_pdata *msm_pm_bus_pdata;
	static uint32_t msm_pm_bus_client;

	msm_pm_bus_pdata = msm_bus_cl_get_pdata(pdev);

	if (msm_pm_bus_pdata) {
		msm_pm_bus_client =
			msm_bus_scale_register_client(msm_pm_bus_pdata);

		if (!msm_pm_bus_client) {
			pr_err("%s: Failed to register SNOC client", __func__);
			rc = -ENXIO;
			goto snoc_cl_probe_done;
		}

		rc = msm_bus_scale_client_update_request(msm_pm_bus_client, 1);

		if (rc)
			pr_err("%s: Error setting bus rate", __func__);
	}

snoc_cl_probe_done:
	return rc;
}

static int msm_cpu_status_probe(struct platform_device *pdev)
{
	struct msm_pm_sleep_status_data *pdata;
	char *key;
	u32 cpu;

	if (!pdev)
		return -EFAULT;

	msm_pm_slp_sts = devm_kzalloc(&pdev->dev,
			sizeof(*msm_pm_slp_sts) * num_possible_cpus(),
			GFP_KERNEL);

	if (!msm_pm_slp_sts)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		struct resource *res;
		u32 offset;
		int rc;
		u32 mask;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			return -ENODEV;

		key = "qcom,cpu-alias-addr";
		rc = of_property_read_u32(pdev->dev.of_node, key, &offset);

		if (rc)
			return -ENODEV;

		key = "qcom,sleep-status-mask";
		rc = of_property_read_u32(pdev->dev.of_node, key, &mask);

		if (rc)
			return -ENODEV;

		for_each_possible_cpu(cpu) {
			phys_addr_t base_c = res->start + cpu * offset;
			msm_pm_slp_sts[cpu].base_addr =
				devm_ioremap(&pdev->dev, base_c,
						resource_size(res));
			msm_pm_slp_sts[cpu].mask = mask;

			if (!msm_pm_slp_sts[cpu].base_addr)
				return -ENOMEM;
		}
	} else {
		pdata = pdev->dev.platform_data;
		if (!pdev->dev.platform_data)
			return -EINVAL;

		for_each_possible_cpu(cpu) {
			msm_pm_slp_sts[cpu].base_addr =
				pdata->base_addr + cpu * pdata->cpu_offset;
			msm_pm_slp_sts[cpu].mask = pdata->mask;
		}
	}

	return 0;
};

static struct of_device_id msm_slp_sts_match_tbl[] = {
	{.compatible = "qcom,cpu-sleep-status"},
	{},
};

static struct platform_driver msm_cpu_status_driver = {
	.probe = msm_cpu_status_probe,
	.driver = {
		.name = "cpu_slp_status",
		.owner = THIS_MODULE,
		.of_match_table = msm_slp_sts_match_tbl,
	},
};

static struct of_device_id msm_snoc_clnt_match_tbl[] = {
	{.compatible = "qcom,pm-snoc-client"},
	{},
};

static struct platform_driver msm_cpu_pm_snoc_client_driver = {
	.probe = msm_pm_snoc_client_probe,
	.driver = {
		.name = "pm_snoc_client",
		.owner = THIS_MODULE,
		.of_match_table = msm_snoc_clnt_match_tbl,
	},
};

static int msm_pm_init(void)
{
	enum msm_pm_time_stats_id enable_stats[] = {
		MSM_PM_STAT_IDLE_WFI,
		MSM_PM_STAT_RETENTION,
		MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE,
		MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE,
		MSM_PM_STAT_IDLE_POWER_COLLAPSE,
		MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE,
		MSM_PM_STAT_SUSPEND,
	};
	msm_pm_mode_sysfs_add();
	msm_pm_add_stats(enable_stats, ARRAY_SIZE(enable_stats));

#ifdef ZTE_PM_NOTIFY_MODEM_APP_SUSPENDED
	zte_global	=	ioremap(ZTE_SMEM_LOG_GLOBAL_BASE,
			sizeof(zte_smem_global));	//zte jiangfeng
	if(zte_global)
		zte_global->app_suspend_state	=	0;		//zte jiangfeng
#endif
#if 0
	zte_imem_ptr->modemsleeptime = 1000;
	zte_imem_ptr->modemawaketime = 2000;
	zte_imem_ptr->modemsleep_or_awake = 5;
#endif


#ifdef CONFIG_ZTE_BOOT_MODE
	    if (socinfo_get_ftm_flag() == 1)
	    {
	        zte_enable2record_flag = 1;
			pr_info("ZTE_PM set zte_enable2record_flag to true in FTM mode");
	    }
#endif		
	return 0;
}

static void msm_pm_set_flush_fn(uint32_t pc_mode)
{
	msm_pm_disable_l2_fn = NULL;
	msm_pm_enable_l2_fn = NULL;
	msm_pm_flush_l2_fn = outer_flush_all;

	if (pc_mode == MSM_PM_PC_NOTZ_L2_EXT) {
		msm_pm_disable_l2_fn = outer_disable;
		msm_pm_enable_l2_fn = outer_resume;
	}
}

struct msm_pc_debug_counters_buffer {
	void __iomem *reg;
	u32 len;
	char buf[MAX_BUF_SIZE];
};

static inline u32 msm_pc_debug_counters_read_register(
		void __iomem *reg, int index , int offset)
{
	return readl_relaxed(reg + (index * 4 + offset) * 4);
}

static char *counter_name[] = {
		"PC Entry Counter",
		"Warmboot Entry Counter",
		"PC Bailout Counter"
};

static int msm_pc_debug_counters_copy(
		struct msm_pc_debug_counters_buffer *data)
{
	int j;
	u32 stat;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		data->len += scnprintf(data->buf + data->len,
				sizeof(data->buf)-data->len,
				"CPU%d\n", cpu);

		for (j = 0; j < MSM_PC_NUM_COUNTERS; j++) {
			stat = msm_pc_debug_counters_read_register(
					data->reg, cpu, j);
			data->len += scnprintf(data->buf + data->len,
					sizeof(data->buf)-data->len,
					"\t%s : %d\n", counter_name[j],
					stat);
		}

	}

	return data->len;
}

static int msm_pc_debug_counters_file_read(struct file *file,
		char __user *bufu, size_t count, loff_t *ppos)
{
	struct msm_pc_debug_counters_buffer *data;

	data = file->private_data;

	if (!data)
		return -EINVAL;

	if (!bufu || count < 0)
		return -EINVAL;

	if (!access_ok(VERIFY_WRITE, bufu, count))
		return -EFAULT;

	if (*ppos >= data->len && data->len == 0)
		data->len = msm_pc_debug_counters_copy(data);

	return simple_read_from_buffer(bufu, count, ppos,
			data->buf, data->len);
}

static int msm_pc_debug_counters_file_open(struct inode *inode,
		struct file *file)
{
	struct msm_pc_debug_counters_buffer *buf;
	void __iomem *msm_pc_debug_counters_reg;

	msm_pc_debug_counters_reg = inode->i_private;

	if (!msm_pc_debug_counters_reg)
		return -EINVAL;

	file->private_data = kzalloc(
		sizeof(struct msm_pc_debug_counters_buffer), GFP_KERNEL);

	if (!file->private_data) {
		pr_err("%s: ERROR kmalloc failed to allocate %d bytes\n",
		__func__, sizeof(struct msm_pc_debug_counters_buffer));

		return -ENOMEM;
	}

	buf = file->private_data;
	buf->reg = msm_pc_debug_counters_reg;

	return 0;
}

static int msm_pc_debug_counters_file_close(struct inode *inode,
		struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations msm_pc_debug_counters_fops = {
	.open = msm_pc_debug_counters_file_open,
	.read = msm_pc_debug_counters_file_read,
	.release = msm_pc_debug_counters_file_close,
	.llseek = no_llseek,
};

static int msm_pm_clk_init(struct platform_device *pdev)
{
	bool synced_clocks;
	u32 cpu;
	char clk_name[] = "cpu??_clk";
	bool cpu_as_clocks;
	char *key;

	key = "qcom,cpus-as-clocks";
	cpu_as_clocks = of_property_read_bool(pdev->dev.of_node, key);

	if (!cpu_as_clocks) {
		use_acpuclk_apis = true;
		return 0;
	}

	key = "qcom,synced-clocks";
	synced_clocks = of_property_read_bool(pdev->dev.of_node, key);

	for_each_possible_cpu(cpu) {
		struct clk *clk;
		snprintf(clk_name, sizeof(clk_name), "cpu%d_clk", cpu);
		clk = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(clk)) {
			if (cpu && synced_clocks)
				return 0;
			else
				return PTR_ERR(clk);
		}
		per_cpu(cpu_clks, cpu) = clk;
	}

	if (synced_clocks)
		return 0;

	l2_clk = devm_clk_get(&pdev->dev, "l2_clk");

	return PTR_RET(l2_clk);
}

static int msm_cpu_pm_probe(struct platform_device *pdev)
{
	char *key = NULL;
	struct dentry *dent = NULL;
	struct resource *res = NULL;
	int i;
	struct msm_pm_init_data_type pdata_local;
	struct device_node *lpm_node;
	int ret = 0;

	memset(&pdata_local, 0, sizeof(struct msm_pm_init_data_type));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return  0;
	msm_pc_debug_counters_phys = res->start;
	WARN_ON(resource_size(res) < SZ_64);
	msm_pc_debug_counters = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (msm_pc_debug_counters) {
		for (i = 0; i < resource_size(res)/4; i++)
			__raw_writel(0, msm_pc_debug_counters + i * 4);

		dent = debugfs_create_file("pc_debug_counter", S_IRUGO, NULL,
				msm_pc_debug_counters,
				&msm_pc_debug_counters_fops);
		if (!dent)
			pr_err("%s: ERROR debugfs_create_file failed\n",
					__func__);
	} else {
		msm_pc_debug_counters = 0;
		msm_pc_debug_counters_phys = 0;
	}

	lpm_node = of_parse_phandle(pdev->dev.of_node, "qcom,lpm-levels", 0);
	if (!lpm_node) {
		pr_warn("Could not get qcom,lpm-levels handle\n");
		return -EINVAL;
	}
	need_scm_handoff_lock = of_property_read_bool(lpm_node,
						      "qcom,allow-synced-levels");
	if (need_scm_handoff_lock) {
		ret = remote_spin_lock_init(&scm_handoff_lock,
					    SCM_HANDOFF_LOCK_ID);
		if (ret) {
			pr_err("%s: Failed initializing scm_handoff_lock (%d)\n",
				__func__, ret);
			return ret;
		}
	}

	if (pdev->dev.of_node) {
		enum msm_pm_pc_mode_type pc_mode;

		ret = msm_pm_clk_init(pdev);
		if (ret) {
			pr_info("msm_pm_clk_init returned error\n");
			return ret;
		}

		key = "qcom,pc-mode";
		ret = msm_pm_get_pc_mode(pdev->dev.of_node, key, &pc_mode);
		if (ret) {
			pr_debug("%s: Error reading key %s", __func__, key);
			return -EINVAL;
		}
		msm_pm_set_flush_fn(pc_mode);
	}

	msm_pm_init();
	if (pdev->dev.of_node)
		of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	return ret;
}

static struct of_device_id msm_cpu_pm_table[] = {
	{.compatible = "qcom,pm-8x60"},
	{},
};

static struct platform_driver msm_cpu_pm_driver = {
	.probe = msm_cpu_pm_probe,
	.driver = {
		.name = "pm-8x60",
		.owner = THIS_MODULE,
		.of_match_table = msm_cpu_pm_table,
	},
};

static int __init msm_cpu_pm_init(void)
{
	int rc;

	cpumask_clear(&retention_cpus);

	rc = platform_driver_register(&msm_cpu_pm_snoc_client_driver);

	if (rc) {
		pr_err("%s(): failed to register driver %s\n", __func__,
				msm_cpu_pm_snoc_client_driver.driver.name);
		return rc;
	}

	return platform_driver_register(&msm_cpu_pm_driver);
}
device_initcall(msm_cpu_pm_init);

void __init msm_pm_sleep_status_init(void)
{
	platform_driver_register(&msm_cpu_status_driver);
}
