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
 *      PSVITA grafics driver implementation.
 *
 *      Written by 
 *      Amnon-Dan Meir (ammeir).
 *
 *      Based on PSP code by diedel
 *
 *      See readme.txt for copyright information.
 */


#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "allegro/platform/aintpsv.h"
#include "psvita.h"
#include <vita2d.h>
#include <psp2/display.h>
#include <psp2/kernel/threadmgr.h>
#include <math.h>

#ifndef ALLEGRO_PSV
   #error something is wrong with the makefile
#endif

#define MAX_SCR_WIDTH   (960)
#define MAX_SCR_HEIGHT  (544)
#define DISPLAY_WIDTH   960
#define DISPLAY_HEIGHT	544

typedef struct{
    int mutex;
    int cond;
    int signalled;
} EVENT;

static vita2d_texture*	gs_view_tex;
static char*			gs_view_tex_data;
static int				gs_viewBitDepth;
static int              gs_texture_format;

static float			gs_scaleX = 1; 
static float			gs_scaleY = 1;
static char				gs_gfx_lock[32] __attribute__ ((aligned (8)));

/* The video bitmap that is rendered to the Vita screen. */
BITMAP*					g_displayed_video_bitmap = NULL;

/* Hardware mouse cursor emulation */
static int				gs_psv_mouse_on = FALSE;
static int				gs_psv_mouse_was_on = FALSE;
static BITMAP*			gs_psv_mouse_sprite = NULL;
static BITMAP*			gs_psv_mouse_frontbuffer = NULL;
static BITMAP*			gs_psv_mouse_backbuffer = NULL;
static int				gs_psv_mouse_xfocus, gs_psv_mouse_yfocus;
static int				gs_psv_mouse_xpos, gs_psv_mouse_ypos;
static EVENT			gs_retrace_event;

static BITMAP*          psv_display_init(int w, int h, int v_w, int v_h, int color_depth);
static int				psv_scroll(int x, int y);
static void				psv_vsync(void);
static void				psv_hw_blit(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);
static void				psv_set_palette(AL_CONST RGB *p, int from, int to, int retracesync);
static int				psv_request_scroll(int x, int y);
static int				psv_poll_scroll(void);
static BITMAP*			psv_create_video_bitmap(int width, int height);
static void				psv_destroy_video_bitmap(BITMAP *bitmap);
static int				psv_show_video_bitmap(BITMAP *bitmap);
static int				psv_request_video_bitmap(BITMAP *bitmap);
static void				psv_gfx_lock(struct BITMAP *bmp);
static void				psv_gfx_unlock(struct BITMAP *bmp);
static int				psv_set_mouse_sprite(struct BITMAP *sprite, int xfocus, int yfocus);
static int				psv_show_mouse(struct BITMAP *bmp, int x, int y);
static void				psv_hide_mouse();
static void				psv_move_mouse(int x, int y);
static void				psv_update_mouse_pointer(int x, int y, int retrace);

static void				setEvent(EVENT* event);
static void				waitForEvent(EVENT* event);

//extern BITMAP*			psv_create_bitmap(int color_depth, int width, int height);
//extern int				psv_destroy_bitmap(BITMAP *bitmap);


GFX_DRIVER gfx_psv =
{
   GFX_PSV,
   empty_string,
   empty_string,
   "PSVITA gfx driver",
   psv_display_init,             /* AL_METHOD(struct BITMAP *, init, (int w, int h, int v_w, int v_h, int color_depth)); */
   NULL,                         /* AL_METHOD(void, exit, (struct BITMAP *b)); */
   psv_scroll,                   /* AL_METHOD(int, scroll, (int x, int y)); */
   psv_vsync,                    /* AL_METHOD(void, vsync, (void)); */
   psv_set_palette,              /* AL_METHOD(void, set_palette, (AL_CONST struct RGB *p, int from, int to, int retracesync)); */
   NULL,                         /* AL_METHOD(int, request_scroll, (int x, int y)); */
   NULL,                         /* AL_METHOD(int, poll_scroll, (void)); */
   NULL,                         /* AL_METHOD(void, enable_triple_buffer, (void)); */
   NULL/*psv_create_video_bitmap*/, /* AL_METHOD(struct BITMAP *, create_video_bitmap, (int width, int height)); */
   NULL/*psv_destroy_video_bitmap*/, /* AL_METHOD(void, destroy_video_bitmap, (struct BITMAP *bitmap)); */
   NULL/*psv_show_video_bitmap*/, /* AL_METHOD(int, show_video_bitmap, (BITMAP *bitmap)); */
   NULL,                         /* AL_METHOD(int, request_video_bitmap, (BITMAP *bitmap)); */
   NULL,                         /* AL_METHOD(BITMAP *, create_system_bitmap, (int width, int height)); */
   NULL,                         /* AL_METHOD(void, destroy_system_bitmap, (BITMAP *bitmap)); */
   psv_set_mouse_sprite/*NULL*/, /* AL_METHOD(int, set_mouse_sprite, (BITMAP *sprite, int xfocus, int yfocus)); */
   psv_show_mouse/*NULL*/,       /* AL_METHOD(int, show_mouse, (BITMAP *bmp, int x, int y)); */
   psv_hide_mouse/*NULL*/,       /* AL_METHOD(void, hide_mouse, (void)); */
   psv_move_mouse/*NULL*/,       /* AL_METHOD(void, move_mouse, (int x, int y)); */
   NULL,                         /* AL_METHOD(void, drawing_mode, (void)); */
   NULL,                         /* AL_METHOD(void, save_video_state, (void)); */
   NULL,                         /* AL_METHOD(void, restore_video_state, (void)); */
   NULL,                         /* AL_METHOD(void, set_blender_mode, (int mode, int r, int g, int b, int a)); */
   NULL,                         /* AL_METHOD(int, fetch_mode_list, (void)); */
   0, 0,                         /* physical (not virtual!) screen size */
   TRUE,                         /* true if video memory is linear */
   0,                            /* bank size, in bytes */
   0,                            /* bank granularity, in bytes */
   0,                            /* video memory size, in bytes */
   0,                            /* physical address of video memory */
   FALSE                         /* true if driver runs windowed */
};

/* psv_draw_to_screen:
 *  Timer proc that updates the screen.
 */
void psv_draw_to_screen()
{
	//PSV_DEBUG("psv_draw_to_screen");

	if (!g_displayed_video_bitmap)
		return;

	sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);

	/* update mouse pointer if needed */
    if (gs_psv_mouse_on) {
        psv_update_mouse_pointer(gs_psv_mouse_xpos, gs_psv_mouse_ypos, TRUE);
    }

	vita2d_start_drawing();
	vita2d_clear_screen();

	vita2d_draw_texture_scale(
		gs_view_tex, 
		0, 
		0, 
		gs_scaleX, 
		gs_scaleY);

    vita2d_end_drawing();
	vita2d_wait_rendering_done();
    vita2d_swap_buffers();

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);

	setEvent(&gs_retrace_event);
}
END_OF_FUNCTION(psv_draw_to_screen);

void psv_install_render_timer()
{
	//PSV_DEBUG("psv_install_render_timer()");

	// Are these necessary?
	//LOCK_VARIABLE(g_displayed_video_bitmap);
	//LOCK_VARIABLE(gs_view_tex);
	//LOCK_FUNCTION(psv_draw_to_screen); 
	install_int(psv_draw_to_screen, 17); // About 60 FPS
}

void psv_remove_render_timer()
{
	//PSV_DEBUG("psv_remove_render_timer()");
	remove_int(psv_draw_to_screen);
}

/* psv_display_init:
 *  Initializes the gfx mode.
 */
static BITMAP *psv_display_init(int w, int h, int v_w, int v_h, int color_depth)
{
	PSV_DEBUG("psv_display_init()");
	PSV_DEBUG("w=%d, h=%d, v_w=%d, v_h=%d, color_depth=%d", w, h, v_w, v_h, color_depth);

	
	vita2d_init();
	
	BITMAP *psv_screen;
	int bytes_per_line;

	if (w > MAX_SCR_WIDTH || h > MAX_SCR_HEIGHT) {
		PSV_DEBUG("Resolution not supported");
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Resolution not supported"));
		return NULL;
	}

	/* Virtual screen management. */
	if (v_w < w)
		v_w = w;
	if (v_h < h)
		v_h = h;

	switch(color_depth){
	case 8:
		// format: 8 bit indexed
		// NOTE: Only this color depth has been tested so far on Worminator.
		gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_BASE_FORMAT_P8);
		gs_viewBitDepth = 8;
		gs_texture_format = SCE_GXM_TEXTURE_BASE_FORMAT_P8;
		break;
	case 15:
		gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U5U5U5U1_RGBA);
		gs_viewBitDepth = 15;
		gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U5U5U5U1_RGBA;
		break;
	case 16:
		// format: 16 bit 5-6-5
		gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB);
		gs_viewBitDepth = 16;
		gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB;
		break;
	case 32:
		gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA);
		gs_viewBitDepth = 32;
		gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA;
		break;
	default:
         ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Unsupported color depth"));
         return NULL;
	}

	gs_view_tex_data = (char*) vita2d_texture_get_datap(gs_view_tex);

	// Texture filtering. Smoothen the picture with linear filter. 
	// This helps making the screen scroll less flickery.
	vita2d_texture_set_filters(gs_view_tex, 
		                      (SceGxmTextureFilter)SCE_GXM_TEXTURE_FILTER_LINEAR, 
							  (SceGxmTextureFilter)SCE_GXM_TEXTURE_FILTER_LINEAR);

 
   /* Create the allegro screen bitmap. */
	bytes_per_line = v_w * BYTES_PER_PIXEL(color_depth);
	psv_screen = _make_bitmap(v_w, v_h, (uintptr_t)gs_view_tex_data, &gfx_psv, color_depth, bytes_per_line);
	
	if (psv_screen)
		psv_screen->extra = _AL_MALLOC(sizeof(struct BMP_EXTRA_INFO));

	if (!psv_screen || !psv_screen->extra) {
		PSV_DEBUG("Not enough memory");
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Not enough memory"));
		return NULL;
	}

	// Screen bitmap access lock functions.
	// To prevent simultaneous reading and writing of screen bitmap.
   _screen_vtable.acquire = psv_gfx_lock;
   _screen_vtable.release = psv_gfx_unlock;

   BMP_EXTRA(psv_screen)->pitch = v_w;


   ///* Physical (not virtual!) screen size */
   gfx_psv.w = psv_screen->cr = w;
   gfx_psv.h = psv_screen->cb = h;

   g_displayed_video_bitmap = psv_screen;
   gs_scaleX = (float)DISPLAY_WIDTH / psv_screen->w;
   gs_scaleY = (float)DISPLAY_HEIGHT / psv_screen->h;

   // Create screen bitmap access mutex.
   if (sceKernelCreateLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, "psv_gfx_mutex", 0, 0, 0) < 0){
		PSV_DEBUG("Failed to create graphics driver mutex!");
		ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Failed to create graphics driver mutex!"));
		return NULL;
   }

   // End of screen draw -event.
   gs_retrace_event.mutex = sceKernelCreateMutex("psv_retrace_event_mutex", 0, 0, NULL);
   gs_retrace_event.cond = sceKernelCreateCond("psv_retrace_event_cond", 0, gs_retrace_event.mutex, NULL);
   gs_retrace_event.signalled = 0;

   // Are these necessary?
   //LOCK_VARIABLE(g_displayed_video_bitmap);
   //LOCK_VARIABLE(gs_view_tex);
   //LOCK_FUNCTION(psv_draw_to_screen); 

   /* Create render timer */
   install_int(psv_draw_to_screen, 17); // About 60 FPS

   return psv_screen;
}

/* psv_set_palette:
 *  Sets the hardware palette for the 8 bpp video mode.
 */
static void psv_set_palette(AL_CONST RGB *p, int from, int to, int retracesync)
{
	//PSV_DEBUG("psv_set_palette");
   
	uint32_t* palette_tbl = (uint32_t*)vita2d_texture_get_palette(gs_view_tex);

	if (!palette_tbl){
		PSV_DEBUG("Palette table not found!");
		return;
	}

	unsigned char r, g, b;
	int j = 0;

	sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);
	
	for (int i=from; i<=to; ++i){
		r = _rgb_scale_6[p[i].r];
		g = _rgb_scale_6[p[i].g];
		b = _rgb_scale_6[p[i].b];

		palette_tbl[j++] = r | (g << 8) | (b << 16) | (0xFF << 24);
	}

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);

	if (retracesync){
		waitForEvent(&gs_retrace_event);
		//sceDisplayWaitVblankStart(); // Is this better to use?
	}
}

static void psv_gfx_lock(struct BITMAP *bmp)
{
	//PSV_DEBUG("psv_gfx_lock()");
	sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);
	bmp->id |= BMP_ID_LOCKED;
}

static void psv_gfx_unlock(struct BITMAP *bmp)
{
	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);
	bmp->id &= ~BMP_ID_LOCKED;
}

static int psv_set_mouse_sprite(struct BITMAP *sprite, int xfocus, int yfocus)
{
	//PSV_DEBUG("psv_set_mouse_sprite()");
   if (gs_psv_mouse_sprite) {
      destroy_bitmap(gs_psv_mouse_sprite);
      gs_psv_mouse_sprite = NULL;

      destroy_bitmap(gs_psv_mouse_frontbuffer);
      gs_psv_mouse_frontbuffer = NULL;

      destroy_bitmap(gs_psv_mouse_backbuffer);
      gs_psv_mouse_backbuffer = NULL;
   }

   gs_psv_mouse_sprite = create_bitmap(sprite->w, sprite->h);
   blit(sprite, gs_psv_mouse_sprite, 0, 0, 0, 0, sprite->w, sprite->h);

   gs_psv_mouse_xfocus = xfocus;
   gs_psv_mouse_yfocus = yfocus;

   gs_psv_mouse_frontbuffer = create_bitmap(sprite->w, sprite->h);
   gs_psv_mouse_backbuffer = create_bitmap(sprite->w, sprite->h);

   return 0;
}

static int psv_show_mouse(struct BITMAP *bmp, int x, int y)
{
	//PSV_DEBUG("psv_show_mouse()");
   /* handle only the screen */
   if (bmp != g_displayed_video_bitmap)
      return -1;

   gs_psv_mouse_on = TRUE;

   x -= gs_psv_mouse_xfocus;
   y -= gs_psv_mouse_yfocus;

   psv_update_mouse_pointer(x, y, FALSE);

   return 0;
}

static void psv_hide_mouse()
{
	//PSV_DEBUG("psv_hide_mouse()");
	if (!gs_psv_mouse_on)
		return; 

	blit(gs_psv_mouse_backbuffer, 
		 g_displayed_video_bitmap, 
		 0, 0, gs_psv_mouse_xpos, gs_psv_mouse_ypos, 
		 gs_psv_mouse_backbuffer->w, 
		 gs_psv_mouse_backbuffer->h);

	gs_psv_mouse_on = FALSE;
}

/* gfx_gdi_move_mouse:
 */
static void psv_move_mouse(int x, int y)
{
	//PSV_DEBUG("psv_move_mouse()");
   if (!gs_psv_mouse_on)
      return;

   x -= gs_psv_mouse_xfocus;
   y -= gs_psv_mouse_yfocus;

   if ((gs_psv_mouse_xpos == x) && (gs_psv_mouse_ypos == y))
      return;

   psv_update_mouse_pointer(x, y, TRUE);
} 

/* update_mouse_pointer:
 *  Worker function that updates the mouse pointer.
 */
static void psv_update_mouse_pointer(int x, int y, int retrace)
{
	//PSV_DEBUG("psv_update_mouse_pointer()");
   /* put the screen contents located at the new position into the frontbuffer */
   blit(g_displayed_video_bitmap, 
	    gs_psv_mouse_frontbuffer, 
		x, y, 0, 0, 
		gs_psv_mouse_frontbuffer->w, 
		gs_psv_mouse_frontbuffer->h);

   /* draw the mouse pointer onto the frontbuffer */
   draw_sprite(gs_psv_mouse_frontbuffer, gs_psv_mouse_sprite, 0, 0);

   if (retrace) {
      /* restore the screen contents located at the old position */
      //blit_to_hdc(mouse_backbuffer, hdc, 0, 0, mouse_xpos, mouse_ypos, mouse_backbuffer->w, mouse_backbuffer->h);
	  blit(gs_psv_mouse_backbuffer, 
		  g_displayed_video_bitmap, 
		  0, 0, gs_psv_mouse_xpos, gs_psv_mouse_ypos, 
		  gs_psv_mouse_backbuffer->w, 
		  gs_psv_mouse_backbuffer->h);
   }

   /* save the screen contents located at the new position into the backbuffer */
   blit(g_displayed_video_bitmap, 
	    gs_psv_mouse_backbuffer, 
		x, y, 0, 0, 
		gs_psv_mouse_backbuffer->w, 
		gs_psv_mouse_backbuffer->h);

   /* blit the mouse pointer onto the screen */
   //blit_to_hdc(mouse_frontbuffer, hdc, 0, 0, x, y, mouse_frontbuffer->w, mouse_frontbuffer->h);
   blit(gs_psv_mouse_frontbuffer, 
		g_displayed_video_bitmap, 
		0, 0, x, y, 
		gs_psv_mouse_frontbuffer->w, 
		gs_psv_mouse_frontbuffer->h);

   /* save the new position */
   gs_psv_mouse_xpos = x;
   gs_psv_mouse_ypos = y;
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

/* psv_scroll:
 *  PSVITA scrolling routine.
 *  This handle horizontal scrolling in 8 pixel increments under 15 or 16
 *  bpp modes and 4 pixel increments under 32 bpp mode.
 *  The 8 bpp mode has no restrictions.
 */
static int psv_scroll(int x, int y)
{
	PSV_DEBUG("UNIMPLEMENTED: psv_scroll()");
	return 0;
}

/* psv_vsync:
 *  Waits for a retrace.
 */
void psv_vsync(void)
{
   waitForEvent(&gs_retrace_event);
   //sceDisplayWaitVblankStart(); Is this better to use?
}

/* psv_hw_blit:
 *  PSVITA accelerated blitting routine.
 */
static void psv_hw_blit(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height)
{
	PSV_DEBUG("UNIMPLEMENTED: psv_hw_blit()");
	return;
}

/* psv_request_scroll:
 *  Attempts to initiate a triple buffered hardware scroll, which will
 *  take place during the next retrace. Returns 0 on success.
 */ 
static int psv_request_scroll(int x, int y)
{
	PSV_DEBUG("UNIMPLEMENTED: psv_request_scroll");
	return 0;
}

/* psv_poll_scroll:
 *  This function is used for triple buffering. It checks the status of
 *  a hardware scroll previously initiated by the request_scroll() or by
 *  the request_video_bitmap() routines. 
 */
static int psv_poll_scroll(void)
{
	PSV_DEBUG("UNIMPLEMENTED: psv_poll_scroll()");
	return 0;
}

/* psv_create_video_bitmap:
 *  Attempts to make a bitmap object for accessing offscreen video memory.
 */
static BITMAP *psv_create_video_bitmap(int width, int height)
{
	PSV_DEBUG("UNIMPLEMENTED: psv_create_video_bitmap() ");
	return NULL;
}

/* psv_destroy_video_bitmap:
 *  Restores the video ram used for the video bitmap and the system ram
 *  used for the bitmap management structures.
 */
static void psv_destroy_video_bitmap(BITMAP *bitmap)
{
	PSV_DEBUG("psv_destroy_video_bitmap() ");

	if (!is_sub_bitmap(bitmap)){
		if (BMP_EXTRA(bitmap)->ptr_tex)
			vita2d_free_texture((vita2d_texture*)BMP_EXTRA(bitmap)->ptr_tex);
	}

	_AL_FREE(bitmap->extra);
	_AL_FREE(bitmap);
}

/* psp_show_video_bitmap:
 *  Page flipping function: swaps to display the specified video memory 
 *  bitmap object (this must be the same size as the physical screen).
 */
int psv_show_video_bitmap(BITMAP *bitmap)
{
	PSV_DEBUG("UNIMPLEMENTED: psv_show_video_bitmap()");
	return 0;
}

/* psv_request_video_bitmap:
 *  Triple buffering function: triggers a swap to display the specified 
 *  video memory bitmap object, which will take place on the next retrace.
 */
static int psv_request_video_bitmap(BITMAP *bitmap)
{
	PSV_DEBUG("UNIMPLEMENTED: psv_request_video_bitmap()");
	return 0;
}


