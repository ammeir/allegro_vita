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
 *      List of PSVITA drivers.
 *
 *      Written by 
 *      Amnon-Dan Meir (ammeir).
 *
 *      See readme.txt for copyright information.
 */


#include "allegro.h"

#ifndef ALLEGRO_PSV
#error Something is wrong with the makefile
#endif


_DRIVER_INFO _system_driver_list[] =
{
   { SYSTEM_PSV,              &system_psv,              TRUE  },
   { SYSTEM_NONE,             &system_none,             FALSE },
   { 0,                       NULL,                     0     }
};


_DRIVER_INFO _keyboard_driver_list[] =
{
   { KEYSIM_PSV,              &keyboard_simulator_psv,     TRUE  },
   { 0,                       NULL,                     0     }
};


_DRIVER_INFO _timer_driver_list[] =
{
   { TIMER_PSV,               &timer_psv,               TRUE  },
   { 0,                       NULL,                     0     }
};


_DRIVER_INFO _mouse_driver_list[] =
{
   { MOUSE_PSV,               &mouse_psv,               TRUE  },
   { 0,                       NULL,                     0     }
};


_DRIVER_INFO _gfx_driver_list[] =
{
   { GFX_PSV,                 &gfx_psv,                 TRUE  },
   { 0,                       NULL,                     0     }
};


_DRIVER_INFO _digi_driver_list[] =
{
   { DIGI_PSV,                 &digi_psv,               TRUE  },
   { 0,                        NULL,                    0     }
};


BEGIN_MIDI_DRIVER_LIST
MIDI_DRIVER_DIGMID
END_MIDI_DRIVER_LIST


BEGIN_JOYSTICK_DRIVER_LIST
{   JOYSTICK_PSV,              &joystick_psv,           TRUE  },
END_JOYSTICK_DRIVER_LIST
