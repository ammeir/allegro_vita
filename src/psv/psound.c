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

// Globals
static SceUID	gs_audio_thread_UID = -1;
static EVENT	gs_thread_end_event;
static int		gs_audio_port = -1; 
static int		gs_buffer_id = 0;
static int		gs_psv_audio_on = FALSE;
int				g_psv_audio_mute = FALSE;
// Two buffers. Double the size for stereo sound.
static short	gs_sound_buffer[2][2 * SAMPLES_PER_BUFFER];
static short	gs_mute_buffer[2 * SAMPLES_PER_BUFFER];

// Driver function implementations
static int		psv_digi_detect(int);
static int		psv_digi_init(int, int);
static void		psv_digi_exit(int);
static int		psv_digi_buffer_size();

static int		audioChannelThread(SceSize args, void *argp);
static void		setEvent(EVENT* event);
static void		waitForEvent(EVENT* event);
// API extension for convinience. Use 'extern' keyword when declaring the functions in client.
void			al_psv_reset_sound();


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
   psv_digi_detect,
   psv_digi_init,
   psv_digi_exit,
   NULL,
   NULL,
   NULL,
   NULL,
   psv_digi_buffer_size,
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
static int audioChannelThread(SceSize args, void *argp)
{
	PSV_DEBUG("audioChannelThread()");

    gs_psv_audio_on = TRUE;

	while (gs_psv_audio_on) {

		if (!g_psv_audio_mute){
			void *bufptr = &gs_sound_buffer[gs_buffer_id];
      
			// Ask allegro mixer to fill the buffer.
			_mix_some_samples((uintptr_t)bufptr, 0, IS_SIGNED);
			
			// Send sound data to the audio hardware.
			sceAudioOutOutput(gs_audio_port, bufptr); // Blocking function
	
			gs_buffer_id ^= 1;
		}
		else{
			// Output silence
			sceAudioOutOutput(gs_audio_port, gs_mute_buffer);
		}
	}

	setEvent(&gs_thread_end_event);

	sceKernelExitThread(0);

	return 0;
}

/* psv_digi_detect:
 *  Returns TRUE if the audio hardware is present.
 */
static int psv_digi_detect(int input)
{
	//PSV_DEBUG("psv_digi_detect()");

   if (input) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Input is not supported"));
      return FALSE;
   }

   return TRUE;
}

/* psv_digi_init:
 *  Initializes the PSVITA digital sound driver.
 */
static int psv_digi_init(int input, int voices)
{
	PSV_DEBUG("psv_digi_init()");
	PSV_DEBUG("input = %d", input);
	PSV_DEBUG("voices = %d", voices);

	char digi_psv_desc[512] = EMPTY_STRING;
	char tmp1[256];

	if (input) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Input is not supported"));
		//PSV_DEBUG("psv_digi_init(), error (1)");
		return -1;
	}

	// First release audio port if left open.
	if (gs_audio_port > 0){
		sceAudioOutReleasePort(gs_audio_port);
	}

	// Open audio channel.
	gs_audio_port = sceAudioOutOpenPort(/*SCE_AUDIO_OUT_PORT_TYPE_BGM*/SCE_AUDIO_OUT_PORT_TYPE_MAIN,
									SAMPLES_PER_BUFFER, /*44100*/48000,
									SCE_AUDIO_OUT_MODE_STEREO);
   
	if (gs_audio_port <= 0) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Failed reserving hardware sound channel"));
		//PSV_DEBUG("psv_digi_init(), error (2)");
		return -1;
	}

	// Set channel volume.
	int vols[2]={SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL};
	sceAudioOutSetVolume(gs_audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH|SCE_AUDIO_VOLUME_FLAG_R_CH, vols);

	// Create the audio playing thread.
	gs_audio_thread_UID = sceKernelCreateThread("psv_audio_thread",
		&audioChannelThread, 0x10000100, 0x4000, 0, 0x10000, NULL);

	if (gs_audio_thread_UID < 0) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot create audio thread"));
		psv_digi_exit(FALSE);
		//PSV_DEBUG("psv_digi_init(), error (3)");
		return -1;
	}

	//sceKernelChangeThreadCpuAffinityMask(gs_audio_thread_UID, SCE_KERNEL_CPU_MASK_USER_1);

	PSV_DEBUG("Created audio thread: 0x%x", gs_audio_thread_UID);

	// Create the end of audio thread -event.
    gs_thread_end_event.mutex = sceKernelCreateMutex("psv_end_thread_event_mutex", 0, 0, NULL);
    gs_thread_end_event.cond = sceKernelCreateCond("psv_end_thread_event_cond", 0, gs_thread_end_event.mutex, NULL);
    gs_thread_end_event.signalled = 0;

	int arr2 = SAMPLES_PER_BUFFER * 2;
	for(int i=0; i<arr2; i++) {
        gs_mute_buffer[i] = 0;
	}

	/* Setup Allegro sound mixer */
	_sound_bits = 16;
	_sound_stereo = TRUE;
	_sound_freq = /*44100*/48000;

	digi_psv.voices = voices;

	if (_mixer_init(SAMPLES_PER_BUFFER * 2, _sound_freq, _sound_stereo,
			(_sound_bits == 16)? 1:0, &digi_psv.voices))
	{
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Error initializing mixer"));
		psv_digi_exit(FALSE);
		//PSV_DEBUG("psv_digi_init(), error (4)");
		return -1;
	}

	uszprintf(digi_psv_desc, sizeof(digi_psv_desc),
              get_config_text("%d bits, %d bps, %s"), _sound_bits,
              _sound_freq, uconvert_ascii(_sound_stereo ? "stereo" : "mono", tmp1));

	digi_psv.desc = digi_psv_desc;


	// Start the thread.
	if (sceKernelStartThread(gs_audio_thread_UID, 0, NULL) != 0) {
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Cannot start audio thread"));
		psv_digi_exit(FALSE);

		//PSV_DEBUG("psv_digi_init(), error (5)");
		return -1;
	}

	PSV_DEBUG("Started audio thread: 0x%x", gs_audio_thread_UID);

	return 0;
}

/* psv_digi_exit:
 *  Shuts down the sound driver.
 */
static void psv_digi_exit(int input)
{
	PSV_DEBUG("psv_digi_exit()");

	gs_psv_audio_on = FALSE;

	// Wait for the thread to finish before cleaning up.
	//waitForEvent(&gs_thread_end_event);

	if (gs_audio_thread_UID >= 0){
		sceKernelDeleteThread(gs_audio_thread_UID);
		PSV_DEBUG("Deleted audio thread: 0x%x", gs_audio_thread_UID);
		gs_audio_thread_UID = -1;
	}

	sceAudioOutReleasePort(gs_audio_port);
	gs_audio_port = -1;
	sceKernelDeleteMutex(gs_thread_end_event.mutex);
	sceKernelDeleteCond(gs_thread_end_event.cond);
	_mixer_exit();
}

static int psv_digi_buffer_size()
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

void al_psv_reset_sound()
{
	PSV_DEBUG("al_psv_reset_sound");
	psv_digi_exit(0);
	if (digi_psv.voices < 0)
		digi_psv.voices = 0;
	psv_digi_init(0, digi_psv.voices);
	
}