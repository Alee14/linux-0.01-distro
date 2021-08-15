/* I/O Utilities
   Copyright (C) 1988 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <varargs.h>
#include <sys/file.h>
#include <string.h>
#define index(s, c) strchr (s, c)
#include <errno.h>
#include <setjmp.h>
#include "ioutil.h"

char *iou_program_name;

void
iou_set_program_name (argv)
     char *argv [];
{
  register char *start;
  register char *slash;

  start = (argv [0]);
  while (1)
    {
      slash = (index (start, '/'));
      if (slash == NULL) break;
      start = (slash + 1);
    }
  iou_program_name = start;
  return;
}

struct abort_list
  {
    struct abort_list *next;
    void (* handler) ();
    jmp_buf catch_point;
  };
typedef struct abort_list *abort_list;

static abort_list abort_handlers = NULL;

void
iou_abort ()
{
  register abort_list handlers;

  handlers = abort_handlers;
  if (handlers == NULL) abort ();
  longjmp ((handlers -> catch_point), 1); /*NOTREACHED*/
}

void
iou_abort_handler_bind (thunk, handler)
     void (* thunk) ();
     void (* handler) ();
{
  register abort_list handlers;
  register int setjmp_result;

  handlers = ((abort_list) (iou_malloc (sizeof (struct abort_list))));
  (handlers -> next) = abort_handlers;
  (handlers -> handler) = handler;
  abort_handlers = handlers;
  setjmp_result = (setjmp (handlers -> catch_point));
  switch (setjmp_result)
    {
    case 0:
      (* thunk) ();
      if (handlers != abort_handlers)
	iou_fatal_error ("iou_abort_handler_bind: sync error");
      abort_handlers = (handlers -> next);
      iou_free (handlers);
      return;

    case 1:
      {
	register abort_list handlers;
	register void (* handler) ();

	handlers = abort_handlers;
	handler = (handlers -> handler);
	abort_handlers = (handlers -> next);
	iou_free (handlers);
	(* handler) ();
	return;
      }

    default:
      iou_fatal_error ("Illegal setjmp value: %d", setjmp_result);
      /*NOTREACHED*/
    }
}

#ifndef hpux
#ifdef bsd

static void
vfprintf (stream, format, args)
     FILE *stream;
     char *format;
     char *args;
{
  _doprnt (format, args, stderr);
  return;
}

#else
#include "error: no definition for `vfprintf' procedure"
#endif
#endif

#define WARN()								\
{									\
  va_list args;								\
  char *format;								\
									\
  fprintf (stderr, "%s: ", iou_program_name);				\
  va_start (args);							\
  format = (va_arg (args, char *));					\
  vfprintf (stderr, format, args);					\
  va_end (args);							\
  putc ('\n', stderr);							\
}

/*VARARGS0*/ void
iou_warning (va_alist)
     va_dcl
{
  WARN();
  return;
}

/*VARARGS0*/ void
iou_error (va_alist)
     va_dcl
{
  WARN();
  iou_abort (); /*NOTREACHED*/
}

/*VARARGS0*/ void
iou_fatal_error (va_alist)
     va_dcl
{
  WARN();
  abort (); /*NOTREACHED*/
}

void
iou_call_warning (name)
     char *name;
{
  extern int sys_nerr;
  extern char *sys_errlist [];

  if (errno < sys_nerr)
    iou_warning ("%s during system call: %s", (sys_errlist [errno]), name);
  else
    iou_warning ("error code %d during system call: %s", errno, name);
  return;
}

void
iou_call_error (name)
     char *name;
{
  iou_call_warning (name);
  iou_abort (); /*NOTREACHED*/
}

void
iou_stat (path, buf)
     char *path;
     struct stat *buf;
{
  extern int stat ();
  register int result;

  while (1)
    {
      result = (stat (path, buf));
      if (result == 0) break;
      if (errno != EINTR) iou_call_error ("stat");
    }
  return;
}

void
iou_unlink (path)
     char *path;
{
  extern int unlink ();

  if ((unlink (path)) != 0)
    iou_call_error ("unlink");
  return;
}

int
iou_open (path, oflag, mode)
     char *path;
     int oflag;
     int mode;
{
  register int result;

  while (1)
    {
      result = (open (path, oflag, mode));
      if (result >= 0) break;
      if (errno != EINTR) iou_call_error ("open");
    }
  return (result);
}

void
iou_close (descriptor)
     int descriptor;
{
  if ((close (descriptor)) != 0)
    iou_call_error ("close");
  return;
}

long
iou_lseek (descriptor, offset, whence)
     int descriptor;
     long offset;
     int whence;
{
  extern long lseek ();
  register long result;

  while (1)
    {
      result = (lseek (descriptor, offset, whence));
      if (result >= 0) break;
      if (errno != EINTR) iou_call_error ("lseek");
    }
  return (result);
}

int
iou_read (input_file, buffer, length)
     int input_file;
     register char *buffer;
     int length;
{
  extern int read ();
  register int bytes_remaining;
  register int result;

  bytes_remaining = length;
  while (bytes_remaining > 0)
    {
      result = (read (input_file, buffer, bytes_remaining));
      if (result == 0) return (length - bytes_remaining);
      if (result > 0)
	{
	  buffer += result;
	  bytes_remaining -= result;
	  continue;
	}
      if (errno != EINTR) iou_call_error ("read");
    }
  return (length);
}

void
iou_write (output_file, buffer, length)
     int output_file;
     register char *buffer;
     register int length;
{
  extern int write ();
  register int bytes_remaining;
  register int result;

  bytes_remaining = length;
  while (bytes_remaining > 0)
    {
      result = (write (output_file, buffer, bytes_remaining));
      if (result >= 0)
	{
	  buffer += result;
	  bytes_remaining -= result;
	  continue;
	}
      if (errno != EINTR) iou_call_error ("write");
    }
  return;
}

char *
iou_malloc (length)
     unsigned length;
{
  extern char *malloc ();
  register char *result;

  result = (malloc (length));
  if (result == NULL) iou_error ("virtual memory exhausted");
  return (result);
}

char *
iou_realloc (pointer, length)
     char *pointer;
     unsigned length;
{
  extern char *realloc ();
  register char *result;

  result = (realloc (pointer, length));
  if (result == NULL) iou_error ("virtual memory exhausted");
  return (result);
}
