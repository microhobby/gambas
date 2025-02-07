/***************************************************************************

  gbx_jit.c

  (c) 2018 Benoît Minisini <benoit.minisini@gambas-basic.org>

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

#define __GBX_JIT_C

#include "gb_common.h"
#include "gb_common_buffer.h"
#include "gb_common_case.h"
#include "gbx_component.h"
#include "gbx_exec.h"
#include "gbx_object.h"
#include "gbx_api.h"
#include "gbx_jit.h"

typedef
	struct {
		JIT_FUNC addr;
		PCODE *code;
	}
	JIT_FUNCTION;

bool JIT_disabled = FALSE;

static bool _component_loaded = FALSE;
static GB_FUNCTION _jit_compile_func;
static GB_FUNCTION _jit_wait_func;

static char _jit_state = JIT_NOT_COMPILED;
static void *_jit_library = NULL;

static JIT_FUNCTION *_jit_func = NULL;

static bool _debug = FALSE;

static void find_method(GB_FUNCTION *func, const char *method, const char *sign, const char *type)
{
	if (GB_GetFunction(func, CLASS_find_global("Jit"), method, sign, type))
		ERROR_panic("Unable to find JIT.&1() method", method);
}

void JIT_init(void)
{
	char *var = getenv("GB_NO_JIT");
	if (var && var[0] && !(var[0] == '0' && var[1] == 0))
		JIT_disabled = TRUE;
}

void JIT_abort(void)
{
	GB_FUNCTION func;
	
	if (!_component_loaded || JIT_disabled)
		return;
	
	find_method(&func, "_Abort", NULL, NULL);
	GB_Call(&func, 0, FALSE);
	GB_Wait(-1);
}

void JIT_exit(void)
{
	ARRAY_delete(&_jit_func);
}

static int get_state(ARCHIVE *arch)
{
	return arch ? arch->jit_state : _jit_state;
}

bool JIT_can_compile(ARCHIVE *arch)
{
	return get_state(arch) == JIT_NOT_COMPILED;
}

void JIT_compile(ARCHIVE *arch)
{
	COMPONENT *current;
	
	if (JIT_disabled)
		return;
	
	if (arch ? arch->jit_library : _jit_library)
		return;
	
	if (!_component_loaded)
	{
		char *var;
		GB_FUNCTION init_func;
		GB_VALUE *ret;
		
		_component_loaded = TRUE;
		
		var = getenv("GB_JIT_DEBUG");
		if (var && var[0] && !(var[0] == '0' && var[1] == 0))
			_debug = TRUE;
		
		if (_debug)
			fprintf(stderr, "gbx3: loading gb.jit component\n");
		
		COMPONENT_load(COMPONENT_create("gb.jit"));

		find_method(&init_func, "_Search", NULL, NULL);
		
		ret = GB_Call(&init_func, 0, FALSE);
		if (ret->_boolean.value)
		{
			JIT_disabled = TRUE;
			fprintf(stderr, "gbx3: no compiler found. JIT is disabled.\n");
			return;
		}
		
		find_method(&_jit_compile_func, "_Compile", "s", "b");
		find_method(&_jit_wait_func, "_Wait", "s", "s");
	}
	
	arch ? (arch->jit_state = JIT_COMPILING) : (_jit_state = JIT_COMPILING);
	
	current = COMPONENT_current;
	COMPONENT_current = NULL;
	
	GB_Push(1, T_STRING, arch ? arch->name : "", -1);
	GB_Call(&_jit_compile_func, 1, FALSE);
	
	COMPONENT_current = current;
}

bool wait_for_compilation(ARCHIVE *arch)
{
	COMPONENT *current;
	void *lib;
	void **iface;
	GB_VALUE *ret;
	char *path;

	if (JIT_disabled)
		return TRUE;
	
	if (arch ? arch->jit_library : _jit_library)
		return FALSE;
	
	current = COMPONENT_current;
	COMPONENT_current = NULL;
	
	GB_Push(1, T_STRING, arch ? arch->name : "", -1);
	ret = GB_Call(&_jit_wait_func, 1, FALSE);
	
	COMPONENT_current = current;
	
	arch ? (arch->jit_state = JIT_COMPILED) : (_jit_state = JIT_COMPILED);
	
	path = GB_ToZeroString((GB_STRING *)ret);
	if (!*path)
		ERROR_panic("Unable to compile JIT source file");

	//fprintf(stderr, "gbx3: shared jit library is: %s\n", path);

	lib = dlopen(path, RTLD_NOW);
	if (!lib)
		ERROR_panic("Unable to load JIT library: %s", dlerror());
	
	if (arch)
		arch->jit_library = lib;
	else
		_jit_library = lib;
	
  iface = dlsym(lib, "GB_PTR");
	if (iface) *((void **)iface) = &GAMBAS_Api;
	
  iface = dlsym(lib, "JIT_PTR");
	if (iface) *((void **)iface) = &GAMBAS_JitApi;
	
	return FALSE;
}

static bool create_function(CLASS *class, int index)
{
	ARCHIVE *arch;
	FUNCTION *func;
	JIT_FUNCTION *jit;
	void *lib;
	void *addr;
	int i;
	int len;
	char *name;
	int jit_index;
	char c;
	
	arch = class->component ? class->component->archive : NULL;
	
	if (!class->loaded)
		return TRUE;
	
	/*if (get_state(arch) == JIT_NOT_COMPILED) // don't use JIT if we are compiling the corresponding archive
		return TRUE;*/
	
	if (wait_for_compilation(arch))
	{
		for (i = 0; i < class->load->n_func; i++)
			class->load->func[i].fast = FALSE;
		return TRUE;
	}
	
	func = &class->load->func[index];
	
	if (!arch)
		lib = _jit_library;
	else
		lib = arch->jit_library;
	
	name = class->name;
	while (*name == '^')
		name++;
	
	len = sprintf(COMMON_buffer, "jit_%s_%d", name, index);
	
	for (i = 0; i < len; i++)
	{
		c = tolower(COMMON_buffer[i]);
		if (c == ':')
			c = '$';
		COMMON_buffer[i] = c;
	}
	
	addr = dlsym(lib, COMMON_buffer);
	if (!addr)
	{
		func->fast = FALSE;
		return TRUE;
	}
	
	if (_debug && func->debug)
		fprintf(stderr, "gbx3: loading jit function: %s.%s\n", class->name, func->debug->name);

	if (!_jit_func)
		ARRAY_create(&_jit_func);

	jit_index = ARRAY_count(_jit_func);
	jit = (JIT_FUNCTION *)ARRAY_add(&_jit_func);
	
	jit->addr = addr;
	jit->code = func->code;
	
	func->code = (PCODE *)(intptr_t)jit_index;
	func->fast_linked = TRUE;

	return FALSE;
}


bool JIT_exec(bool ret_on_stack)
{
	VALUE *sp = SP;
	JIT_FUNCTION *jit;
	CLASS *class = EXEC.class;
	void *object = EXEC.object;
	char nparam = EXEC.nparam;
	VALUE ret;
	FUNCTION *func = EXEC.func;
	
	if (JIT_disabled)
		return TRUE;
	
	if (nparam < func->npmin)
		THROW(E_NEPARAM);
	else if (nparam > func->n_param && !func->vararg)
		THROW(E_TMPARAM);

	if (!func->fast_linked)
	{
		if (create_function(class, EXEC.index))
			return TRUE;
	}
	
	STACK_push_frame(&EXEC_current, func->stack_usage);
	
	CP = class;
	EXEC_check_bytecode();
	OP = object;
	FP = func;
	EC = NULL;
	
	jit = &_jit_func[(intptr_t)func->code];
	
	PROFILE_ENTER_FUNCTION();
	
	TRY
	{
		(*(jit->addr))(nparam);
	}
	CATCH
	{
		PROFILE_LEAVE_FUNCTION();
		if (SP != sp)
			ERROR_panic("Stack mismatch in JIT function (SP %+ld)\n", SP - sp);
		RELEASE_MANY(SP, nparam);
		STACK_pop_frame(&EXEC_current);
		PROPAGATE();
	}
	END_TRY
	
	PROFILE_LEAVE_FUNCTION();
	
	if (SP != sp)
		ERROR_panic("Stack mismatch in JIT function (SP %+ld)\n", SP - sp);
	
	if (func->type != T_VOID)
	{
		ret = TEMP;
		BORROW(&ret);
	}
	else
		ret.type = T_VOID;
	
	RELEASE_MANY(SP, nparam);
	
	STACK_pop_frame(&EXEC_current);
	
	if (ret_on_stack)
	{
		if (SP[-1].type == T_FUNCTION)
		{
			SP--;
			OBJECT_UNREF(SP->_function.object);
		}

		*SP++ = ret;
		ret.type = T_VOID;
	}
	else
		RET = ret;
	
	return FALSE;
}

PCODE *JIT_get_code(FUNCTION *func)
{
	if (func->fast_linked)
		return _jit_func[(intptr_t)func->code].code;
	else
		return func->code;
}

void *JIT_get_class_ref(int index)
{
	return CP->load->class_ref[index];
}

CLASS_CONST *JIT_get_constant(int index)
{
	return &CP->load->cst[index];
}

void JIT_debug(const char *fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

typedef
	struct {
		PCODE *pc;
		VALUE **psp;
		PCODE code;
	}
	JIT_call_unknown_ERROR;

static void error_JIT_call_unknown(JIT_call_unknown_ERROR *save)
{
	save->pc[1] = (PCODE)save->code;
	*save->psp = SP;
}

void JIT_call_unknown(PCODE *pc, VALUE **psp)
{
	JIT_call_unknown_ERROR save;
	
	PC = pc;
	SP = *psp;
	
	save.pc = pc;
	save.psp = psp;
	save.code = pc[1];
	
	pc[1] = 0x140B;
	
	ON_ERROR_1(error_JIT_call_unknown, &save)
	{
		EXEC_function_loop();
	}
	END_ERROR

	error_JIT_call_unknown(&save);
}

void JIT_load_class(CLASS *class)
{
	CLASS_load(class);
}

void JIT_load_class_without_init(CLASS *class)
{
	TRY
	{
		CLASS_load_without_init(class);
	}
	CATCH
	{
		class->error = FALSE;
	}
	END_TRY
}

void JIT_add_string_local(GB_STRING *str, GB_STRING val)
{
	int len;
	char *add;
	int len_add;
	
	add = val.value.addr + val.value.start;
	len_add = val.value.len;
  len = str->value.len;
	
	if (len_add == 0)
		return;

	if (len && STRING_from_ptr(str->value.addr)->ref == 1 && str->value.start == 0)
	{
		str->value.addr = STRING_add(str->value.addr, add, len_add);
	}
	else
	{
		char *str_new = STRING_new(NULL, len + len_add);
		if (len) memcpy(str_new, str->value.addr + str->value.start, len);
		memcpy(&str_new[len], add, len_add);
		if (str->type == T_STRING) 
			STRING_unref(&str->value.addr);
		else
			str->type = T_STRING;
		str->value.addr = str_new;
	}

	str->value.len += len_add;

	if (val.type == T_STRING) STRING_unref(&val.value.addr);
}

void JIT_add_string_global(char **pstr, GB_STRING val)
{
	char *str;
	int len;
	char *add;
	int len_add;
	
	add = val.value.addr + val.value.start;
	len_add = val.value.len;
	
	if (len_add == 0)
		return;

	str = *pstr;
	len = STRING_length(str);
	
	if (len && STRING_from_ptr(str)->ref == 1)
		*pstr = STRING_add(str, add, len_add);
	else
	{
		char *str_new = STRING_new(NULL, len + len_add);
		if (len) memcpy(str_new, str, len);
		memcpy(&str_new[len], add, len_add);
		STRING_unref(pstr);
		*pstr = str_new;
	}
	
	if (val.type == T_STRING) STRING_unref(&val.value.addr);
}
