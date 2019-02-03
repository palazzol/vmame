/* Miscelancelous utility functions

   Copyright 2000 Hans de Goede

   This file and the acompanying files in this directory are free software;
   you can redistribute them and/or modify them under the terms of the GNU
   Library General Public License as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later version.

   These files are distributed in the hope that they will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with these files; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/
/* Changelog
Version 0.1, February 2000
-initial release (Hans de Goede)
Version 0.2, May 2000
-moved time system headers from .h to .c to avoid header conflicts (Hans de Goede)
-made uclock a funtion on systems without gettimeofday too (Hans de Goede)
-UCLOCKS_PER_SECOND is now always 1000000, instead of making it depent on
 the system headers. (Hans de Goede)
*/
/* Todo ?
-rename uclock to sysdep_clock (Hans de Goede)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _MSC_VER
#include <sys/time.h>
#endif

#include "misc.h"


#ifdef HAVE_GETTIMEOFDAY
/* Standard UNIX clock() is based on CPU time, not real time.
   Here is a real-time drop in replacement for UNIX systems that have the
   gettimeofday() routine.  This results in much more accurate timing.
*/
uclock_t uclock(void)
{
  static uclock_t init_sec = 0;
  struct timeval tv;

  gettimeofday(&tv, 0);
  if (init_sec == 0) init_sec = tv.tv_sec;

  return (tv.tv_sec - init_sec) * 1000000 + tv.tv_usec;
}
#else

/* some platforms don't define CLOCKS_PER_SEC, according to posix it should
   always be 1000000, so asume that's the case if it is not defined,
   except for openstep which doesn't define it and has it at 64 */
#ifndef CLOCKS_PER_SEC
#ifdef openstep
#define CLOCKS_PER_SEC 64     /* this is correct for OS4.2 intel */
#else
#define CLOCKS_PER_SEC 1000000
#endif
#endif

uclock_t uclock(void)
{
   return (clock() * (1000000 / CLOCKS_PER_SEC));
}

#endif

void print_colums(const char *text1, const char *text2)
{
   fprint_colums(stdout, text1, text2);
}

void fprint_colums(FILE *f, const char *text1, const char *text2)
{
   const char *text[2];
   int i, j, cols, width[2], done = 0;
   char *e_cols = getenv("COLUMNS");

   cols = e_cols? atoi(e_cols):80;
   if ( cols < 6 ) cols = 6;  /* minimum must be 6 */
   cols--;

   /* initialize our arrays */
   text[0] = text1;
   text[1] = text2;
   width[0] = cols * 0.4;
   width[1] = cols - width[0];

   while(!done)
   {
      done = 1;
      for(i = 0; i < 2; i++)
      {
         int to_print = width[i]-1; /* always leave one space open */

         /* we don't want to print more then we have */
         j = strlen(text[i]);
         if (to_print > j)
           to_print = j;

         /* if they have preffered breaks, try to give them to them */
         for(j=0; j<to_print; j++)
            if(text[i][j] == '\n')
            {
               to_print = j;
               break;
            }

         /* if we don't have enough space, break at the first ' ' or '\n' */
         if(to_print < strlen(text[i]))
         {
           while(to_print && (text[i][to_print] != ' ') &&
              (text[i][to_print] != '\n'))
              to_print--;

           /* if it didn't work, just print the columnwidth */
           if(!to_print)
              to_print = width[i]-1;
         }
         fprintf(f, "%-*.*s", width[i], to_print, text[i]);

         /* adjust ptr */
         text[i] += to_print;

         /* skip ' ' and '\n' */
         while((text[i][0] == ' ') || (text[i][0] == '\n'))
            text[i]++;

         /* do we still have text to print */
         if(text[i][0])
            done = 0;
      }
      fprintf(f, "\n");
   }
}
