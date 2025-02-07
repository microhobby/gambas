/***************************************************************************

	gbx_debug.c

	(c) 2000-2017 Benoît Minisini <benoit.minisini@gambas-basic.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
	MA 02110-1301, USA.

***************************************************************************/

#define __GBX_DEBUG_C

#include "gb_common.h"
#include "gb_common_buffer.h"
#include "gb_common_case.h"
#include <stdarg.h>

#include "gb_buffer.h"
#include "gb_array.h"
#include "gb_error.h"
#include "gbx_exec.h"
#include "gbx_api.h"
#include "gbx_class.h"
#include "gbx_c_array.h"
#include "gbx_project.h"
#include "gbx_stack.h"
#include "gbx_subr.h"
#include "gbx_jit.h"

#include "gbx_debug.h"

DEBUG_INTERFACE DEBUG;
DEBUG_INFO *DEBUG_info = NULL;
int DEBUG_inside_eval = 0;

static bool calc_line_from_position(CLASS *class, FUNCTION *func, PCODE *addr, ushort *line)
{
	int i;
	ushort pos = addr - JIT_get_code(func);
	ushort *post;

	if (func->debug)
	{
		post =  func->debug->pos;
		for (i = 0; i < (func->debug->nline - 1); i++)
		{
			if (pos >= post[i] && pos < post[i + 1])
			{
				*line = i + func->debug->line;
				return FALSE;
			}
		}

		/*printf("pos = %d addr=%p func->code=%p\n", pos, addr, func->code);*/
	}

	return TRUE;
}

const char *DEBUG_get_position(CLASS *cp, FUNCTION *fp, PCODE *pc)
{
#if DEBUG_MEMORY
	static char buffer[256];
	const int buffer_size = sizeof(buffer);
#else
	char *buffer = COMMON_buffer;
	const int buffer_size = COMMON_BUF_MAX;
#endif

	ushort line = 0;

	if (!cp || !pc)
		return "?";

	if (fp != NULL && fp->debug)
		calc_line_from_position(cp, fp, pc, &line);

	if (cp->component)
	{
		snprintf(buffer, buffer_size, "[%s].%s.%s.%d",
			cp->component->name, cp->name,
			(fp && fp->debug) ? fp->debug->name : "?",
			line);
	}
	else
	{
		snprintf(buffer, buffer_size, "%s.%s.%d",
			cp->name,
			(fp && fp->debug) ? fp->debug->name : "?",
			line);
	}

	return buffer;
}


const char *DEBUG_get_current_position(void)
{
	return DEBUG_get_position(CP, FP, PC);
}


void DEBUG_init(void)
{
	const char *dir;
	const char *fifo_name;
	int pid;
	int fd_lock;
	int n;
	
	if (!EXEC_debug)
	{
		sprintf(COMMON_buffer, DEBUG_WAIT_LINK, PROJECT_name);

		dir = FILE_readlink(COMMON_buffer);
		if (!dir)
			return;
		
		for (n = DEBUG_WAIT_IGNORE_MAX; n >= 1; n--)
		{
			sprintf(COMMON_buffer, DEBUG_WAIT_IGNORE, PROJECT_name, n);
			if (unlink(COMMON_buffer) == 0)
				return;
		}
		
		sprintf(COMMON_buffer, DEBUG_WAIT_LINK, PROJECT_name);
		
		if (unlink(COMMON_buffer))
			return;
		
		pid = atoi(FILE_get_name(dir));
		if (!pid)
			return;
		
		sprintf(COMMON_buffer, DEBUG_FIFO_PATTERN, getuid(), pid, "lock");
		fd_lock = open(COMMON_buffer, O_CREAT | O_WRONLY | O_CLOEXEC, 0666);
		if (fd_lock < 1)
			return;
		
		STREAM_lock_all_fd(fd_lock); // On program end, that file will be automatically closed, and the lock released.
		
		EXEC_debug = TRUE;
		EXEC_fifo = TRUE;
		
		sprintf(COMMON_buffer, DEBUG_FIFO_PATTERN, getuid(), pid, "");
		fifo_name = COMMON_buffer;
	}
	else
		fifo_name = EXEC_fifo_name;

	COMPONENT_load(COMPONENT_create("gb.debug"));
	LIBRARY_get_interface_by_name("gb.debug", DEBUG_INTERFACE_VERSION, &DEBUG);

	DEBUG_info = DEBUG.Init((GB_DEBUG_INTERFACE *)(void *)GAMBAS_DebugApi, EXEC_fifo, fifo_name);

	if (!DEBUG_info)
		ERROR_panic("Cannot initialize debug mode");

	if (EXEC_profile)
	{
		EXEC_profile_instr = TRUE;
		DEBUG.Profile.Init(EXEC_profile_path);
	}
}


void DEBUG_exit(void)
{
	if (!EXEC_debug || !DEBUG_is_init())
		return;

	DEBUG.Exit();
	if (EXEC_profile)
		DEBUG.Profile.Exit();
}

void DEBUG_enter_event_loop(void)
{
	if (EXEC_profile)
		DEBUG.Profile.Begin(NULL, NULL);
}

void DEBUG_leave_event_loop(void)
{
	if (EXEC_profile)
		DEBUG.Profile.End(NULL, NULL);
}

void DEBUG_where(void)
{
	//static bool breakpoint = FALSE;
	const char *where = DEBUG_get_current_position();
	/*if (!breakpoint && !strcmp(where, "FForm._new.97"))
	{
		breakpoint = TRUE;
		BREAKPOINT();
	}*/
	fprintf(stderr, "%s: ", where);
}


bool DEBUG_get_value(const char *sym, int len, GB_VARIANT *ret)
{
	int i;
	VALUE value;
	LOCAL_SYMBOL *lp;
	GLOBAL_SYMBOL *gp;
	CLASS_VAR *var;
	char *addr;
	CLASS *class;
	void *ref;

	if (DEBUG_info->fp)
	{
		int n_local = DEBUG_info->fp->n_param + DEBUG_info->fp->n_local;
		
		for (i = 0; i < DEBUG_info->fp->debug->n_local; i++)
		{
			lp = &DEBUG_info->fp->debug->local[i];
			if (len == lp->sym.len && strncasecmp(sym, lp->sym.name, len) == 0)
			{
				if (i >= n_local)
				{
					var = &DEBUG_info->cp->load->stat[lp->value];
					addr = (char *)DEBUG_info->cp->stat + var->pos;
					ref = DEBUG_info->cp;
					VALUE_class_read(DEBUG_info->cp, &value, addr, var->type, ref);
					goto __FOUND_NO_BORROW;
				}
				else if (lp->value >= 0)
					value = DEBUG_info->bp[lp->value];
				else
					value = DEBUG_info->pp[lp->value];
				goto __FOUND;
			}
		}
	}

	if (DEBUG_info->cp)
	{
		for (i = 0; i < DEBUG_info->cp->load->n_global; i++)
		{
			gp = &DEBUG_info->cp->load->global[i];
			if (len != gp->sym.len || strncasecmp(sym, gp->sym.name, len) != 0)
				continue;

			if (CTYPE_get_kind(gp->ctype) == TK_VARIABLE)
			{
				if (!CTYPE_is_static(gp->ctype))
				{
					if (!DEBUG_info->op)
						continue;
					var = &DEBUG_info->cp->load->dyn[gp->value];
					addr = (char *)DEBUG_info->op + var->pos;
					ref = DEBUG_info->op;
				}
				else
				{
					var = &DEBUG_info->cp->load->stat[gp->value];
					addr = (char *)DEBUG_info->cp->stat + var->pos;
					ref = DEBUG_info->cp;
				}

				VALUE_class_read(DEBUG_info->cp, &value, addr, var->type, ref);
				goto __FOUND_NO_BORROW;
			}
			else if (CTYPE_get_kind(gp->ctype) == TK_CONST)
			{
				VALUE_class_constant(DEBUG_info->cp, &value, gp->value);
				goto __FOUND;
			}
		}
	}

	//class = CLASS_look_global(sym, len);
	class = CLASS_look(sym, len);
	if (class)
	{
		if (class->auto_create && class->instance)
		{
			value._object.class = class;
			value._object.object = class->instance;
			value._object.super = NULL;
		}
		else
		{
			value.type = T_CLASS;
			value._class.class = class;
			value._class.super = NULL;
		}
		goto __FOUND;
	}

	return TRUE;

__FOUND:

	BORROW(&value);

__FOUND_NO_BORROW:

	VALUE_conv_variant(&value);
	UNBORROW(&value);

	*((VALUE *)ret) = value;
	return FALSE;
}

int DEBUG_set_value(const char *sym, int len, VALUE *value)
{
	int i;
	LOCAL_SYMBOL *lp;
	GLOBAL_SYMBOL *gp;
	CLASS_VAR *var;
	char *addr;
	VALUE *where;
	bool ret = GB_DEBUG_SET_OK;

	TRY
	{
		if (DEBUG_info->fp)
		{
			for (i = 0; i < DEBUG_info->fp->debug->n_local; i++)
			{
				lp = &DEBUG_info->fp->debug->local[i];
				if (len == lp->sym.len && strncasecmp(sym, lp->sym.name, len) == 0)
				{
					if (lp->value >= 0)
						where = &DEBUG_info->bp[lp->value];
					else
						where = &DEBUG_info->pp[lp->value];
					VALUE_conv(value, where->type);
					RELEASE(where);
					*where = *value;
					BORROW(where);
					goto __FOUND;
				}
			}
		}

		if (DEBUG_info->cp)
		{
			for (i = 0; i < DEBUG_info->cp->load->n_global; i++)
			{
				gp = &DEBUG_info->cp->load->global[i];
				if (len != gp->sym.len || strncasecmp(sym, gp->sym.name, len) != 0)
					continue;

				if (CTYPE_get_kind(gp->ctype) == TK_VARIABLE)
				{
					if (!CTYPE_is_static(gp->ctype) && DEBUG_info->op)
					{
						var = &DEBUG_info->cp->load->dyn[gp->value];
						addr = (char *)DEBUG_info->op + var->pos;
					}
					else
					{
						var = &DEBUG_info->cp->load->stat[gp->value];
						addr = (char *)DEBUG_info->cp->stat + var->pos;
					}

					VALUE_class_write(DEBUG_info->cp, value, addr, var->type);
					goto __FOUND;
				}
			}
		}

		ret = GB_DEBUG_SET_READ_ONLY;

__FOUND:

		0;
	}
	CATCH
	{
		ret = GB_DEBUG_SET_ERROR;
	}
	END_TRY

	if (ret == GB_DEBUG_SET_ERROR)
		EXEC_set_native_error(TRUE);

	return ret;
}

int DEBUG_get_object_access_type(void *object, CLASS *class, int *count)
{
	CLASS_DESC *desc;
	CLASS_DESC_METHOD *dm;
	char type;
	int access = GB_DEBUG_ACCESS_NORMAL;

	//fprintf(stderr, "DEBUG_can_be_used_like_an_array: %p %s ?\n", object, class->name);

	if (!object)
		goto __NORMAL;

	if (class == CLASS_Class || OBJECT_is_class(object))
	{
		class = (CLASS *)object;
		object = NULL;
		CLASS_load(class);
	}

	dm = CLASS_get_special_desc(class, SPEC_GET);
	if (!dm)
	{
		//fprintf(stderr, "No _get method\n");
		goto __NORMAL;
	}

	if (dm->npmin != 1 || dm->npmax != 1)
	{
		//fprintf(stderr, "No _get(Arg AS Integer) method\n");
		goto __NORMAL;
	}

	if (*dm->signature == T_INTEGER)
		access = GB_DEBUG_ACCESS_ARRAY;
	else if (*dm->signature == T_STRING)
		access = GB_DEBUG_ACCESS_COLLECTION;
	else
		goto __NORMAL;

	desc = CLASS_get_symbol_desc(class, "Count");
	if (!desc)
		goto __NORMAL;

	type = CLASS_DESC_get_type(desc);

	// The two only possible cases:
	// A static read-only property, and object == NULL
	// or a dynamic read-only property, and object != NULL

	if (!((type == CD_PROPERTY_READ && object) || (type == CD_STATIC_PROPERTY_READ && !object)))
		goto __NORMAL;

	TRY
	{
		if (desc->property.native)
		{
			if (EXEC_call_native(desc->property.read, object, desc->property.type, 0))
				access = GB_DEBUG_ACCESS_NORMAL;
		}
		else
		{
			EXEC.class = desc->property.class;
			EXEC.object = object;
			EXEC.nparam = 0;
			EXEC.native = FALSE;
			EXEC.index = (int)(intptr_t)desc->property.read;
			//EXEC.func = &class->load->func[(int)desc->property.read];

			EXEC_function_keep();
			EXEC_move_ret_to_temp();
		}

		if (access != GB_DEBUG_ACCESS_NORMAL)
		{
			VALUE_conv_integer(&TEMP);
			*count = TEMP._integer.value;
		}
	}
	CATCH
	{
		access = GB_DEBUG_ACCESS_NORMAL;
	}
	END_TRY


	// For collection-like objects, check _next and Key property

	if (access == GB_DEBUG_ACCESS_COLLECTION)
	{
		dm = CLASS_get_special_desc(class, SPEC_NEXT);
		if (!dm)
			goto __NORMAL;

		desc = CLASS_get_symbol_desc(class, "Key");
		if (!desc)
			goto __NORMAL;

		type = CLASS_DESC_get_type(desc);
		if (type != CD_PROPERTY_READ && type != CD_PROPERTY && type != CD_STATIC_PROPERTY_READ && type != CD_STATIC_PROPERTY && type != CD_VARIABLE && type != CD_STATIC_VARIABLE)
			goto __NORMAL;
	}

	return access;

__NORMAL:
	return GB_DEBUG_ACCESS_NORMAL;
}

void DEBUG_print_backtrace(STACK_BACKTRACE *bt)
{
	int i, n;
	bool stop;
	STACK_BACKTRACE *end;
	//STACK_CONTEXT *sc = (STACK_CONTEXT *)(STACK_base + STACK_size) - err->bt_count;

	//fprintf(stderr, "0: %s\n", DEBUG_get_position(err->cp, err->fp, err->pc));
	for (i = 0, n = 0, stop = FALSE; !stop; i++)
	{
		//fprintf(stderr, "%d: %s\n", i, DEBUG_get_position(bt[i].cp, bt[i].fp, bt[i].pc));
		if (STACK_backtrace_is_end(&bt[i]))
		{
			stop = TRUE;
			end = &bt[i];
			STACK_backtrace_clear_end(end);
		}
		if (bt[i].pc)
		{
			n++;
			fprintf(stderr, "%s ", DEBUG_get_position(bt[i].cp, bt[i].fp, bt[i].pc));
		}
	}
	fputc('\n', stderr);

	STACK_backtrace_set_end(end);
}


void DEBUG_print_current_backtrace(void)
{
	STACK_BACKTRACE *bt = STACK_get_backtrace();
	if (bt)
	{
		DEBUG_print_backtrace(bt);
		STACK_free_backtrace(&bt);
	}
}


GB_ARRAY DEBUG_get_string_array_from_backtrace(STACK_BACKTRACE *bt)
{
	GB_ARRAY array;
	int i, n, size;
	STACK_BACKTRACE *end;

	for (i = 0, size = 0;; i++)
	{
		if (bt[i].pc)
			size++;
		if (STACK_backtrace_is_end(&bt[i]))
		{
			end = &bt[i];
			STACK_backtrace_clear_end(end);
			break;
		}
	}

	GB_ArrayNew(&array, GB_T_STRING, size);
	//*((char **)GB_ArrayGet(array, 0)) = STRING_new_zero(DEBUG_get_position(err->cp, err->fp, err->pc));

	for (i = 0, n = 0; n < size; i++)
	{
		if (!bt[i].pc)
			continue;
		*((char **)GB_ArrayGet(array, n)) = STRING_new_zero(DEBUG_get_position(bt[i].cp, bt[i].fp, bt[i].pc));
		n++;
	}

	STACK_backtrace_set_end(end);
	return array;
}

GB_CLASS DEBUG_find_class(const char *comp_name, const char *class_name)
{
	CLASS *class;
	COMPONENT *save_comp;
	CLASS *save_class;

	if (comp_name)
	{
		save_comp = COMPONENT_current;
		COMPONENT_current = COMPONENT_find(comp_name);
	}
	else
	{
		save_class = CP;
		CP = NULL;
	}

	/*// As the startup class is automatically exported, this is the only way for the debugger to find it.
	if (PROJECT_class && !strcmp(name, PROJECT_class->name))
		return (GB_CLASS)PROJECT_class;*/

	class = CLASS_find(class_name);

	if (comp_name)
		COMPONENT_current = save_comp;
	else
		CP = save_class;

	return (GB_CLASS)class;
}

void DEBUG_enum_keys(void *object, GB_DEBUG_ENUM_CB cb)
{
	CENUM *old;
	CENUM *cenum = NULL;
	CLASS *class;
	bool err = FALSE;
	char *str;
	int len;
	char *save_key;
	int save_lkey;
	GB_VALUE value;
	void *prop_ob;

	old = EXEC_enum;

	class = OBJECT_class(object);
	if (class == CLASS_Class)
	{
		class = (CLASS *)object;
		object = NULL;
	}
	
	prop_ob = object ? object : class;

	TRY
	{
		GB_GetProperty(prop_ob, "Key");
		SUBR_get_string_len(&TEMP, &save_key, &save_lkey);
		(*cb)(save_key, save_lkey);

		cenum = CENUM_create(prop_ob);

		EXEC_enum = cenum;
		EXEC_special(SPEC_FIRST, class, object, 0, TRUE);
		EXEC_enum = old;

		if (cenum->stop)
		{
			err = TRUE;
			goto __END;
		}

		for(;;)
		{
			EXEC_enum = cenum;
			err = EXEC_special(SPEC_NEXT, class, object, 0, TRUE);
			EXEC_enum = old;

			if (err || cenum->stop)
			{
				err = TRUE;
				break;
			}

			GB_GetProperty(prop_ob, "Key");

			SUBR_get_string_len(&TEMP, &str, &len);
			(*cb)(str, len);
		}

	__END:

		value.type = GB_T_CSTRING;
		value._string.value.addr = save_key;
		value._string.value.start = 0;
		value._string.value.len = save_lkey;
		GB_SetProperty(prop_ob, "Key", &value);

		if (err)
			OBJECT_UNREF(cenum);
	}
	CATCH
	{
		OBJECT_UNREF(cenum);
	}
	END_TRY
}

void DEBUG_enter_eval()
{
	DEBUG_inside_eval++;
}

void DEBUG_leave_eval()
{
	DEBUG_inside_eval--;
}
