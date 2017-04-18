/*
 * kernel/power/autosleep.c
 *
 * Opportunistic sleep support.
 *
 * Copyright (C) 2012 Rafael J. Wysocki <rjw@sisk.pl>
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>

#include "power.h"

#ifdef CONFIG_ZTE_HALL_AVAILABLE
extern suspend_state_t new_state_backup;
#endif
static suspend_state_t autosleep_state;
static struct workqueue_struct *autosleep_wq;
/*
 * Note: it is only safe to mutex_lock(&autosleep_lock) if a wakeup_source
 * is active, otherwise a deadlock with try_to_suspend() is possible.
 * Alternatively mutex_lock_interruptible() can be used.  This will then fail
 * if an auto_sleep cycle tries to freeze processes.
 */
static DEFINE_MUTEX(autosleep_lock);
static struct wakeup_source *autosleep_ws;

#ifdef CONFIG_ICE40_IRDA
extern int irda_suspend_screenoff(void);
extern int ice40_resume_screenon(void);
#endif

static void try_to_suspend(struct work_struct *work)
{
	unsigned int initial_count, final_count;

	if (!pm_get_wakeup_count(&initial_count, true))
		goto out;

	mutex_lock(&autosleep_lock);

	if (!pm_save_wakeup_count(initial_count)) {
		mutex_unlock(&autosleep_lock);
		goto out;
	}

	if (autosleep_state == PM_SUSPEND_ON) {
		mutex_unlock(&autosleep_lock);
		return;
	}
	if (autosleep_state >= PM_SUSPEND_MAX)
		hibernate();
	else
		pm_suspend(autosleep_state);

	mutex_unlock(&autosleep_lock);

	if (!pm_get_wakeup_count(&final_count, false))
		goto out;

	/*
	 * If the wakeup occured for an unknown reason, wait to prevent the
	 * system from trying to suspend and waking up in a tight loop.
	 */
	if (final_count == initial_count)
		schedule_timeout_uninterruptible(HZ / 2);

 out:
	queue_up_suspend_work();
}

static DECLARE_WORK(suspend_work, try_to_suspend);

void queue_up_suspend_work(void)
{
	if (!work_pending(&suspend_work) && autosleep_state > PM_SUSPEND_ON)
		queue_work(autosleep_wq, &suspend_work);
}

suspend_state_t pm_autosleep_state(void)
{
	return autosleep_state;
}

int pm_autosleep_lock(void)
{
	return mutex_lock_interruptible(&autosleep_lock);
}

void pm_autosleep_unlock(void)
{
	mutex_unlock(&autosleep_lock);
}

#ifndef CONFIG_ZTE_PLATFORM_SUSPEND_MODULES_WHEN_SCREENOFF
#define CONFIG_ZTE_PLATFORM_SUSPEND_MODULES_WHEN_SCREENOFF //LHX_PM_20131008_01 add to suspend modules such as TouchScreen when screen off,and resume it when LCD on.
#endif
#ifdef CONFIG_ZTE_PLATFORM_SUSPEND_MODULES_WHEN_SCREENOFF 
static bool screen_has_been_off = false;
extern int (*touchscreen_suspend_pm)(void);
extern int (*touchscreen_resume_pm)(void);

extern int (*gsensor_suspend_pm)(void);
extern int (*gsensor_resume_pm)(void);

extern void (*suspend_to_disable_light)(void);
extern void (*resume_to_recover_light)(void);

#endif
#ifndef CONFIG_ZTE_PLATFORM_LCD_ON_TIME
#define CONFIG_ZTE_PLATFORM_LCD_ON_TIME
#endif

#ifdef CONFIG_ZTE_PLATFORM_LCD_ON_TIME
extern void zte_update_lateresume_2_earlysuspend_time(bool resume_or_earlysuspend);	//LHX_PM_20110411_01 resume_or_earlysuspend? lateresume : earlysuspend
#endif
int pm_autosleep_set_state(suspend_state_t state)
{

#ifndef CONFIG_HIBERNATION
	if (state >= PM_SUSPEND_MAX)
		return -EINVAL;
#endif
#ifdef CONFIG_ZTE_HALL_AVAILABLE
	new_state_backup = state; /////liukejing
#endif
	__pm_stay_awake(autosleep_ws);

	mutex_lock(&autosleep_lock);

	autosleep_state = state;

	__pm_relax(autosleep_ws);

	if (state > PM_SUSPEND_ON) {
		pm_wakep_autosleep_enabled(true);
#ifdef CONFIG_ZTE_PLATFORM_LCD_ON_TIME
		zte_update_lateresume_2_earlysuspend_time(false);//LHX_PM_20110411_01,update earlysuspend time
#endif
#ifdef CONFIG_ZTE_PLATFORM_SUSPEND_MODULES_WHEN_SCREENOFF
        screen_has_been_off = true;
		if(touchscreen_suspend_pm)
		{
			pr_info("ZTE_PM: suspend TS \n");
			touchscreen_suspend_pm();
		}
		else
		{
			pr_info("ZTE_PM: touchscreen_suspend_pm = NULL \n ");
		}

		if(gsensor_suspend_pm)
		{
			pr_info("ZTE_PM: suspend gsensor \n");
			gsensor_suspend_pm();
		}
		else
		{
			pr_info("ZTE_PM: gsensor_suspend_pm = NULL \n ");
		}

		if(suspend_to_disable_light)
		{
			pr_info("ZTE_PM: suspend light sensor \n");
			suspend_to_disable_light();
		}
		else
		{
			pr_info("ZTE_PM: suspend_to_disable_light = NULL \n ");
		}


#ifdef CONFIG_ICE40_IRDA
		irda_suspend_screenoff();
#endif

#endif
		queue_up_suspend_work();
	} else {
		pm_wakep_autosleep_enabled(false);
#ifdef CONFIG_ZTE_PLATFORM_LCD_ON_TIME
		zte_update_lateresume_2_earlysuspend_time(true);//LHX_PM_20110411_01 update resume time
#endif

#ifdef CONFIG_ZTE_PLATFORM_SUSPEND_MODULES_WHEN_SCREENOFF

#ifdef CONFIG_ICE40_IRDA
			ice40_resume_screenon();
#endif

		if(touchscreen_resume_pm && screen_has_been_off)
		{
			pr_info("ZTE_PM: resume TS \n");
			touchscreen_resume_pm();
		}
		else
		{
			pr_info("ZTE_PM: touchscreen_resume_pm = NULL or screen_has_been_off ? %s\n ",screen_has_been_off?"YES already":"NOT YET");
		}

		if(gsensor_resume_pm && screen_has_been_off)
		{
			pr_info("ZTE_PM: resume gsensor \n");
			gsensor_resume_pm();
		}
		else
		{
			pr_info("ZTE_PM: gsensor_resume_pm = NULL or screen_has_been_off ? %s\n ",screen_has_been_off?"YES already":"NOT YET");
		}

		if(resume_to_recover_light && screen_has_been_off)
		{
			pr_info("ZTE_PM: resume light sensor \n");
			resume_to_recover_light();
		}
		else
		{
			pr_info("ZTE_PM: resume_to_recover_light = NULL or screen_has_been_off ? %s\n ",screen_has_been_off?"YES already":"NOT YET");
		}


		screen_has_been_off = false;
#endif
	}

	mutex_unlock(&autosleep_lock);
	return 0;
}

int __init pm_autosleep_init(void)
{
	autosleep_ws = wakeup_source_register("autosleep");
	if (!autosleep_ws)
		return -ENOMEM;

	autosleep_wq = alloc_ordered_workqueue("autosleep", 0);
	if (autosleep_wq)
		return 0;

	wakeup_source_unregister(autosleep_ws);
	return -ENOMEM;
}
