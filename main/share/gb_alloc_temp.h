/***************************************************************************

  alloc_temp.h

  Memory management routines template

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

#define __GB_ALLOC_C

#include <config.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <stdlib.h>

#include "gb_common.h"
#include "gb_error.h"
#include "gb_alloc.h"

PUBLIC int MEMORY_count = 0;

#if DEBUG_MEMORY

#include "gb_error.h"

typedef
  struct ALLOC {
    int _void;
    struct ALLOC *next;
    struct ALLOC *prev;
    int id;
    size_t size;
    }
  PACKED
  ALLOC;

PUBLIC int MEMORY_size = 0;

static int _id = 0;
ALLOC *_alloc = NULL;
extern void DEBUG_where(void);

#elif OPTIMIZE_MEMORY

int THROW_MEMORY()
{
	THROW(E_MEMORY);
}

#endif

PUBLIC void MEMORY_init(void)
{
  /*mcheck(NULL);*/
/*#if DEBUG_MEMORY
# ifdef __GNU_LIBRARY__
  mtrace();
  if (getenv("LD_PRELOAD"))
    unsetenv("MALLOC_TRACE");
  mcheck(NULL);
# endif
#endif*/
}

PUBLIC void MEMORY_exit(void)
{
#if DEBUG_MEMORY
  while (_alloc)
  {
    fprintf(stderr, "<%d>\n", _alloc->id);
    _alloc = _alloc->next;
  }
#endif
}

#if OPTIMIZE_MEMORY
#else

#if DEBUG_MEMORY
PUBLIC void MEMORY_alloc(void *p_ptr, size_t size, const char *src)
{
  ALLOC *alloc;

  alloc = (ALLOC *)malloc(sizeof(ALLOC) + size);
  if (!alloc)
    THROW(E_MEMORY);

  _id++;
  alloc->id = _id;
  alloc->prev = NULL;
  alloc->next = _alloc;
  alloc->size = size;

  if (_alloc)
    _alloc->prev = alloc;
      
  _alloc = alloc;
    
  *((void **)p_ptr) = (char *)alloc + sizeof(ALLOC);
  MEMORY_count++;
  MEMORY_size += size;

  #ifndef DO_NOT_PRINT_MEMORY
  DEBUG_where();
  fprintf(stderr, "<%d> %s: MEMORY_alloc(%d) -> %p\n", _id, src, (int)size, (char *)alloc + sizeof(ALLOC));
  fflush(stdout);
  #endif
}
#else
PUBLIC void MEMORY_alloc(void *p_ptr, size_t size)
{
  void *alloc;

  alloc = malloc(size);

  if (!alloc)
    THROW(E_MEMORY);

  *((void **)p_ptr) = alloc;
  MEMORY_count++;
}
#endif


#if DEBUG_MEMORY
PUBLIC void MEMORY_alloc_zero(void *p_ptr, size_t size, const char *src)
{
  MEMORY_alloc(p_ptr, size, src);
  memset(*((void **)p_ptr), 0, size);
}
#else
PUBLIC void MEMORY_alloc_zero(void *p_ptr, size_t size)
{
  void *alloc;

  alloc = calloc(size, 1);

  if (!alloc)
    THROW(E_MEMORY);

  *((void **)p_ptr) = alloc;
  MEMORY_count++;
}
#endif


#if DEBUG_MEMORY
PUBLIC void MEMORY_realloc(void *p_ptr, size_t size, const char *src)
{
  ALLOC *alloc = (ALLOC *)(*((char **)p_ptr) - sizeof(ALLOC));
  ALLOC *old = alloc;
  
  alloc = realloc(alloc, sizeof(ALLOC) + size);
  
  if (!alloc)
    THROW(E_MEMORY);

  MEMORY_size += size - alloc->size;
  
  if (_alloc == old)
    _alloc = alloc;
  
  if (alloc->prev)
    alloc->prev->next = alloc;
  if (alloc->next)
    alloc->next->prev = alloc;
  
  #ifndef DO_NOT_PRINT_MEMORY
  DEBUG_where();
  fprintf(stderr, "<%d> %s: MEMORY_realloc(%p, %d) -> %p\n", alloc->id, src, *((void **)p_ptr), (int)size, (char *)alloc + sizeof(ALLOC));
  fflush(stdout);
  #endif
  
  *((void **)p_ptr) = (char *)alloc + sizeof(ALLOC);
}
#else
PUBLIC void MEMORY_realloc(void *p_ptr, size_t size)
{
  void *alloc = *((void **)p_ptr);

  alloc = realloc(alloc, size);

  if (!alloc)
    THROW(E_MEMORY);

  *((void **)p_ptr) = alloc;
}
#endif


#if DEBUG_MEMORY
PUBLIC void MEMORY_free(void *p_ptr, const char *src)
{
  ALLOC *alloc = (ALLOC *)(*((char **)p_ptr) - sizeof(ALLOC));

	if (!(*((void **)p_ptr)))
		return;

  if (alloc->prev)
    alloc->prev->next = alloc->next;
  if (alloc->next)
    alloc->next->prev = alloc->prev;
    
  if (alloc == _alloc)
    _alloc = alloc->next;
  
  #ifndef DO_NOT_PRINT_MEMORY
  DEBUG_where();
  fprintf(stderr, "<%d> %s: MEMORY_free(%p)\n", alloc->id, src, (char *)alloc + sizeof(ALLOC));
  fflush(stdout);
  #endif

  MEMORY_size -= alloc->size;
    
  //*((long *)alloc) = 0x31415926;
  free(alloc);

  *((void **)p_ptr) = NULL;
  MEMORY_count--;
}
#else
PUBLIC void MEMORY_free(void *p_ptr)
{
  void *alloc = *((void **)p_ptr);

	if (!alloc)
		return;
		
  free(alloc);

  *((void **)p_ptr) = NULL;
  MEMORY_count--;
}
#endif

#endif // OPTIMIZE_MEMORY
