
#ifndef DEBUG_H
#define DEBUG_H


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */
#include <psp2/kernel/clib.h>

// Use psp2shell utility to read the sceClibPrintf debug messages
#define printf sceClibPrintf


//#ifdef PSV_DEBUG_CODE
static void PSV_DEBUG(const char* str, ...) {
	va_list list;
	char buf[512];
	FILE* fp;

	va_start(list, str);
	vsprintf(buf, str, list);
	va_end(list);

	fp = fopen("ux0:data/trace.log", "a+"); // Linux
	
	if (fp != NULL){
		fprintf(fp, buf);
		fprintf(fp, "\x0D\x0A"); // new line
		fclose(fp);
	}

	printf(buf);
	printf("\n");

}
//#else
//static void PSV_DEBUG(const char* str, ...) {}
//#endif


#endif