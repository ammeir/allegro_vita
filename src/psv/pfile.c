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
 *      Helper routines to make file.c work on PSVITA platform.
 *
  *     Written by 
 *      Amnon-Dan Meir (ammeir).
 *      
 *      Based (heavily) on UNIX code by
 *      Michael Bukin. 
 *
 *      See readme.txt for copyright information.
 */


#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "psvita.h"
#include <psp2/kernel/processmgr.h> 
#include <psp2/appmgr.h> 
#include <sys/dirent.h> 


#ifndef ALLEGRO_PSV
#error Something is wrong with the makefile
#endif

extern int    __crt0_argc;
extern char** __crt0_argv;
extern char* psv_get_app_dir();

#define NAMLEN(dirent) (strlen((dirent)->d_name))
#define FF_MAXPATHLEN 1024
#define FF_MATCH_TRY 0
#define FF_MATCH_ONE 1
#define FF_MATCH_ANY 2

/* structures used by the directory scanning routines */
struct FF_DATA
{
   DIR *dir;
   char dirname[FF_MAXPATHLEN];
   char pattern[FF_MAXPATHLEN];
   int attrib;
   uint64_t size;
};

struct FF_MATCH_DATA
{
   int type;
   AL_CONST char *s1;
   AL_CONST char *s2;
};

static char *ff_get_filename(AL_CONST char *path);
static int ff_get_attrib(AL_CONST char *name, struct stat *s);
static int ff_match(AL_CONST char *s1, AL_CONST char *s2);
static void ff_put_backslash(char *filename, int size);

// Not relevant for Vita. Returning zero seems to be working.
gid_t geteuid()
{
	return 0;
}

gid_t getegid()
{
	return 0;
}

/* _al_file_isok:
 *  Helper function to check if it is safe to access a file on a floppy
 *  drive. This really only applies to the DOS library, so we don't bother
 *  with it.
 */
int _al_file_isok(AL_CONST char *filename)
{
   return TRUE;
}

/* _al_file_size_ex:
 *  Measures the size of the specified file.
 */
uint64_t _al_file_size_ex(AL_CONST char *filename)
{
	//PSV_DEBUG("PSVITA: _al_file_size_ex()");
    struct stat s;
	char tmp[1024];

	if (stat(uconvert(filename, U_CURRENT, tmp, U_UTF8, sizeof(tmp)), &s) != 0) {
		*allegro_errno = errno;
		return 0;
	}

	return s.st_size;
}


/* _al_file_time:
 *  Returns the timestamp of the specified file.
 */
time_t _al_file_time(AL_CONST char *filename)
{
	//PSV_DEBUG("PSVITA: _al_file_time()");
    struct stat s;
	char tmp[1024];

	if (stat(uconvert(filename, U_CURRENT, tmp, U_UTF8, sizeof(tmp)), &s) != 0) {
		*allegro_errno = errno;
		return 0;
	}

   return s.st_mtime;
}

int _al_drive_exists(int drive)
{ 
	//PSV_DEBUG("PSVITA: _al_drive_exists(), drive = %d", drive);

	// Support two drives: ux0 (drive 0) and uma0 (drive 1).
	if (drive == 0 || drive == 1)
		return 1;

	return 0;
}

/* _al_getdrive:
 *  Returns the current drive number (0=ux0, 1=uma0).
 */ 
int _al_getdrive()
{
	//PSV_DEBUG("_al_getdrive()");
	// TODO: Add uma0 support.
	return 0;
}

/* _al_getdcwd:
 *  Returns the current working directory on the specified drive.
 */
void _al_getdcwd(int drive, char *buf, int size)
{
	//PSV_DEBUG("PSVITA: _al_getdcwd(), drive = %d", drive);

	if (drive == 0){
		char* app_dir = psv_get_app_dir();

		if (app_dir)
			do_uconvert(app_dir, U_ASCII, buf, U_CURRENT, size); 
		else
			do_uconvert("ux0:data/", U_ASCII, buf, U_CURRENT, size); 
	}else if (drive == 1){
		do_uconvert("uma0:", U_ASCII, buf, U_CURRENT, size); 
	}
}

/* al_findfirst:
    Initiates a directory search.
    Low-level function for searching files. This function finds the first 
    file which matches the given wildcard specification and file attributes 
    (see above). The information about the file (if any) will be put in the 
    al_ffblk structure which you have to provide. 
 */
int al_findfirst(AL_CONST char *pattern, struct al_ffblk *info, int attrib)
{
   //PSV_DEBUG("PSVITA: al_findfirst()");
   //PSV_DEBUG("pattern = %s", pattern);
   struct FF_DATA *ff_data;
   struct stat s;
   int actual_attrib;
   char tmp[1024];
   char *p;

   /* allocate ff_data structure */
   ff_data = _AL_MALLOC(sizeof(struct FF_DATA));
   if (!ff_data) {
      *allegro_errno = ENOMEM;
      return -1;
   }

   memset(ff_data, 0, sizeof *ff_data);
   info->ff_data = (void *) ff_data;

   /* if the pattern contains no wildcard, we use stat() */
   if (!ustrpbrk(pattern, uconvert("?*", U_ASCII, tmp, U_CURRENT, sizeof(tmp)))) {
      /* start the search */
      errno = *allegro_errno = 0;

      if (stat(uconvert(pattern, U_CURRENT, tmp, U_UTF8, sizeof(tmp)), &s) == 0) {
         /* get file attributes */
         actual_attrib = ff_get_attrib(ff_get_filename(uconvert(pattern, U_CURRENT, tmp, U_UTF8, sizeof(tmp))), &s);

         /* does it match ? */
         if ((actual_attrib & ~attrib) == 0) {
            info->attrib = actual_attrib;
            info->time = s.st_mtime;
            info->size = s.st_size; /* overflows at 2GB */
            ff_data->size = s.st_size;
            ustrzcpy(info->name, sizeof(info->name), get_filename(pattern));
            return 0;
         }
      }

       _AL_FREE(ff_data);
      info->ff_data = NULL;
      *allegro_errno = (errno ? errno : ENOENT);
      return -1;
   }

   ff_data->attrib = attrib;

   do_uconvert(pattern, U_CURRENT, ff_data->dirname, U_UTF8, sizeof(ff_data->dirname));
   p = ff_get_filename(ff_data->dirname);
   _al_sane_strncpy(ff_data->pattern, p, sizeof(ff_data->pattern));
   if (p == ff_data->dirname)
      _al_sane_strncpy(ff_data->dirname, "./", FF_MAXPATHLEN);
   else
      *p = 0;

   /* nasty bodge, but gives better compatibility with DOS programs */
   if (strcmp(ff_data->pattern, "*.*") == 0)
      _al_sane_strncpy(ff_data->pattern, "*", FF_MAXPATHLEN);

   /* start the search */
   errno = *allegro_errno = 0;

   ff_data->dir = opendir(ff_data->dirname);

   if (!ff_data->dir) {
      *allegro_errno = (errno ? errno : ENOENT);
      _AL_FREE(ff_data);
      info->ff_data = NULL;
      return -1;
   }

   if (al_findnext(info) != 0) {
      al_findclose(info);
      return -1;
   }

   return 0;
}

/* al_findnext:
 *  Retrieves the next file from a directory search.
 */
int al_findnext(struct al_ffblk *info)
{
   //PSV_DEBUG("PSVITA: al_findnext()");
   char tempname[FF_MAXPATHLEN];
   char filename[FF_MAXPATHLEN];
   int attrib;
   struct dirent *entry;
   struct stat s;
   struct FF_DATA *ff_data = (struct FF_DATA *) info->ff_data;

   ASSERT(ff_data);

   /* if the pattern contained no wildcard */
   if (!ff_data->dir)
      return -1;

   while (TRUE) {
      /* read directory entry */
      entry = readdir(ff_data->dir);
      if (!entry) {
         *allegro_errno = (errno ? errno : ENOENT);
         return -1;
      }

      /* try to match file name with pattern */
      tempname[0] = 0;
      if (NAMLEN(entry) >= sizeof(tempname))
         strncat(tempname, entry->d_name, sizeof(tempname) - 1);
      else
         strncat(tempname, entry->d_name, NAMLEN(entry));

      if (ff_match(tempname, ff_data->pattern)) {
         _al_sane_strncpy(filename, ff_data->dirname, FF_MAXPATHLEN);
         ff_put_backslash(filename, sizeof(filename));
         strncat(filename, tempname, sizeof(filename) - strlen(filename) - 1);

         /* get file attributes */
         if (stat(filename, &s) == 0) {
            attrib = ff_get_attrib(tempname, &s);

            /* does it match ? */
            if ((attrib & ~ff_data->attrib) == 0)
               break;
         }
         else {
            /* evil! but no other way to avoid exiting for_each_file() */
            *allegro_errno = 0;
         }
      }
   }

   info->attrib = attrib;
   info->time = s.st_mtime;
   info->size = s.st_size; /* overflows at 2GB */
   ff_data->size = s.st_size;

   do_uconvert(tempname, U_UTF8, info->name, U_CURRENT, sizeof(info->name));

   return 0;
}

/* al_findclose:
 *  Cleans up after a directory search.
 */
void al_findclose(struct al_ffblk *info)
{
	//PSV_DEBUG("PSVITA: al_findclose()");
   struct FF_DATA *ff_data = (struct FF_DATA *) info->ff_data;

   if (ff_data) {
      if (ff_data->dir) {
         closedir(ff_data->dir);
      }
      _AL_FREE(ff_data);
      info->ff_data = NULL;

      /* to avoid leaking memory */
      ff_match(NULL, NULL);
   }
}

void _al_detect_filename_encoding(void)
{
	PSV_DEBUG("UNIMPLEMENTED: _al_detect_filename_encoding()");
}


/* ff_get_filename:
 *  When passed a completely specified file path, this returns a pointer
 *  to the filename portion.
 */
static char *ff_get_filename(AL_CONST char *path)
{
   char *p = (char*)path + strlen(path);

   while ((p > path) && (*(p - 1) != '/'))
      p--;

   return p;
}



/* ff_put_backslash:
 *  If the last character of the filename is not a /, this routine will
 *  concatenate a / on to it.
 */
static void ff_put_backslash(char *filename, int size)
{
   int len = strlen(filename);

   if ((len > 0) && (len < (size - 1)) && (filename[len - 1] != '/')) {
      filename[len] = '/';
      filename[len + 1] = 0;
   }
}

/* ff_match:
 *  Matches two strings ('*' matches any number of characters,
 *  '?' matches any character).
 */
static int ff_match(AL_CONST char *s1, AL_CONST char *s2)
{
   static unsigned int size = 0;
   static struct FF_MATCH_DATA *data = NULL;
   AL_CONST char *s1end;
   int index, c1, c2;

   /* handle NULL arguments */
   if ((!s1) && (!s2)) {
      if (data) {
         _AL_FREE(data);
         data = NULL;
      }

      return 0;
   }

   s1end = s1 + strlen(s1);

   /* allocate larger working area if necessary */
   if (data && (size < strlen(s2))) {
      _AL_FREE(data);
      data = NULL;
   }

   if (!data) {
      size = strlen(s2);
      data = _AL_MALLOC(sizeof(struct FF_MATCH_DATA) * size * 2 + 1);
      if (!data)
         return 0;
   }

   index = 0;
   data[0].s1 = s1;
   data[0].s2 = s2;
   data[0].type = FF_MATCH_TRY;

   while (index >= 0) {
      s1 = data[index].s1;
      s2 = data[index].s2;
      c1 = *s1;
      c2 = *s2;

      switch (data[index].type) {

      case FF_MATCH_TRY:
         if (c2 == 0) {
            /* pattern exhausted */
            if (c1 == 0)
               return 1;
            else
               index--;
         }
         else if (c1 == 0) {
            /* string exhausted */
            while (*s2 == '*')
               s2++;
            if (*s2 == 0)
               return 1;
            else
               index--;
         }
         else if (c2 == '*') {
            /* try to match the rest of pattern with empty string */
            data[index++].type = FF_MATCH_ANY;
            data[index].s1 = s1end;
            data[index].s2 = s2 + 1;
            data[index].type = FF_MATCH_TRY;
         }
         else if ((c2 == '?') || (c1 == c2)) {
            /* try to match the rest */
            data[index++].type = FF_MATCH_ONE;
            data[index].s1 = s1 + 1;
            data[index].s2 = s2 + 1;
            data[index].type = FF_MATCH_TRY;
         }
         else
            index--;
         break;

      case FF_MATCH_ONE:
         /* the rest of string did not match, try earlier */
         index--;
         break;

      case FF_MATCH_ANY:
         /* rest of string did not match, try add more chars to string tail */
         if (--data[index + 1].s1 >= s1) {
            data[index + 1].type = FF_MATCH_TRY;
            index++;
         }
         else
            index--;
         break;

      default:
         /* this is a bird? This is a plane? No it's a bug!!! */
         return 0;
      }
   }

   return 0;
}



/* ff_get_attrib:
 *  Builds up the attribute list of the file pointed to by name and s.
 */
static int ff_get_attrib(AL_CONST char *name, struct stat *s)
{
   int attrib = 0;
   uid_t euid = geteuid();

   if (euid != 0) {
      if (s->st_uid == euid) {
	 if ((s->st_mode & S_IWUSR) == 0)
	    attrib |= FA_RDONLY;
      }
      else if (s->st_gid == getegid()) {
	 if ((s->st_mode & S_IWGRP) == 0)
	    attrib |= FA_RDONLY;
      }
      else if ((s->st_mode & S_IWOTH) == 0) {
	 attrib |= FA_RDONLY;
      }
   }

   if (S_ISDIR(s->st_mode))
      attrib |= FA_DIREC;

   if ((name[0] == '.') && ((name[1] != '.') || (name[2] != '\0')))
      attrib |= FA_HIDDEN;

   return attrib;
}