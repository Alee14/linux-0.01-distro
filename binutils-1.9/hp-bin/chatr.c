/* Change Attributes program for GNU.
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

#include <a.out.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/file.h>
#include "ioutil.h"

#define forward extern

enum boolean { false, true };
typedef enum boolean boolean;

boolean change_to_shared_p;
boolean change_to_demand_loaded_p;
boolean silent_p;
int failures;

char *input_filename;
char output_filename [20];
int output_descriptor;

void
main (argc, argv)
     int argc;
     char *argv[];
{
  register int argi;
  register char **argp;
  forward void usage ();
  forward void process_file ();
  forward void file_abort_handler ();
  extern char *mktemp ();

  iou_set_program_name (argv);
  argi = 1;
  argp = (& (argv [1]));

  change_to_shared_p = false;
  change_to_demand_loaded_p = false;
  silent_p = false;

  while ((argi < argc) && (((*argp) [0]) == '-'))
    {
      switch ((*argp) [1])
	{
	case 'n':
	  change_to_shared_p = true;
	  break;
	case 'q':
	  change_to_demand_loaded_p = true;
	  break;
	case 's':
	  silent_p = true;
	  break;
	default:
	  usage ();
	}
      if (((*argp) [2]) != '\0')
	usage ();
      argi += 1;
      argp += 1;
    }

  if (change_to_shared_p && change_to_demand_loaded_p)
    iou_error ("conflicting options: -n and -q");

  if ((! change_to_shared_p) && (! change_to_demand_loaded_p) && silent_p)
    exit (0);

  if (argi == argc)
    exit (255);

  strcpy (output_filename, "/tmp/chatrXXXXXX");
  if (output_filename != (mktemp (output_filename)))
    iou_error ("mktemp failure");

  failures = 0;
  for (; (argi < argc); argi += 1)
    {
      input_filename = (*argp++);
      output_descriptor =
	(iou_open (output_filename, (O_RDWR | O_CREAT | O_TRUNC), 0666));
      iou_abort_handler_bind (process_file, file_abort_handler);
      iou_close (output_descriptor);
    }
  iou_unlink (output_filename);
  exit (failures);
}

void
usage ()
{
  fprintf (stderr, "usage: %s [-n] [-q] [-s] file ...\n", iou_program_name);
  iou_error ();
}

void
file_abort_handler ()
{
  failures += 1;
  return;
}

void
file_error (message)
     char *message;
{
  char buffer [256];

  sprintf (buffer, "%s: \"%s\"", message, input_filename);
  iou_error (buffer);
}

void
file_copy (input_descriptor, output_descriptor)
     int input_descriptor;
     int output_descriptor;
{
  char buffer [8192];
  register int bytes_read;

  while (1)
    {
      bytes_read = (iou_read (input_descriptor, buffer, 8192));
      if (bytes_read == 0) break;
      iou_write (output_descriptor, buffer, bytes_read);
    }
  return;
}

void
process_file ()
{
  int input_descriptor;
  struct exec input_exec;
  struct exec output_exec;

  input_descriptor = (iou_open (input_filename, O_RDONLY));

  if ((iou_read (input_descriptor, (& input_exec), (sizeof (input_exec)))) !=
      (sizeof (input_exec)))
    file_error ("unable to read file header");

  switch (N_MAGIC (input_exec))
    {
    case NMAGIC:
      if (change_to_shared_p)
	file_error ("file not demand load executable");
      break;

    case ZMAGIC:
      if (change_to_demand_loaded_p)
	file_error ("file not shared executable");
      break;

    default:
      file_error ("file not executable format");
    }

  if (! silent_p)
    {
      printf ("%s:\n", input_filename);
      if (change_to_shared_p || change_to_demand_loaded_p)
	printf ("   current values:\n");
      printf
	("         %s executable\n",
	 (((N_MAGIC (input_exec)) == NMAGIC) ? "shared" : "demand loaded"));
      fflush (stdout);
    }

  if ((! change_to_shared_p) && (! change_to_demand_loaded_p))
    return;

  output_exec = input_exec;
  N_SET_MAGIC (output_exec, (change_to_shared_p ? NMAGIC : ZMAGIC));

  iou_write (output_descriptor, (& output_exec), (sizeof (output_exec)));
  iou_lseek (input_descriptor, (N_TXTOFF (input_exec)), 0);
  iou_lseek (output_descriptor, (N_TXTOFF (output_exec)), 0);
  file_copy (input_descriptor, output_descriptor);

  /* Now copy the temporary output file back into the input file. */
  iou_close (input_descriptor);
  iou_lseek (output_descriptor, 0, 0);
  input_descriptor = (iou_open (input_filename, (O_WRONLY | O_TRUNC), 0777));
  file_copy (output_descriptor, input_descriptor);
  iou_close (input_descriptor);

  if (! silent_p)
    {
      printf ("   new values:\n");
      printf
	("         %s executable\n",
	 (((N_MAGIC (output_exec)) == NMAGIC) ? "shared" : "demand loaded"));
      fflush (stdout);
    }
  return;
}
