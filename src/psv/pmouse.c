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
#include <psp2/touch.h>


#ifndef ALLEGRO_PSV
#error something is wrong with the makefile
#endif

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif 

// Driver function implementations
static int   psv_mouse_init(void);
static void  psv_mouse_exit(void);
static void  psv_mouse_position(int, int);
static void  psv_mouse_set_range(int, int, int, int);
static void  psv_mouse_get_mickeys(int *, int *);
static void  psv_mouse_poll(void);

static int 	 gs_touch_panel = 0;
static int	 gs_joystick_side = 0;
static int	 gs_touch_accelerated = 0;
static int   gs_left_mouse_b = SCE_CTRL_LTRIGGER;
static int   gs_right_mouse_b = SCE_CTRL_RTRIGGER;
static int	 gs_mymickey_x = 0;            /* for get_mouse_mickeys() */
static int	 gs_mymickey_y = 0;
static int	 gs_mymickey_ox = 0;
static int	 gs_mymickey_oy = 0;
static int	 gs_mymickey_joy_oldx = 0;
static int	 gs_mymickey_joy_oldy = 0;
static int   gs_mouse_minx = 0;
static int   gs_mouse_miny = 0;
static int   gs_mouse_maxx = 959;  /* 0 to 959 = 960 */
static int   gs_mouse_maxy = 543;  /* 0 to 543 = 544 */   
static int   gs_stick_x = 0;
static int   gs_stick_y = 0;
static float gs_stick_radius;
static float gs_stick_angle;
static float gs_precision = 360.0f;
static int   gs_joystick_mouse_speed = 3;//4

// API extension for convinience. Use extern keyword when declaring the functions in client.
void		 al_psv_set_touch_panel(int val);
void		 al_psv_set_joystick_side(int val);
void		 al_psv_set_mouse_accelerated(int val);
void		 al_psv_mouse_button_map(int psv_code, int mouse_button);


extern int	 _mouse_x;
extern int	 _mouse_y;
extern int	 _mouse_b;


MOUSE_DRIVER mouse_psv =
{
   MOUSE_PSV,
   empty_string,
   empty_string,
   "PSVITA mouse driver",
   psv_mouse_init,
   psv_mouse_exit,
   psv_mouse_poll,					// AL_METHOD(void, poll, (void));
   NULL, 							// AL_METHOD(void, timer_poll, (void));
   psv_mouse_position,
   NULL, //psv_mouse_set_range,
   NULL,							// AL_METHOD(void, set_speed, (int xspeed, int yspeed));
   psv_mouse_get_mickeys,
   NULL,							// AL_METHOD(int,  analyse_data, (AL_CONST char *buffer, int size));
   NULL,							// AL_METHOD(void,  enable_hardware_cursor, (AL_CONST int mode));
   NULL								// AL_METHOD(int,  select_system_cursor, (AL_CONST int cursor));
};

static inline int getSqrRadius(int X, int Y) 
{
   return sqrt((X * X) + (Y * Y));
}

static int psv_mouse_init(void)
{
	//PSV_DEBUG("psv_mouse_init()");
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

	// Enable front and back touchscreens
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
	sceTouchEnableTouchForce(SCE_TOUCH_PORT_FRONT);
	sceTouchEnableTouchForce(SCE_TOUCH_PORT_BACK);
    
	return 3; //Num of buttons.
}

static void psv_mouse_poll(void)
{
	//PSV_DEBUG("psv_mouse_poll()");
	static int x = 0;
	static int y = 0;
	static int stick_x = 0;
	static int stick_y = 0;
	static int first_touch = 1;
	static int first_joy_move = 1;
	static SceCtrlData pad;
	static SceTouchData touch;

	sceCtrlPeekBufferPositive(0, &pad, 1);

	_mouse_b = 0;
	if (pad.buttons != 0){
		if (pad.buttons & gs_left_mouse_b) _mouse_b = 1; // Left mouse button
		if (pad.buttons & gs_right_mouse_b) _mouse_b = 2; // Right mouse button
		if ( (pad.buttons & gs_left_mouse_b) && (pad.buttons & gs_right_mouse_b)) _mouse_b = 4; // Middle mouse button
	}

	if (gs_joystick_side == 1){
		// Right joystick
		stick_x = pad.rx; 
		stick_y = pad.ry;
	}
	else{
		// Left joystick
		stick_x = pad.lx; 
		stick_y = pad.ly;
	}

	if (stick_x || stick_y){
		// Calculate x and y positions

		if(!first_joy_move){
			gs_stick_x = stick_y - 128;
			gs_stick_y = stick_x - 128;

			gs_stick_radius = getSqrRadius(gs_stick_x, gs_stick_y);
			if (gs_stick_radius > 100) {
				if (gs_stick_radius > 30) {
					gs_stick_angle = ((atan2f(gs_stick_x, -gs_stick_y) + M_PI) / (M_PI * 2)) * gs_precision;
				} else {
					gs_stick_angle = -1;     
				}

				//1° a 89°
				if (gs_stick_angle >= 1 && gs_stick_angle <= (89 * (gs_precision / 360.0f))) {
					x += (gs_joystick_mouse_speed * (1 - (gs_stick_angle / (gs_precision / 360.0f) / 90)));
					y -= (gs_joystick_mouse_speed * (0 + (gs_stick_angle / (gs_precision / 360.0f) / 90)));
				}
				else if (gs_stick_angle >= (91 * (gs_precision / 360.0f)) && gs_stick_angle <= (179 * (gs_precision / 360.0f))) {
					x -= (gs_joystick_mouse_speed * (0 + (((gs_stick_angle / (gs_precision / 360.0f)) - 89) / 90)));
					y -= (gs_joystick_mouse_speed * (1 - (((gs_stick_angle / (gs_precision / 360.0f)) - 89) / 90)));
				}
				else if (gs_stick_angle >= (181 * (gs_precision / 360.0f)) && gs_stick_angle <= (269 * (gs_precision / 360.0f))) {
					x -= (gs_joystick_mouse_speed * (1 - (((gs_stick_angle / (gs_precision / 360.0f)) - 179) / 90)));
					y += (gs_joystick_mouse_speed * (0 + (((gs_stick_angle / (gs_precision / 360.0f)) - 179) / 90)));
				}
				else if (gs_stick_angle >= (271 * (gs_precision / 360.0f)) && gs_stick_angle <= (359 * (gs_precision / 360.0f))) {
					x += (gs_joystick_mouse_speed * (0 + (((gs_stick_angle / (gs_precision / 360.0f)) - 269) / 90)));
					y += (gs_joystick_mouse_speed * (1 - (((gs_stick_angle / (gs_precision / 360.0f)) - 269) / 90)));
				}
				else if (gs_stick_angle == 0) {
					x += gs_joystick_mouse_speed;
				}
				else if (gs_stick_angle == (90 * (gs_precision / 360.0f))) {
					y -= gs_joystick_mouse_speed;
				}
				else if (gs_stick_angle == (180 * (gs_precision / 360.0f))) {
					x -= gs_joystick_mouse_speed;
				}
				else if (gs_stick_radius == (270 * (gs_precision / 360.0f))) {
					y += gs_joystick_mouse_speed;
				}
			}

			_mouse_x = x;
			_mouse_y = y;

			// Calculate mouse movement deltas. This makes the cursor move.
			gs_mymickey_x += x - gs_mymickey_joy_oldx;
			gs_mymickey_y += y - gs_mymickey_joy_oldy;
			gs_mymickey_joy_oldx = x;
			gs_mymickey_joy_oldy = y;

		}else{
			// First poll after joystick move. Make the cursor stay put and not jump.
			gs_mymickey_x = 0;
			gs_mymickey_y = 0;
			first_joy_move = 0;
		}
	}else{
		first_joy_move = 1;
	}

	/* Read touch panel */
	if (gs_touch_panel >= 0){
		sceTouchPeek(gs_touch_panel, &touch, 1);

		//PSV_DEBUG("touch.report[0].x = %d", touch.report[0].x);
		//PSV_DEBUG("touch.report[0].y = %d", touch.report[0].y);

		// touch.reportNum indicates how many touches we have on the screen
		if (touch.reportNum){
			// Here we already have the x and y positions. Just calculate the deltas.
			
			int x = touch.report[0].x;
			int y = touch.report[0].y;

			// Vita touch screen coordinates are for some reason 1920x1088, double the actual screen resolution. 
			// If left as is the cursor will actually move faster, which is good for same games. 
			if (!gs_touch_accelerated){
				// Slow the speed by converting to actual size. 
				x >>= 1;
				y >>= 1;
			}

			// Calculate mouse movement deltas. This makes the cursor move.
			gs_mymickey_x += x - gs_mymickey_ox;
			gs_mymickey_y += y - gs_mymickey_oy;
			gs_mymickey_ox = x;
			gs_mymickey_oy = y;

			if (first_touch){
				// First poll after touching screen. Make the cursor stay put and not jump.
				gs_mymickey_x = 0;
				gs_mymickey_y = 0;
				first_touch = 0;
			}
		}
		else{
			first_touch = 1;
		}
	}
}

/* psv_mouse_position:
 *  Mouse coordinate range checking.
 */ 
static void psv_mouse_position(int x, int y)
{
   //PSV_DEBUG("psv_mouse_position(), x=%d, y=%d", x,y);

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
	//PSV_DEBUG("psv_mouse_set_range()");
	//PSV_DEBUG("Range x: %d-%d, Range y: %d-%d", x1, x2, y1, y2);

	//gs_mouse_minx = x1;
	//gs_mouse_maxx = x2;
	//gs_mouse_miny = y1;
	//gs_mouse_maxy = y2;
	
	//_mouse_x = CLAMP(gs_mouse_minx, _mouse_x, gs_mouse_maxx);
	//_mouse_y = CLAMP(gs_mouse_miny, _mouse_y, gs_mouse_maxy);
}

/* psv_mouse_get_mickeys:
 *  Measures the mickey count (how far the mouse has moved since the last
 *  call to this function).
 */ 
static void psv_mouse_get_mickeys(int *mickeyx, int *mickeyy)
{
	//PSV_DEBUG("psv_mouse_get_mickeys()");
	int temp_x = gs_mymickey_x;
	int temp_y = gs_mymickey_y;

	gs_mymickey_x -= temp_x;
	gs_mymickey_y -= temp_y;

   *mickeyx = temp_x;
   *mickeyy = temp_y;
}

static void psv_mouse_exit(void)
{
	sceTouchDisableTouchForce(SCE_TOUCH_PORT_FRONT);
    sceTouchDisableTouchForce(SCE_TOUCH_PORT_BACK);
}

void al_psv_set_touch_panel(int val)
{
	//PSV_DEBUG("al_psv_set_touch_panel(), val = %d", val);
	if (val == 0)
		gs_touch_panel = SCE_TOUCH_PORT_FRONT;
	else if (val == 1)
		gs_touch_panel = SCE_TOUCH_PORT_BACK;
	else if (val == -1)
		gs_touch_panel = -1; // Disable touch
}

void al_psv_set_joystick_side(int val)
{
	if (val == 0)
		gs_joystick_side = 0; // Left joystick
	else if (val == 1)
		gs_joystick_side = 1; // Right joystick
}

void al_psv_set_mouse_accelerated(int val)
{
	if (val == 0){
		gs_touch_accelerated = 0;
		gs_joystick_mouse_speed = 3;
	}
	else if (val == 1){
		gs_touch_accelerated = 1;
		gs_joystick_mouse_speed = 10;
	}
}

void al_psv_mouse_button_map(int psv_code, int mouse_button)
{
	//PSV_DEBUG("al_psv_mouse_button_map()");
	if (mouse_button == 1){
		gs_left_mouse_b = psv_code;
		//PSV_DEBUG("gs_left_mouse_b = 0x%x", gs_left_mouse_b);
	}
	else if (mouse_button == 2){
		gs_right_mouse_b = psv_code;
		//PSV_DEBUG("gs_right_mouse_b = 0x%x", gs_right_mouse_b);
	}
}


