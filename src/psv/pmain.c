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
 *      Replacement for the Allegro program main() function for capturing program arguments.
 *
 *      Written by
 *      Amnon-Dan Meir (ammeir).
 *
 *      See readme.txt for copyright information.
 */

#include "psvita.h"

#ifdef ALLEGRO_WITH_MAGIC_MAIN

#undef main

extern int    __crt0_argc;
extern char** __crt0_argv;
extern void*  _mangled_main_address; 


/* main:
 *  Replacement for main function (capture arguments and call real main).
 */
int main(int argc, char *argv[])
{
	PSV_DEBUG("Replacement main()");
	
	int (*real_main) (int, char*[]) = (int (*) (int, char*[])) _mangled_main_address;

	__crt0_argc = argc;
	__crt0_argv = argv;

   return (*real_main)(argc, argv);
}

#endif 
