/***************************************************************************

  gbc_class.c

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

#define __GBC_CLASS_C

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gb_common.h"
#include "gb_error.h"
#include "gb_str.h"
#include "gb_file.h"

#include "gb_table.h"
#include "gbc_compile.h"
#include "gb_code.h"

static int _array_class[17];

void CLASS_create(CLASS **result)
{
	CLASS *class;
	TRANS_FUNC func;

	ALLOC_ZERO(&class, sizeof(CLASS));

	ARRAY_create_inc(&class->function, 256);
	ARRAY_create(&class->event);
	ARRAY_create(&class->prop);
	ARRAY_create(&class->ext_func);
	ARRAY_create_inc(&class->constant, 256);
	ARRAY_create(&class->class);
	ARRAY_create_inc(&class->unknown, 256);
	ARRAY_create_inc(&class->stat, 256);
	ARRAY_create_inc(&class->dyn, 256);
	ARRAY_create(&class->array);
	ARRAY_create(&class->structure);
	ARRAY_create(&class->names);

	TABLE_create(&class->table, sizeof(CLASS_SYMBOL), TF_IGNORE_CASE);
	TABLE_create(&class->string, sizeof(SYMBOL), TF_NORMAL);

	CLEAR(&func);
	TYPE_clear(&func.type);

	func.index = CLASS_add_symbol(class, "@init");
	CLASS_add_function(class, &func);

	func.index = CLASS_add_symbol(class, "@new");
	CLASS_add_function(class, &func);

	class->name = STR_copy(FILE_set_ext(FILE_get_name(JOB->name), NULL));
	class->parent = NO_SYMBOL;

	memset(_array_class, 0, sizeof(_array_class));

	*result = class;
}


static void delete_function(FUNCTION *func)
{
	ARRAY_delete(&func->local);
	ARRAY_delete(&func->stat);
	if (func->code)
		FREE(&func->code);

	if (JOB->debug)
		ARRAY_delete(&func->pos_line);

	if (func->param)
		FREE(&func->param);
}


static void delete_event(EVENT *event)
{
	if (event->param)
		FREE(&event->param);
}


static void delete_extfunc(EXTFUNC *extfunc)
{
	if (extfunc->param)
		FREE(&extfunc->param);
}


static void delete_structure(CLASS_STRUCT *structure)
{
	if (structure->field)
		ARRAY_delete(&structure->field);
}


void CLASS_delete(CLASS **class)
{
	int i;

	if (!*class)
		return;
	
	CLASS_set_current_function(NULL);
	
	TABLE_delete(&((*class)->table));
	TABLE_delete(&((*class)->string));

	for (i = 0; i < ARRAY_count((*class)->function); i++)
		delete_function(&((*class)->function[i]));

	for (i = 0; i < ARRAY_count((*class)->event); i++)
		delete_event(&((*class)->event[i]));

	for (i = 0; i < ARRAY_count((*class)->ext_func); i++)
		delete_extfunc(&((*class)->ext_func[i]));

	for (i = 0; i < ARRAY_count((*class)->structure); i++)
		delete_structure(&((*class)->structure[i]));

	for (i = 0; i < ARRAY_count((*class)->names); i++)
		FREE(&((*class)->names[i]));

	ARRAY_delete(&((*class)->function));
	ARRAY_delete(&((*class)->event));
	ARRAY_delete(&((*class)->prop));
	ARRAY_delete(&((*class)->ext_func));
	ARRAY_delete(&((*class)->constant));
	ARRAY_delete(&((*class)->class));
	ARRAY_delete(&((*class)->unknown));
	ARRAY_delete(&((*class)->stat));
	ARRAY_delete(&((*class)->dyn));
	ARRAY_delete(&((*class)->array));
	ARRAY_delete(&((*class)->structure));
	ARRAY_delete(&((*class)->names));

	if ((*class)->name) STR_free((*class)->name);
	if ((*class)->export_name) STR_free((*class)->export_name);

	FREE(class);
}


CLASS_SYMBOL *CLASS_declare(CLASS *class, int index, int type, bool global)
{
	const char *name;
	CLASS_SYMBOL *sym;

	sym = CLASS_get_symbol(class, index);

	if ((global && !TYPE_is_null(sym->global.type))
			|| (!global && !TYPE_is_null(sym->local.type))) // || !TYPE_is_null(sym->global.type))))
	{
		char name[sym->symbol.len + 1];
		memcpy(name, sym->symbol.name, sym->symbol.len);
		name[sym->symbol.len] = 0;
		THROW("'&1' already declared", name);
	}
	
	if (type == TK_VARIABLE && sym->class && JOB->class->class[sym->class - 1].has_static)
	{
		name = SYMBOL_get_name(&sym->symbol);
		if (global)
			COMPILE_print(MSG_WARNING, -1, "class name hidden by global declaration: &1", name);
		else
			COMPILE_print(MSG_WARNING, -1, "class name hidden by local declaration: &1", name);
	}

	if (!global && !TYPE_is_null(sym->global.type))
	{
		name = SYMBOL_get_name(&sym->symbol);

		switch (TYPE_get_kind(sym->global.type))
		{
			case TK_VARIABLE:
				COMPILE_print(MSG_WARNING, -1, "global variable hidden by local declaration: &1", name);
				break;
				
			case TK_FUNCTION:
				COMPILE_print(MSG_WARNING, -1, "function hidden by local declaration: &1", name);
				break;
			
			case TK_EXTERN:
				COMPILE_print(MSG_WARNING, -1, "extern function hidden by local declaration: &1", name);
				break;

			case TK_CONST:
				COMPILE_print(MSG_WARNING, -1, "constant hidden by local declaration: &1", name);
				break;
		}
	}

	if (global)
		sym->global.line = JOB->line;
	else
		sym->local.line = JOB->line;
	
	return sym;
}


void CLASS_check_unused_global(CLASS *class)
{
	CLASS_SYMBOL *sym;
	int i;
	TYPE type;
	
	for (i = 0; i < TABLE_count(class->table); i++)
	{
		sym = CLASS_get_symbol(class, i);
		type = sym->global.type;
		
		if (sym->global_no_warning)
			continue;
		
		if (sym->global_used && sym->global_assigned)
			continue;
		
		if (TYPE_is_null(type) || TYPE_is_public(type))
			continue;
		
		if (!sym->global_used)
		{
			if (TYPE_get_kind(type) == TK_VARIABLE)
				COMPILE_print(MSG_WARNING, sym->global.line, "unused global variable: &1", SYMBOL_get_name(&sym->symbol));
			else if (TYPE_get_kind(type) == TK_FUNCTION)
				COMPILE_print(MSG_WARNING, class->function[sym->global.value].line - 1, "unused function: &1", SYMBOL_get_name(&sym->symbol));
			else if (TYPE_get_kind(type) == TK_EXTERN)
				COMPILE_print(MSG_WARNING, sym->global.line, "unused extern function: &1", SYMBOL_get_name(&sym->symbol));
		}
		else
		{
			if (TYPE_get_kind(type) == TK_VARIABLE)
				COMPILE_print(MSG_WARNING, sym->global.line, "uninitialized global variable: &1", SYMBOL_get_name(&sym->symbol));
		}
	}
}


int CLASS_add_function(CLASS *class, TRANS_FUNC *decl)
{
	FUNCTION *func;
	int i;
	CLASS_SYMBOL *sym;
	PARAM *param;
	int count;

	count = ARRAY_count(class->function);
	if (count >= MAX_CLASS_FUNCTION)
		THROW("Too many functions");

	func = ARRAY_add_void(&class->function);
	TYPE_clear(&func->type);
	func->nparam = 0;
	func->name = NO_SYMBOL;

	ARRAY_create(&func->local);
	ARRAY_create(&func->stat);
	//ARRAY_create_inc(&func->code, 512);
	func->code = NULL;
	func->ncode = 0;
	func->ncode_max = 0;

	if (JOB->debug)
		ARRAY_create(&func->pos_line);

	if (decl->index != NO_SYMBOL)
	{
		sym = CLASS_declare(class, decl->index, TK_FUNCTION, TRUE);
		sym->global.type = decl->type;
		sym->global.value = count;
		sym->global_no_warning = decl->no_warning;
	}

	if (TYPE_is_static(decl->type))
		class->has_static = TRUE;

	func->nparam = decl->nparam;

	if (decl->nparam)
	{
		ALLOC(&func->param, decl->nparam * sizeof(PARAM));

		for (i = 0; i < decl->nparam; i++)
			func->param[i] = decl->param[i];
	}

	func->type = decl->type;
	func->start = decl->start;
	func->line = decl->line;
	func->name = decl->index;
	func->last_code = CODE_NO_POS;
	func->last_code2 = CODE_NO_POS;
	func->stack = 8; // Some stack may be needed for initialization functions
	func->finally = 0;
	func->catch = 0;
	func->npmin = -1;
	func->vararg = decl->vararg;
	func->fast = decl->fast;
	func->unsafe = decl->unsafe;
	
	// Function startup

	CODE_begin_function(func);
	JOB->func = func;

	// Byref check at function startup
	
	if (decl->byref)
		CODE_byref(decl->byref);

	// Optional parameters

	for (i = 0; i < func->nparam; i++)
	{
		param = &func->param[i];
		if (param->optional == NULL)
			continue;

		if (func->npmin < 0)
			func->npmin = i;

		TRANS_init_optional(param);
		CODE_pop_optional(i - func->nparam);
	}

	if (func->npmin < 0)
		func->npmin = func->nparam;
	
	return count;
}


void CLASS_add_event(CLASS *class, TRANS_EVENT *decl)
{
	EVENT *event;
	int i;
	CLASS_SYMBOL *sym;
	int count;
	
	count = ARRAY_count(class->event);
	if (count >= MAX_CLASS_EVENT)
		THROW("Too many events");

	event = ARRAY_add_void(&class->event);
	TYPE_clear(&event->type);
	event->nparam = 0;
	event->name = NO_SYMBOL;

	if (!decl) return;

	sym = CLASS_declare(class, decl->index, TK_EVENT, TRUE);
	/*CLASS_add_symbol(class, JOB->table, decl->index, &sym, NULL);*/

	sym->global.type = decl->type;
	sym->global.value = ARRAY_count(class->event) - 1;

	event->nparam = decl->nparam;

	if (event->nparam)
	{
		ALLOC_ZERO(&event->param, decl->nparam * sizeof(PARAM));

		for (i = 0; i < decl->nparam; i++)
		{
			event->param[i].type = decl->param[i].type;
			event->param[i].index = decl->param[i].index;
		}
	}

	event->type = decl->type;
	event->name = decl->index;
}


void CLASS_add_property(CLASS *class, TRANS_PROPERTY *decl)
{
	PROPERTY *prop;
	CLASS_SYMBOL *sym;
	int index;
	int i;

	index = ARRAY_count(class->prop);
	prop = ARRAY_add_void(&class->prop);
	TYPE_clear(&prop->type);
	prop->name = NO_SYMBOL;
	prop->read = decl->write_only ? 0 : -1;
	prop->write = decl->read_only ? 0 : -1;
	prop->synonymous = -1;

	sym = CLASS_declare(class, decl->index, TK_PROPERTY, TRUE);
	/*CLASS_add_symbol(class, JOB->table, decl->index, &sym, NULL);*/

	sym->global.type = decl->type;
	sym->global.value = ARRAY_count(class->prop) - 1;

	if (TYPE_is_static(decl->type))
		class->has_static = TRUE;

	prop->type = decl->type;
	prop->name = decl->index;
	prop->line = decl->line;
	prop->comment = decl->comment;
	prop->use = decl->use;
	
	for (i = 0; i < decl->nsynonymous; i++)
	{
		prop = ARRAY_add_void(&class->prop);
		TYPE_clear(&prop->type);
		prop->name = NO_SYMBOL;
		prop->read = decl->write_only ? 0 : -1;
		prop->write = decl->read_only ? 0 : -1;
		prop->synonymous = index;

		sym = CLASS_declare(class, decl->synonymous[i], TK_PROPERTY, TRUE);
		/*CLASS_add_symbol(class, JOB->table, decl->index, &sym, NULL);*/

		sym->global.type = decl->type;
		sym->global.value = ARRAY_count(class->prop) - 1;

		prop->type = decl->type;
		prop->name = decl->synonymous[i];
		prop->line = decl->line;
		prop->comment = decl->comment;
	}
	
}


void CLASS_add_extern(CLASS *class, TRANS_EXTERN *decl)
{
	EXTFUNC *extfunc;
	int i;
	CLASS_SYMBOL *sym;
	int count;
	
	count = ARRAY_count(class->ext_func);
	if (count >= MAX_CLASS_EXTERN)
		THROW("Too many external functions");

	extfunc = ARRAY_add_void(&class->ext_func);
	TYPE_clear(&extfunc->type);
	extfunc->nparam = 0;
	extfunc->vararg = FALSE;
	extfunc->name = NO_SYMBOL;

	if (!decl) return;

	sym = CLASS_declare(class, decl->index, TK_EXTERN, TRUE);
	/*CLASS_add_symbol(class, JOB->table, decl->index, &sym, NULL);*/

	sym->global.type = decl->type;
	sym->global.value = ARRAY_count(class->ext_func) - 1;

	extfunc->nparam = decl->nparam;
	extfunc->vararg = decl->vararg;

	if (extfunc->nparam)
	{
		ALLOC(&extfunc->param, decl->nparam * sizeof(PARAM));

		for (i = 0; i < decl->nparam; i++)
		{
			extfunc->param[i].type = decl->param[i].type;
			extfunc->param[i].index = decl->param[i].index;
		}
	}

	extfunc->type = decl->type;
	extfunc->name = decl->index;
	extfunc->library = decl->library;
	extfunc->alias = decl->alias;
}


int CLASS_add_constant(CLASS *class, TRANS_DECL *decl)
{
	CONSTANT *desc;
	int num;

	num =  ARRAY_count(class->constant);
	if (num >= MAX_CLASS_CONST)
		THROW("Too many constants");

	desc = ARRAY_add(&class->constant);
	desc->type = decl->type;
	desc->index = decl->index;

	desc->value = decl->value;
	if (TYPE_get_id(decl->type) == T_LONG)
		desc->lvalue = decl->lvalue;
	desc->is_integer = decl->is_integer;

	desc->line = JOB->line;

	return num;
}


static int add_class(CLASS *class, int index, bool used, bool exported)
{
	int num;
	CLASS_REF *desc;
	CLASS_SYMBOL *sym = CLASS_get_symbol(class, index);

	num = sym->class - 1;
	if (num < 0)
	{
		num =  ARRAY_count(class->class);
		if (num >= MAX_CLASS_CLASS)
			THROW("Too many different classes used");

		desc = ARRAY_add_void(&class->class);
		desc->index = index;

		sym->class = num + 1;

		if (COMP_verbose)
			fprintf(stderr, "Adding class %.*s %s%s\n", sym->symbol.len, sym->symbol.name, used ? "" : "Unused ", exported ? "Exported" : "");
		
		JOB->class->class[num].exported = exported;
	}

	if (used != JOB->class->class[num].used)
	{
		if (COMP_verbose)
			fprintf(stderr, "Switching class %.*s to %s\n", sym->symbol.len, sym->symbol.name, used ? "Used" : "Unused");
		
		JOB->class->class[num].used = used;
	}
	
	return num;
}

int CLASS_add_class(CLASS *class, int index)
{
	return add_class(class, index, TRUE, FALSE);
}

int CLASS_add_class_unused(CLASS *class, int index)
{
	return add_class(class, index, FALSE, FALSE);
}

int CLASS_add_class_exported(CLASS *class, int index)
{
	return add_class(class, index, TRUE, TRUE);
}

int CLASS_add_class_exported_unused(CLASS *class, int index)
{
	return add_class(class, index, FALSE, TRUE);
}

bool CLASS_exist_class(CLASS *class, int index)
{
	return CLASS_get_symbol(class, index)->class > 0;
}

static char *CLASS_add_name(CLASS *class, const char *name, int len)
{
	char *ret;
	 
	ALLOC(&ret, len + 1);
	memcpy(ret, name, len);
	ret[len] = 0;
	
	*((char **)ARRAY_add(&class->names)) = ret;
	return ret;
}

int CLASS_get_array_class(CLASS *class, int type, int value)
{
	static char *names[] = {
		NULL, "Boolean[]", "Byte[]", "Short[]", "Integer[]", "Long[]", "Single[]", "Float[]",
		"Date[]", "String[]", "String[]", "Pointer[]", "Variant[]", NULL, NULL, NULL, "Object[]"
		};

	int index;
	//CLASS_REF *cref;

	if (value < 0)
	{
		if (type <= T_VOID || type > T_OBJECT)
			ERROR_panic("Bad native array class");
	
		index = _array_class[type];
	
		if (index == 0)
		{
			if (!TABLE_find_symbol(class->table, names[type], strlen(names[type]), &index))
				index = CLASS_add_symbol(class, names[type]);
			index = CLASS_add_class_exported(class, index);
			_array_class[type] = index;
		}
	}
	else
	{
		CLASS_SYMBOL *sym = CLASS_get_symbol(class, class->class[value].index);
		int len = sym->symbol.len;
		char name[len + 2];
		
		memcpy(name, sym->symbol.name, len);
		memcpy(&name[len], "[]", 2);
		
		if (!TABLE_find_symbol(class->table, name, len + 2, &index))
		{
			char *name_alloc = CLASS_add_name(class, name, len + 2);
			index = TABLE_add_symbol(class->table, name_alloc, len + 2);
		}
		
		if (class->class[value].exported)
			index = CLASS_add_class_exported(JOB->class, index);
		else
			index = CLASS_add_class(JOB->class, index);
	}

	JOB->class->class[index].type = TYPE_make(type, value, 0);
	
	/*cref = &class->class[index];
	if (TYPE_is_null(cref->array))
	{
		cref->array.t.id = type;
		cref->array.t.value = value;
	}*/
	
	return index;
}


int CLASS_add_unknown(CLASS *class, int index)
{
	int num;
	int *desc;
	CLASS_SYMBOL *sym = CLASS_get_symbol(class, index);

	num = sym->unknown - 1;
	if (num < 0)
	{
		num =  ARRAY_count(class->unknown);
		if (num >= MAX_CLASS_UNKNOWN)
			THROW("Too many unknown symbols");

		desc = ARRAY_add(&class->unknown);
		*desc = index;

		sym->unknown = num + 1;
	}

	return num;
}

int CLASS_add_array(CLASS *class, TRANS_ARRAY *array)
{
	CLASS_ARRAY *desc;
	int num;
	int i;

	num =  ARRAY_count(class->array);
	if (num >= MAX_CLASS_ARRAY)
		THROW("Too many array declarations");

	desc = ARRAY_add(&class->array);
	desc->type = array->type;
	desc->ndim = array->ndim;
	for (i = 0; i < desc->ndim; i++)
		desc->dim[i] = array->dim[i];

	return num;
}


FUNCTION *CLASS_set_current_function(FUNCTION *func)
{
	JOB->func = func;
	return CODE_set_function(func);
}


void CLASS_add_static_declaration(CLASS *class, int index, TYPE type, CLASS_SYMBOL *sym, bool local)
{
	VARIABLE *var;
	int count = ARRAY_count(class->stat);
	
	if (count >= MAX_CLASS_SYMBOL)
		THROW("Too many static variables");
	
	if (local)
		sym->local.value = count;
	else
		sym->global.value = count;
	
	var = ARRAY_add(&class->stat);

	var->type = type;
	var->index = index;

	class->has_static = TRUE;
}


void CLASS_init_global_declaration(CLASS *class, TRANS_DECL *decl, CLASS_SYMBOL *sym, bool local)
{
	FUNCTION *prev_func;
	bool is_static;
	
	if (!TRANS_has_init_var(decl))
		return;
	
	is_static = TYPE_is_static(decl->type);
	prev_func = CLASS_set_current_init_function(class, is_static ? FUNC_INIT_STATIC : FUNC_INIT_DYNAMIC);

	FUNCTION_add_all_pos_line();
	TRANS_init_var(decl);
	if (local)
	{
		CODE_pop_global(sym->local.value, is_static);
		sym->local_assigned = TRUE;
	}
	else
	{
		CODE_pop_global(sym->global.value, is_static);
		sym->global_assigned = TRUE;
	}
	
	CLASS_set_current_function(prev_func);
}

void CLASS_add_declaration(CLASS *class, TRANS_DECL *decl)
{
	CLASS_SYMBOL *sym;
	VARIABLE *var;
	int count;
	bool save_warnings = FALSE;

	if (decl->no_warning)
	{
		save_warnings = JOB->warnings;
		JOB->warnings = FALSE;
	}

	sym = CLASS_declare(class, decl->index, TYPE_get_kind(decl->type), TRUE);

	if (decl->no_warning)
	{
		JOB->warnings = save_warnings;
	}

	sym->global.type = decl->type;

	if (TYPE_get_kind(decl->type) == TK_CONST)
	{
		sym->global.value = CLASS_add_constant(class, decl);
		class->has_static = TRUE;
	}
	else if (TYPE_is_static(decl->type))
	{
		CLASS_add_static_declaration(class, decl->index, decl->type, sym, FALSE);
		CLASS_init_global_declaration(class, decl, sym, FALSE);
	}
	else
	{
		count = ARRAY_count(class->dyn);
		if (count >= MAX_CLASS_SYMBOL)
			THROW("Too many dynamic variables");
		
		sym->global.value = count;
		var = ARRAY_add(&class->dyn);

		var->type = decl->type;
		var->index = decl->index;

		CLASS_init_global_declaration(class, decl, sym, FALSE);
		
		/*CLASS_begin_init_function(class, FUNC_INIT_DYNAMIC);

		if (TRANS_has_init_var(decl))
		{
			FUNCTION_add_all_pos_line();
			TRANS_init_var(decl);
			CODE_pop_global(sym->global.value, FALSE);
			sym->global_assigned = TRUE;
		}*/
	}
	
	CLASS_check_variable_prefix(sym, FALSE);
}

// Don't do that! The order of variables must be kept.

void CLASS_sort_declaration(CLASS *class)
{
	//reorder_decl(class, class->stat, "static");
	//reorder_decl(class, class->dyn, "dynamic");
}


int CLASS_add_symbol(CLASS *class, const char *name)
{
	int index;

	index = TABLE_add_symbol(class->table, name, strlen(name));
	return index;
}


void FUNCTION_add_last_pos_line(void)
{
	int current_pos;

	if (!JOB->debug || JOB->nobreak)
		return;
	
	current_pos = CODE_get_current_pos();
	*ARRAY_add(&JOB->func->pos_line) = current_pos;
}

void FUNCTION_add_all_pos_line(void)
{
	int line;
	int current_pos;
	
	if (!JOB->debug || JOB->nobreak)
		return;
	
	line = JOB->func->line + ARRAY_count(JOB->func->pos_line) - 1;
	current_pos = CODE_get_current_pos();
	
	//fprintf(stderr, "FUNCTION_add_all_pos_line: line = %d JOB->line = %d\n", line, JOB->line);
	
	while (line < JOB->line)
	{
		*ARRAY_add(&JOB->func->pos_line) = current_pos;
		line++;
	}
}

char *FUNCTION_get_fake_name(int func)
{
	static char buf[6];

	snprintf(buf, sizeof(buf), "$%d", func);
	return buf;
}


static int check_one_property_func(CLASS *class, PROPERTY *prop, bool write)
{
	CLASS_SYMBOL *sym;
	char *name;
	//char *name_alloc;
	bool is_static;
	FUNCTION *func;
	int index;
	int value = 0;

	JOB->line = prop->line;
	JOB->current = NULL;

	is_static = TYPE_is_static(prop->type);

	name = STR_copy(TABLE_get_symbol_name_suffix(class->table, prop->name, write ? "_Write" : "_Read"));

	if (!TABLE_find_symbol(class->table, name, strlen(name), &index))
	{
		TRANS_FUNC decl;
		
		if (prop->use == 0)
			THROW("&1 is not declared", name);
		
		CLEAR(&decl);
		//name_alloc = CLASS_add_name(class, name, strlen(name));
		decl.index = index = NO_SYMBOL; //CLASS_add_symbol(class, name_alloc);
		
		if (write)
		{
			decl.nparam = 1;
			decl.param[0].type = prop->type;
			decl.param[0].index = NO_SYMBOL;
		}
		else
		{
			decl.type = prop->type;
			TYPE_clear_flag(&decl.type, TF_PUBLIC);
		}
		
		decl.line = prop->line;
		
		TYPE_set_kind(&decl.type, TK_FUNCTION);
		if (is_static)
			TYPE_set_flag(&decl.type, TF_STATIC);
		
		value = CLASS_add_function(JOB->class, &decl);
		JOB->nobreak = TRUE;
		
		sym = CLASS_get_symbol(JOB->class, prop->use);
		
		if (write)
		{
			sym->global_assigned = TRUE;
			CODE_push_local(-1);
			CODE_pop_global(sym->global.value, is_static);
			CODE_return(2);
		}
		else
		{
			sym->global_used = TRUE;
			CODE_push_global(sym->global.value, is_static, FALSE);
			CODE_return(1);
		}
		
		FUNCTION_add_last_pos_line();
		JOB->func->stack = CODE_stack_usage;
		JOB->nobreak = FALSE;
	}

	if (index != NO_SYMBOL)
	{
		sym = (CLASS_SYMBOL *)TABLE_get_symbol(class->table, index);
		
		if (TYPE_get_kind(sym->global.type) != TK_FUNCTION)
			THROW("&1 is declared but is not a function", name);

		func = &class->function[sym->global.value];
		JOB->line = func->line;

		if (TYPE_is_public(sym->global.type))
			THROW("A property implementation cannot be public");

		if (is_static != TYPE_is_static(sym->global.type))
		{
			if (is_static)
				THROW("&1 must be static", name);
			else
				THROW("&1 cannot be static", name);
		}

		if (write)
		{
			if (TYPE_get_id(func->type) != T_VOID)
				goto _BAD_SIGNATURE;

			if (func->nparam != 1 || func->npmin != 1)
				goto _BAD_SIGNATURE;

			if (!TYPE_compare(&func->param[0].type, &prop->type))
				goto _BAD_SIGNATURE;
		}
		else
		{
			if (!TYPE_compare(&func->type, &prop->type))
				goto _BAD_SIGNATURE;

			if (func->nparam != 0 || func->npmin != 0)
				goto _BAD_SIGNATURE;
		}
	
		sym->global_used = TRUE;
		value = sym->global.value;
	}

	STR_free(name);
	return value;

_BAD_SIGNATURE:

	THROW("&1 declaration does not match", name);
}


void CLASS_check_properties(CLASS *class)
{
	int i;
	PROPERTY *prop;

	for (i = 0; i < ARRAY_count(class->prop); i++)
	{
		prop = &class->prop[i];
		if (prop->synonymous >= 0)
		{
			prop->read = class->prop[prop->synonymous].read;
			prop->write = class->prop[prop->synonymous].write;
		}
		else
		{
			// TODO: if COMPILER_version < 0x3180000, and if there is no prop->read, then
			// replace it by a function that raises an error.
			prop->read = prop->read ? check_one_property_func(class, prop, FALSE) : NO_SYMBOL;
			prop->write = prop->write ? check_one_property_func(class, prop, TRUE) : NO_SYMBOL;
		}
	}
}


void CLASS_check_variable_prefix(CLASS_SYMBOL *sym, bool local)
{
	char *name = sym->symbol.name;
	int len = sym->symbol.len;
	int len_prefix;
	char *p;
	TYPE type;
	
	if (!JOB->check_prefix)
		return;
	
	type = local ? sym->local.type : sym->global.type;
	if (TYPE_is_null(type))
		return;
	
	if (len >= 1 && *name == '$')
	{
		name++;
		len--;
	}
	
	if (len <= 1)
		return;
	
	len_prefix = 0;
	p = name;
	while (len > 0 && islower(*p))
	{
		p++;
		len--;
		len_prefix++;
	}
	
	if (TYPE_check_prefix(type, name, len_prefix))
		COMPILE_print(MSG_WARNING, -1, "variable prefix does not match its datatype: &1", SYMBOL_get_name(&sym->symbol));
}


char *CLASS_get_export_name(CLASS *class)
{
	if (!class->exported)
		return NULL;
	
	if (class->export_name)
		return class->export_name;
	
	return class->name;
}
