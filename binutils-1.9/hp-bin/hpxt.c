/* Program to translate binary format files from HP-UX to GNU.
   Input formats for HP 9000 series 300/400 only.
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

#if !defined(hp9000s300) && !defined(__hp9000s300)
#include "error: hpxt is for 300- or 400-series machines only"
#endif

#include <stdio.h>
#include <varargs.h>
#include <fcntl.h>
#include <sys/file.h>
#include <string.h>
#define index(s, c) strchr (s, c)
#define bzero(p, n) memset (p, 0, n)
#include "ioutil.h"

#define forward extern

/* IO primitives */

void
xread (input_file, buffer, length)
     int input_file;
     char *buffer;
     int length;
{
  if ((iou_read (input_file, buffer, length)) != length)
    iou_error ("read failed to complete");
  return;
}

/* HP-UX data structure descriptions */

struct hp_magic
  {
    unsigned short system_id;
    unsigned short file_type;
  };

#define HP_SYSID_98x6		0x020A
#define HP_SYSID_9000S200	0x020C

#define HP_SYSID_OK(sysid)						\
  (((sysid) == HP_SYSID_9000S200)					\
   || ((sysid) == HP_SYSID_98x6))

#define HP_FILETYPE_RELOC	0x0106 /* relocatable only */
#define HP_FILETYPE_EXEC	0x0107 /* normal executable */
#define HP_FILETYPE_SHARE	0x0108 /* shared executable */
#define HP_FILETYPE_DEMAND	0x010B /* demand-load executable */

#define HP_FILETYPE_OK(filetype)					\
  (((filetype) == HP_FILETYPE_EXEC)					\
   || ((filetype) == HP_FILETYPE_SHARE)					\
   || ((filetype) == HP_FILETYPE_DEMAND)				\
   || ((filetype) == HP_FILETYPE_RELOC))

#define HP_MAGIC_OK(magic)						\
  ((HP_SYSID_OK ((magic) . system_id))					\
   && (HP_FILETYPE_OK ((magic) . file_type)))

struct hp_exec
  {
    struct hp_magic a_magic;
    short a_stamp;		/* version id */
    short a_unused;
    long a_sparehp;
    long a_text;		/* size of text segment */
    long a_data;		/* size of data segment */
    long a_bss;			/* size of bss segment */
    long a_trsize;		/* text relocation size */
    long a_drsize;		/* data relocation size */
    long a_pasint;		/* pascal interface size */
    long a_lesyms;		/* symbol table size */
    long a_dnttsize;		/* debug name table size */
    long a_entry;		/* entry point */
    long a_sltsize;		/* source line table size */
    long a_vtsize;		/* value table size */
    long a_spare3;
    long a_spare4;
  };

#define HP_EXEC_MAGIC(header) (((header) . a_magic) . file_type)

#define HP_PAGE_SIZE 0x1000

#define HP_PAGE_SEEK(descriptor, header, offset)			\
{									\
  if ((HP_EXEC_MAGIC (header)) == HP_FILETYPE_DEMAND)			\
    {									\
      int HP_PAGE_SEEK_remainder;					\
									\
      HP_PAGE_SEEK_remainder = ((offset) % HP_PAGE_SIZE);		\
      if (HP_PAGE_SEEK_remainder != 0)					\
	iou_lseek							\
	  ((descriptor), (HP_PAGE_SIZE - HP_PAGE_SEEK_remainder), 1);	\
    }									\
}

struct hp_nlist
  {
    long n_value;
    unsigned char n_type;
    unsigned char n_length;	/* length of ascii symbol name */
    short n_almod;		/* alignment mod */
    short n_unused;
  };

#define HP_SYMTYPE_UNDEFINED	0x00
#define HP_SYMTYPE_ABSOLUTE	0x01
#define HP_SYMTYPE_TEXT		0x02
#define HP_SYMTYPE_DATA		0x03
#define HP_SYMTYPE_BSS		0x04
#define HP_SYMTYPE_COMMON	0x05
#define HP_SYMTYPE_FILENAME	0x1F

#define HP_SYMTYPE_ALIGN	0x10
#define HP_SYMTYPE_EXTERNAL	0x20

#define HP_SYMTYPE_TYPE		0x0F

struct hp_relocation_info
  {
    long r_address;		/* position of relocation in segment */
    short r_symbolnum;		/* id of the symbol of external relocations */
    char r_segment;
    char r_length;
  };

#define HP_RSEGMENT_TEXT	0x00
#define HP_RSEGMENT_DATA	0x01
#define HP_RSEGMENT_BSS		0x02
#define HP_RSEGMENT_EXTERNAL	0x03
#define HP_RSEGMENT_NOOP	0x3F

#define HP_RLENGTH_BYTE		0x00
#define HP_RLENGTH_WORD		0x01
#define HP_RLENGTH_LONG		0x02
#define HP_RLENGTH_ALIGN	0x03

#define HP_ARMAG "!<arch>\n"
#define HP_SARMAG 8
#define HP_ARFMAG "`\n"

struct hp_ar_hdr
  {
    char ar_name [16];		/* file member name - `/' terminated */
    char ar_date [12];		/* file member date - decimal */
    char ar_uid [6];		/* file member user id - decimal */
    char ar_gid [6];		/* file member group id - decimal */
    char ar_mode [8];		/* file member mode - octal */
    char ar_size [10];		/* file member size - decimal */
    char ar_fmag [2];		/* ARFMAG - string to end header */
  };

struct hp_ar_symtab_hdr
  {
    unsigned short n_entries;
    long string_table_size;
  };

struct hp_ar_symtab_entry
  {
    long string_index;
    long file_index;
  };

/* GNU data structure descriptions */

#ifdef hpux

/* The `exec' structure and overall layout must be close to HP's when
   we are running on an HP system, otherwise we will not be able to
   execute the resulting file. */

#define GNU_OMAGIC HP_FILETYPE_EXEC
#define GNU_NMAGIC HP_FILETYPE_SHARE
#define GNU_ZMAGIC HP_FILETYPE_SHARE

struct gnu_exec
  {
    struct hp_magic a_magic;
    unsigned long a_spare1;
    unsigned long a_spare2;
    unsigned long a_text;	/* size of text segment */
    unsigned long a_data;	/* size of data segment */
    unsigned long a_bss;	/* size of bss segment */
    unsigned long a_trsize;	/* text relocation size */
    unsigned long a_drsize;	/* data relocation size */
    unsigned long a_spare3;	/* HP = pascal interface size */
    unsigned long a_spare4;	/* HP = symbol table size */
    unsigned long a_spare5;	/* HP = debug name table size */
    unsigned long a_entry;	/* entry point */
    unsigned long a_spare6;	/* HP = source line table size */
    unsigned long a_spare7;	/* HP = value table size */
    unsigned long a_syms;	/* symbol table size */
    unsigned long a_spare8;
  };

#define GNU_EXEC_MAGIC(header) (((header) . a_magic) . file_type)

#define GNU_EXEC_SET_MAGIC(header, magic)				\
{									\
  (((header) . a_magic) . system_id) = HP_SYSID_9000S200;		\
  (((header) . a_magic) . file_type) = (magic);				\
}

#define GNU_PAGE_SEEK_HEADER HP_PAGE_SEEK
#define GNU_PAGE_SEEK_TEXT HP_PAGE_SEEK

#else /* not hpux */

#define	GNU_OMAGIC	0407	/* old impure format */
#define	GNU_NMAGIC	0410	/* read-only text */
#define	GNU_ZMAGIC	0413	/* demand load format */

struct gnu_exec
  {
    unsigned long a_magic;	/* magic number */
    unsigned long a_text;	/* size of text segment */
    unsigned long a_data;	/* size of initialized data */
    unsigned long a_bss;	/* size of uninitialized data */
    unsigned long a_syms;	/* size of symbol table */
    unsigned long a_entry;	/* entry point */
    unsigned long a_trsize;	/* size of text relocation */
    unsigned long a_drsize;	/* size of data relocation */
  };

#define GNU_EXEC_MAGIC(header) ((header) . a_magic)
#define GNU_EXEC_SET_MAGIC(header, magic) ((header) . a_magic) = (magic)

#define GNU_PAGE_SIZE 0x400

#define GNU_PAGE_SEEK_HEADER(descriptor, header, offset)		\
{									\
  if ((GNU_EXEC_MAGIC (header)) == GNU_ZMAGIC)				\
    {									\
      int GNU_PAGE_SEEK_remainder;					\
									\
      GNU_PAGE_SEEK_remainder = ((offset) % GNU_PAGE_SIZE);		\
      if (GNU_PAGE_SEEK_remainder != 0)					\
	iou_lseek							\
	  ((descriptor), (GNU_PAGE_SIZE - GNU_PAGE_SEEK_remainder), 1);	\
    }									\
}

#define GNU_PAGE_SEEK_TEXT(descriptor, header, offset)

#endif /* hpux */

struct gnu_nlist
  {
    union
      {
	char *n_name;		/* for use when in-core */
	long n_strx;		/* index into file string table */
      } n_un;
    unsigned char n_type;	/* type flag (N_TEXT,..)  */
    char n_other;		/* unused */
    short n_desc;		/* see <stab.h> */
    unsigned long n_value;	/* value of symbol (or sdb offset) */
  };

#define	GNU_SYMTYPE_UNDEFINED	0x0
#define	GNU_SYMTYPE_ABSOLUTE	0x2
#define	GNU_SYMTYPE_TEXT	0x4
#define	GNU_SYMTYPE_DATA	0x6
#define	GNU_SYMTYPE_BSS		0x8
#define	GNU_SYMTYPE_COMMON	0x12
#define	GNU_SYMTYPE_FILENAME	0x1F
#define	GNU_SYMTYPE_EXTERNAL	0x01

struct gnu_relocation_info
  {
    long r_address;		/* address which is relocated */
    unsigned int
      r_symbolnum : 24,		/* local symbol ordinal */
      r_pcrel : 1,		/* was relocated pc relative already */
      r_length : 2,		/* BYTE, WORD, or LONG */
      r_extern : 1,		/* does not include value of sym referenced */
      : 4;			/* nothing, yet */
  };

#define GNU_RLENGTH_BYTE	0x00
#define GNU_RLENGTH_WORD	0x01
#define GNU_RLENGTH_LONG	0x02

#define	GNU_ARMAG "!<arch>\n"
#define	GNU_SARMAG 8
#define	GNU_ARFMAG "`\n"

struct gnu_ar_hdr
  {
    char ar_name [16];
    char ar_date [12];
    char ar_uid [6];
    char ar_gid [6];
    char ar_mode [8];
    char ar_size [10];
    char ar_fmag [2];
  };

struct gnu_symdef
  {
    int symbol_name_string_index;
    int library_member_offset;
  };

/* List abstraction */

struct pair
  {
    char *car;
    char *cdr;
  };

struct pair *
pair_cons (car, cdr)
     char *car;
     char *cdr;
{
  register struct pair *result;

  result = ((struct pair *) (iou_malloc (sizeof (struct pair))));
  (result -> car) = car;
  (result -> cdr) = cdr;
  return (result);
}

#define LIST_CDR(list) ((struct pair *) ((list) -> cdr))
#define LIST_SET_CDR(list, newcdr) (list -> cdr) = ((char *) newcdr)

struct pair *
list_reverse (list)
     register struct pair *list;
{
  register struct pair *previous;
  register struct pair *next;

  previous = NULL;
  while (list != NULL)
    {
      next = (LIST_CDR (list));
      LIST_SET_CDR (list, previous);
      previous = list;
      list = next;
    }
  return (previous);
}

struct pair *
read_list (input_file, n_bytes, field_name, read_item, length)
     int input_file;
     int n_bytes;
     char * field_name;
     char * (* read_item) ();
     register int * length;	/* return value */
{
  register struct pair *result;

  result = NULL;
  (* length) = 0;
  while (n_bytes > 0)
    {
      result = (pair_cons (((* read_item) (input_file, (& n_bytes))), result));
      (* length) += 1;
    }
  if (n_bytes != 0)
    iou_error ("%s field size incorrect", field_name);
  return (list_reverse (result));
}

/* Symbol Table */

struct gnu_nlist *
read_symbol (input_file, n_bytes)
     int input_file;
     int *n_bytes;		/* return value */
{
  struct hp_nlist input_nlist;
  register struct gnu_nlist *output_nlist;
  register char *name;

  {
    register int name_length;

    xread (input_file, (& input_nlist), (sizeof (input_nlist)));
    name_length = (input_nlist . n_length);
    name = (iou_malloc (name_length + 1));
    xread (input_file, name, name_length);
    (name [name_length]) = '\0';
    (* n_bytes) -= ((sizeof (input_nlist)) + name_length);
  }

  output_nlist =
    ((struct gnu_nlist *) (iou_malloc (sizeof (struct gnu_nlist))));
  ((output_nlist -> n_un) . n_name) = name;
  (output_nlist -> n_other) = 0;
  (output_nlist -> n_desc) = 0;
  (output_nlist -> n_value) = (input_nlist . n_value);

  {
    register int name_type;

    name_type = (input_nlist . n_type);
    if ((name_type & HP_SYMTYPE_ALIGN) != 0)
      iou_error ("aligned symbol encountered: %s", name);
    if (name_type == HP_SYMTYPE_FILENAME)
      (output_nlist -> n_type) = GNU_SYMTYPE_FILENAME;
    else
      {
	switch (name_type & HP_SYMTYPE_TYPE)
	  {
	  case HP_SYMTYPE_UNDEFINED:
	    (output_nlist -> n_type) = GNU_SYMTYPE_UNDEFINED;
	    break;

	  case HP_SYMTYPE_ABSOLUTE:
	    (output_nlist -> n_type) = GNU_SYMTYPE_ABSOLUTE;
	    break;

	  case HP_SYMTYPE_TEXT:
	    (output_nlist -> n_type) = GNU_SYMTYPE_TEXT;
	    break;

	  case HP_SYMTYPE_DATA:
	    (output_nlist -> n_type) = GNU_SYMTYPE_DATA;
	    break;

	  case HP_SYMTYPE_BSS:
	    (output_nlist -> n_type) = GNU_SYMTYPE_BSS;
	    break;

	  case HP_SYMTYPE_COMMON:
	    (output_nlist -> n_type) = GNU_SYMTYPE_COMMON;
	    break;

	  default:
	    iou_error ("unknown symbol type encountered: %x", name_type);
	  }
	if ((name_type & HP_SYMTYPE_EXTERNAL) != 0)
	  (output_nlist -> n_type) |= GNU_SYMTYPE_EXTERNAL;
      }
  }

  return (output_nlist);
}

struct pair *
read_symbol_table (input_file, n_bytes, length)
     int input_file;
     int n_bytes;
     int * length;		/* return value */
{
  return
    (read_list (input_file, n_bytes, "symbol table", read_symbol, length));
}

void
write_symbol_table (output_file, symbol_table, length)
     int output_file;
     struct pair *symbol_table;
     int length;
{
  register struct pair *scan;
  register char *name;
  struct gnu_nlist output_nlist;
  int string_table_index;

  (output_nlist . n_other) = 0;
  (output_nlist . n_desc) = 0;

  string_table_index = (sizeof (string_table_index));
  for (scan = symbol_table; (scan != NULL); scan = (LIST_CDR (scan)))
    {
      register struct gnu_nlist *symbol;

      symbol = ((struct gnu_nlist *) (scan -> car));
      name = ((symbol -> n_un) . n_name);

      ((output_nlist . n_un) . n_strx) = string_table_index;
      (output_nlist . n_type) = (symbol -> n_type);
      (output_nlist . n_value) = (symbol -> n_value);
      iou_write (output_file, (& output_nlist), (sizeof (output_nlist)));

      string_table_index += ((strlen (name)) + 1);
      (scan -> car) = name;
      iou_free (symbol);
    }
  iou_write
    (output_file, (& string_table_index), (sizeof (string_table_index)));

  scan = symbol_table;
  while (scan != NULL)
    {
      register struct pair *cdr;

      name = (scan -> car);
      iou_write (output_file, name, ((strlen (name)) + 1));
      iou_free (name);
      cdr = (LIST_CDR (scan));
      iou_free (scan);
      scan = cdr;
    }
  return;
}

/* Relocation Info */

struct gnu_relocation_info *
read_relocation_info_1 (input_file, n_bytes)
     int input_file;
     register int * n_bytes;	/* return value */
{
  struct hp_relocation_info input_info;
  register struct gnu_relocation_info *output_info;

  xread (input_file, (& input_info), (sizeof (input_info)));
  (* n_bytes) -= (sizeof (input_info));

  output_info =
    ((struct gnu_relocation_info *)
     (iou_malloc (sizeof (struct gnu_relocation_info))));
  (output_info -> r_pcrel) = 0;
  (output_info -> r_address) = (input_info . r_address);

  switch (input_info . r_segment)
    {
    case HP_RSEGMENT_TEXT:
      (output_info -> r_symbolnum) = GNU_SYMTYPE_TEXT;
      (output_info -> r_extern) = 0;
      break;

    case HP_RSEGMENT_DATA:
      (output_info -> r_symbolnum) = GNU_SYMTYPE_DATA;
      (output_info -> r_extern) = 0;
      break;

    case HP_RSEGMENT_BSS:
      (output_info -> r_symbolnum) = GNU_SYMTYPE_BSS;
      (output_info -> r_extern) = 0;
      break;

    case HP_RSEGMENT_EXTERNAL:
      (output_info -> r_symbolnum) = (input_info . r_symbolnum);
      (output_info -> r_extern) = 1;
      break;

    default:
      iou_error
	("illegal relocation segment type: %x", (input_info . r_segment));
    }

  switch (input_info . r_length)
    {
    case HP_RLENGTH_BYTE:
      (output_info -> r_length) = GNU_RLENGTH_BYTE;
      break;

    case HP_RLENGTH_WORD:
      (output_info -> r_length) = GNU_RLENGTH_WORD;
      break;

    case HP_RLENGTH_LONG:
      (output_info -> r_length) = GNU_RLENGTH_LONG;
      break;

    default:
      iou_error ("illegal relocation length: %x", (input_info . r_length));
    }

  return (output_info);
}

struct pair *
read_relocation_info (input_file, n_bytes, length)
     int input_file;
     int n_bytes;
     int * length;
{
  return
    (read_list
     (input_file, n_bytes, "relocation info", read_relocation_info_1, length));
}

void
write_relocation_info (output_file, relocation_info, length)
     int output_file;
     struct pair *relocation_info;
     int length;
{
  register struct pair *this;
  register struct gnu_relocation_info *car;
  register struct pair *cdr;

  this = relocation_info;
  while (this != NULL)
    {
      car = ((struct gnu_relocation_info *) (this -> car));
      iou_write (output_file, car, (sizeof (struct gnu_relocation_info)));
      iou_free (car);
      cdr = (LIST_CDR (this));
      iou_free (this);
      this = cdr;
    }
  return;
}

/* Convert an executable (or relocatable) file. */

void
process_executable_file (input_file, output_file)
     int input_file;
     int output_file;
{
  struct hp_exec input_exec;
  struct gnu_exec output_exec;
  char *text_segment;
  char *data_segment;
  struct pair *symbol_table;
  int symbol_table_length;
  struct pair *text_relocation_info;
  int text_relocation_info_length;
  struct pair *data_relocation_info;
  int data_relocation_info_length;

  xread (input_file, (& input_exec), (sizeof (input_exec)));
  HP_PAGE_SEEK (input_file, input_exec, (sizeof (input_exec)));

  if ((input_exec . a_text) != 0)
    {
      text_segment = (iou_malloc (input_exec . a_text));
      xread (input_file, text_segment, (input_exec . a_text));
      HP_PAGE_SEEK (input_file, input_exec, (input_exec . a_text));
    }
  else
    text_segment = NULL;

  if ((input_exec . a_data) != 0)
    {
      data_segment = (iou_malloc (input_exec . a_data));
      xread (input_file, data_segment, (input_exec . a_data));
      HP_PAGE_SEEK (input_file, input_exec, (input_exec . a_data));
    }
  else
    data_segment = NULL;

  /* Skip the pascal interface text */
  iou_lseek (input_file, (input_exec . a_pasint), 1);

  symbol_table =
    (read_symbol_table
     (input_file, (input_exec . a_lesyms), (& symbol_table_length)));

  /* Skip the debugging tables */
  iou_lseek
    (input_file,
     ((input_exec . a_dnttsize)
      + (input_exec . a_sltsize)
      + (input_exec . a_vtsize)),
     1);

  text_relocation_info =
    (read_relocation_info
     (input_file, (input_exec . a_trsize), (& text_relocation_info_length)));
  data_relocation_info =
    (read_relocation_info
     (input_file, (input_exec . a_drsize), (& data_relocation_info_length)));

  bzero ((& output_exec), (sizeof (output_exec)));
  switch (HP_EXEC_MAGIC (input_exec))
    {
    case HP_FILETYPE_RELOC:
    case HP_FILETYPE_EXEC:
      GNU_EXEC_SET_MAGIC (output_exec, GNU_OMAGIC);
      break;

    case HP_FILETYPE_SHARE:
      GNU_EXEC_SET_MAGIC (output_exec, GNU_NMAGIC);
      break;

    case HP_FILETYPE_DEMAND:
      GNU_EXEC_SET_MAGIC (output_exec, GNU_ZMAGIC);
      break;

    default:
      iou_error ("unknown magic type: %x", (HP_EXEC_MAGIC (input_exec)));
    }
  (output_exec . a_text) = (input_exec . a_text);
  (output_exec . a_data) = (input_exec . a_data);
  (output_exec . a_bss) = (input_exec . a_bss);
  (output_exec . a_syms) = (symbol_table_length * (sizeof (struct gnu_nlist)));
  (output_exec . a_entry) = (input_exec . a_entry);
  (output_exec . a_trsize) = (input_exec . a_trsize);
  (output_exec . a_drsize) = (input_exec . a_drsize);

  iou_write (output_file, (& output_exec), (sizeof (output_exec)));
  GNU_PAGE_SEEK_HEADER (output_file, output_exec, (sizeof (output_exec)));
  if (text_segment != NULL)
    {
      iou_write (output_file, text_segment, (output_exec . a_text));
      iou_free (text_segment);
      GNU_PAGE_SEEK_TEXT (output_file, output_exec, (output_exec . a_text));
    }
  if (data_segment != NULL)
    {
      iou_write (output_file, data_segment, (output_exec . a_data));
      iou_free (data_segment);
      GNU_PAGE_SEEK_TEXT (output_file, output_exec, (output_exec . a_data));
    }
  write_relocation_info
    (output_file, text_relocation_info, text_relocation_info_length);
  write_relocation_info
    (output_file, data_relocation_info, data_relocation_info_length);
  write_symbol_table (output_file, symbol_table, symbol_table_length);

  return;
}

/* Convert an archive file. */

/* These procedures take advantage of the fact that the HP and GNU
   formats for struct ar_hdr are virtually identical. */

long symtab_start_position;
char *symtab_member_data;
struct hp_ar_symtab_entry *hp_symtab_entries;
struct gnu_symdef *gnu_symtab_entries;
long symtab_entries_length;
long symtab_names_length;

void
process_archive_file (input_file, output_file)
     int input_file;
     int output_file;
{
  int start_position;
  int length;
  struct hp_ar_hdr input_header;
  int member_length;
  forward void process_archive_symtab ();
  forward void update_symtab_entries ();
  forward void output_symtab_entries ();
  forward void process_archive_entry ();

  gnu_symtab_entries = NULL;
  while (1)
    {
      start_position = (iou_lseek (input_file, 0, 1));
      length = (read (input_file, (& input_header), (sizeof (input_header))));
      if (length == 0)
	break;
      if (length != (sizeof (input_header)))
	iou_error ("input archive ends prematurely");
      if (((strncmp
	    ((input_header . ar_fmag),
	     HP_ARFMAG,
	     (sizeof (input_header . ar_fmag))))
	   != 0)
	  ||
	  ((sscanf ((input_header . ar_size), "%d", (& member_length)))
	   != 1))
	iou_error ("malformatted header of archive member");

      if (((input_header . ar_name) [0]) == '/')
	process_archive_symtab
	  (input_file, output_file, (& input_header), member_length);
      else
	{
	  if (gnu_symtab_entries != NULL)
	    update_symtab_entries
	      (start_position, (iou_lseek (output_file, 0, 1)));
	  process_archive_entry (input_file, output_file, (& input_header),
				 member_length);
	}
    }
  if (gnu_symtab_entries != NULL)
    output_symtab_entries (output_file, (& input_header));
  return;
}

#ifdef DEBUG_SYMTAB

void
dump_symtab_data ()
{
  register struct hp_ar_symtab_entry *symtab_entries;
  register char *symtab_names;
  register int i;

  symtab_names = (symtab_member_data + (sizeof (struct hp_ar_symtab_hdr)));
  symtab_entries =
    ((struct hp_ar_symtab_entry *) (symtab_names + symtab_names_length));
  for (i = 0; (i < symtab_entries_length); i += 1)
    {
      fprintf
	(stdout,
	 "Entry %d: string_index = %d; file_index = %d; name = %s\n",
	 i,
	 ((symtab_entries [i]) . string_index),
	 ((symtab_entries [i]) . file_index),
	 (symtab_names + ((symtab_entries [i]) . string_index)));
    }
  fflush (stdout);
  return;
}

#endif /* DEBUG_SYMTAB */

void
process_archive_symtab (input_file, output_file, input_header, member_length)
     int input_file;
     int output_file;
     struct hp_ar_hdr *input_header;
     int member_length;
{
  long symtab_entries_bytes;
  void initialize_symtab_entries ();

  symtab_member_data = (iou_malloc (member_length));
  xread (input_file, symtab_member_data, member_length);

  symtab_entries_length =
    (((struct hp_ar_symtab_hdr *) symtab_member_data) -> n_entries);
  symtab_entries_bytes =
    (symtab_entries_length * (sizeof (struct hp_ar_symtab_entry)));
  symtab_names_length =
    (((struct hp_ar_symtab_hdr *) symtab_member_data) -> string_table_size);

#ifdef DEBUG_SYMTAB
  dump_symtab_data ();
#endif

  {
    char string_buffer [((sizeof (input_header -> ar_name)) + 1)];

    sprintf
      (string_buffer, "%-*s", (sizeof (input_header -> ar_name)), "__.SYMDEF");
    strncpy
      ((input_header -> ar_name),
       string_buffer,
       (sizeof (input_header -> ar_name)));
  }
  {
    char string_buffer [((sizeof (input_header -> ar_size)) + 1)];

    sprintf
      (string_buffer,
       "%-*d",
       (sizeof (input_header -> ar_size)),
       (symtab_entries_bytes + symtab_names_length + (2 * (sizeof (long)))));
    strncpy
      ((input_header -> ar_size),
       string_buffer,
       (sizeof (input_header -> ar_size)));
  }
  iou_write (output_file, input_header, (sizeof (* input_header)));
  iou_write (output_file, (& symtab_entries_bytes), (sizeof (long)));

  /* Leave slot for entries, which we fill in later. */
  symtab_start_position = (iou_lseek (output_file, 0, 1));
  initialize_symtab_entries ();
  iou_lseek (output_file, symtab_entries_bytes, 1);

  iou_write (output_file, (& symtab_names_length), (sizeof (long)));
  iou_write
    (output_file,
     (symtab_member_data + (sizeof (struct hp_ar_symtab_hdr))),
     symtab_names_length);

  return;
}

void
initialize_symtab_entries ()
{
  register struct hp_ar_symtab_entry *scan_hp;
  register struct hp_ar_symtab_entry *end;
  register struct gnu_symdef *scan_gnu;

  hp_symtab_entries =
    ((struct hp_ar_symtab_entry *)
     (symtab_member_data
      + (sizeof (struct hp_ar_symtab_hdr))
      + symtab_names_length));
  gnu_symtab_entries =
    ((struct gnu_symdef *)
     (iou_malloc (symtab_entries_length * (sizeof (struct gnu_symdef)))));

  scan_hp = hp_symtab_entries;
  end = (scan_hp + symtab_entries_length);
  scan_gnu = gnu_symtab_entries;
  while (scan_hp < end)
    {
      (scan_gnu -> symbol_name_string_index) = (scan_hp -> string_index);
      (scan_gnu -> library_member_offset) = (-1);
      scan_hp += 1;
      scan_gnu += 1;
    }

  return;
}

void
update_symtab_entries (old_offset, new_offset)
     register long old_offset;
     long new_offset;
{
  register struct hp_ar_symtab_entry *scan_hp;
  register struct hp_ar_symtab_entry *end;
  register struct gnu_symdef *scan_gnu;

#ifdef DEBUG_SYMTAB
  fprintf (stdout, "Update: old = %d, new = %d\n", old_offset, new_offset);
#endif

  scan_hp = hp_symtab_entries;
  end = (scan_hp + symtab_entries_length);
  scan_gnu = gnu_symtab_entries;
  while (scan_hp < end)
    {
      if ((scan_hp -> file_index) == old_offset)
	{
#ifdef DEBUG_SYMTAB
	  fprintf
	    (stdout,
	     "\t%s\n",
	     (symtab_member_data
	      + (sizeof (struct hp_ar_symtab_hdr))
	      + (scan -> string_index)));
#endif
	  (scan_gnu -> library_member_offset) = new_offset;
	}
      scan_hp += 1;
      scan_gnu += 1;
    }
  return;
}

void
check_symtab_entries ()
{
  register struct gnu_symdef *scan;
  register struct gnu_symdef *end;

  scan = gnu_symtab_entries;
  end = (scan + symtab_entries_length);
  while (scan < end)
    if (((scan++) -> library_member_offset) == (-1))
      iou_error ("Uninitialized __.SYMTAB entry");

  return;
}

void
output_symtab_entries (output_file, input_header)
     int output_file;
     struct hp_ar_hdr *input_header;
{
  check_symtab_entries ();
  iou_lseek (output_file, symtab_start_position, 0);
  iou_write
    (output_file,
     gnu_symtab_entries,
     (symtab_entries_length * (sizeof (struct gnu_symdef))));
  iou_free (symtab_member_data);
  iou_free (gnu_symtab_entries);
  return;
}

void
process_archive_entry (input_file, output_file, input_header, member_length)
     int input_file;
     int output_file;
     struct hp_ar_hdr *input_header;
     int member_length;
{
  long header_position;
  long output_start;
  long output_end;
  long input_end;
  char *slashptr;

  /* Strategy: leave a hole for the header, then process the file.
     Afterwards, back up and write the header in place. */
  header_position = (iou_lseek (output_file, 0, 1));
  output_start = (iou_lseek (output_file, (sizeof (* input_header)), 1));
  input_end = ((iou_lseek (input_file, 0, 1)) + member_length);

  process_executable_file (input_file, output_file);
  if ((iou_lseek (input_file, 0, 1)) != input_end)
    iou_error ("process_executable_file used wrong amount of input");

  output_end = (iou_lseek (output_file, 0, 1));
  iou_lseek (output_file, header_position, 0);
  if (slashptr = index(input_header->ar_name, '/'))
    *slashptr = ' ';
  {
    char string_buffer [((sizeof (input_header -> ar_size)) + 1)];

    sprintf
      (string_buffer,
       "%-*d",
       (sizeof (input_header -> ar_size)),
       (output_end - output_start));
    strncpy
      ((input_header -> ar_size),
       string_buffer,
       (sizeof (input_header -> ar_size)));
  }
  iou_write (output_file, input_header, (sizeof (* input_header)));
  iou_lseek (output_file, output_end, 0);

  /* Pad to even byte boundary if needed */
  if ((input_end & 1) != 0)
    iou_lseek (input_file, 1, 1);
  if ((output_end & 1) != 0)
    iou_write (output_file, "\0", 1);

  return;
}

void
main (argc, argv)
     int argc;
     char *argv[];
{
  int input_file;
  int output_file;
  register int length;
  struct hp_magic input_magic;

  iou_set_program_name (argv);
  if (argc != 3)
    {
      fprintf (stderr, "usage: %s input-file output-file\n", iou_program_name);
      iou_error ();
    }

  input_file = (iou_open ((argv [1]), O_RDONLY));
  output_file = (iou_open ((argv [2]), (O_WRONLY | O_CREAT | O_TRUNC), 0666));

  xread (input_file, (& input_magic), (sizeof (input_magic)));
  if (HP_MAGIC_OK (input_magic))
    {
      iou_lseek (input_file, 0, 0);
      process_executable_file (input_file, output_file);
    }
  else
    {
      char armag [HP_SARMAG];

      iou_lseek (input_file, 0, 0);
      xread (input_file, armag, HP_SARMAG);
      if ((strncmp (armag, HP_ARMAG, HP_SARMAG)) != 0)
	iou_error ("input file not executable or archive");
      iou_write (output_file, GNU_ARMAG, GNU_SARMAG);
      process_archive_file (input_file, output_file);
    }
  exit (0);
  /*NOTREACHED*/
}
