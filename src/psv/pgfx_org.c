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

static vita2d_texture*	gs_view_tex = NULL;
static char*			gs_view_tex_data;
static int				gs_viewBitDepth;
static int              gs_texture_format;
static int				gs_render_screen = 1;

static float			gs_scaleX = 1; 
static float			gs_scaleY = 1;
static int				gs_posX = 0;
static int				gs_posY = 0;
static char				gs_gfx_lock[32] __attribute__ ((aligned (8)));
//static struct SceKernelLwMutexWork* gs_gfx_lock = NULL;
static int				gs_screen_stretch = 1;
//static int				gs_screen_smooth = 1;


/* The video bitmap that is rendered to the Vita screen. */
BITMAP*					g_displayed_video_bitmap = NULL;
int						g_dirty_screen = 0;


/* Software version of some blitting methods */
static void (*orig_blit_from_memory) (BITMAP * source, BITMAP * dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);
static void (*orig_blit_from_system) (BITMAP * source, BITMAP * dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);
static void (*orig_blit_from_video) (BITMAP * source, BITMAP * dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);

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
static BITMAP*          psv_display_init_win(int w, int h, int v_w, int v_h, int color_depth);
static void				psv_exit(BITMAP *bitmap);
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
static void             psv_blit_from_memory(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);
static void             psv_blit_from_system(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);
static void             psv_blit_from_video(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);

static void				setEvent(EVENT* event);
static void				waitForEvent(EVENT* event);

// Additional API for convinience. Use extern keyword when declaring the functions in client.
void					al_psv_set_screen_stretch(int val);
void					al_psv_set_screen_smooth(int val);
void					al_psv_install_render_timer();
void					al_psv_remove_render_timer();
void					al_psv_al_render_screen(int val);
void					al_psv_draw_last_buffer();


GFX_DRIVER gfx_psv =
{
   GFX_PSV,
   empty_string,
   empty_string,
   "PSVITA gfx driver fullscreen",
   psv_display_init,             /* AL_METHOD(struct BITMAP *, init, (int w, int h, int v_w, int v_h, int color_depth)); */
   psv_exit, //NULL,                         /* AL_METHOD(void, exit, (struct BITMAP *b)); */
   NULL, //psv_scroll,                   /* AL_METHOD(int, scroll, (int x, int y)); */
   psv_vsync,                    /* AL_METHOD(void, vsync, (void)); */
   psv_set_palette,              /* AL_METHOD(void, set_palette, (AL_CONST struct RGB *p, int from, int to, int retracesync)); */
   NULL,                         /* AL_METHOD(int, request_scroll, (int x, int y)); */
   NULL,                         /* AL_METHOD(int, poll_scroll, (void)); */
   NULL,                         /* AL_METHOD(void, enable_triple_buffer, (void)); */
   NULL/*psv_create_video_bitmap*/, /* AL_METHOD(struct BITMAP *, create_video_bitmap, (int width, int height)); */
   NULL/*psv_destroy_video_bitmap*/, /* AL_METHOD(void, destroy_video_bitmap, (struct BITMAP *bitmap)); */
   NULL,//psv_show_video_bitmap, /* AL_METHOD(int, show_video_bitmap, (BITMAP *bitmap)); */
   NULL,                         /* AL_METHOD(int, request_video_bitmap, (BITMAP *bitmap)); */
   NULL,                         /* AL_METHOD(BITMAP *, create_system_bitmap, (int width, int height)); */
   NULL,                         /* AL_METHOD(void, destroy_system_bitmap, (BITMAP *bitmap)); */
   NULL,//psv_set_mouse_sprite, /* AL_METHOD(int, set_mouse_sprite, (BITMAP *sprite, int xfocus, int yfocus)); */
   NULL,//psv_show_mouse/*NULL*/,       /* AL_METHOD(int, show_mouse, (BITMAP *bmp, int x, int y)); */
   NULL,//psv_hide_mouse/*NULL*/,       /* AL_METHOD(void, hide_mouse, (void)); */
   NULL,//psv_move_mouse/*NULL*/,       /* AL_METHOD(void, move_mouse, (int x, int y)); */
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

	// This is disabled because some games loose game graphics.
	// Alex 4 has missing screens in the intro.
	/*if (!g_dirty_screen){
		setEvent(&gs_retrace_event);
		return;
	}*/

	if (!g_displayed_video_bitmap)
		return;

	if (!gs_render_screen)
		return;

	//if (!gs_render_screen){
	//	setEvent(&gs_retrace_event);
	//	return;
	//}

	/* update mouse pointer if needed */
    //if (gs_psv_mouse_on) {
    //    psv_update_mouse_pointer(gs_psv_mouse_xpos, gs_psv_mouse_ypos, TRUE);
    //}

	sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);

	vita2d_start_drawing();
	vita2d_clear_screen();

	if (!gs_screen_stretch){
		vita2d_draw_texture(
			gs_view_tex, 
			gs_posX, 
			gs_posY);
	}
	else{
		// Stretch to fullscreen
		vita2d_draw_texture_scale(
			gs_view_tex, 
			gs_posX, 
			gs_posY, 
			gs_scaleX, 
			gs_scaleY);
	}

    vita2d_end_drawing();
	vita2d_wait_rendering_done();
    vita2d_swap_buffers();

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);

	//setEvent(&gs_retrace_event);
	//g_dirty_screen = 0;
}
END_OF_FUNCTION(psv_draw_to_screen);

void al_psv_install_render_timer()
{
	//PSV_DEBUG("psv_install_render_timer()");

	// Are these necessary?
	LOCK_VARIABLE(g_displayed_video_bitmap);
	LOCK_VARIABLE(gs_view_tex);
	LOCK_FUNCTION(psv_draw_to_screen); 
	//install_int(psv_draw_to_screen, 17); // About 60 FPS
	install_int(psv_draw_to_screen, 30); // About 60 FPS
}

void al_psv_remove_render_timer()
{
	//PSV_DEBUG("psv_remove_render_timer()");
	remove_int(psv_draw_to_screen);
}

void al_psv_set_screen_stretch(int val)
{
	//PSV_DEBUG("psv_set_screen_stretch()");
	
	if (!g_displayed_video_bitmap)
		return;

	gs_screen_stretch = val;

	if (val == 0){
		// Original size
		gs_posX = (DISPLAY_WIDTH - g_displayed_video_bitmap->w) / 2;
		gs_posY = (DISPLAY_HEIGHT - g_displayed_video_bitmap->h) / 2;
	}else if (val == 1){
		// Full screen
		gs_posX = 0;
		gs_posY = 0;
		gs_scaleX = (float)DISPLAY_WIDTH / g_displayed_video_bitmap->w;
		gs_scaleY = (float)DISPLAY_HEIGHT / g_displayed_video_bitmap->h;
	}else if (val == 2){
		// Original size stretched
		gs_scaleX = (float)DISPLAY_HEIGHT / g_displayed_video_bitmap->h;
		gs_scaleY = gs_scaleX;
		gs_posX = (DISPLAY_WIDTH - (g_displayed_video_bitmap->w * gs_scaleX)) / 2;
		gs_posY = 0;
	}
}

void al_psv_set_screen_smooth(int val)
{
	//PSV_DEBUG("psv_set_screen_smooth()");
	
	sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);

	if (!g_displayed_video_bitmap)
		return;

	//gs_screen_smooth = val;

	if (val == 1){
		vita2d_texture_set_filters(gs_view_tex, 
		                      (SceGxmTextureFilter)SCE_GXM_TEXTURE_FILTER_LINEAR, 
							  (SceGxmTextureFilter)SCE_GXM_TEXTURE_FILTER_LINEAR);
	}else{
		vita2d_texture_set_filters(gs_view_tex, 
		                      (SceGxmTextureFilter)SCE_GXM_TEXTURE_FILTER_POINT, 
							  (SceGxmTextureFilter)SCE_GXM_TEXTURE_FILTER_POINT);
	}

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);
}

void al_psv_al_render_screen(int val)
{
	//if (val == 0){
		// Release any possible thread waiting for this event.
		//setEvent(&gs_retrace_event);
	//}
	gs_render_screen = val;
}

void al_psv_draw_last_buffer()
{
	if (!g_displayed_video_bitmap)
		return;

	sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);

	if (!gs_screen_stretch){
		vita2d_draw_texture(
			gs_view_tex, 
			gs_posX, 
			gs_posY);
	}
	else{
		// Stretch to fullscreen
		vita2d_draw_texture_scale(
			gs_view_tex, 
			gs_posX, 
			gs_posY, 
			gs_scaleX, 
			gs_scaleY);
	}

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);
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
		gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_BASE_FORMAT_P8);
		gs_viewBitDepth = 8;
		gs_texture_format = SCE_GXM_TEXTURE_BASE_FORMAT_P8;
		break;
	case 15:
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U5U5U5U1_BGRA);
		gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U5U5U5U1_RGBA);
		gs_viewBitDepth = 15;
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U5U5U5U1_BGRA;
		gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U5U5U5U1_RGBA;
		break;
	case 16:
		// format: 16 bit 5-6-5
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR);
		gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB);
		gs_viewBitDepth = 16;
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR;
		gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB;
		break;
	case 32:
		gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB);
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_BGRA);
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA);
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U8U8U8X8_RGB1);
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_P8_RGBA);
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
		//gs_view_tex = vita2d_create_empty_texture_format(v_w, v_h, (SceGxmTextureFormat)SCE_GXM_TEXTURE_FORMAT_S8S8S8S8_RGBA);
		gs_viewBitDepth = 32;
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_BGRA; 
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA;
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_S8S8S8S8_ARGB; 
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_S8S8S8S8_RGBA; 
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8X8_RGB1;
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_P8_RGBA;
		//gs_texture_format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB;
		gs_texture_format = SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB;
		break;
	default:
         ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Unsupported color depth"));
         return NULL;
	}

	gs_view_tex_data = (char*) vita2d_texture_get_datap(gs_view_tex);

	// Texture filtering.  By default smoothen the picture with linear filter. 
	// This can help making the screen scroll less flickery.
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

    /* Keep the original blitting methods */
   //orig_blit_from_memory = _screen_vtable.blit_from_memory;
   //orig_blit_from_system = _screen_vtable.blit_from_system;
   //orig_blit_from_video = _screen_vtable.blit_to_self;

   //GFX_VTABLE *vtable = _get_vtable(color_depth);
   // vtable->blit_to_self = psv_blit_from_video;

   /*
   _screen_vtable.blit_from_memory = psv_blit_from_memory;
   _screen_vtable.blit_from_system = psv_blit_from_system;
   _screen_vtable.blit_to_self = psv_blit_from_video;

   GFX_VTABLE *vtable = _get_vtable(color_depth);

   vtable->blit_from_memory = psv_blit_from_memory;
   vtable->blit_from_system = psv_blit_from_system;
   vtable->blit_to_self = psv_blit_from_video;
   gfx_capabilities |= GFX_HW_MEM_BLIT;*/



   BMP_EXTRA(psv_screen)->pitch = v_w;


   ///* Physical (not virtual!) screen size */
   gfx_psv.w = psv_screen->cr = w;
   gfx_psv.h = psv_screen->cb = h;

   g_displayed_video_bitmap = psv_screen;

   al_psv_set_screen_stretch(gs_screen_stretch);

  /* if (!gs_screen_stretch){
		gs_posX = (DISPLAY_WIDTH - psv_screen->w) / 2;
		gs_posY = (DISPLAY_HEIGHT - psv_screen->h) / 2;
   }else{
		gs_posX = 0;
		gs_posY = 0;
		gs_scaleX = (float)DISPLAY_WIDTH / psv_screen->w;
		gs_scaleY = (float)DISPLAY_HEIGHT / psv_screen->h;
   }*/

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

   /* Create render timer */
   al_psv_install_render_timer();

   return psv_screen;
}


static void	psv_exit(BITMAP *bitmap)
{
	PSV_DEBUG("pgfx: psv_exit()");

	if (gs_view_tex){
		vita2d_free_texture(gs_view_tex);
		gs_view_tex = NULL;
	}

	
	sceKernelDeleteLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock);
	sceKernelDeleteMutex(gs_retrace_event.mutex);
	sceKernelDeleteCond(gs_retrace_event.cond); 


	g_displayed_video_bitmap = NULL;

	PSV_DEBUG("pgfx: psv_exit() - End");
}

/* psv_set_palette:
 *  Sets the palette for the 8 bpp video mode.
 */
static void psv_set_palette(AL_CONST RGB *p, int from, int to, int retracesync)
{
	//PSV_DEBUG(

	//g_dirty_screen = 1;

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
		//waitForEvent(&gs_retrace_event);
		sceDisplayWaitVblankStart(); // Is this better to use?
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
	PSV_DEBUG("psv_set_mouse_sprite()");
   if (!sprite)
      return -1;

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
	PSV_DEBUG("psv_show_mouse()");
   /* handle only the screen */
   if (bmp != g_displayed_video_bitmap)
      return -1;//FALSE;

   gs_psv_mouse_on = TRUE;

   x -= gs_psv_mouse_xfocus;
   y -= gs_psv_mouse_yfocus;

   psv_update_mouse_pointer(x, y, FALSE);

   return 0;//TRUE;
}

static void psv_hide_mouse()
{
	PSV_DEBUG("psv_hide_mouse()");
	if (!gs_psv_mouse_on)
		return; 
	
	sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);

	blit(gs_psv_mouse_backbuffer, 
		 g_displayed_video_bitmap, 
		 0, 0, gs_psv_mouse_xpos, gs_psv_mouse_ypos, 
		 gs_psv_mouse_backbuffer->w, 
		 gs_psv_mouse_backbuffer->h);

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);

	gs_psv_mouse_on = FALSE;
}

/* psv_move_mouse:
 */
static void psv_move_mouse(int x, int y)
{
	PSV_DEBUG("psv_move_mouse()");
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
	  
	   sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);
	   
	   blit(gs_psv_mouse_backbuffer, 
		  g_displayed_video_bitmap, 
		  0, 0, gs_psv_mouse_xpos, gs_psv_mouse_ypos, 
		  gs_psv_mouse_backbuffer->w, 
		  gs_psv_mouse_backbuffer->h);

	   sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);
   }

   /* save the screen contents located at the new position into the backbuffer */
   blit(g_displayed_video_bitmap, 
	    gs_psv_mouse_backbuffer, 
		x, y, 0, 0, 
		gs_psv_mouse_backbuffer->w, 
		gs_psv_mouse_backbuffer->h);

   /* blit the mouse pointer onto the screen */
   //blit_to_hdc(mouse_frontbuffer, hdc, 0, 0, x, y, mouse_frontbuffer->w, mouse_frontbuffer->h);
   sceKernelLockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1, 0);

   blit(gs_psv_mouse_frontbuffer, 
		g_displayed_video_bitmap, 
		0, 0, x, y, 
		gs_psv_mouse_frontbuffer->w, 
		gs_psv_mouse_frontbuffer->h);

   sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)gs_gfx_lock, 1);

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
	
	//if (gs_render_screen)
		event->signalled = 0;
	//else
	//	event->signalled = 1; // Don't block if we are not rendering

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
	// Don't block if we are not rendering
	//if (gs_render_screen)
	//	waitForEvent(&gs_retrace_event);
   sceDisplayWaitVblankStart(); //Is this better to use?
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

/* psv_show_video_bitmap:
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


static void psv_blit_from_memory(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height)
{
	//PSV_DEBUG("psv_blit_from_memory()");
	g_dirty_screen = 1;

	return orig_blit_from_memory(source, dest, source_x, source_y, dest_x, dest_y, width, height);
}

static void psv_blit_from_system(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height)
{
	//PSV_DEBUG("psv_blit_from_system()");
	g_dirty_screen = 1;

	return orig_blit_from_system(source, dest, source_x, source_y, dest_x, dest_y, width, height);
}

static void psv_blit_from_video(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height)
{
	PSV_DEBUG("psv_blit_from_video()");
	g_dirty_screen = 1;
	
	return orig_blit_from_video(source, dest, source_x, source_y, dest_x, dest_y, width, height);
}

