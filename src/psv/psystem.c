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
 *      PSVITA system driver.
 *
 *      Written by 
 *      Amnon-Dan Meir (ammeir)
 *
 *      Based on PSP code by diedel.
 *
 *      See readme.txt for copyright information.
 */

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "psvita.h"
#include "debugScreen.h"
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>


#ifndef ALLEGRO_PSV
   #error something is wrong with the makefile
#endif


#define DEFAULT_SCREEN_WIDTH       960
#define DEFAULT_SCREEN_HEIGHT      544
#define DEFAULT_COLOR_DEPTH         16//8

// Globals
int    __crt0_argc;
char** __crt0_argv;
char*  gs_psv_app_dir = NULL;

static int psv_sys_init(void);
static void psv_sys_exit(void);
static void psv_get_exe_name(char *output, int size);
static void psv_message(AL_CONST char *msg);
BITMAP * psv_create_bitmap(int color_depth, int width, int height);
static void psv_created_sub_bitmap(BITMAP *bmp, BITMAP *parent);
static void psv_get_gfx_safe_mode(int *driver, struct GFX_MODE *mode);
static void* psv_create_mutex();
static void psv_destroy_mutex(void* handle);
static void psv_lock_mutex(void *handle);
static void psv_unlock_mutex(void* handle);

extern void psv_init_controller(int cycle, int mode);


typedef struct BMP_EXTRA_INFO
{
   int pitch;
   /* For video bitmaps. */
   int size;
   uintptr_t hw_addr;
} BMP_EXTRA_INFO;

/* Extra bitmap info. */
#define BMP_EXTRA(bmp)         ((BMP_EXTRA_INFO *)((bmp)->extra))

#define ALIGN_TO(v,n)          ((v + n-1) & ~(n-1))


SYSTEM_DRIVER system_psv =
{
   SYSTEM_PSV,
   empty_string,
   empty_string,
   "PSVITA",
   psv_sys_init,
   psv_sys_exit,
   NULL/*psv_get_exe_name*/,  /* AL_METHOD(void, get_executable_name, (char *output, int size)); */
   NULL,  /* AL_METHOD(int, find_resource, (char *dest, AL_CONST char *resource, int size)); */
   NULL,  /* AL_METHOD(void, set_window_title, (AL_CONST char *name)); */
   NULL,  /* AL_METHOD(int, set_close_button_callback, (AL_METHOD(void, proc, (void)))); */
   psv_message,  /* AL_METHOD(void, message, (AL_CONST char *msg)); */
   NULL,  /* AL_METHOD(void, assert, (AL_CONST char *msg)); */
   NULL,  /* AL_METHOD(void, save_console_state, (void)); */
   NULL,  /* AL_METHOD(void, restore_console_state, (void)); */
   NULL,  /* AL_METHOD(struct BITMAP *, create_bitmap, (int color_depth, int width, int height)); */
   NULL,  /* AL_METHOD(void, created_bitmap, (struct BITMAP *bmp)); */
   NULL,  /* AL_METHOD(struct BITMAP *, create_sub_bitmap, (struct BITMAP *parent, int x, int y, int width, int height)); */
   psv_created_sub_bitmap,  /* AL_METHOD(void, created_sub_bitmap, (struct BITMAP *bmp, struct BITMAP *parent)); */
   NULL,  /* AL_METHOD(int, destroy_bitmap, (struct BITMAP *bitmap)); */
   NULL,  /* AL_METHOD(void, read_hardware_palette, (void)); */
   NULL,  /* AL_METHOD(void, set_palette_range, (AL_CONST struct RGB *p, int from, int to, int retracesync)); */
   NULL,  /* AL_METHOD(struct GFX_VTABLE *, get_vtable, (int color_depth)); */
   NULL,  /* AL_METHOD(int, set_display_switch_mode, (int mode)); */
   NULL,  /* AL_METHOD(void, display_switch_lock, (int lock, int foreground)); */
   NULL,  /* AL_METHOD(int, desktop_color_depth, (void)); */
   NULL,  /* AL_METHOD(int, get_desktop_resolution, (int *width, int *height)); */
   psv_get_gfx_safe_mode,  /*AL_METHOD(void, get_gfx_safe_mode, (int *driver, struct GFX_MODE *mode));*/
   NULL,  /* AL_METHOD(void, yield_timeslice, (void)); */
   psv_create_mutex/*NULL*/,  /* AL_METHOD(void *, create_mutex, (void)); */
   psv_destroy_mutex/*NULL*/,  /* AL_METHOD(void, destroy_mutex, (void *handle)); */
   psv_lock_mutex/*NULL*/,  /* AL_METHOD(void, lock_mutex, (void *handle)); */
   psv_unlock_mutex/*NULL*/,  /* AL_METHOD(void, unlock_mutex, (void *handle)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, gfx_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, digi_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, midi_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, keyboard_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, mouse_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, joystick_drivers, (void)); */
   NULL   /* AL_METHOD(_DRIVER_INFO *, timer_drivers, (void)); */
};


void psv_set_app_dir(const char* dir)
{
	PSV_DEBUG("psv_set_app_dir()");
	
	if (!dir)
		return;

	PSV_DEBUG("dir = %s", dir);

	gs_psv_app_dir = _AL_MALLOC(1024);

	strncpy(gs_psv_app_dir, dir, 1024);
}

char* psv_get_app_dir()
{
	return gs_psv_app_dir;
}

/* psv_sys_init:
 *  Initializes the PSVITA system driver.
 */
static int psv_sys_init(void)
{
	/* Setup OS type & version */
	PSV_DEBUG("psv_sys_init");
	os_type = OSTYPE_PSV;

	return 0;
}

/* psv_sys_exit:
 *  Shuts down the system driver.
 */
static void psv_sys_exit(void)
{
   //sceKernelExitGame();
}

/* psv_get_exe_name:
 *  Return full path to the current executable.
 */
static void psv_get_exe_name(char *output, int size)
{
	// This does not work on Vita. Executable name in argv[0] is empty.
	// TODO: Think of another solution.

	PSV_DEBUG("psv_get_exe_name()");
	PSV_DEBUG("Returning %s", __crt0_argv[0]);
    /* Program name is stored in the __crt0_argv[] global */
	do_uconvert(__crt0_argv[0], U_ASCII, output, U_CURRENT, size);
}
 
/* psv_message:
 *  Prints a text message on the PSVITA screen.
 */
static void psv_message(AL_CONST char *msg)
{
   SceCtrlData pad;
   int buffers_to_read = 1;

   psvDebugScreenInit();
   psvDebugScreenPrintf(msg);

   /* The message is displayed until a button is pressed. */
   psv_init_controller(0, SCE_CTRL_MODE_DIGITAL);
   do {
      sceCtrlPeekBufferPositive(0, &pad, buffers_to_read);
   } while (!pad.buttons);
}



/* psv_create_bitmap:
 *  Creates a RAM bitmap with proper PSP pitch.
 */
BITMAP* psv_create_bitmap(int color_depth, int width, int height)
{
	//PSV_DEBUG("psv_create_bitmap");

   GFX_VTABLE *vtable;
   BITMAP *bitmap;
   int nr_pointers;
   int i;
   int pitch;

   vtable = _get_vtable(color_depth);
   if (!vtable)
      return NULL;

   /* We need at least two pointers when drawing, otherwise we get crashes with
    * Electric Fence.  We think some of the assembly code assumes a second line
    * pointer is always available.
    */
   nr_pointers = MAX(2, height);
   bitmap = _AL_MALLOC(sizeof(BITMAP) + (sizeof(char *) * nr_pointers));
   if (!bitmap)
      return NULL;

   /* The memory bitmap width must be multiple of 8 pixels
    * in order to blit properly using sceGuCopyImage().
    */
    pitch = ALIGN_TO(width, 8);
   //pitch = width;//ALIGN_TO(width, 8);

   //bitmap->dat = memalign(64, pitch * height * BYTES_PER_PIXEL(color_depth));
   bitmap->dat = _AL_MALLOC_ATOMIC(pitch * height * BYTES_PER_PIXEL(color_depth));
   if (!bitmap->dat) {
      _AL_FREE(bitmap);
      return NULL;
   }

   bitmap->w = bitmap->cr = width;
   bitmap->h = bitmap->cb = height;
   bitmap->clip = TRUE;
   bitmap->cl = bitmap->ct = 0;
   bitmap->vtable = vtable;
   bitmap->write_bank = bitmap->read_bank = _stub_bank_switch;
   bitmap->id = 0;
   bitmap->x_ofs = 0;
   bitmap->y_ofs = 0;
   bitmap->seg = _default_ds();

   if (height > 0) {
      bitmap->line[0] = bitmap->dat;
      for (i=1; i<height; i++)
         bitmap->line[i] = bitmap->line[i-1] + pitch * BYTES_PER_PIXEL(color_depth);
   }

   /* Setup info structure to store additional information. */
   bitmap->extra = malloc(sizeof(struct BMP_EXTRA_INFO));
   if (!bitmap->extra) {
      _AL_FREE(bitmap);
      return NULL;
   }
   BMP_EXTRA(bitmap)->pitch = pitch;

   return bitmap;
}



/* psp_created_sub_bitmap:
 *  Set the needed sub bitmap info.
 */
static void psv_created_sub_bitmap(BITMAP *bmp, BITMAP *parent)
{
	PSV_DEBUG("psv_created_sub_bitmap");
   if (BMP_EXTRA(parent)) {
      bmp->extra = malloc(sizeof(struct BMP_EXTRA_INFO));
      if (bmp->extra)
         BMP_EXTRA(bmp)->pitch = BMP_EXTRA(parent)->pitch;
   }
}

/* psp_destroy_bitmap:
 *  Destroys the bitmap extra info structure.
 */
int psv_destroy_bitmap(BITMAP *bitmap)
{
   _AL_FREE(bitmap->extra);

   return 0;
}

/* psv_get_gfx_safe_mode:
 *  Defines the safe graphics mode for this system.
 */
static void psv_get_gfx_safe_mode(int *driver, struct GFX_MODE *mode)
{
	PSV_DEBUG("psv_get_gfx_safe_mode()");

   *driver = GFX_PSV;
   mode->width = 320;//DEFAULT_SCREEN_WIDTH;
   mode->height = 200;//DEFAULT_SCREEN_HEIGHT;
   mode->bpp = 8;//DEFAULT_COLOR_DEPTH;
}


static void* psv_create_mutex()
{
	int mid = sceKernelCreateMutex("psv_system_mutex", 0, 0, 0);
	return (void*)mid;
}

static void psv_destroy_mutex(void* handle)
{
	//int mid = (int)*handle;
	sceKernelDeleteMutex((int)handle);
}

static void psv_lock_mutex(void* handle)
{
	//int mid = (int)*handle;
	sceKernelLockMutex((int)handle, 1, 0);
}


static void psv_unlock_mutex(void* handle)
{
	//int mid = (int)*handle;
	sceKernelUnlockMutex((int)handle, 1);
}








