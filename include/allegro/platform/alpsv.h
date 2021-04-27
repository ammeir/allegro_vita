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
 *      PSVITA specific header defines.
 *
 *      Written by.
 *      Amnon-Dan Meir (ammeir).
 *
 *      See readme.txt for copyright information.
 */


#ifndef ALPSVITA_H
#define ALPSVITA_H

#ifndef ALLEGRO_PSV
   #error bad include
#endif


/* System driver */
#define SYSTEM_PSV              AL_ID('P','S','V',' ')
AL_VAR(SYSTEM_DRIVER, system_psv);

/* Timer driver */
#define TIMER_PSV               AL_ID('P','S','V','T')
AL_VAR(TIMER_DRIVER, timer_psv);

/* Keyboard driver */
#define KEYSIM_PSV              AL_ID('P','S','V','K')
AL_VAR(KEYBOARD_DRIVER, keyboard_simulator_psv);

/* Mouse drivers */
#define MOUSE_PSV               AL_ID('P','S','V','M')
AL_VAR(MOUSE_DRIVER, mouse_psv);

/* Gfx driver */
#define GFX_PSV                 AL_ID('P','S','V','G')
AL_VAR(GFX_DRIVER, gfx_psv);

/* Digital sound driver */
#define DIGI_PSV                AL_ID('P','S','V','S')
AL_VAR(DIGI_DRIVER, digi_psv);

/* Joystick drivers */
#define JOYSTICK_PSV            AL_ID('P','S','V','J')
AL_VAR(JOYSTICK_DRIVER, joystick_psv);

#endif
