/*         ______   ___    ___
 *        /\  _  \ /\_ \  /\_ \
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      Timer driver for the PSVITA.
 *     
 *      Written by 
 *      Amnon-Dan Meir (ammeir)
 *
 *      Base on PSP code by diedel.
 *
 *      See readme.txt for copyright information.
 */

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "psvita.h"
#include <psp2/kernel/threadmgr.h> 
#include <psp2/rtc.h>
#include <pthread.h>

#define TIMER_TO_USEC(t)      ((SceUInt)((float)(t) / TIMERS_PER_SECOND * 1000000))
#define PSVTIMER_TO_TIMER(t)  ((int)((t) * (TIMERS_PER_SECOND / (float)gs_tick_resolution)))
#define USEC_TO_TIMER(x) ((unsigned long)(x) * (TIMERS_PER_SECOND / 1000000))

static int		psv_timer_thread(SceSize args, void *argp);
// Driver function implementations
static int		psv_timer_init(void);
static void		psv_timer_exit(void);
static void		psv_rest(unsigned int time, void (*callback)(void));

static uint32_t gs_tick_resolution;
static int		gs_timer_on = FALSE;
static SceUID	gs_timer_thread_UID = -1;

TIMER_DRIVER timer_psv =
{
   TIMER_PSV,
   empty_string,
   empty_string,
   "PSVITA timer",
   psv_timer_init,
   psv_timer_exit,
   NULL, NULL,       /* install_int, remove_int */
   NULL, NULL,       /* install_param_int, remove_param_int */
   NULL, NULL,       /* can_simulate_retrace, simulate_retrace */
   psv_rest          /* rest */
};

/* psv_timer_thread:
 *  This thread measures the elapsed time.
 */
static int psv_timer_thread(SceSize args, void *argp)
{
	PSV_DEBUG("psv_timer_thread");

	SceRtcTick old_tick, new_tick, tmp_tick;
	int interval;
	long delay;

	long ind = 0;

	sceRtcGetCurrentTick(&old_tick);

	while (gs_timer_on) {

		/* Calculate actual time elapsed. */
		sceRtcGetCurrentTick(&new_tick);

		long delta = new_tick.tick - old_tick.tick;

		if (delta > 1000000){
			// Unusually long delta. Propably user pressed the PS BUTTON. 
			// We have to lower the interval value to prevent game freezing/stuttering.
			interval = 10000;
		}
		else
			interval = PSVTIMER_TO_TIMER(delta);


		old_tick = new_tick;

		/* Handle a tick and rest. */
		delay = _handle_timer_tick(interval);

		sceKernelDelayThread(TIMER_TO_USEC(delay));
	}

	sceKernelExitThread(0);

	return 0;
}

/* psv_timer_init:
 *  Installs the PSVITA timer thread.
 */
static int psv_timer_init(void)
{
	PSV_DEBUG("psv_timer_init()");

	/* Get the ticks per second */
	gs_tick_resolution = sceRtcGetTickResolution();

	//PSV_DEBUG("gs_tick_resolution = %d", gs_tick_resolution);
   
	gs_timer_on = TRUE;

    SceKernelThreadInfo info;
    int priority = 32;

    /* Set priority of new thread to the same as the current thread */
    info.size = sizeof(SceKernelThreadInfo);
    if (sceKernelGetThreadInfo(sceKernelGetThreadId(), &info) == 0) {
        priority = info.currentPriority;
    }

	gs_timer_thread_UID = sceKernelCreateThread("psv_timer_thread", &psv_timer_thread, 0x10000100/*priority*/, 0x4000, 0, 0x10000, NULL);

	if (gs_timer_thread_UID < 0) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot create timer thread"));
		PSV_DEBUG("Cannot create timer thread!!!");
		psv_timer_exit();
		return -1;
	}
   
	//sceKernelChangeThreadCpuAffinityMask(gs_timer_thread_UID, SCE_KERNEL_CPU_MASK_USER_1);

	if (sceKernelStartThread(gs_timer_thread_UID, 0, NULL) != 0) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot start timer thread"));
		PSV_DEBUG("Cannot start timer thread!!!");
		psv_timer_exit();
		return -1;
	}

	return 0;
}

/* psv_timer_exit:
 *  Shuts down the PSVITA timer thread.
 */
static void psv_timer_exit(void)
{
	PSV_DEBUG("psv_timer_exit()");
	gs_timer_on = FALSE;
	if (gs_timer_thread_UID >= 0){
		sceKernelDeleteThread(gs_timer_thread_UID);
		gs_timer_thread_UID = -1;
	}

	return;
}

static void psv_rest(unsigned int time, void (*callback)(void))
{
	sceKernelDelayThread(time*1000);
}
