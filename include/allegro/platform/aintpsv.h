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
 *      Internal header file for the PSVITA Allegro library port.
 *
 *      Written by
 *      Amnon-Dan Meir (ammeir).
 *
 *      See readme.txt for copyright information.
 */

#ifndef AINTPSV_H
#define AINTPSV_H


/* Extra bitmap info. */
#define BMP_EXTRA(bmp)         ((BMP_EXTRA_INFO *)((bmp)->extra))

typedef struct BMP_EXTRA_INFO
{
   int pitch;
   /* For video bitmaps. */
   int size;
   uintptr_t hw_addr;
   uintptr_t ptr_tex;
} BMP_EXTRA_INFO;

#define ALIGN_TO(v,n)          ((v + n-1) & ~(n-1))

AL_FUNC(void, psv_draw_to_screen, (void));
AL_FUNC(void, psv_init_controller, (int cycle, int mode));
AL_FUNC(void, psv_install_render_timer, (void));
AL_FUNC(void, psv_remove_render_timer, (void));

#endif

