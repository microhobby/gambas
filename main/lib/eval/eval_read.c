/***************************************************************************

  eval_read.c

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

#define __EVAL_READ_C

#include <ctype.h>

#include "gb_common.h"
#include "gb_error.h"
#include "gb_table.h"

#include "eval.h"
#include "eval_read.h"

//#define DEBUG 1
//#define ENABLE_BIG_COMMENT

PUBLIC const char *READ_source_ptr;
#define source_ptr READ_source_ptr

static bool is_init = FALSE;
static bool _begin_line = FALSE;

static char ident_car[256];
static char first_car[256];
static char noop_car[256];
static char canres_car[256];

#undef isspace
#define isspace(_c) (((uchar)_c) <= ' ')
#undef isdigit
#define isdigit(_c) ((_c) >= '0' && (_c) <= '9')

enum
{
	GOTO_BREAK,
	GOTO_SPACE,
	GOTO_NEWLINE,
	GOTO_COMMENT,
	GOTO_STRING,
	GOTO_IDENT,
	GOTO_QUOTED_IDENT,
	GOTO_NUMBER,
	GOTO_ERROR,
	GOTO_SHARP,
	GOTO_OTHER
};

static void READ_init(void)
{
	unsigned char i;

	if (!is_init)
	{
		for (i = 0; i < 255; i++)
		{
			ident_car[i] = (i != 0) && ((i >= 'A' && i <= 'Z') || (i >= 'a' && i <= 'z') || (i >= '0' && i <= '9') || strchr("$_?@", i));
			noop_car[i] = ident_car[i] || isdigit(i) || i <= ' ';
			canres_car[i] = (i != ':') && (i != '.') && (i != '!') && (i != '(');

			if (i == 0)
				first_car[i] = GOTO_BREAK;
			else if (i == '\n')
				first_car[i] = GOTO_NEWLINE;
			else if (i <= ' ')
				first_car[i] = GOTO_SPACE;
			else if (i == '\'')
				first_car[i] = GOTO_COMMENT;
			else if (i == '"')
				first_car[i] = GOTO_STRING;
			else if (i == '#')
				first_car[i] = GOTO_SHARP;
			else if ((i >= 'A' && i <= 'Z') || (i >= 'a' && i <= 'z') || i == '$' || i == '_')
				first_car[i] = GOTO_IDENT;
			else if (i == '{')
				first_car[i] = GOTO_QUOTED_IDENT;
			else if (i >= '0' && i <= '9')
				first_car[i] = GOTO_NUMBER;
			else if (i >= 127)
				first_car[i] = GOTO_ERROR;
			else
				first_car[i] = GOTO_OTHER;
		}

		is_init = TRUE;
	}
}

static bool _no_quote = FALSE;
#define BUF_MAX 255
static char _buffer[BUF_MAX + 1];

PUBLIC char *READ_get_pattern(PATTERN *pattern)
{
	int type = PATTERN_type(*pattern);
	int index = PATTERN_index(*pattern);
	const char *str;
	const char *before = _no_quote ? "" : "'";
	const char *after = _no_quote ? "" : "'";

	switch(type)
	{
		case RT_RESERVED:
			//snprintf(COMMON_buffer, COMMON_BUF_MAX, "%s%s%s", before, TABLE_get_symbol_name(COMP_res_table, index), after);
			str = COMP_res_info[index].name;
			if (ispunct((unsigned char)*str))
				snprintf(_buffer, BUF_MAX, "%s%s%s", before, str, after);
			else
				strcpy(_buffer, str);
			break;

		case RT_INTEGER:
			snprintf(_buffer, BUF_MAX, "%s%d%s", before, PATTERN_signed_index(*pattern), after);
			break;

		case RT_NUMBER:
		case RT_IDENTIFIER:
		case RT_CLASS:
			snprintf(_buffer, BUF_MAX, "%s%s%s", before, TABLE_get_symbol_name(EVAL->table, index), after);
			break;

		case RT_STRING:
		case RT_TSTRING:
			if (_no_quote)
				snprintf(_buffer, BUF_MAX, "\"%s\"", TABLE_get_symbol_name(EVAL->string, index));
			else
				strcpy(_buffer, "string");
			break;

		case RT_NEWLINE:
		case RT_END:
			strcpy(_buffer, "end of expression");
			break;

		case RT_SUBR:
			//snprintf(COMMON_buffer, COMMON_BUF_MAX, "%s%s%s", bafore, COMP_subr_info[index].name, after);
			strcpy(_buffer, COMP_subr_info[index].name);
			break;

		case RT_COMMENT:
			strncpy(_buffer, TABLE_get_symbol_name(EVAL->string, index), BUF_MAX);
			_buffer[BUF_MAX] = 0;
			break;
			
		case RT_SPACE:
			snprintf(_buffer, BUF_MAX, "[%d]", index);
			break;

		default:
			sprintf(_buffer, "%s?%08X?%s", before, *pattern, after);
	}

	return _buffer;
}

#if DEBUG
PUBLIC void READ_dump_pattern(PATTERN *pattern)
{
	int type = PATTERN_type(*pattern);
	int index = PATTERN_index(*pattern);
	int pos;

	pos = (int)(pattern - EVAL->pattern);
	if (pos < 0 || pos >= EVAL->pattern_count)
		return;

	printf("%d ", pos);

	if (PATTERN_flag(*pattern) & RT_FIRST)
		printf("!");
	else
		printf(" ");

	if (PATTERN_flag(*pattern) & RT_POINT)
		printf(".");
	else
		printf(" ");

	if (PATTERN_flag(*pattern) & RT_CLASS)
		printf("C");
	else
		printf(" ");

	printf(" ");

	_no_quote = TRUE;

	if (type == RT_RESERVED)
		printf("RESERVED     %s\n", READ_get_pattern(pattern));
	else if (type == RT_INTEGER)
		printf("INTEGER      %s\n", READ_get_pattern(pattern));
	else if (type == RT_NUMBER)
		printf("NUMBER       %s\n", READ_get_pattern(pattern));
	else if (type == RT_IDENTIFIER)
		printf("IDENTIFIER   %s\n", READ_get_pattern(pattern));
	else if (type == RT_CLASS)
		printf("CLASS        %s\n", READ_get_pattern(pattern));
	else if (type == RT_STRING)
		printf("STRING       %s\n", READ_get_pattern(pattern));
	else if (type == RT_TSTRING)
		printf("TSTRING      %s\n", READ_get_pattern(pattern));
	else if (type == RT_NEWLINE)
		printf("NEWLINE      (%d)\n", index);
	else if (type == RT_END)
		printf("END\n");
	else if (type == RT_PARAM)
		printf("PARAM        %d\n", index);
	else if (type == RT_SUBR)
		printf("SUBR         %s\n", READ_get_pattern(pattern));
	else if (type == RT_COMMENT)
		printf("COMMENT      %s\n", READ_get_pattern(pattern));
	else if (type == RT_SPACE)
		printf("SPACE        %s\n", READ_get_pattern(pattern));
	else
		printf("?            %d\n", index);

	_no_quote = FALSE;
}
#endif

#define get_char_offset(_offset) ((unsigned char)source_ptr[(_offset)])
#define get_char() ((unsigned char)(*source_ptr))

static unsigned char next_char(void)
{
	source_ptr++;
	return get_char();
}

#ifdef DEBUG

static void add_pattern(int type, int index)
{
	EVAL->pattern[EVAL->pattern_count++] = PATTERN_make(type, index);
	READ_dump_pattern(&EVAL->pattern[EVAL->pattern_count - 1]);
}

#else

#define add_pattern(_type, _index) EVAL->pattern[EVAL->pattern_count++] = PATTERN_make((_type), (_index));

#endif

static PATTERN get_previous_pattern(int n)
{
	int i;
	PATTERN pattern;
	
	for (i = EVAL->pattern_count - 1; i >= 0; i--)
	{
		pattern = EVAL->pattern[i];
		if (!PATTERN_is_space(pattern))
		{
			n--;
			if (n == 0)
				return pattern;
		}
	}

	return NULL_PATTERN;
}

#define get_last_pattern() get_previous_pattern(1)
#define get_last_last_pattern() get_previous_pattern(2)

static void add_newline()
{
	add_pattern(RT_NEWLINE, 0);
}


static void add_end()
{
	add_pattern(RT_END, 0);
}


#include "gbc_read_temp.h"

#if 0
static void add_identifier()
{
	unsigned char car;
	const char *start;
	int len;
	int index;
	int type;
	int flag;
	PATTERN last_pattern, last_last_pattern;
	bool not_first;
	bool can_be_reserved;
	bool last_identifier, last_type, last_class, last_pub;
	bool exist = TRUE;

	last_pattern = get_last_pattern();

	if (PATTERN_is_reserved(last_pattern))
	{
		flag = RES_get_ident_flag(PATTERN_index(last_pattern));
		if (flag & RSF_PREV)
		{
			last_last_pattern = get_last_last_pattern();
			if (PATTERN_is_reserved(last_last_pattern))
				flag = RES_get_ident_flag(PATTERN_index(last_last_pattern));
			else
				flag = 0;
		}
	}
	else
		flag = 0;

	type = RT_IDENTIFIER;

	last_class = (flag & RSF_CLASS) != 0;
	last_type = (flag & RSF_AS) != 0;

	start = source_ptr;
	for(;;)
	{
		source_ptr++;
		if (!ident_car[get_char()])
			break;
	}
	
	if (last_type && get_char() == ':')
	{
		for(;;)
		{
			source_ptr++;
			if (!ident_car[get_char()])
				break;
		}
	}
	
	len = source_ptr - start;

	if (last_type)
	{
		source_ptr--;
		
		for(;;)
		{
			source_ptr++;
			len++;
			car = get_char();
			if (car == '[')
			{
				car = get_char_offset(1);
				if (car == ']')
				{
					source_ptr++;
					len++;
					index = TABLE_add_symbol(EVAL->table, start, len - 2);
					continue;
				}
			}

			len--;
			break;
		}
	}

	not_first = (flag & RSF_POINT) != 0;

	car = get_char();

	//can_be_reserved = !not_first && TABLE_find_symbol(COMP_res_table, &comp->source[start], len, NULL, &index);

	can_be_reserved = !not_first && !last_class;

	if (can_be_reserved)
	{
		index = RESERVED_find_word(start, len);
		can_be_reserved = (index >= 0);
	}

	if (can_be_reserved)
	{
		static void *jump[] = {
			&&__OTHERS, &&__ME_NEW_LAST_SUPER, &&__CLASS, &&__STRUCT, &&__SUB_PROCEDURE_FUNCTION, &&__CONST_EXTERN, &&__ENUM, &&__READ, &&__DATATYPE
		};

		last_identifier = (flag & RSF_IDENT) != 0;
		last_pub = (flag & RSF_PUB) != 0;

		goto *jump[RES_get_read_switch(index)];

		do
		{
		__ME_NEW_LAST_SUPER:
			can_be_reserved = !last_identifier;
			break;

		__CLASS:
			can_be_reserved = canres_car[car] && (_begin_line || PATTERN_is(last_pattern, RS_END));
			break;

		__STRUCT:
			can_be_reserved = canres_car[car] && (_begin_line || last_pub || PATTERN_is(last_pattern, RS_AS) || PATTERN_is(last_pattern, RS_END) || PATTERN_is(last_pattern, RS_NEW));
			break;

		__SUB_PROCEDURE_FUNCTION:
			can_be_reserved = canres_car[car] && (_begin_line || last_pub || PATTERN_is(last_pattern, RS_END));
			break;

		__CONST_EXTERN:
			can_be_reserved = canres_car[car] && (_begin_line || last_pub);
			break;

		__ENUM:
			can_be_reserved = canres_car[car] && (_begin_line || last_pub);
			break;

		__READ:
			can_be_reserved = canres_car[car] && (!last_identifier || PATTERN_is(last_pattern, RS_PROPERTY));
			break;

		__DATATYPE:
			if (car == '[' && get_char_offset(1) == ']')
			{
				len += 2;
				source_ptr += 2;
				can_be_reserved = FALSE;
			}
			else
			{
				if (last_type || PATTERN_is(last_pattern, RS_OPEN))
					can_be_reserved = TRUE;
				else
					can_be_reserved = FALSE;
			}
			break;

		__OTHERS:
			if (last_type || last_identifier || (PATTERN_is(last_pattern, RS_LBRA) && car == ')' && PATTERN_is_reserved(get_last_last_pattern())))
				can_be_reserved = FALSE;
			else
				can_be_reserved = canres_car[car];
			break;
		}
		while (0);
	}

	if (can_be_reserved)
	{
		type = RT_RESERVED;
		goto __ADD_PATTERN;
	}

	if ((flag == 0) && car != '.' && car != '!')
	{
		index = RESERVED_find_subr(start, len);
		if (index >= 0)
		{
			if (COMP_subr_info[index].min_param == 0 || car == '(')
			{
				type = RT_SUBR;

				if (EVAL->custom)
				{
					GB_FUNCTION func;
					GB_VALUE *ret;

					if (!GB.GetFunction(&func, (void *)GB.GetClass(EVAL->parent), "IsSubr", NULL, NULL))
					{
						GB.Push(1, GB_T_STRING, start, len);
						ret = GB.Call(&func, 1, FALSE);
						if (!ret->_boolean.value)
						{
							exist = TABLE_add_symbol_exist(EVAL->table, start, len, &index);
							type = RT_IDENTIFIER;
						}
					}
				}

				goto __ADD_PATTERN;
			}
		}
	}

	if (last_type)
		type = RT_CLASS;

	/*if (flag & RSF_EVENT)
	{
		start--;
		len++;
		*((char *)start) = ':';
	}*/

	if (!EVAL->analyze && PATTERN_is(last_pattern, RS_EXCL))
	{
		index = TABLE_add_symbol(EVAL->string, start, len);
		type = RT_STRING;
	}
	else
	{
		exist = TABLE_add_symbol_exist(EVAL->table, start, len, &index);
	}

__ADD_PATTERN:

	if (EVAL->custom && !exist)
	{
		GB_FUNCTION func;
		GB_VALUE *ret;

		if (!GB.GetFunction(&func, (void *)GB.GetClass(EVAL->parent), "IsIdentifier", NULL, NULL))
		{
			GB.Push(1, GB_T_STRING, start, len);
			ret = GB.Call(&func, 1, FALSE);
			if (!ret->_boolean.value)
				THROW("Unknown symbol");
		}
	}

	add_pattern(type, index);
}
#endif

static void add_quoted_identifier(void)
{
	unsigned char car;
	const char *start;
	int len;
	int index;
	int type;
	PATTERN last_pattern;

	last_pattern = get_last_pattern();
	
	type = RT_IDENTIFIER;

	start = source_ptr;
	len = 1;

	for(;;)
	{
		source_ptr++;
		car = get_char();
		len++;
		if (!ident_car[car])
			break;
	}

	source_ptr++;
	
	if (!EVAL->analyze)
	{
		if (car != '}')
			THROW("Missing '}'");
	
		if (len == 2)
			THROW("Void identifier");
	}
	else
	{
		if (!car)
			len--;
		
		if (car != '}' || len <= 2)
			type = RT_ERROR;
	}
	
	if (!EVAL->analyze && PATTERN_is(last_pattern, RS_EXCL))
	{
		index = TABLE_add_symbol(EVAL->string, start + 1, len - 2);
		type = RT_STRING;
	}
	else
	{
		if (!EVAL->rewrite && type != RT_ERROR)
		{
			start++;
			len -= 2;
		}
		index = TABLE_add_symbol(type == RT_ERROR ? EVAL->string : EVAL->table, start, len);
	}

	add_pattern(type, index);
}


static void add_operator()
{
	unsigned char car;
	const char *start;
	const char *end;
	int len;
	int op = NO_SYMBOL;
	int index;

	start = source_ptr;
	end = start;
	len = 1;

	for(;;)
	{
		source_ptr++;

		index = RESERVED_find_word(start, len);
		if (index >= 0)
		{
			op = index;
			end = source_ptr;
		}

		car = get_char();
		//if (!isascii(car) || !ispunct(car))
		if (noop_car[car])
			break;
		len++;
	}

	source_ptr = end;

	if (EVAL->analyze && op == RS_QUES)
		op = RS_PRINT;

	if (op < 0)
		THROW("Unknown operator");

	add_pattern(RT_RESERVED, op);
}


static int xdigit_val(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	else if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	else
		return (-1);
}


static void add_string()
{
	unsigned char car;
	const char *start;
	int len;
	int index;
	ushort newline;
	bool jump;
	char *p;
	int i;

	start = source_ptr;
	len = 0;
	newline = 0;
	jump = FALSE;
	p = (char *)source_ptr;

	//fprintf(stderr, "EVAL_read: add_string: [%d] %s\n", source_ptr - EVAL->source, source_ptr);

	for(;;)
	{
		source_ptr++;
		car = get_char();

		//fprintf(stderr, "EVAL_read: car = %d jump = %d\n", car, jump);

		if (jump)
		{
			if (car == '\n')
				newline++;
			else if (car == '"')
				jump = FALSE;
			else if (!car || !isspace(car))
				break;
		}
		else
		{
			p++;
			len++;

			if (!car || car == '\n')
				THROW("Non terminated string");

			if (car == '\\')
			{
				source_ptr++;
				car = get_char();

				if (car == 'n')
					*p = '\n';
				else if (car == 't')
					*p = '\t';
				else if (car == 'r')
					*p = '\r';
				else if (car == 'b')
					*p = '\b';
				else if (car == 'v')
					*p = '\v';
				else if (car == 'f')
					*p = '\f';
				else if (car == 'e')
					*p = '\x1B';
				else if (car == '0')
					*p = 0;
				else if (car == '\"' || car == '\'' || car == '\\')
					*p = car;
				else
				{
					if (car == 'x')
					{
						i = xdigit_val(get_char_offset(1));
						if (i >= 0)
						{
							car = i;
							i = xdigit_val(get_char_offset(2));
							if (i >= 0)
							{
								car = (car << 4) | (uchar)i;
								*p = car;
								source_ptr += 2;
								continue;
							}
						}
					}

					THROW("Bad character constant in string");
				}
			}
			else if (car == '"')
			{
				p--;
				len--;
				jump = TRUE;
			}
			else
				*p = car;
		}
	}

	p[1] = 0;

	//fprintf(stderr, "EVAL_read: add_string (end): [%d] %s\n", source_ptr - EVAL->source, source_ptr);
	if (len > 0)
	{
		index = TABLE_add_symbol(EVAL->string, start + 1, len);
		add_pattern(RT_STRING, index);
	}
	else
		add_pattern(RT_STRING, VOID_STRING_INDEX);

	for (i = 0; i < newline; i++)
		add_newline();

	source_ptr -= newline;
}

#if ENABLE_BIG_COMMENT
static void add_big_comment()
{
	unsigned char car;
	const char *start;
	int len;
	int index;
	int type;
	//bool space = FALSE;

	start = source_ptr;
	len = 0;
	EVAL->comment = TRUE;

	/*for(;;)
	{
		if (start == EVAL->source)
			break;

		start--;
		car = *start;
		if (car == '\n')
			break;

		if (car > ' ')
		{
			start++;
			space = TRUE;
			break;
		}
		len++;
	}

	if (!space)
	{
		start = source_ptr;
		len = 1;
	}*/

	for(;;)
	{
		car = get_char();
		if (car == 0)
			break;

		/*if (car == '\n')
		{
			TABLE_add_symbol(EVAL->string, start, len, NULL, &index);
			type = RT_COMMENT;
			add_pattern(type, index);
			add_newline();
			len = 1;
			start = source_ptr + 1;
			continue;
		}*/

		if (car == '*' && get_char_offset(1) == '/')
		{
			source_ptr += 2;
			len += 2;
			EVAL->comment = FALSE;
			break;
		}
		len++;
		source_ptr++;
	}

	index = TABLE_add_symbol(EVAL->string, start, len);
	type = RT_COMMENT;
	add_pattern(type, index);
}
#endif

static void add_comment()
{
	unsigned char car;
	const char *start;
	int len;
	int index;
	int type;

	start = source_ptr;
	len = 1;

	for(;;)
	{
		source_ptr++;
		car = get_char();
		if (car == 0 || car == '\n')
			break;
		len++;
	}

	index = TABLE_add_symbol(EVAL->string, start, len);
	type = RT_COMMENT;

	add_pattern(type, index);
}


static void add_string_for_analyze()
{
	unsigned char car;
	const char *start;
	int len;
	int index;
	int type;

	start = source_ptr;
	len = 0;

	for(;;)
	{
		source_ptr++;
		car = get_char();
		if (car == '\\')
		{
			source_ptr++;
			car = get_char();
			len++;
			if (car == 0)
				break;
		}
		else if (car == 0 || car == '\n' || car == '"')
			break;
		len++;
	}

	if (car == '"')
		source_ptr++;

	index = TABLE_add_symbol(EVAL->string, start + 1, len);
	type = RT_STRING;

	add_pattern(type, index);
	//fprintf(stderr, "add_string_for_analyze: %s\n", TABLE_get_symbol_name(EVAL->string, index));
}


static void add_spaces()
{
	unsigned char car;
	int len;

	len = 1;

	for(;;)
	{
		source_ptr++;
		car = get_char();
		if (car > ' ' || car == '\n' || car == 0)
			break;
		len++;
	}

	add_pattern(RT_SPACE, len);
}


PUBLIC void EVAL_read(void)
{
	static void *jump_char[11] =
	{
		&&__BREAK,
		&&__SPACE,
		&&__NEWLINE,
		&&__COMMENT,
		&&__STRING,
		&&__IDENT,
		&&__QUOTED_IDENT,
		&&__NUMBER,
		&&__ERROR,
		&&__SHARP,
		&&__OTHER
	};

	unsigned char car;

	READ_init();

	source_ptr = EVAL->source;
	_begin_line = TRUE;

#if ENABLE_BIG_COMMENT
	if (EVAL->comment)
		goto __BIG_COMMENT;
#endif

	for(;;)
	{
		car = get_char();
		goto *jump_char[(int)first_car[car]];

	__ERROR:

		THROW(E_SYNTAX);

	__SPACE:
	
		if (EVAL->analyze)
			add_spaces();
		else
			source_ptr++;
		
		continue;
		
	__NEWLINE:

		source_ptr++;
		add_newline();
		_begin_line = TRUE;
		continue;

	__COMMENT:

		if (EVAL->analyze)
		{
			add_comment();
		}
		else
		{
			do
			{
				source_ptr++;
				car = get_char();
			}
			while (car != '\n' && car != 0);
		}

		_begin_line = FALSE;
		continue;

	__STRING:

		if (EVAL->analyze)
			add_string_for_analyze();
		else
			add_string();
		_begin_line = FALSE;
		continue;

	__IDENT:

		add_identifier();
		_begin_line = FALSE;
		continue;

	__QUOTED_IDENT:

		add_quoted_identifier();
		_begin_line = FALSE;
		continue;

	__NUMBER:

		add_number();
		_begin_line = FALSE;
		continue;

	__SHARP:

		if (_begin_line)
		{
			_begin_line = FALSE;
			if (get_char_offset(1) == '!' && EVAL->analyze)
				add_comment();
			else
				add_identifier();
			continue;
		}

	__OTHER:

#if ENABLE_BIG_COMMENT
		if (car == '/' && get_char_offset(1) == '*')
			goto __BIG_COMMENT;
#endif

		if (add_number())
			add_operator();

		_begin_line = FALSE;
		continue;

#if ENABLE_BIG_COMMENT
	__BIG_COMMENT:

		if (EVAL->analyze)
			add_big_comment();
		else
		{
			for(;;)
			{
				car = get_char();
				if (car == 0)
					break;
				if (car == '*' && get_char_offset(1) == '/')
				{
					source_ptr += 2;
					break;
				}
				if (car == '\n')
					add_newline();
				source_ptr++;
			}
		}

		_begin_line = FALSE;
		continue;
#endif
	}

__BREAK:

	add_end();
	add_end();
	add_end();
	add_end();
}

