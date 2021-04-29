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
 *      PSVITA digital sound driver using the Allegro mixer.
 *      
 *      Written by
 *      Amnon-Dan Meir (ammeir).
 *
 *      See readme.txt for copyright information.
 */

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "psvita.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/audioout.h>


#ifndef ALLEGRO_PSV
#error something is wrong with the makefile
#endif


#define IS_SIGNED TRUE
#define SAMPLES_PER_BUFFER 1024

typedef struct{
    int mutex;
    int cond;
    int signalled;
} EVENT;

static EVENT	gs_thread_end_event;
static SceUID	gs_audio_thread_UID;
static int		gs_psv_audio_on = FALSE;
static int		gs_audio_port;     
static int		gs_buffer_id = 0;
// Two buffers. Double the buffer size if using stereo sound.
static short	gs_sound_buffer[2][2 * SAMPLES_PER_BUFFER];
//static unsigned char* gs_sound_bufdata;

static int		digi_psv_detect(int);
static int		digi_psv_init(int, int);
static void		digi_psv_exit(int);
static int		digi_psv_buffer_size();
static int		audioChannelThread();
static void		setEvent(EVENT* event);
static void		waitForEvent(EVENT* event);


DIGI_DRIVER digi_psv =
{
   DIGI_PSV,
   empty_string,
   empty_string,
   "PSVITA digital sound driver",
   0,
   0,
   MIXER_MAX_SFX,
   MIXER_DEF_SFX,
   digi_psv_detect,
   digi_psv_init,
   digi_psv_exit,
   NULL,
   NULL,
   NULL,
   NULL,
   digi_psv_buffer_size,
   _mixer_init_voice,
   _mixer_release_voice,
   _mixer_start_voice,
   _mixer_stop_voice,
   _mixer_loop_voice,
   _mixer_get_position,
   _mixer_set_position,
   _mixer_get_volume,
   _mixer_set_volume,
   _mixer_ramp_volume,
   _mixer_stop_volume_ramp,
   _mixer_get_frequency,
   _mixer_set_frequency,
   _mixer_sweep_frequency,
   _mixer_stop_frequency_sweep,
   _mixer_get_pan,
   _mixer_set_pan,
   _mixer_sweep_pan,
   _mixer_stop_pan_sweep,
   _mixer_set_echo,
   _mixer_set_tremolo,
   _mixer_set_vibrato,
   0, 0,
   0,
   0,
   0,
   0,
   0,
   0
};


/* audioChannelThread:
 *  This thread manages the audio playing.
 */
static int audioChannelThread()
{
	PSV_DEBUG("audioChannelThread()");
	while (gs_psv_audio_on) {
		void *bufptr = &gs_sound_buffer[gs_buffer_id];
      
		// Ask allegro mixer to fill the buffer.
		_mix_some_samples((uintptr_t)bufptr, 0, IS_SIGNED);
		//_mix_some_samples((uintptr_t)gs_sound_bufdata, 0, IS_SIGNED);
      
		// Send sound data to the audio hardware.
		sceAudioOutOutput(gs_audio_port, bufptr);
		//sceAudioOutOutput(gs_audio_port, gs_sound_bufdata);
      
		gs_buffer_id ^= 1;
	}

	setEvent(&gs_thread_end_event);

	sceKernelExitThread(0);

	return 0;
}

/* digi_psv_detect:
 *  Returns TRUE if the audio hardware is present.
 */
static int digi_psv_detect(int input)
{
	//PSV_DEBUG("digi_psv_detect()");

   if (input) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Input is not supported"));
      return FALSE;
   }

   return TRUE;
}

/* digi_psv_init:
 *  Initializes the PSVITA digital sound driver.
 */
static int digi_psv_init(int input, int voices)
{
	PSV_DEBUG("digi_psv_init()");
	char digi_psv_desc[512] = EMPTY_STRING;
	char tmp1[256];

	if (input) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Input is not supported"));
		PSV_DEBUG("digi_psv_init(), error (1)");
		return -1;
	}

	// Open audio channel.
	gs_audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,
									SAMPLES_PER_BUFFER, 48000,
									SCE_AUDIO_OUT_MODE_STEREO);
   
	if (gs_audio_port < 0) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Failed reserving hardware sound channel"));
		PSV_DEBUG("digi_psv_init(), error (2)");
		return -1;
	}

	gs_psv_audio_on = TRUE;

	// Set channel volume.
	int vols[2]={SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL};
	sceAudioOutSetVolume(gs_audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH|SCE_AUDIO_VOLUME_FLAG_R_CH, vols);

	// Create the audio playing thread.
	gs_audio_thread_UID = sceKernelCreateThread("psv_audio_thread",
		(void *)&audioChannelThread, 0x10000100, 0x10000, 0, 0, NULL);

	if (gs_audio_thread_UID < 0) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot create audio thread"));
		digi_psv_exit(FALSE);
		PSV_DEBUG("digi_psv_init(), error (3)");
		return -1;
	}

	// Start the thread.
	if (sceKernelStartThread(gs_audio_thread_UID, 0, NULL) != 0) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot start audio thread"));
		digi_psv_exit(FALSE);

		PSV_DEBUG("digi_psv_init(), error (4)");
		return -1;
	}

	// Create the end of audio thread -event.
    gs_thread_end_event.mutex = sceKernelCreateMutex("psv_end_thread_event_mutex", 0, 0, NULL);
    gs_thread_end_event.cond = sceKernelCreateCond("psv_end_thread_event_cond", 0, gs_thread_end_event.mutex, NULL);
    gs_thread_end_event.signalled = 0;

	// Size = buffer size * channel count * bytes per sample.
	//gs_sound_bufdata = _AL_MALLOC_ATOMIC(SAMPLES_PER_BUFFER * 2 * 2); 

	/* Setup Allegro sound mixer */
	_sound_bits = 16;
	_sound_stereo = TRUE;
	_sound_freq = 48000;

	digi_psv.voices = voices;

	if (_mixer_init(SAMPLES_PER_BUFFER * 2, _sound_freq, _sound_stereo,
			(_sound_bits == 16)? 1:0, &digi_psv.voices))
	{
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Error initializing mixer"));
		digi_psv_exit(FALSE);
		PSV_DEBUG("digi_psv_init(), error (5)");
		return -1;
	}

	uszprintf(digi_psv_desc, sizeof(digi_psv_desc),
              get_config_text("%d bits, %d bps, %s"), _sound_bits,
              _sound_freq, uconvert_ascii(_sound_stereo ? "stereo" : "mono", tmp1));

	digi_psv.desc = digi_psv_desc;

	return 0;
}

/* digi_psv_exit:
 *  Shuts down the sound driver.
 */
static void digi_psv_exit(int input)
{
	PSV_DEBUG("digi_psv_exit()");

	if (input)
		return;

	gs_psv_audio_on = FALSE;

	// Wait for the thread to finish before cleaning up.
	waitForEvent(&gs_thread_end_event);

	sceAudioOutReleasePort(gs_audio_port);
	sceKernelDeleteMutex(gs_thread_end_event.mutex);
	sceKernelDeleteCond(gs_thread_end_event.cond);
	_mixer_exit();
}

static int digi_psv_buffer_size()
{
   return SAMPLES_PER_BUFFER;
}

static void setEvent(EVENT* event)
{
	//PSV_DEBUG("setEvent()");
	sceKernelLockMutex(event->mutex, 1, NULL);
    event->signalled = 1;
    sceKernelUnlockMutex(event->mutex, 1);
    sceKernelSignalCondAll(event->cond);
}

static void waitForEvent(EVENT* event)
{
	//PSV_DEBUG("waitForEvent()");
	sceKernelLockMutex(event->mutex, 1, NULL); // Mutex must be locked before calling wait
	event->signalled = 0;
	while (!event->signalled) { // Protection against spurious wakeups.
        sceKernelWaitCond(event->cond, NULL); // Blocks until cond signaled
    }
	sceKernelUnlockMutex(event->mutex, 1);

}
