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
#define PSVTIMER_TO_TIMER(t)  ((int)((t) * (TIMERS_PER_SECOND / (float)psv_tick_resolution)))


//static int psv_timer_thread();
static int psv_timer_init(void);
static void psv_timer_exit(void);
static void psv_rest(unsigned int time, void (*callback)(void));


static uint32_t psv_tick_resolution;
static int psv_timer_on = FALSE;
static SceUID timer_thread_UID;



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
void* psv_timer_thread(void* arg)
{
	PSV_DEBUG("psv_timer_thread");

	SceRtcTick old_tick, new_tick, tmp_tick;
	int interval;
	long delay;

	long ind = 0;

	sceRtcGetCurrentTick(&old_tick);

	while (psv_timer_on) {

		/* Calculate actual time elapsed. */
		sceRtcGetCurrentTick(&new_tick);

		long delta = new_tick.tick - old_tick.tick;

		if (delta > 1000000){
			//PSV_DEBUG("delta = %ld", delta);
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
   psv_tick_resolution = sceRtcGetTickResolution();

   PSV_DEBUG("psv_tick_resolution = %d", psv_tick_resolution);
   
   psv_timer_on = TRUE;


    SceKernelThreadInfo info;
    int priority = 32;

    /* Set priority of new thread to the same as the current thread */
    info.size = sizeof(SceKernelThreadInfo);
    if (sceKernelGetThreadInfo(sceKernelGetThreadId(), &info) == 0) {
        priority = info.currentPriority;
    }

	timer_thread_UID = sceKernelCreateThread("psv_timer_thread", (void *)&psv_timer_thread, priority, 0x10000, 0, 0, NULL);

   if (timer_thread_UID < 0) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot create timer thread"));
      PSV_DEBUG("Cannot create timer thread!!!");
	  psv_timer_exit();
      return -1;
   }
   
   if (sceKernelStartThread(timer_thread_UID, 4, NULL) != 0) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot start timer thread"));
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
   psv_timer_on = FALSE;
   return;
}

static void psv_rest(unsigned int time, void (*callback)(void))
{
	sceKernelDelayThread(time*1000);
}
