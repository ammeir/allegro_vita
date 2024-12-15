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
 *      Joystick driver routines for PSVITA.
 *
 *      Written by 
 *      Amnon-Dan Meir (ammeir).
 *
 *      Based on PSP code by diedel.
 *
 *      See readme.txt for copyright information.
 */


#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "psvita.h"
#include <psp2/ctrl.h>

#ifndef ALLEGRO_PSV
#error something is wrong with the makefile
#endif


struct PSV_BUTTON
{
   char *name;
   int  code;
};

struct PSV_BUTTON psv_controller_buttons[] = {
   {"TRIANGLE", SCE_CTRL_TRIANGLE},
   {"CIRCLE",   SCE_CTRL_CIRCLE},
   {"CROSS",    SCE_CTRL_CROSS},
   {"SQUARE",   SCE_CTRL_SQUARE},
   {"LTRIGGER", SCE_CTRL_LTRIGGER},
   {"RTRIGGER", SCE_CTRL_RTRIGGER},
   {"SELECT",   SCE_CTRL_SELECT},
   {"START",    SCE_CTRL_START}
};

#define NBUTTONS (sizeof psv_controller_buttons / sizeof psv_controller_buttons[0])


static int psv_joy_init(void);
static void psv_joy_exit(void);
static int psv_joy_poll(void);


JOYSTICK_DRIVER joystick_psv = {
   JOYSTICK_PSV,         // int  id;
   empty_string,         // AL_CONST char *name;
   empty_string,         // AL_CONST char *desc;
   "PSVITA joystick",    // AL_CONST char *ascii_name;
   psv_joy_init,         // AL_METHOD(int, init, (void));
   psv_joy_exit,         // AL_METHOD(void, exit, (void));
   psv_joy_poll,         // AL_METHOD(int, poll, (void));
   NULL,                 // AL_METHOD(int, save_data, (void));
   NULL,                 // AL_METHOD(int, load_data, (void));
   NULL,                 // AL_METHOD(AL_CONST char *, calibrate_name, (int n));
   NULL                  // AL_METHOD(int, calibrate, (int n));
};


/* psv_init_controller:
 *  Internal routine to initialize the PSVITA controller.
 */
void psv_init_controller(int cycle, int mode)
{
   sceCtrlSetSamplingMode(mode);
}

/* psv_joy_init:
 *  Initializes the PSVITA joystick driver.
 */
static int psv_joy_init(void)
{
   uint8_t b;

   // Digital buttons + analog support.
   psv_init_controller(0, SCE_CTRL_MODE_ANALOG);

   num_joysticks = 1;

   for (b=0; b<NBUTTONS; b++) {
      joy[0].button[b].name = psv_controller_buttons[b].name;
      joy[0].num_buttons++;
   }

   joy[0].num_sticks = 1;
   joy[0].stick[0].flags = JOYFLAG_ANALOGUE | JOYFLAG_DIGITAL;
   joy[0].stick[0].num_axis = 2;
   joy[0].stick[0].axis[0].name = "X axis";
   joy[0].stick[0].axis[1].name = "Y axis";
   joy[0].stick[0].name = "PSVITA joystick";

   return 0;
}

/* psv_joy_exit:
 *  Shuts down the PSVITA joystick driver.
 */
static void psv_joy_exit(void)
{
}


/* psv_joy_poll:
 *  Polls the active joystick devices and updates internal states.
 */
static int psv_joy_poll(void)
{
	//PSV_DEBUG("psv_joy_poll()");
	SceCtrlData pad;

	sceCtrlPeekBufferPositive(0, &pad, 1);

	/* Report the status of the non directional buttons. */
	for (uint8_t b=0; b<NBUTTONS; b++)
		joy[0].button[b].b = pad.buttons & psv_controller_buttons[b].code;

	/* Report the status of the directional buttons/left analog stick. */
	// LEFT
	if (pad.buttons & SCE_CTRL_LEFT || pad.lx <= 40)
		joy[0].stick[0].axis[0].d1 = TRUE;
	else
		joy[0].stick[0].axis[0].d1 = FALSE;
	// RIGHT
	if (pad.buttons & SCE_CTRL_RIGHT || pad.lx >= 216)
		joy[0].stick[0].axis[0].d2 = TRUE;
	else
		joy[0].stick[0].axis[0].d2 = FALSE;
	// UP
	if (pad.buttons & SCE_CTRL_UP || pad.ly <= 40)
		joy[0].stick[0].axis[1].d1 = TRUE;
	else
		joy[0].stick[0].axis[1].d1 = FALSE;
	// DOWN
	if (pad.buttons & SCE_CTRL_DOWN || pad.ly >= 216)
		joy[0].stick[0].axis[1].d2 = TRUE;
	else
		joy[0].stick[0].axis[1].d2 = FALSE;
  
	// Analog stick range.
	joy[0].stick[0].axis[0].pos = pad.lx; 
	joy[0].stick[0].axis[1].pos = pad.ly; 

	return 0;
}

