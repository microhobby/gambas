/***************************************************************************

  file.h

  The file management routines

  (c) 2000-2007 Benoit Minisini <gambas@users.sourceforge.net>

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
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

***************************************************************************/

#ifndef __GB_FILE_H
#define __GB_FILE_H

#include <sys/time.h>

#include "gb_common.h"

#ifdef PROJECT_EXEC

#ifndef GBX_INFO

typedef
  struct {
    short type;
    short mode;
    int64_t size;
    int atime;
    int mtime;
    int ctime;
    uid_t uid;
    gid_t gid;
    bool hidden;
    }
  PACKED
  FILE_STAT;

#endif

/* Constants for Stat() function */

#define GB_STAT_FILE        1
#define GB_STAT_DIRECTORY   2
#define GB_STAT_DEVICE      3
#define GB_STAT_PIPE        4
#define GB_STAT_SOCKET      5
#define GB_STAT_LINK        6

#define GB_STAT_READ        R_OK
#define GB_STAT_WRITE       W_OK
#define GB_STAT_EXEC        X_OK

#define GB_STAT_USER        0
#define GB_STAT_GROUP       1
#define GB_STAT_OTHER       2

#define FILE_TEMP_PREFIX "/tmp/gambas.%d"
#define FILE_TEMP_DIR "/tmp/gambas.%d/%d"
#define FILE_TEMP_FILE "/tmp/gambas.%d/%d/%d.tmp"
#define FILE_TEMP_PATTERN "/tmp/gambas.%d/%d/%s.tmp"

#endif

#ifndef GBX_INFO

const char *FILE_cat(const char *path, ...);
char *FILE_buffer(void);
int FILE_buffer_length(void);
const char *FILE_get_dir(const char *path);
const char *FILE_get_name(const char *path);
const char *FILE_get_ext(const char *path);
const char *FILE_get_basename(const char *path);
/*PUBLIC const char *FILE_get(const char *path);*/
const char *FILE_set_ext(const char *path, const char *ext);

const char *FILE_getcwd(const char *subdir);
#define FILE_get_current_dir() FILE_getcwd(NULL)

const char *FILE_readlink(const char *link);
bool FILE_is_dir(const char *path);
#define FILE_isdir FILE_is_dir

const char *FILE_find_gambas(void);

bool FILE_exist(const char *path);

#ifdef PROJECT_EXEC

void FILE_init(void);
void FILE_remove_temp_file(void);
void FILE_exit(void);

bool FILE_exist_real(const char *path);

void FILE_stat(const char *path, FILE_STAT *info, bool follow);
void FILE_dir_first(const char *path, const char *pattern, int attr);
bool FILE_dir_next(char **path, int *len);

void FILE_unlink(const char *path);
void FILE_rmdir(const char *path);
void FILE_mkdir(const char *path);
void FILE_rename(const char *src, const char *dst);
void FILE_copy(const char *src, const char *dst);

bool FILE_access(const char *path, int mode);
void FILE_link(const char *src, const char *dst);

char *FILE_make_temp(int *len, char *pattern);

void FILE_recursive_dir(const char *dir, void (*found)(const char *), void (*afterfound)(const char *), int attr);

void FILE_make_path_dir(const char *path);

int64_t FILE_free(const char *path);

#else

time_t FILE_get_time(const char *path);

#endif

#define FILE_is_absolute(_path) (*(_path) == '/')
#define FILE_is_relative(_path) (*(_path) != '/')

#endif

#endif
