/***************************************************************************

	gbc_jit.c

	(c) 2000-2018 Benoît Minisini <g4mba5@gmail.com>

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

#define __GBC_JIT_BODY_C

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "gb_file.h"
#include "gb_str.h"
#include "gb_error.h"
#include "gb_pcode.h"
#include "gb_reserved.h"
#include "gbc_compile.h"
#include "gbc_chown.h"
#include "gbc_class.h"
#include "gbc_jit.h"

#define MAX_STACK 256

typedef
	struct {
		TYPE type;
		char *expr;
	}
	STACK_SLOT;

static STACK_SLOT _stack[MAX_STACK];
static int _stack_current = 0;

static bool _decl_rs;
static bool _decl_ro;
static bool _decl_rv;

static int _ctrl_count;
static ushort _pc;

static bool _no_release = FALSE;

static char _func_type;
static int _func_index;

static void init(void)
{
	_ctrl_count = 0;
	
	JIT_print("  VALUE *sp = SP;\n");
	JIT_print("  ushort *pc = PC;\n");
	JIT_print("  void *gp = 0;\n");
}


static void free_stack(int n)
{
	if (n < 0) n = _stack_current - n;
	STR_free(_stack[n].expr);
	_stack[n].expr = NULL;
}


static void check_stack(int n)
{
	if (_stack_current < n)
		THROW("Stack mismatch");
}


static void pop_stack(int n)
{
	int i;
	
	for (i = 1; i <= n; i++)
		free_stack(-i);
	
	_stack_current -= n;
}


static void check_labels(ushort pos)
{
	int i;
	
	for (i = 0; i < ARRAY_count(TRANS_labels); i++)
	{
		if (TRANS_labels[i] == pos)
		{
			JIT_print("__L%d:;\n", pos);
			break;
		}
	}
}


static void declare(bool *flag, const char *expr)
{
	if (*flag)
		return;
	
	JIT_print("  %s;\n", expr);
	*flag = TRUE;
}


static void push_one(TYPE type, const char *fmt, va_list args)
{
	if (_stack_current > MAX_STACK)
		THROW("Expression too complex");
	
	_stack[_stack_current].expr = NULL;
	
	if (fmt)
		STR_vadd(&_stack[_stack_current].expr, fmt, args);
	else
		_stack[_stack_current].expr = STR_copy(va_arg(args, char *));
	
	_stack[_stack_current].type = type;
	_stack_current++;
}


static void push(TYPE type, const char *fmt, ...)
{
  va_list args;
	//char *borrow = NULL;
	
  va_start(args, fmt);
	
	/*switch (TYPE_get_id(type))
	{
		case T_STRING:
		case T_VARIANT:
		case T_OBJECT:
			STR_add(&borrow, "BORROW_%s(%s)", JIT_get_type(type), fmt);
			push_one(type, borrow, args);
			STR_free(borrow);
			break;
			
		default:
			push_one(type, fmt, args);
	}*/
	
	push_one(type, fmt, args);
	
	va_end(args);
}


static TYPE get_type(int n)
{
	if (n < 0) n += _stack_current;
	return _stack[n].type;
}

#define get_type_id(_n) TYPE_get_id(get_type(_n))


static char *get_conv(TYPE src, TYPE dest)
{
	static char buffer[32];
	char s, d;
	
	s = TYPE_get_id(src);
	d = TYPE_get_id(dest);
	
	if (s == T_UNKNOWN)
	{
		sprintf(buffer, "GB.Conv(%%s,%d)", d);
		return buffer;
	}
	
	switch(d)
	{
		case T_VOID:
			
			switch(s)
			{
				case T_STRING:
					return "RELEASE_s(%s)";
				case T_OBJECT:
					return "RELEASE_o(%s)";
				case T_VARIANT:
					return "RELEASE_v(%s)";
				default:
					return "((void)%s)";
			}
			
		case T_BOOLEAN:
			
			switch(s)
			{
				case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG: case T_SINGLE: case T_FLOAT: case T_POINTER:
					return "((%s)!=0)";
				case T_OBJECT:
					return "((%s).value!=0)";
			}
			break;
			
		case T_BYTE:
			
			switch(s)
			{
				case T_BOOLEAN:
					return "((uchar)(%s)?255:0)";
				case T_SHORT: case T_INTEGER: case T_LONG: case T_SINGLE: case T_FLOAT:
					return "((uchar)(%s))";
			}
			break;
			
		case T_SHORT:
			
			switch(s)
			{
				case T_BOOLEAN:
					return "((short)(%s)?-1:0)";
				case T_BYTE: case T_INTEGER: case T_LONG: case T_SINGLE: case T_FLOAT:
					return "((short)(%s))";
			}
			break;

		case T_INTEGER:
			
			switch(s)
			{
				case T_BOOLEAN:
					return "((int)(%s)?-1:0)";
				case T_BYTE: case T_SHORT: case T_LONG: case T_SINGLE: case T_FLOAT: case T_POINTER:
					return "((int)(%s))";
			}
			break;
		
		case T_LONG:
			
			switch(s)
			{
				case T_BOOLEAN:
					return "((int64_t)(%s)?-1:0)";
				case T_BYTE: case T_SHORT: case T_INTEGER: case T_SINGLE: case T_FLOAT: case T_POINTER:
					return "((int64_t)(%s))";
			}
			break;
			
		case T_SINGLE:
			
			switch(s)
			{
				case T_BOOLEAN:
					return "((float)(%s)?-1:0)";
				case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG: case T_FLOAT:
					return "((float)(%s))";
			}
			break;
			
		case T_FLOAT:
			
			switch(s)
			{
				case T_BOOLEAN:
					return "((double)(%s)?-1:0)";
				case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG: case T_SINGLE:
					return "((double)(%s))";
			}
			break;
			
		case T_STRING:
			
			switch(s)
			{
				case T_CSTRING: return "%s";
			}
			break;

		case T_CSTRING:
			
			switch(s)
			{
				case T_STRING: return "%s";
			}
			break;
	}
	
	sprintf(buffer, "CONV_%s_%s(%%s)", JIT_get_type(src), JIT_get_type(dest));
	return buffer;
}

static char *peek(int n, TYPE conv, const char *fmt, va_list args)
{
	char *dest = NULL;
	char *expr;
	TYPE type;
	char *op;
	
	if (n < 0) n += _stack_current;
	
	expr = _stack[n].expr;
	type = _stack[n].type;
	
	if (fmt)
	{
		STR_vadd(&dest, fmt, args);
	
		if (!_no_release)
		{
			switch (TYPE_get_id(conv))
			{
				case T_STRING:
					declare(&_decl_rs, "char *rs");
					JIT_print("  rs = (%s).value.addr;\n", dest);
					break;
					
				case T_OBJECT:
					declare(&_decl_ro, "void *ro");
					JIT_print("  ro = (%s).value;\n", dest);
					break;

				case T_VARIANT:
					declare(&_decl_rv, "GB_VARIANT rv");
					JIT_print("  rv = (%s);\n", dest);
					break;
			}
		}
	}
	
	if (!TYPE_compare(&type, &conv))
	{
		char *expr2 = NULL;
		STR_add(&expr2, get_conv(type, conv), expr);
		STR_free(expr);
		expr = expr2;
		_stack[n].expr = expr2;
	}
	
	if (fmt)
	{
		if (_no_release)
		{
			JIT_print("  ");
			JIT_print(dest, expr);
			JIT_print(";\n");
		}
		else
		{
			if (dest[strlen(dest) - 1] != '=')
				op = " =";
			else
				op = "";
			
			switch (TYPE_get_id(conv))
			{
				case T_STRING:
					JIT_print("  %s%s BORROW_s(%s);\n", dest, op, expr);
					break;
					
				case T_OBJECT:
					JIT_print("  %s%s BORROW_o(%s);\n", dest, op, expr);
					break;

				case T_VARIANT:
					JIT_print("  %s%s BORROW_v(%s);\n", dest, op, expr);
					break;
					
				default:
					JIT_print("  %s%s %s;\n", dest, op, expr);
			}

			
			
			if (!_no_release)
			{
				switch (TYPE_get_id(conv))
				{
					case T_STRING: JIT_print("  GB.FreeString(&rs);\n"); break;
					case T_OBJECT: JIT_print("  GB.Unref(&ro);\n"); break;
					case T_VARIANT:  JIT_print("  GB.ReleaseValue((GB_VALUE *)&rv);\n"); break;
				}
			}
		}
		STR_free(dest);
	}
	
	return expr;
}


static void pop(TYPE type, const char *fmt, ...)
{
	va_list args;
	char *expr;
	
	check_stack(1);
	
	_stack_current--;
	
	va_start(args, fmt);
	expr = peek(_stack_current, type, fmt, args);
	va_end(args);

	if (!fmt)
		JIT_print("  %s;\n", expr);
	
	free_stack(_stack_current);
}


static void push_constant(int index)
{
	CONSTANT *c = &JOB->class->constant[index];
	
	switch(TYPE_get_id(c->type))
	{
		case T_SINGLE:
			push(c->type, "(float)%s", TABLE_get_symbol_name(JOB->class->table, c->value));
			break;
		case T_FLOAT:
			push(c->type, "(double)%s", TABLE_get_symbol_name(JOB->class->table, c->value));
			break;
		default:
			push(TYPE_get_id(c->type) == T_STRING ? TYPE_make_simple(T_CSTRING) : c->type, "CONSTANT_%s(%d)", JIT_get_type(c->type), index);
	}
}


static void push_subr(ushort code, const char *call)
{
	char type_id;
	TYPE type;
	int i, narg;
	char *expr = NULL;
	ushort op;
	bool rst = FALSE;
	
	//declare_sp();
	
	//JIT_print("  static ushort s%d = 0x%04X;\n", _subr_count, code);
	
	op = code >> 8;
	
	if (op == (C_NEW >> 8))
	{
		narg = code & 0x3F;
		type_id = T_OBJECT;
		type = get_type(-narg);
	}
	else if (op < CODE_FIRST_SUBR)
	{
		int index = RESERVED_get_from_opcode(code);
		
		if (index < 0)
			THROW("Unknown operator");
		
		if (RES_is_unary(index))
			narg = 1;
		else if (RES_is_binary(index))
			narg = 2;
		else
			narg = code & 0x3F;

		type_id = COMP_res_info[index].type;
		rst = TRUE;
	}
	else
	{
		SUBR_INFO *info = SUBR_get_from_opcode(op - CODE_FIRST_SUBR, code & 0x3F);
		if (info->min_param != info->max_param)
			narg = code & 0x3F;
		else
			narg = info->min_param;
		
		type_id = info->type;
		rst = TRUE;
	}

	check_stack(narg);
	
	if (rst)
	{
		switch(type_id)
		{
			case RST_SAME:
			case RST_BCLR:
				type_id = get_type_id(-narg);
				break;
				
			case RST_MIN:
				type_id = Max(get_type_id(-1), get_type_id(-2));
				if (type_id > T_DATE && type_id != T_VARIANT)
					type_id = T_UNKNOWN;
		}
	}
	
	if (type_id > T_OBJECT)
		type_id = T_UNKNOWN;
	
	for (i = _stack_current - narg; i < _stack_current; i++)
	{
		STR_add(&expr, "PUSH_%s(%s),", JIT_get_type(_stack[i].type), _stack[i].expr);
		free_stack(i);
	}
	
	_stack_current -= narg;
	
	STR_add(&expr, "%s(%d, 0x%04X)", call, _pc, code);
	if (type_id == T_VOID)
		STR_add(&expr, ",sp--");
	else
		STR_add(&expr, ",POP_%s()", JIT_get_type(TYPE_make_simple(type_id)));
	
	if (op == (C_NEW >> 8))
	{
		if (TYPE_get_id(type) == T_CLASS)
			TYPE_set_id(&type, T_OBJECT);
		else
			type = TYPE_make_simple(T_OBJECT);
	}
	else
		type = TYPE_make_simple(type_id);
	
	push(type, "(%s)", expr);
	
	STR_free(expr);
}


static void push_subr_add(ushort code, const char *op, const char *opb, bool allow_pointer)
{
	char *expr;
	char *expr1, *expr2;
	TYPE type1, type2, type;
	
	check_stack(2);
	
	type1 = get_type(-2);
	type2 = get_type(-1);
	
	if (TYPE_get_id(type1) > TYPE_get_id(type2))
		type = type1;
	else
		type = type2;
	
	switch(TYPE_get_id(type))
	{
		case T_BOOLEAN: case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG: case T_SINGLE: case T_FLOAT:
			break;
			
		case T_DATE: case T_STRING: case T_CSTRING:
			type = TYPE_make_simple(T_FLOAT);
			break;
			
		case T_POINTER:
			if (allow_pointer)
				break;
			
		default:
			push_subr(code, "CALL_SUBR_CODE");
			return;
	}
	
	expr1 = peek(-2, type, NULL, NULL);
	expr2 = peek(-1, type, NULL, NULL);

	switch(TYPE_get_id(type))
	{
		case T_BOOLEAN:
			expr = STR_print("%s %s %s", expr1, opb, expr2);
			break;
			
		default:
			expr = STR_print("%s %s %s", expr1, op, expr2);
			break;
	}
	
	pop_stack(2);
	
	push(type, "(%s)", expr);
	STR_free(expr);
}


static void push_subr_div(ushort code)
{
	char *expr;
	char *expr1, *expr2;
	TYPE type1, type2, type;
	
	check_stack(2);
	
	type1 = get_type(-2);
	type2 = get_type(-1);
	
	if (TYPE_get_id(type1) > TYPE_get_id(type2))
		type = type1;
	else
		type = type2;
	
	switch(TYPE_get_id(type))
	{
		case T_SINGLE: case T_FLOAT:
			break;
			
		case T_BOOLEAN: case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG:
			type = TYPE_make_simple(T_FLOAT);
			break;
			
		default:
			push_subr(code, "CALL_SUBR_CODE");
			return;
	}
	
	expr1 = peek(-2, type, NULL, NULL);
	expr2 = peek(-1, type, NULL, NULL);
	expr = STR_print("%s / %s", expr1, expr2);

	pop_stack(2);
	
	push(type, "(%s)", expr);
	STR_free(expr);
}


static void push_subr_neg(ushort code)
{
	TYPE type;
	char *expr;
	
	check_stack(1);
	
	type = get_type(-1);
	
	switch(TYPE_get_id(type))
	{
		case T_BOOLEAN:
			return;

		case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG: case T_SINGLE: case T_FLOAT:
			break;

		default:
			push_subr(code, "CALL_SUBR_CODE");
			return;
	}
	
	expr = STR_copy(peek(-1, type, NULL, NULL));
	
	pop_stack(1);
	push(type, "(- %s)", expr);
	STR_free(expr);
}



static void push_subr_and(ushort code, const char *op)
{
	char *expr;
	char *expr1, *expr2;
	TYPE type1, type2, type;
	
	check_stack(2);
	
	type1 = get_type(-2);
	type2 = get_type(-1);
	
	if (TYPE_get_id(type1) > TYPE_get_id(type2))
		type = type1;
	else
		type = type2;
	
	switch(TYPE_get_id(type))
	{
		case T_BOOLEAN: case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG:
			break;
			
		case T_DATE: case T_STRING: case T_CSTRING:
			type = TYPE_make_simple(T_BOOLEAN);
			break;
			
		default:
			push_subr(code, "CALL_SUBR_CODE");
			return;
	}
	
	expr1 = peek(-2, type, NULL, NULL);
	expr2 = peek(-1, type, NULL, NULL);

	expr = STR_print("%s %s %s", expr1, op, expr2);
	
	pop_stack(2);
	push(type, "(%s)", expr);
	STR_free(expr);
}


static void push_subr_not(ushort code)
{
	TYPE type;
	char *expr;
	char *op;
	
	check_stack(1);
	
	type = get_type(-1);
	
	switch(TYPE_get_id(type))
	{
		case T_BOOLEAN:
			op = "!";
			break;

		case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG:
			op = "~";
			break;

		default:
			push_subr(code, "CALL_SUBR_CODE");
			return;
	}
	
	expr = STR_print("%s%s", op, peek(-1, type, NULL, NULL));
	
	pop_stack(1);
	push(type, "(%s)", expr);
	STR_free(expr);
}


static void push_subr_comp(ushort code)
{
	char *op = NULL;
	char *expr;
	char *expr1, *expr2;
	TYPE type1, type2, type;
	
	check_stack(2);
	
	type1 = get_type(-2);
	type2 = get_type(-1);
	
	if (TYPE_get_id(type1) > TYPE_get_id(type2))
		type = type1;
	else
		type = type2;
	
	switch(TYPE_get_id(type))
	{
		case T_BOOLEAN: case T_BYTE: case T_SHORT: case T_INTEGER: case T_LONG: case T_SINGLE: case T_FLOAT: case T_POINTER:
			
			switch(code & 0xFF00)
			{
				case C_EQ: op = "=="; break;
				case C_NE: op = "!="; break;
				case C_GT: op = ">"; break;
				case C_LT: op = "<"; break;
				case C_GE: op = ">="; break;
				case C_LE: op = "<="; break;
			}
			break;
	}
	
	if (!op)
	{
		push_subr(code, "CALL_SUBR_CODE");
		return;
	}
	
	expr1 = peek(-2, type, NULL, NULL);
	expr2 = peek(-1, type, NULL, NULL);

	expr = STR_print("%s %s %s", expr1, op, expr2);

	pop_stack(2);
	
	push(TYPE_make_simple(T_BOOLEAN), "(%s)", expr);
	STR_free(expr);
}


static void push_subr_conv(ushort code)
{
	char *expr;
	TYPE type;
	TYPE conv = TYPE_make_simple(code & 0x3F);

	check_stack(1);
	type = get_type(-1);
	
	if (TYPE_compare(&type, &conv))
		return;
	
	expr = peek(-1, conv, NULL, NULL);
	pop_stack(1);
	push(conv, "(%s)", expr);
	STR_free(expr);
}


static void push_subr_len(ushort code)
{
	char *expr;

	check_stack(1);
	
	expr = peek(-1, TYPE_make_simple(T_STRING), NULL, NULL);
	pop_stack(1);
	push(TYPE_make_simple(T_INTEGER), "((%s).value.len)", expr);
	STR_free(expr);
}


/*static void push_subr_left(ushort code)
{
	char *expr = NULL;
	TYPE type;
	char *expr_str, *expr_len = NULL;
	
	declare_sp();
	
	JIT_print("  static ushort s%d = 0x%04X;\n", _subr_count, code);
	
	check_stack(1);
	
	if (_stack_current >= 2)
	{
		expr_len = peek(-1, TYPE_make_simple(T_INTEGER), NULL, NULL);
		pop_stack(1);
	}
	
	type = get_type(-1);
	expr_str = peek(-1, TYPE_make_simple(T_CSTRING), NULL, NULL);
	pop_stack(1);
	
	STR_add(&expr, "(PUSH_t(%s)", expr_str);
	STR_free(expr_str);
	if (expr_len)
	{
		STR_add(&expr, ",PUSH_i(%s)", expr_len);
		STR_free(expr_len);
	}

	push(type, "%s,CALL_SUBR_CODE(s%d),POP_t())", expr, _subr_count, code);
	_subr_count++;
}*/


static void push_array(ushort code)
{
	TYPE type;
	int i, narg;
	char *expr = NULL;
	char *expr1, *expr2;
	
	narg = code & 0x3F;
	check_stack(narg);
	
	type = get_type(-narg);
	
	if (TYPE_get_id(type) == T_OBJECT)
	{
		CLASS_REF *cr = &JOB->class->class[TYPE_get_value(type)];

		SYMBOL *sym = (SYMBOL *)CLASS_get_symbol(JOB->class, cr->index);
		JIT_print("  // %.*s\n", sym->len, sym->name);
		
		if (!TYPE_is_null(cr->type))
		{
			if (narg == 2)
			{
				expr1 = peek(-2, get_type(-2), NULL, NULL);
				expr2 = peek(-1, TYPE_make_simple(T_INTEGER), NULL, NULL);
				
				expr = STR_print("PUSH_ARRAY_%s(%s,%s)", JIT_get_type(cr->type), expr1, expr2);
				
				pop_stack(2);
				
				push(cr->type, "(%s)", expr);
				
				return;
			}
		}
		
		type = cr->type;
	}
	else
		type = TYPE_make_simple(T_UNKNOWN);
	
	//declare_sp();
	
	for (i = _stack_current - narg; i < _stack_current; i++)
	{
		STR_add(&expr, "PUSH_%s(%s),", JIT_get_type(_stack[i].type), _stack[i].expr);
		free_stack(i);
	}
	
	_stack_current -= narg;
	
	STR_add(&expr, "CALL_PUSH_ARRAY(%d),sp--", _pc);
	
	push(type, "(%s)", expr);
	
	STR_free(expr);
}


static void pop_array(ushort code)
{
	TYPE type;
	int i, narg;
	char *expr = NULL;
	char *expr1, *expr2;
	
	narg = code & 0x3F;
	check_stack(narg + 1);
	
	type = get_type(-narg);
	
	if (TYPE_get_id(type) == T_OBJECT)
	{
		CLASS_REF *cr = &JOB->class->class[TYPE_get_value(type)];

		SYMBOL *sym = (SYMBOL *)CLASS_get_symbol(JOB->class, cr->index);
		JIT_print("  // %.*s\n", sym->len, sym->name);
		
		if (!TYPE_is_null(cr->type))
		{
			if (narg == 2)
			{
				expr = peek(-3, cr->type, NULL, NULL);
				expr1 = peek(-2, get_type(-2), NULL, NULL);
				expr2 = peek(-1, TYPE_make_simple(T_INTEGER), NULL, NULL);
				
				JIT_print("  POP_ARRAY_%s(%s,%s,%s);\n", JIT_get_type(cr->type), expr1, expr2, expr);
				
				pop_stack(3);
				
				return;
			}
		}
		
		type = cr->type;
	}
	else
		type = TYPE_make_simple(T_UNKNOWN);
	
	//declare_sp();
	
	narg++;
	
	for (i = _stack_current - narg; i < _stack_current; i++)
	{
		STR_add(&expr, "PUSH_%s(%s),", JIT_get_type(_stack[i].type), _stack[i].expr);
		free_stack(i);
	}
	
	_stack_current -= narg;
	
	STR_add(&expr, "CALL_POP_ARRAY(%d),sp--", _pc);
	
	push(TYPE_make_simple(T_VOID), "(%s)", expr);
	
	STR_free(expr);
	
	pop(TYPE_make_simple(T_VOID), NULL);
}


static void push_call(ushort code)
{
	char *call = NULL;
	int i;
	int narg;
	FUNCTION *func;
	
	narg = code & 0x3F;
	
	switch (_func_type)
	{
		case FUNCTION_PRIVATE:
			
			func = &JOB->class->function[_func_index];
			
			STR_add(&call,"%s_%d_IMPL(", JIT_prefix, _func_index);
			
			for (i = 0; i < narg; i++)
				STR_add(&call, ((i < (narg - 1)) ? "%s," : "%s"), peek(i - narg, func->param[i].type, NULL, NULL));
			
			STR_add(&call, ")");
			
			pop_stack(narg);
			
			push(func->type, "(%s)", call);
			
			break;
	}
	
	_func_type = -1;
}


static void push_subr_isnan(ushort code)
{
	char *func;
	char *expr;
	
	check_stack(1);
	
	switch (code & 0xFF)
	{
		case 1: // IsNan
			func = "isnan";
			break;

		case 2: // IsInf
			func = "isinf";
			break;
		
		default:
			push_subr(code, "CALL_SUBR_CODE");
			return;
	}

	expr = STR_print("%s(%s) != 0", func, peek(-1, TYPE_make_simple(T_FLOAT), NULL, NULL));
	
	pop_stack(1);
	push(TYPE_make_simple(T_BOOLEAN), "(%s)", expr);
	STR_free(expr);
}


#define GET_XXX()   (((signed short)(code << 4)) >> 4)
#define GET_UXX()   (code & 0xFFF)
#define GET_7XX()   (code & 0x7FF)
#define GET_XX()    ((signed char)code)
#define GET_UX()    ((unsigned char)code)
#define GET_3X()    (code & 0x3F)
#define TEST_XX()   (code & 1)
#define PC (&func->code[p])

void JIT_translate_body(FUNCTION *func)
{
	static const void *jump_table[256] =
	{
		/* 00 NOP             */  &&_MAIN,
		/* 01 PUSH LOCAL      */  &&_PUSH_LOCAL,
		/* 02 PUSH PARAM      */  &&_PUSH_PARAM,
		/* 03 PUSH ARRAY      */  &&_PUSH_ARRAY,
		/* 04 PUSH UNKNOWN    */  &&_PUSH_UNKNOWN,
		/* 05 PUSH EXTERN     */  &&_PUSH_EXTERN,
		/* 06 BYREF           */  &&_BYREF,
		/* 07 PUSH EVENT      */  &&_PUSH_EVENT,
		/* 08 QUIT            */  &&_QUIT,
		/* 09 POP LOCAL       */  &&_POP_LOCAL,
		/* 0A POP PARAM       */  &&_POP_PARAM,
		/* 0B POP ARRAY       */  &&_POP_ARRAY,
		/* 0C POP UNKNOWN     */  &&_POP_UNKNOWN,
		/* 0D POP OPTIONAL    */  &&_POP_OPTIONAL,
		/* 0E POP CTRL        */  &&_POP_CTRL,
		/* 0F BREAK           */  &&_BREAK,
		/* 10 RETURN          */  &&_RETURN,
		/* 11 PUSH SHORT      */  &&_PUSH_SHORT,
		/* 12 PUSH INTEGER    */  &&_PUSH_INTEGER,
		/* 13 PUSH CHAR       */  &&_PUSH_CHAR,
		/* 14 PUSH MISC       */  &&_PUSH_MISC,
		/* 15 PUSH ME         */  &&_PUSH_ME,
		/* 16 TRY             */  &&_TRY,
		/* 17 END TRY         */  &&_END_TRY,
		/* 18 CATCH           */  &&_CATCH,
		/* 19 DUP             */  &&_DUP,
		/* 1A DROP            */  &&_DROP,
		/* 1B NEW             */  &&_NEW,
		/* 1C CALL            */  &&_CALL,
		/* 1D CALL QUICK      */  &&_CALL_QUICK,
		/* 1E CALL EASY       */  &&_CALL_SLOW,
		/* 1F ON              */  &&_ON_GOTO_GOSUB,
		/* 20 JUMP            */  &&_JUMP,
		/* 21 JUMP IF TRUE    */  &&_JUMP_IF_TRUE,
		/* 22 JUMP IF FALSE   */  &&_JUMP_IF_FALSE,
		/* 23 GOSUB           */  &&_GOSUB,
		/* 24 JUMP FIRST      */  &&_JUMP_FIRST,
		/* 25 JUMP NEXT       */  &&_JUMP_NEXT,
		/* 26 FIRST           */  &&_ENUM_FIRST,
		/* 27 NEXT            */  &&_ENUM_NEXT,
		/* 28 =               */  &&_SUBR_COMPE,
		/* 29 <>              */  &&_SUBR_COMPN,
		/* 2A >               */  &&_SUBR_COMPGT,
		/* 2B <=              */  &&_SUBR_COMPLE,
		/* 2C <               */  &&_SUBR_COMPLT,
		/* 2D >=              */  &&_SUBR_COMPGE,
		/* 2E ==              */  &&_SUBR,
		/* 2F CASE            */  &&_SUBR_CODE,
		/* 30 +               */  &&_SUBR_ADD,
		/* 31 -               */  &&_SUBR_SUB,
		/* 32 *               */  &&_SUBR_MUL,
		/* 33 /               */  &&_SUBR_DIV,
		/* 34 NEG             */  &&_SUBR_NEG,
		/* 35 \               */  &&_SUBR_QUO,
		/* 36 MOD             */  &&_SUBR_REM,
		/* 37 ^               */  &&_SUBR_CODE,
		/* 38 AND             */  &&_SUBR_AND,
		/* 39 OR              */  &&_SUBR_OR,
		/* 3A XOR             */  &&_SUBR_XOR,
		/* 3B NOT             */  &&_SUBR_NOT,
		/* 3C &               */  &&_SUBR_CODE,
		/* 3D LIKE            */  &&_SUBR_CODE,
		/* 3E &/              */  &&_SUBR_CODE,
		/* 3F Is              */  &&_SUBR_CODE,
		/* 40 Left$           */  &&_SUBR_CODE,
		/* 41 Mid$            */  &&_SUBR_CODE,
		/* 42 Right$          */  &&_SUBR_CODE,
		/* 43 Len             */  &&_SUBR_LEN,
		/* 44 Space$          */  &&_SUBR,
		/* 45 String$         */  &&_SUBR,
		/* 46 Trim$           */  &&_SUBR_CODE,
		/* 47 UCase$          */  &&_SUBR_CODE,
		/* 48 Oct$            */  &&_SUBR_CODE,
		/* 49 Chr$            */  &&_SUBR,
		/* 4A Asc             */  &&_SUBR_CODE,
		/* 4B InStr           */  &&_SUBR_CODE,
		/* 4C RInStr          */  &&_SUBR_CODE,
		/* 4D Subst$          */  &&_SUBR_CODE,
		/* 4E Replace$        */  &&_SUBR_CODE,
		/* 4F Split           */  &&_SUBR_CODE,
		/* 50 Scan            */  &&_SUBR,
		/* 51 Comp            */  &&_SUBR_CODE,
		/* 52 Conv            */  &&_SUBR,
		/* 53 DConv           */  &&_SUBR_CODE,
		/* 54 Abs             */  &&_SUBR_CODE,
		/* 55 Int             */  &&_SUBR_CODE,
		/* 56 Fix             */  &&_SUBR_CODE,
		/* 57 Sgn             */  &&_SUBR_CODE,
		/* 58 Frac...         */  &&_SUBR_CODE,
		/* 59 Pi              */  &&_SUBR_CODE,
		/* 5A Round           */  &&_SUBR_CODE,
		/* 5B Randomize       */  &&_SUBR_CODE,
		/* 5C Rnd             */  &&_SUBR_CODE,
		/* 5D Min             */  &&_SUBR_CODE,
		/* 5E Max             */  &&_SUBR_CODE,
		/* 5F IIf             */  &&_SUBR_CODE,
		/* 60 Choose          */  &&_SUBR_CODE,
		/* 61 Array           */  &&_SUBR_CODE,
		/* 62 ATan2...        */  &&_SUBR_CODE,
		/* 63 IsAscii...      */  &&_SUBR_CODE,
		/* 64 BClr...         */  &&_SUBR_CODE,
		/* 65 IsBoolean...    */  &&_SUBR_CODE,
		/* 66 TypeOf          */  &&_SUBR_CODE,
		/* 67 CBool...        */  &&_SUBR_CONV,
		/* 68 Bin$            */  &&_SUBR_CODE,
		/* 69 Hex$            */  &&_SUBR_CODE,
		/* 6A Val             */  &&_SUBR,
		/* 6B Str             */  &&_SUBR,
		/* 6C Format          */  &&_SUBR_CODE,
		/* 6D Timer           */  &&_SUBR,
		/* 6E Now             */  &&_SUBR,
		/* 6F Year...         */  &&_SUBR_CODE,
		/* 70 Week            */  &&_SUBR_CODE,
		/* 71 Date            */  &&_SUBR_CODE,
		/* 72 Time...         */  &&_SUBR_CODE,
		/* 73 DateAdd...      */  &&_SUBR_CODE,
		/* 74 Eval            */  &&_SUBR_CODE,
		/* 75 Error           */  &&_SUBR,
		/* 76 Debug           */  &&_SUBR,
		/* 77 Wait            */  &&_SUBR_CODE,
		/* 78 Open            */  &&_SUBR_CODE,
		/* 79 Close           */  &&_SUBR,
		/* 7A Input           */  &&_SUBR_CODE,
		/* 7B LineInput       */  &&_SUBR,
		/* 7C Print           */  &&_SUBR_CODE,
		/* 7D Read            */  &&_SUBR_CODE,
		/* 7E Write           */  &&_SUBR_CODE,
		/* 7F Flush           */  &&_SUBR,
		/* 80 Lock...         */  &&_SUBR_CODE,
		/* 81 InputFrom...    */  &&_SUBR_CODE,
		/* 82 Eof             */  &&_SUBR_CODE,
		/* 83 Lof             */  &&_SUBR_CODE,
		/* 84 Seek            */  &&_SUBR_CODE,
		/* 85 Kill            */  &&_SUBR_CODE,
		/* 86 Mkdir           */  &&_SUBR_CODE,
		/* 87 Rmdir           */  &&_SUBR_CODE,
		/* 88 Move            */  &&_SUBR_CODE,
		/* 89 Copy            */  &&_SUBR_CODE,
		/* 8A Link            */  &&_SUBR_ISNAN,
		/* 8B Exist           */  &&_SUBR_CODE,
		/* 8C Access          */  &&_SUBR_CODE,
		/* 8D Stat            */  &&_SUBR_CODE,
		/* 8E Dfree           */  &&_SUBR,
		/* 8F Temp$           */  &&_SUBR_CODE,
		/* 90 IsDir           */  &&_SUBR,
		/* 91 Dir             */  &&_SUBR_CODE,
		/* 92 RDir            */  &&_SUBR_CODE,
		/* 93 Exec...         */  &&_SUBR_CODE,
		/* 94 Alloc           */  &&_SUBR_CODE,
		/* 95 Free            */  &&_SUBR,
		/* 96 Realloc         */  &&_SUBR_CODE,
		/* 97 StrPtr          */  &&_SUBR_CODE,
		/* 98 Sleep...        */  &&_SUBR_CODE,
		/* 99 VarPtr          */  &&_SUBR_CODE,
		/* 9A Collection      */  &&_SUBR_CODE,
		/* 9B Tr$             */  &&_SUBR,
		/* 9C Quote$...       */  &&_SUBR_CODE,
		/* 9D Unquote$...     */  &&_SUBR_CODE,
		/* 9E MkInt$...       */  &&_SUBR_CODE,
		/* 9F Byte@...        */  &&_SUBR_CODE,
		/* A0 ADD QUICK       */  &&_ADD_QUICK,
		/* A1 ADD QUICK       */  &&_ADD_QUICK,
		/* A2 ADD QUICK       */  &&_ADD_QUICK,
		/* A3 ADD QUICK       */  &&_ADD_QUICK,
		/* A4 ADD QUICK       */  &&_ADD_QUICK,
		/* A5 ADD QUICK       */  &&_ADD_QUICK,
		/* A6 ADD QUICK       */  &&_ADD_QUICK,
		/* A7 ADD QUICK       */  &&_ADD_QUICK,
		/* A8 ADD QUICK       */  &&_ADD_QUICK,
		/* A9 ADD QUICK       */  &&_ADD_QUICK,
		/* AA ADD QUICK       */  &&_ADD_QUICK,
		/* AB ADD QUICK       */  &&_ADD_QUICK,
		/* AC ADD QUICK       */  &&_ADD_QUICK,
		/* AD ADD QUICK       */  &&_ADD_QUICK,
		/* AE ADD QUICK       */  &&_ADD_QUICK,
		/* AF ADD QUICK       */  &&_ADD_QUICK,
		/* B0 PUSH CLASS      */  &&_PUSH_CLASS,
		/* B1 PUSH CLASS      */  &&_PUSH_CLASS,
		/* B2 PUSH CLASS      */  &&_PUSH_CLASS,
		/* B3 PUSH CLASS      */  &&_PUSH_CLASS,
		/* B4 PUSH CLASS      */  &&_PUSH_CLASS,
		/* B5 PUSH CLASS      */  &&_PUSH_CLASS,
		/* B6 PUSH CLASS      */  &&_PUSH_CLASS,
		/* B7 PUSH CLASS      */  &&_PUSH_CLASS,
		/* B8 PUSH FUNCTION   */  &&_PUSH_FUNCTION,
		/* B9 PUSH FUNCTION   */  &&_PUSH_FUNCTION,
		/* BA PUSH FUNCTION   */  &&_PUSH_FUNCTION,
		/* BB PUSH FUNCTION   */  &&_PUSH_FUNCTION,
		/* BC PUSH FUNCTION   */  &&_PUSH_FUNCTION,
		/* BD PUSH FUNCTION   */  &&_PUSH_FUNCTION,
		/* BE PUSH FUNCTION   */  &&_PUSH_FUNCTION,
		/* BF PUSH FUNCTION   */  &&_PUSH_FUNCTION,
		/* C0 PUSH DYNAMIC    */  &&_PUSH_DYNAMIC,
		/* C1 PUSH DYNAMIC    */  &&_PUSH_DYNAMIC,
		/* C2 PUSH DYNAMIC    */  &&_PUSH_DYNAMIC,
		/* C3 PUSH DYNAMIC    */  &&_PUSH_DYNAMIC,
		/* C4 PUSH DYNAMIC    */  &&_PUSH_DYNAMIC,
		/* C5 PUSH DYNAMIC    */  &&_PUSH_DYNAMIC,
		/* C6 PUSH DYNAMIC    */  &&_PUSH_DYNAMIC,
		/* C7 PUSH DYNAMIC    */  &&_PUSH_DYNAMIC,
		/* C8 PUSH STATIC     */  &&_PUSH_STATIC,
		/* C9 PUSH STATIC     */  &&_PUSH_STATIC,
		/* CA PUSH STATIC     */  &&_PUSH_STATIC,
		/* CB PUSH STATIC     */  &&_PUSH_STATIC,
		/* CC PUSH STATIC     */  &&_PUSH_STATIC,
		/* CD PUSH STATIC     */  &&_PUSH_STATIC,
		/* CE PUSH STATIC     */  &&_PUSH_STATIC,
		/* CF PUSH STATIC     */  &&_PUSH_STATIC,
		/* D0 POP DYNAMIC     */  &&_POP_DYNAMIC,
		/* D1 POP DYNAMIC     */  &&_POP_DYNAMIC,
		/* D2 POP DYNAMIC     */  &&_POP_DYNAMIC,
		/* D3 POP DYNAMIC     */  &&_POP_DYNAMIC,
		/* D4 POP DYNAMIC     */  &&_POP_DYNAMIC,
		/* D5 POP DYNAMIC     */  &&_POP_DYNAMIC,
		/* D6 POP DYNAMIC     */  &&_POP_DYNAMIC,
		/* D7 POP DYNAMIC     */  &&_POP_DYNAMIC,
		/* D8 POP STATIC      */  &&_POP_STATIC,
		/* D9 POP STATIC      */  &&_POP_STATIC,
		/* DA POP STATIC      */  &&_POP_STATIC,
		/* DB POP STATIC      */  &&_POP_STATIC,
		/* DC POP STATIC      */  &&_POP_STATIC,
		/* DD POP STATIC      */  &&_POP_STATIC,
		/* DE POP STATIC      */  &&_POP_STATIC,
		/* DF POP STATIC      */  &&_POP_STATIC,
		/* E0 PUSH CONST      */  &&_PUSH_CONST,
		/* E1 PUSH CONST      */  &&_PUSH_CONST,
		/* E2 PUSH CONST      */  &&_PUSH_CONST,
		/* E3 PUSH CONST      */  &&_PUSH_CONST,
		/* E4 PUSH CONST      */  &&_PUSH_CONST,
		/* E5 PUSH CONST      */  &&_PUSH_CONST,
		/* E6 PUSH CONST      */  &&_PUSH_CONST,
		/* E7 PUSH CONST      */  &&_PUSH_CONST,
		/* E8 PUSH CONST      */  &&_PUSH_CONST,
		/* E9 PUSH CONST      */  &&_PUSH_CONST,
		/* EA PUSH CONST      */  &&_PUSH_CONST,
		/* EB PUSH CONST      */  &&_PUSH_CONST,
		/* EC PUSH CONST      */  &&_PUSH_CONST,
		/* ED PUSH CONST      */  &&_PUSH_CONST,
		/* EE PUSH CONST      */  &&_PUSH_CONST,
		/* EF PUSH CONST      */  &&_PUSH_CONST_EX,
		/* F0 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F1 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F2 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F3 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F4 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F5 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F6 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F7 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F8 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F9 PUSH QUICK      */  &&_PUSH_QUICK,
		/* FA PUSH QUICK      */  &&_PUSH_QUICK,
		/* FB PUSH QUICK      */  &&_PUSH_QUICK,
		/* FC PUSH QUICK      */  &&_PUSH_QUICK,
		/* FD PUSH QUICK      */  &&_PUSH_QUICK,
		/* FE PUSH QUICK      */  &&_PUSH_QUICK,
		/* FF PUSH QUICK      */  &&_PUSH_QUICK
	};

	CLASS *class = JOB->class;
	TYPE type;
	uint p = 0;
	ushort code;
	int index;
	
	init();
	
	//JIT_print("  JIT.debug(\"SP = %%p\\n\", SP);\n");
	
	goto _MAIN;
	
_MAIN:

	//JIT_print("__L%d:\n", p);
	check_labels(p);
	
	if (p >= (func->ncode - 1)) // ignore the last opcode which is RETURN
	{
		if (_stack_current)
			THROW("Stack mismatch");
		STR_free_later(NULL);
		JIT_print("__RETURN:\n");
		JIT_print("  SP = sp;\n");
		return;
	}
	
	_pc = p;
	code = func->code[p++];
	goto *jump_table[code >> 8];

_PUSH_LOCAL:

	push(func->local[func->nparam + GET_XX()].type, "l%d", GET_XX());
	goto _MAIN;

_POP_LOCAL:

	pop(func->local[func->nparam + GET_XX()].type, "l%d", GET_XX());
	goto _MAIN;

_PUSH_PARAM:

	index = func->nparam + GET_XX();
	push(func->param[index].type, "p%d", index);
	goto _MAIN;

_POP_PARAM:

	index = func->nparam + GET_XX();
	pop(func->param[index].type, "p%d", index);
	goto _MAIN;

_PUSH_QUICK:

	push(TYPE_make_simple(T_INTEGER), "%d", GET_XXX());
	goto _MAIN;

_PUSH_SHORT:

	push(TYPE_make_simple(T_INTEGER), "%d", (short)PC[0]);
	p++;
	goto _MAIN;

_PUSH_INTEGER:

	push(TYPE_make_simple(T_INTEGER), "%d", PC[0] | ((uint)PC[1] << 16));
	p += 2;
	goto _MAIN;

_PUSH_STATIC:

	type = class->stat[GET_7XX()].type;
	if (TYPE_get_id(type) == T_OBJECT)
	{
		if (TYPE_get_value(type) < 0)
			push(type, "GET_STATIC_o(%d, GB_T_OBJECT)", GET_7XX());
		else
			push(type, "GET_STATIC_o(%d, GET_CLASS(%d))", GET_7XX(), TYPE_get_value(type));
	}
	else
		push(type, "GET_STATIC_%s(%d)", JIT_get_type(type), GET_7XX());
	goto _MAIN;

_PUSH_DYNAMIC:

	type = class->dyn[GET_7XX()].type;
	push(type, "GET_DYNAMIC_%s(%d)", JIT_get_type(type), GET_7XX());
	goto _MAIN;

_POP_STATIC:

	type = class->stat[GET_7XX()].type;
	_no_release = TRUE;
	pop(type, "SET_STATIC_%s(%d, %%s)", JIT_get_type(type), GET_7XX());
	_no_release = FALSE;
	goto _MAIN;

_POP_DYNAMIC:

	type = class->stat[GET_7XX()].type;
	_no_release = TRUE;
	pop(type, "SET_DYNAMIC_%s(%d, %%s)", JIT_get_type(type), GET_7XX());
	_no_release = FALSE;
	goto _MAIN;

_PUSH_MISC:

	switch (GET_UX())
	{
		case 0:
			push(TYPE_make_simple(T_OBJECT), "NULL");
			break;
			
		case 1:
			push(TYPE_make_simple(T_VOID), "");
			break;
			
		case 2:
			push(TYPE_make_simple(T_BOOLEAN), "1");
			break;
		
		case 3:
			push(TYPE_make_simple(T_BOOLEAN), "0");
			break;
			
		default:
			goto _ILLEGAL;
	}
	goto _MAIN;
	
_PUSH_CHAR:

	push(TYPE_make_simple(T_CSTRING), "GET_CHAR(%d)", GET_UX());
	goto _MAIN;

_POP_OPTIONAL:

	check_stack(1);
	if (TYPE_get_id(get_type(-1)) == T_VOID)
		pop_stack(1);
	else
	{
		index = func->nparam + GET_XX() - func->npmin;
		JIT_print("  if (o%d & %d)\n  ", index / 8, (1 << (index % 8)));
		index = func->nparam + GET_XX();
		pop(func->param[index].type, "p%d", index);
	}
	goto _MAIN;
	
_PUSH_CLASS:

	index = GET_7XX();
	type = TYPE_make(T_OBJECT, index, 0);
	TYPE_set_id(&type, T_CLASS);
	push(type, "GET_CLASS(%d)", index);
	goto _MAIN;

_PUSH_FUNCTION:

	_func_type = FUNCTION_PRIVATE;
	_func_index = GET_7XX();
	goto _MAIN;
	
_CALL:

	push_call(code);
	goto _MAIN;

_SUBR:

	push_subr(code, "CALL_SUBR");
	goto _MAIN;

_SUBR_CODE:

	push_subr(code, "CALL_SUBR_CODE");
	goto _MAIN;

_DROP:

	pop(TYPE_make_simple(T_VOID), NULL);
	goto _MAIN;

_NEW:

	push_subr(code, "CALL_NEW");
	goto _MAIN;

_RETURN:

	switch(code & 0xFF)
	{
		case 0:
			// TODO: Return from GoSub
			JIT_print("  goto __RETURN;\n");
			break;
			
		case 1:
			pop(func->type, "r");
			JIT_print("  goto __RETURN;\n");
			break;
		
		case 2:
			JIT_print("  goto __RETURN;\n");
			break;
			
		default:
			goto _ILLEGAL;
	}
	goto _MAIN;

_GOSUB:

	JIT_print("  PUSH_GOSUB();\n");

_JUMP:

	JIT_print("  goto __L%d;\n", p + (signed short)PC[0] + 1);
	p++;
	goto _MAIN;
	
_JUMP_IF_TRUE:

	JIT_print("  if (%s) goto __L%d;\n", peek(-1, TYPE_make_simple(T_BOOLEAN), NULL, NULL), p + (signed short)PC[0] + 1);
	pop_stack(1);
	goto _MAIN;

_JUMP_IF_FALSE:

	JIT_print("  if (!(%s)) goto __L%d;\n", peek(-1, TYPE_make_simple(T_BOOLEAN), NULL, NULL), p + (signed short)PC[0] + 1);
	pop_stack(1);
	goto _MAIN;

_JUMP_FIRST:

	index = PC[2] & 0xFF;
	type = func->local[func->nparam + index].type;
	pop(type, "const %s c%d", JIT_get_ctype(type), _ctrl_count);
	JIT_print("  goto __L%ds;\n", p + 1);
	
	goto _MAIN;

_JUMP_NEXT:
	{
		char *expr;
		
		expr = peek(-1, type, NULL, NULL);
		pop_stack(1);
		
		JIT_print("  l%d += c%d;\n", index, _ctrl_count);
		JIT_print("__L%ds:\n", p);
		JIT_print("  if (((c%d > 0) && (l%d > %s)) || ((c%d < 0) && (l%d < %s))) goto __L%d;\n", _ctrl_count, index, expr, _ctrl_count, index, expr, p + (signed short)PC[0] + 1);
		
		STR_free(expr);
		_ctrl_count++;
		
		p +=2;
		goto _MAIN;
	}

_PUSH_CONST:

	push_constant(GET_UXX());
	
	index = GET_UXX();
	goto _MAIN;

_PUSH_CONST_EX:

	push_constant(PC[0]);
	p++;
	goto _MAIN;

_PUSH_ARRAY:

	push_array(code);
	goto _MAIN;

_POP_ARRAY:

	pop_array(code);
	goto _MAIN;

_ADD_QUICK:

	index = GET_XXX();
	push(TYPE_make_simple(T_INTEGER), "%d", abs(index));
	if (index < 0)
		goto _SUBR_SUB;

_SUBR_ADD:

	push_subr_add(code, "+", "|", TRUE);
	goto _MAIN;

_SUBR_SUB:

	push_subr_add(code, "-", "^", TRUE);
	goto _MAIN;

_SUBR_MUL:

	push_subr_add(code, "*", "&", FALSE);
	goto _MAIN;

_SUBR_DIV:

	push_subr_div(code);
	goto _MAIN;

_SUBR_NEG:

	push_subr_neg(code);
	goto _MAIN;

_SUBR_AND:

	push_subr_and(code, "&");
	goto _MAIN;
	
_SUBR_OR:

	push_subr_and(code, "|");
	goto _MAIN;
	
_SUBR_XOR:

	push_subr_and(code, "^");
	goto _MAIN;
	
_SUBR_NOT:

	push_subr_not(code);
	goto _MAIN;

_SUBR_COMPE:
_SUBR_COMPN:
_SUBR_COMPGT:
_SUBR_COMPLE:
_SUBR_COMPLT:
_SUBR_COMPGE:

	push_subr_comp(code);
	goto _MAIN;
	
_SUBR_ISNAN:

	push_subr_isnan(code);
	goto _MAIN;

_SUBR_CONV:
	
	push_subr_conv(code);
	goto _MAIN;
	
_SUBR_LEN:

	push_subr_len(code);
	goto _MAIN;

_BREAK:

	goto _MAIN;

_PUSH_UNKNOWN:
_PUSH_EXTERN:
_BYREF:
_PUSH_EVENT:
_QUIT:
_POP_UNKNOWN:
_POP_CTRL:
_PUSH_ME:
_TRY:
_END_TRY:
_CATCH:
_DUP:
_CALL_QUICK:
_CALL_SLOW:
_ON_GOTO_GOSUB:
_ENUM_FIRST:
_ENUM_NEXT:
_SUBR_QUO:
_SUBR_REM:
_ILLEGAL:
	{
		char opcode[8];
		sprintf(opcode, "%04X", code);
		THROW("Unsupported opcode &1", opcode);
	}
}

