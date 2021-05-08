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
 *      PSVITA mouse driver.
 *    
 *      Written by
 *      Amnon-Dan Meir (ammeir)
 *
 *      Based on PSP code by diedel
 *      
 *      See readme.txt for copyright information.
 */

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "math.h"
#include "psvita.h"
#include <psp2/ctrl.h>

#ifndef ALLEGRO_PSV
#error something is wrong with the makefile
#endif

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif 

static int  psv_mouse_init(void);
static void psv_mouse_exit(void);
static void psv_mouse_position(int, int);
static void psv_mouse_set_range(int, int, int, int);
static void psv_mouse_get_mickeys(int *, int *);
static void psv_mouse_timer_poll(void);

extern int _mouse_x;
extern int _mouse_y;
extern int _mouse_b;


MOUSE_DRIVER mouse_psv =
{
   MOUSE_PSV,
   empty_string,
   empty_string,
   "PSVITA mouse driver",
   psv_mouse_init,
   psv_mouse_exit,
   NULL,       // AL_METHOD(void, poll, (void));
   psv_mouse_timer_poll,                 // AL_METHOD(void, timer_poll, (void));
   psv_mouse_position,
   psv_mouse_set_range,
   NULL,       // AL_METHOD(void, set_speed, (int xspeed, int yspeed));
   psv_mouse_get_mickeys,
   NULL,       // AL_METHOD(int,  analyse_data, (AL_CONST char *buffer, int size));
   NULL,       // AL_METHOD(void,  enable_hardware_cursor, (AL_CONST int mode));
   NULL        // AL_METHOD(int,  select_system_cursor, (AL_CONST int cursor));
};

static int   gs_mouse_minx = 0;
static int   gs_mouse_miny = 0;
static int   gs_mouse_maxx = 959;  /* 0 to 959 = 960 */
static int   gs_mouse_maxy = 543;  /* 0 to 543 = 544 */   
//static SceCtrlData pad;
static int   gs_stick_x = 0;
static int   gs_stick_y = 0;
static float gs_stick_radius;
static float gs_stick_angle;
static float gs_precision = 360.0f;
static int   gs_mouse_speed = 5;


static inline int getSqrRadius(int X, int Y) 
{
   return sqrt((X * X) + (Y * Y));
}

static int psv_mouse_init(void)
{
   //sceCtrlSetSamplingCycle(0);
   sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

   return 3; //Num of buttons.
}

static void psv_mouse_timer_poll(void)
{
   static int x = 0;
   static int y = 0;
   static SceCtrlData pad;

   sceCtrlPeekBufferPositive(0, &pad, 1);

   //if (pad.buttons != 0)
   //{
     //sceCtrlReadBufferPositive(0, &pad, 1);
	 //sceCtrlPeekBufferPositive(0, &pad, 1);
     _mouse_b = 0;
     if (pad.buttons & SCE_CTRL_LTRIGGER) _mouse_b = 1;
     if (pad.buttons & SCE_CTRL_RTRIGGER) _mouse_b = 2;
     if ( (pad.buttons & SCE_CTRL_LTRIGGER) && (pad.buttons & SCE_CTRL_RTRIGGER)) _mouse_b = 4;
   //}
   gs_stick_x = pad.ly - 128;
   gs_stick_y = pad.lx - 128;

   gs_stick_radius = getSqrRadius(gs_stick_x, gs_stick_y);
   if (gs_stick_radius > 100) {
      if (gs_stick_radius > 30) {
         gs_stick_angle = ((atan2f(gs_stick_x, -gs_stick_y) + M_PI) / (M_PI * 2)) * gs_precision;
      } else {
         gs_stick_angle = -1;     
      }

      //1° a 89°
      if (gs_stick_angle >= 1 && gs_stick_angle <= (89 * (gs_precision / 360.0f))) {
         x += (gs_mouse_speed * (1 - (gs_stick_angle / (gs_precision / 360.0f) / 90)));
         y -= (gs_mouse_speed * (0 + (gs_stick_angle / (gs_precision / 360.0f) / 90)));
      }
      else if (gs_stick_angle >= (91 * (gs_precision / 360.0f)) && gs_stick_angle <= (179 * (gs_precision / 360.0f))) {
         x -= (gs_mouse_speed * (0 + (((gs_stick_angle / (gs_precision / 360.0f)) - 89) / 90)));
         y -= (gs_mouse_speed * (1 - (((gs_stick_angle / (gs_precision / 360.0f)) - 89) / 90)));
      }
      else if (gs_stick_angle >= (181 * (gs_precision / 360.0f)) && gs_stick_angle <= (269 * (gs_precision / 360.0f))) {
         x -= (gs_mouse_speed * (1 - (((gs_stick_angle / (gs_precision / 360.0f)) - 179) / 90)));
         y += (gs_mouse_speed * (0 + (((gs_stick_angle / (gs_precision / 360.0f)) - 179) / 90)));
      }
      else if (gs_stick_angle >= (271 * (gs_precision / 360.0f)) && gs_stick_angle <= (359 * (gs_precision / 360.0f))) {
         x += (gs_mouse_speed * (0 + (((gs_stick_angle / (gs_precision / 360.0f)) - 269) / 90)));
         y += (gs_mouse_speed * (1 - (((gs_stick_angle / (gs_precision / 360.0f)) - 269) / 90)));
      }
      else if (gs_stick_angle == 0) {
         x += gs_mouse_speed;
      }
      else if (gs_stick_angle == (90 * (gs_precision / 360.0f))) {
         y -= gs_mouse_speed;
      }
      else if (gs_stick_angle == (180 * (gs_precision / 360.0f))) {
         x -= gs_mouse_speed;
      }
      else if (gs_stick_radius == (270 * (gs_precision / 360.0f))) {
         y += gs_mouse_speed;
      }
   }
   if (x < gs_mouse_minx)
      x = gs_mouse_minx;

   if (x > gs_mouse_maxx)
      x = gs_mouse_maxx;

   if (y < gs_mouse_miny)
      y = gs_mouse_miny;

   if (y > gs_mouse_maxy)
      y = gs_mouse_maxy;

   _mouse_x = x;
   _mouse_y = y;
}

/* psv_mouse_position:
 *  Mouse coordinate range checking.
 */ 
static void psv_mouse_position(int x, int y)
{
   _mouse_x = x;
   if (_mouse_x <  gs_mouse_minx) _mouse_x = 0;
   if (_mouse_x > gs_mouse_maxx)  _mouse_x = gs_mouse_maxx;
   _mouse_y = y;
   if (_mouse_y < gs_mouse_miny) _mouse_y = 0;
   if (_mouse_y > gs_mouse_maxy) _mouse_y = gs_mouse_maxy;
}

/* psv_mouse_set_range:
 *  Set mouse movement range.
 */ 
static void psv_mouse_set_range(int x1, int y1, int x2, int y2)
{
	PSV_DEBUG("psv_mouse_set_range()");
	PSV_DEBUG("x2 = %d, y2 = %d", x2, y2);

	gs_mouse_minx = x1;
	gs_mouse_miny = y1;
	gs_mouse_maxx = x2;
	gs_mouse_maxy = y2;

	_mouse_x = CLAMP(gs_mouse_minx, _mouse_x, gs_mouse_maxx);
	_mouse_y = CLAMP(gs_mouse_miny, _mouse_y, gs_mouse_maxy);
}

/* psv_mouse_get_mickeys:
 *  Measures the mickey count (how far the mouse has moved since the last
 *  call to this function).
 */ 
static void psv_mouse_get_mickeys(int *mickeyx, int *mickeyy)
{
   *mickeyx = 0;
   *mickeyy = 0;
}

static void psv_mouse_exit(void)
{
}
