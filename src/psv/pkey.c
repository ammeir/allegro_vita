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
 *      PSVITA keyboard driver implementation.
 *    
 *      Written by
 *      Amnon-Dan Meir (ammeir).
 *
 *      See readme.txt for copyright information.
 */

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "psvita.h"
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>

#ifndef ALLEGRO_PSV
#error Something is wrong with the makefile
#endif

static int psv_keyboard_init(void);
static void psv_keyboard_exit(void);
static void psv_poll_keyboard(void);

/*
 * Lookup table for converting SCE_CTRL_* codes into allegro KEY_* codes
 */
static const int psv_to_scancode[][2] = {
   { SCE_CTRL_SELECT,     KEY_ESC      },
   { SCE_CTRL_START,      KEY_TAB      },
   { SCE_CTRL_UP,         KEY_UP       },
   { SCE_CTRL_RIGHT,      KEY_RIGHT    },
   { SCE_CTRL_DOWN,       KEY_DOWN     },
   { SCE_CTRL_LEFT,       KEY_LEFT     },
   { SCE_CTRL_TRIANGLE,   KEY_BACKSPACE},
   { SCE_CTRL_CIRCLE,     KEY_LCONTROL },
   { SCE_CTRL_CROSS,      KEY_SPACE    },
   { SCE_CTRL_SQUARE,     KEY_ALT      },
   { SCE_CTRL_LTRIGGER,   KEY_LSHIFT   },
   { SCE_CTRL_RTRIGGER,   KEY_RSHIFT   }
};

#define NKEYS (sizeof psv_to_scancode / sizeof psv_to_scancode[0])

extern void psv_init_controller(int cycle, int mode);


KEYBOARD_DRIVER keyboard_simulator_psv =
{
   KEYSIM_PSV,
   empty_string,
   empty_string,
   "PSVITA keyboard simulator",
   FALSE/*TRUE*/,  // int autorepeat;
   psv_keyboard_init,
   psv_keyboard_exit,
   psv_poll_keyboard,
   NULL,   // AL_METHOD(void, set_leds, (int leds));
   NULL,   // AL_METHOD(void, set_rate, (int delay, int rate));
   NULL,   // AL_METHOD(void, wait_for_input, (void));
   NULL,   // AL_METHOD(void, stop_waiting_for_input, (void));
   NULL,   // AL_METHOD(int,  scancode_to_ascii, (int scancode));
   NULL    // scancode_to_name
};


/* psv_keyboard_init:
 *  Installs the keyboard handler.
 */
static int psv_keyboard_init(void)
{
	// Digital buttons + analog support.
	psv_init_controller(0, SCE_CTRL_MODE_ANALOG);
	
	//LOCK_VARIABLE(old_buttons);
	LOCK_FUNCTION(psv_poll_keyboard); 
	
	// Install keyboard poll timer.
	install_int(psv_poll_keyboard, 33); // 30 polls/second
	return 0;
}


/* psv_keyboard_exit:
 *  Removes the keyboard handler.
 */
static void psv_keyboard_exit(void)
{
	remove_int(psv_poll_keyboard);
}


/* psv_poll_keyboard:
 *  Reads the PSVITA control pad.
 */
static void psv_poll_keyboard(void)
{
	//PSV_DEBUG("psv_poll_keyboard()");

	SceCtrlData  pad;

	static int	 old_buttons = 0;
	static char  prev_joystick_mask = 0;
	char		 curr_joystick_mask;
	int			 new_actions;


   sceCtrlPeekBufferPositive(0, &pad, 1);


   // Map digital input to a keyboard keys.

   for (uint8_t i = 0; i < NKEYS; i++) {

      new_actions = (pad.buttons ^ old_buttons) & psv_to_scancode[i][0];
	
      if (new_actions) {
         if (pad.buttons & psv_to_scancode[i][0]) {
			//PSV_DEBUG("PSV Keyboard: [%d] pressed\n", psv_to_scancode[i][1]);
            _handle_key_press(scancode_to_ascii(psv_to_scancode[i][1]), psv_to_scancode[i][1]);
         }
         else {
			//PSV_DEBUG("PSV Keyboard: [%d] released\n", psv_to_scancode[i][1]);
            _handle_key_release(psv_to_scancode[i][1]);
         }
      }
   }

	old_buttons = pad.buttons;

   
	// Map analog input to directional keys.
	// This will give us joystick support for games that use only keyboard input.
	
	// First make a bitmask of analog input. This will help us to identify press/release.
	curr_joystick_mask = 0x00;

	// Y axis
	if (pad.ly <= 40)
		curr_joystick_mask |= 0x01; // UP
	else if (pad.ly >= 216)
		curr_joystick_mask |= 0x02; // DOWN
	// X axis
	if (pad.lx <= 40)
		curr_joystick_mask |= 0x04; // LEFT
	else if (pad.lx >= 216)
		curr_joystick_mask |= 0x08; // RIGHT

	new_actions = prev_joystick_mask ^ curr_joystick_mask;

	if (new_actions){

		if (new_actions & 0x01){
			if (curr_joystick_mask & 0x01)
				_handle_key_press(0, KEY_UP);
			else
				_handle_key_release(KEY_UP);
		}
		if (new_actions & 0x02){
			if (curr_joystick_mask & 0x02)
				_handle_key_press(0, KEY_DOWN);
			else
				_handle_key_release(KEY_DOWN);
		}
		if (new_actions & 0x04){
			if (curr_joystick_mask & 0x04)
				_handle_key_press(0, KEY_LEFT);
			else
				_handle_key_release(KEY_LEFT);
		}
		if (new_actions & 0x08){
			if (curr_joystick_mask & 0x08)
				_handle_key_press(0, KEY_RIGHT);
			else
				_handle_key_release(KEY_RIGHT);
		}
	}

	prev_joystick_mask = curr_joystick_mask;
}
END_OF_STATIC_FUNCTION(psv_poll_keyboard);

