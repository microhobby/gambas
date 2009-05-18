/***************************************************************************

  stack.h

  Stack management

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

#ifndef __GBX_STACK_H
#define __GBX_STACK_H

#include "gbx_value.h"
#include "gb_pcode.h"
#include "gbx_debug.h"

typedef
  struct _stack_context {
  	struct _stack_context *next;
    VALUE *bp;        /* local variables */
    VALUE *pp;        /* local parameters */
    CLASS *cp;        /* current class */
    char *op;         /* current object */
    VALUE *ep;        /* error pointer */
    FUNCTION *fp;     /* current function */
    PCODE *pc;        /* instruction */
    PCODE *ec;        /* instruction if error */
    PCODE *et;        /* TRY save */
    PCODE *tc;        /* Last break in the function */
    VALUE *tp;        /* Stack at the last break in the function */
    }
  PACKED STACK_CONTEXT;

#ifndef __GBX_STACK_C

EXTERN char *STACK_base;
EXTERN size_t STACK_size;
EXTERN char *STACK_limit;

EXTERN int STACK_frame_count;
EXTERN STACK_CONTEXT *STACK_frame;

#endif

void STACK_init(void);
void STACK_exit(void);

#if DEBUG_STACK
void STACK_check(int need);
#else
#define STACK_check(_need) if ((char *)(SP + (_need) + 8) >= STACK_limit) THROW(E_STACK);

#endif

void STACK_push_frame(STACK_CONTEXT *context, int check);
void STACK_pop_frame(STACK_CONTEXT *context);
bool STACK_has_error_handler(void);
STACK_CONTEXT *STACK_get_frame(int frame);

#define STACK_get_previous_pc() ((STACK_frame_count <= 0) ? NULL : STACK_frame->pc)

#define STACK_get_current() ((STACK_frame_count > 0) ? STACK_frame : NULL)

#define STACK_copy(_dst, _src) \
  (_dst)->next = (_src)->next; \
	(_dst)->bp = (_src)->bp; \
	(_dst)->pp = (_src)->pp; \
	(_dst)->cp = (_src)->cp; \
	(_dst)->op = (_src)->op; \
	(_dst)->ep = (_src)->ep; \
	(_dst)->fp = (_src)->fp; \
	(_dst)->pc = (_src)->pc; \
	(_dst)->ec = (_src)->ec; \
	(_dst)->et = (_src)->et; \
	(_dst)->tc = (_src)->tc; \
	(_dst)->tp = (_src)->tp;

#endif
