/***************************************************************************

  gbx_exec_loop.c

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

#define __GBX_EXEC_LOOP_C

#include "gb_common.h"
#include "gb_common_check.h"
#include "gb_error.h"
#include "gbx_type.h"
#include "gbx_debug.h"

#include "gbx_subr.h"
#include "gb_pcode.h"
#include "gbx_stack.h"
#include "gbx_event.h"
#include "gbx_value.h"
#include "gbx_local.h"
#include "gbx_string.h"
#include "gbx_api.h"
#include "gbx_archive.h"
#include "gbx_extern.h"
#include "gbx_exec.h"
#include "gbx_subr.h"
#include "gbx_math.h"
#include "gbx_c_array.h"
#include "gbx_c_string.h"
#include "gbx_struct.h"
#include "gbx_variant.h"
#include "gbx_jit.h"

//#define DEBUG_PCODE 1

#if DEBUG_PCODE
#define PROJECT_EXEC
#include "gb_pcode_temp.h"
#endif

#define SUBR_beep EXEC_ILLEGAL

#define GET_XXX()   (((signed short)(code << 4)) >> 4)
#define GET_UXX()   (code & 0xFFF)
#define GET_7XX()   (code & 0x7FF)
#define GET_XX()    ((signed char)code)
#define GET_UX()    ((unsigned char)code)
#define GET_3X()    (code & 0x3F)
#define GET_0X()    (code & 0xF)
#define TEST_XX()   (code & 1)

NOINLINE static void _break(ushort code);

//static void _SUBR_comp(ushort code);
static void _SUBR_compe(ushort code);
//static void _SUBR_compi(ushort code);
static void _SUBR_add(ushort code);
static void _SUBR_sub(ushort code);
static void _SUBR_mul(ushort code);
static void _SUBR_div(ushort code);

#define my_VALUE_class_read VALUE_class_read_inline

NOINLINE static void SUBR_left(ushort code);
NOINLINE static void SUBR_right(ushort code);
NOINLINE static void SUBR_mid(ushort code);
NOINLINE static void SUBR_len(void);

//---- Subroutine dispatch table --------------------------------------------

const void *EXEC_subr_table[] =
{
	/* 00 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/* 10 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/* 20 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

	/* 28 */  _SUBR_compe,          _SUBR_compe,          NULL,                 NULL,
	/* 2C */  NULL,                 NULL,                 SUBR_near,            SUBR_case,
	/* 30 */  _SUBR_add,            _SUBR_sub,            _SUBR_mul,            _SUBR_div,
	/* 34 */  SUBR_neg,             SUBR_quo,             SUBR_rem,             SUBR_pow,
	/* 38 */  SUBR_and_,            SUBR_and_,            SUBR_and_,            SUBR_not,
	/* 3C */  SUBR_cat,             SUBR_like,            SUBR_file,            SUBR_is,

	SUBR_left,       /* 00 40 */
	SUBR_mid,        /* 01 41 */
	SUBR_right,      /* 02 42 */
	NULL,            /* 03 43 */
	SUBR_space,      /* 04 44 */
	SUBR_string,     /* 05 45 */
	SUBR_trim,       /* 06 46 */
	SUBR_upper,      /* 07 47 */
	SUBR_hex_bin,    /* 08 48 */
	SUBR_chr,        /* 09 49 */
	SUBR_asc,        /* 10 4A */
	SUBR_instr,      /* 11 4B */
	SUBR_instr,      /* 12 4C */
	SUBR_subst,      /* 13 4D */
	SUBR_replace,    /* 14 4E */
	SUBR_split,      /* 15 4F */
	SUBR_scan,       /* 16 50 */
	SUBR_strcomp,    /* 17 51 */
	SUBR_iconv,      /* 18 52 */
	SUBR_sconv,      /* 19 53 */
	SUBR_abs,        /* 20 54 */
	SUBR_int,        /* 21 55 */
	SUBR_fix,        /* 22 56 */
	SUBR_sgn,        /* 23 57 */
	SUBR_math,       /* 24 58 */
	SUBR_pi,         /* 25 59 */
	SUBR_round,      /* 26 5A */
	SUBR_randomize,  /* 27 5B */
	SUBR_rnd,        /* 28 5C */
	SUBR_min_max,    /* 29 5D */
	SUBR_min_max,    /* 30 5E */
	SUBR_if,         /* 31 5F */
	SUBR_choose,     /* 32 60 */
	SUBR_array,      /* 33 61 */
	SUBR_math2,      /* 34 62 */
	SUBR_is_chr,     /* 35 63 */
	SUBR_bit,        /* 36 64 */
	SUBR_is_type,    /* 37 65 */
	SUBR_type,       /* 38 66 */
	NULL,            /* 39 67 */
	SUBR_hex_bin,    /* 40 68 */
	SUBR_hex_bin,    /* 41 69 */
	SUBR_val,        /* 42 6A */
	SUBR_str,        /* 43 6B */
	SUBR_format,     /* 44 6C */
	SUBR_timer,      /* 45 6D */
	SUBR_now,        /* 46 6E */
	SUBR_year,       /* 47 6F */
	SUBR_week,       /* 48 70 */
	SUBR_date,       /* 49 71 */
	SUBR_time,       /* 50 72 */
	SUBR_date_op,    /* 51 73 */
	SUBR_eval,       /* 52 74 */
	SUBR_error,      /* 53 75 */
	SUBR_debug,      /* 54 76 */
	SUBR_wait,       /* 55 77 */
	SUBR_open,       /* 56 78 */
	SUBR_close,      /* 57 79 */
	SUBR_input,      /* 58 7A */
	SUBR_linput,     /* 59 7B */
	SUBR_print,      /* 60 7C */
	SUBR_read,       /* 61 7D */
	SUBR_write,      /* 62 7E */
	SUBR_flush,      /* 63 7F */
	SUBR_lock,       /* 64 80 */
	SUBR_inp_out,    /* 65 81 */
	SUBR_eof,        /* 66 82 */
	SUBR_lof,        /* 67 83 */
	SUBR_seek,       /* 68 84 */
	SUBR_kill,       /* 69 85 */
	SUBR_mkdir,      /* 70 86 */ // deprecated -> Even() & Odd()
	SUBR_rmdir,      /* 71 87 */ // deprecated
	SUBR_move,       /* 72 88 */
	SUBR_swap,       /* 73 89 */ // support for Copy()
	SUBR_link,       /* 74 8A */ // deprecated -> IsNan() & IsInf()
	SUBR_exist,      /* 75 8B */
	SUBR_access,     /* 76 8C */
	SUBR_stat,       /* 77 8D */
	SUBR_dfree,      /* 78 8E */
	SUBR_temp,       /* 79 8F */
	SUBR_isdir,      /* 80 90 */
	SUBR_dir,        /* 81 91 */
	SUBR_rdir,       /* 82 92 */
	SUBR_exec,       /* 83 93 */
	SUBR_alloc,      /* 84 94 */
	SUBR_free,       /* 85 95 */
	SUBR_realloc,    /* 86 96 */
	SUBR_strptr,     /* 87 97 */
	SUBR_sleep,      /* 88 98 */
	SUBR_varptr,     /* 89 99 */
	SUBR_collection, /* 90 9A */
	SUBR_tr,         /* 91 9B */
	SUBR_quote,      /* 92 9C */
	SUBR_unquote,    /* 93 9D */
	SUBR_make,       /* 94 9E */
	SUBR_ptr,        /* 95 9F */
};

//---- Main interpreter loop ------------------------------------------------

/*static void my_VALUE_class_read(CLASS *class, VALUE *value, char *addr, CTYPE ctype, void *ref)
{
	VALUE_class_read_inline(class, value, addr, ctype, ref);
}*/

static const void **_sb_jump_table;
static const void **_sb_jump_table_3_18_AXXX;
static const void **_sb_jump_table_3_18_FXXX;
static bool _sb_not_3_18 = FALSE;

void EXEC_init_bytecode_check()
{
	ushort opcode = C_RETURN + 3;
	PC = &opcode;
	EXEC_loop();
}

void EXEC_check_bytecode()
{
	int i;

	if (!CP)
		return;

	//fprintf(stderr, "EXEC_check_bytecode: %s / %d\n", CP->name, CP->not_3_18);

	if (CP->not_3_18 == _sb_not_3_18)
		return;

	_sb_not_3_18 = !_sb_not_3_18;
	//fprintf(stderr, "switch bytecode to %s\n", _sb_not_3_18 ? "< 3.18" : "3.18");

	if (_sb_not_3_18)
	{
		for (i = 0xA1; i <= 0xAE; i++)
			_sb_jump_table[i] = _sb_jump_table[0xA0];
		for (i = 0xF1; i <= 0xFE; i++)
			_sb_jump_table[i] = _sb_jump_table[0xF0];
	}
	else
	{
		for(i = 1; i <= 14; i++)
		{
			_sb_jump_table[0xA0 + i] = _sb_jump_table_3_18_AXXX[i];
			_sb_jump_table[0xF0 + i] = _sb_jump_table_3_18_FXXX[i];
		}
	}
}

INLINE static void _pop_ctrl(int ind)
{
	VALUE *val = &BP[ind];
	RELEASE(val);
	SP--;
	*val = *SP;
}

NOINLINE static void _push_event(int ind)
{
	if (ind >= 0xFE)
		ind = EXEC_push_unknown_event(ind & 1);
	else if (CP->parent)
		ind += CP->parent->n_event;

	SP->type = T_FUNCTION;
	SP->_function.kind = FUNCTION_EVENT;
	SP->_function.index = ind;
	SP->_function.defined = FALSE;
	SP->_function.class = NULL;
	SP->_function.object = NULL;
	SP++;
}

NOINLINE static void _pop_optional(int ind)
{
	VALUE *val = &PP[ind];

	if (val->type == T_VOID)
	{
		if (SP[-1].type == T_VOID)
			VALUE_default(&SP[-1], val->_void.ptype);
		else
			VALUE_conv(&SP[-1], val->_void.ptype);

		SP--;
		*val = *SP;
	}
	else
		POP();
}

NOINLINE static void _push_me(ushort code)
{
	if (GET_UX() & 1)
	{
		if (DEBUG_info)
		{
			if (DEBUG_info->op)
			{
				SP->_object.class = DEBUG_info->cp;
				SP->_object.object = DEBUG_info->op;
			}
			else if (DEBUG_info->cp)
			{
				SP->type = T_CLASS;
				SP->_class.class = DEBUG_info->cp;
			}
		}
		else
			VALUE_null(SP);
	}
	else
	{
		if (OP)
		{
			SP->_object.class = CP;
			SP->_object.object = OP;
		}
		/*else if (CP->auto_create)
		{
			OP = EXEC_auto_create(CP, FALSE);
			SP->_object.class = CP;
			SP->_object.object = OP;
			OP = NULL;
		}*/
		else
		{
			SP->type = T_CLASS;
			SP->_class.class = CP;
		}
	}

	if (GET_UX() & 2)
	{
		// The used class must be in the stack, because it is tested by exec_push && exec_pop
		if (OP)
		{
			SP->_object.class = SP->_object.class->parent;
			SP->_object.super = EXEC_super;
		}
		else
		{
			SP->_class.class = SP->_class.class->parent;
			SP->_class.super = EXEC_super;
		}

		EXEC_super = SP;

		//fprintf(stderr, "%s\n", DEBUG_get_current_position());
		//BREAKPOINT();
	}

	PUSH();
}

void EXEC_loop(void)
{
	static const void *jump_table[256] =
	{
		/* 00 NOP             */  &&_NEXT,
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
		/* 34 NEG             */  &&_SUBR_CODE,
		/* 35 \               */  &&_SUBR_CODE,
		/* 36 MOD             */  &&_SUBR_CODE,
		/* 37 ^               */  &&_SUBR_CODE,
		/* 38 AND             */  &&_SUBR_CODE,
		/* 39 OR              */  &&_SUBR_CODE,
		/* 3A XOR             */  &&_SUBR_CODE,
		/* 3B NOT             */  &&_SUBR_CODE,
		/* 3C &               */  &&_SUBR_CODE,
		/* 3D LIKE            */  &&_SUBR_CODE,
		/* 3E &/              */  &&_SUBR_CODE,
		/* 3F Is              */  &&_SUBR_CODE,
		/* 40 Left$           */  &&_SUBR_LEFT,
		/* 41 Mid$            */  &&_SUBR_MID,
		/* 42 Right$          */  &&_SUBR_RIGHT,
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
		/* 76 Debug           */  &&_SUBR_CODE,
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
		/* 8A Link            */  &&_SUBR_CODE,
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
		/* A1 ADD QUICK       */  &&_PUSH_ARRAY_NATIVE_INTEGER,
		/* A2 ADD QUICK       */  &&_POP_ARRAY_NATIVE_INTEGER,
		/* A3 ADD QUICK       */  &&_PUSH_ARRAY_NATIVE_FLOAT,
		/* A4 ADD QUICK       */  &&_POP_ARRAY_NATIVE_FLOAT,
		/* A5 ADD QUICK       */  &&_PUSH_ARRAY_NATIVE_COLLECTION,
		/* A6 ADD QUICK       */  &&_POP_ARRAY_NATIVE_COLLECTION,
		/* A7 ADD QUICK       */  &&_ADD_INTEGER,
		/* A8 ADD QUICK       */  &&_ADD_FLOAT,
		/* A9 ADD QUICK       */  &&_SUB_INTEGER,
		/* AA ADD QUICK       */  &&_SUB_FLOAT,
		/* AB ADD QUICK       */  &&_MUL_INTEGER,
		/* AC ADD QUICK       */  &&_MUL_FLOAT,
		/* AD ADD QUICK       */  &&_DIV_INTEGER,
		/* AE ADD QUICK       */  &&_DIV_FLOAT,
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
		/* F1 PUSH QUICK      */  &&_PUSH_LOCAL_NOREF,
		/* F2 PUSH QUICK      */  &&_PUSH_PARAM_NOREF,
		/* F3 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F4 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F5 PUSH QUICK      */  &&_PUSH_VARIABLE,
		/* F6 PUSH QUICK      */  &&_POP_VARIABLE,
		/* F7 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F8 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F9 PUSH QUICK      */  &&_POP_LOCAL_NOREF,
		/* FA PUSH QUICK      */  &&_POP_PARAM_NOREF,
		/* FB PUSH QUICK      */  &&_POP_LOCAL_FAST,
		/* FC PUSH QUICK      */  &&_POP_PARAM_FAST,
		/* FD PUSH QUICK      */  &&_PUSH_QUICK,
		/* FE PUSH QUICK      */  &&_JUMP_NEXT_INTEGER,
		/* FF PUSH QUICK      */  &&_PUSH_QUICK
	};

	static const void *jump_table_3_18_AXXX[] = {
		/* A0 ADD QUICK       */  &&_ADD_QUICK,
		/* A1 ADD QUICK       */  &&_PUSH_ARRAY_NATIVE_INTEGER,
		/* A2 ADD QUICK       */  &&_POP_ARRAY_NATIVE_INTEGER,
		/* A3 ADD QUICK       */  &&_PUSH_ARRAY_NATIVE_FLOAT,
		/* A4 ADD QUICK       */  &&_POP_ARRAY_NATIVE_FLOAT,
		/* A5 ADD QUICK       */  &&_PUSH_ARRAY_NATIVE_COLLECTION,
		/* A6 ADD QUICK       */  &&_POP_ARRAY_NATIVE_COLLECTION,
		/* A7 ADD QUICK       */  &&_ADD_INTEGER,
		/* A8 ADD QUICK       */  &&_ADD_FLOAT,
		/* A9 ADD QUICK       */  &&_SUB_INTEGER,
		/* AA ADD QUICK       */  &&_SUB_FLOAT,
		/* AB ADD QUICK       */  &&_MUL_INTEGER,
		/* AC ADD QUICK       */  &&_MUL_FLOAT,
		/* AD ADD QUICK       */  &&_DIV_INTEGER,
		/* AE ADD QUICK       */  &&_DIV_FLOAT,
		/* AF ADD QUICK       */  &&_ADD_QUICK
	};

	static const void *jump_table_3_18_FXXX[] = {
		/* F0 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F1 PUSH QUICK      */  &&_PUSH_LOCAL_NOREF,
		/* F2 PUSH QUICK      */  &&_PUSH_PARAM_NOREF,
		/* F3 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F4 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F5 PUSH QUICK      */  &&_PUSH_VARIABLE,
		/* F6 PUSH QUICK      */  &&_POP_VARIABLE,
		/* F7 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F8 PUSH QUICK      */  &&_PUSH_QUICK,
		/* F9 PUSH QUICK      */  &&_POP_LOCAL_NOREF,
		/* FA PUSH QUICK      */  &&_POP_PARAM_NOREF,
		/* FB PUSH QUICK      */  &&_POP_LOCAL_FAST,
		/* FC PUSH QUICK      */  &&_POP_PARAM_FAST,
		/* FD PUSH QUICK      */  &&_PUSH_QUICK,
		/* FE PUSH QUICK      */  &&_JUMP_NEXT_INTEGER,
		/* FF PUSH QUICK      */  &&_PUSH_QUICK
	};

	int NO_WARNING(ind);
	ushort code;
	VALUE *NO_WARNING(val);

_CHECK_BYTECODE:

	goto _MAIN;

/*-----------------------------------------------*/

_MAIN:

#if 0
	{
		FILE *f;

		f = fopen("/var/log/thttpd/pcode.log", "a");
		if (f)
		{
			fprintf(f, "%s: ", DEBUG_get_current_position());
			if (*PC >> 8)
				PCODE_dump(f, PC - FP->code, PC);
			else
				fprintf(f, "\n");
			fclose(f);
		}
	}
#endif

#if DEBUG_PCODE
		DEBUG_where();
		fprintf(stderr, "[%4d %ld] ", (int)(intptr_t)(SP - (VALUE *)STACK_base), SP - PP);
		if (*PC >> 8 && FP)
			PCODE_dump(stderr, PC - FP->code, PC);
		else
			fprintf(stderr, "?\n");
		fflush(stderr);
#endif

	code = *PC;
	goto *jump_table[code >> 8];

/*-----------------------------------------------*/

_SUBR_CODE:

	//fprintf(stderr, "gbx3: %02X: %s\n", (code >> 8), DEBUG_get_current_position());
	(*(EXEC_FUNC_CODE)EXEC_subr_table[code >> 8])(code);

	//if (PCODE_is_void(code))
	//  POP();

/*-----------------------------------------------*/

_NEXT:

	code = *(++PC);

#if 0
	{
		FILE *f;

		f = fopen("/var/log/thttpd/pcode.log", "a");
		if (f)
		{
			fprintf(f, "%s: ", DEBUG_get_current_position());
			if (*PC >> 8)
				PCODE_dump(f, PC - FP->code, PC);
			else
				fprintf(f, "\n");
			fclose(f);
		}
	}
#endif

#if DEBUG_PCODE
		DEBUG_where();
		fprintf(stderr, "[%4d %ld] ", (int)(intptr_t)(SP - (VALUE *)STACK_base), SP - PP);
		if (*PC >> 8 && FP)
			PCODE_dump(stderr, PC - FP->code, PC);
		else
			fprintf(stderr, "\n");
		fflush(stderr);
#endif

	goto *jump_table[code >> 8];

/*-----------------------------------------------*/

_SUBR:

	(*(EXEC_FUNC)EXEC_subr_table[code >> 8])();
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_LOCAL:

	*SP = BP[GET_XX()];
	PUSH();

	goto _NEXT;

_PUSH_LOCAL_NOREF:

	*SP++ = BP[GET_XX()];
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_PARAM:

	*SP = PP[GET_XX()];
	PUSH();

	goto _NEXT;

_PUSH_PARAM_NOREF:

	*SP++ = PP[GET_XX()];
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_ARRAY:

	EXEC_push_array(code);
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_UNKNOWN:

	EXEC_push_unknown();
	goto _NEXT;

/*-----------------------------------------------*/

/*_PUSH_SPECIAL:

	EXEC_push_special();
	goto _NEXT;*/

/*-----------------------------------------------*/

_PUSH_EVENT:

	/*
		The function called by raising an event is different at each call,
		but the signature remains the same, so optimizing the next CALL
		instruction with CALL QUICK is safe.

		The only problem is when pushing a 'NULL' function, i.e. a function
		that does nothing, because there is no handler for this event.
		Then CALL QUICK must know how to handle these functions.
	*/

	_push_event(GET_UX());
	goto _NEXT;

/*-----------------------------------------------*/

_POP_LOCAL:

	val = &BP[GET_XX()];
	VALUE_conv(&SP[-1], val->type);
	RELEASE(val);
	SP--;
	*val = *SP;

	goto _NEXT;

_POP_LOCAL_NOREF:

	val = &BP[GET_XX()];
	VALUE_conv(&SP[-1], val->type);
	SP--;
	*val = *SP;

	goto _NEXT;

_POP_LOCAL_FAST:

	SP--;
	BP[GET_XX()] = *SP;

	goto _NEXT;

/*-----------------------------------------------*/

_POP_PARAM:

	val = &PP[GET_XX()];
	VALUE_conv(&SP[-1], val->type);
	RELEASE(val);
	SP--;
	*val = *SP;
	goto _NEXT;

_POP_PARAM_NOREF:

	val = &PP[GET_XX()];
	VALUE_conv(&SP[-1], val->type);
	SP--;
	*val = *SP;
	goto _NEXT;

_POP_PARAM_FAST:

	SP--;
	PP[GET_XX()] = *SP;

	goto _NEXT;

/*-----------------------------------------------*/

_POP_CTRL:

	_pop_ctrl(GET_XX());
	goto _NEXT;

/*-----------------------------------------------*/

_POP_ARRAY:

	EXEC_pop_array(code);
	goto _NEXT;

/*-----------------------------------------------*/

_POP_UNKNOWN:

	EXEC_pop_unknown();
	goto _NEXT;

/*-----------------------------------------------*/

_POP_OPTIONAL:

	_pop_optional(GET_XX());
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_SHORT:

	SP->type = T_INTEGER;
	PC++;
	SP->_integer.value = *((short *)PC);
	SP++;
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_INTEGER:

	SP->type = T_INTEGER;
	PC++;
	SP->_integer.value = PC[0] | ((uint)PC[1] << 16);
	SP++;
	PC += 2;
	goto _MAIN;

/*-----------------------------------------------*/

_PUSH_CHAR:

	STRING_char_value(SP, GET_UX());
	SP++;
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_ME:

	_push_me(code);
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_MISC:

	{
		static const void *_jump[] =
			{ &&__PUSH_NULL, &&__PUSH_VOID, &&__PUSH_FALSE, &&__PUSH_TRUE, &&__PUSH_LAST, &&__PUSH_STRING, &&__PUSH_PINF, &&__PUSH_MINF, &&__PUSH_COMPLEX,
				&&__PUSH_VARGS, &&__PUSH_DROP_VARGS, &&__JIT_RETURN, &&__PUSH_END_VARGS };
			//, &&__POP_LAST };

		goto *_jump[GET_UX()];

	__PUSH_NULL:

		VALUE_null(SP);
		SP++;
		goto _NEXT;

	__PUSH_VOID:

		SP->type = T_VOID;
		SP++;
		goto _NEXT;

	__PUSH_FALSE:

		SP->type = T_BOOLEAN;
		SP->_integer.value = 0;
		SP++;
		goto _NEXT;

	__PUSH_TRUE:

		SP->type = T_BOOLEAN;
		SP->_integer.value = -1;
		SP++;
		goto _NEXT;

	__PUSH_LAST:

		SP->type = T_OBJECT;
		SP->_object.object = EVENT_Last;
		OBJECT_REF_CHECK(EVENT_Last);
		SP++;
		goto _NEXT;

	__PUSH_STRING:

		SP->type = T_CSTRING;
		SP->_string.addr = ""; // NULL
		SP->_string.start = SP->_string.len = 0;
		SP++;
		goto _NEXT;

	__PUSH_PINF:

		SP->type = T_FLOAT;
		SP->_float.value = INFINITY;
		SP++;
		goto _NEXT;

	__PUSH_MINF:

		SP->type = T_FLOAT;
		SP->_float.value = -INFINITY;
		SP++;
		goto _NEXT;

	__PUSH_COMPLEX:

		EXEC_push_complex();
		goto _NEXT;

	__PUSH_VARGS:

		EXEC_push_vargs();
		goto _NEXT;

	__PUSH_DROP_VARGS:

		EXEC_drop_vargs();
		goto _NEXT;
		
	__JIT_RETURN:
		return;
		
	__PUSH_END_VARGS:
	
		EXEC_end_vargs();
		goto _NEXT;

	/*__POP_LAST:

		VALUE_conv(&SP[-1], T_OBJECT);
		OBJECT_UNREF(EVENT_Last);
		SP--;
		EVENT_Last = SP->_object.object;
		goto _NEXT;*/
	}

/*-----------------------------------------------*/

/*
_PUSH_RETURN:

	*SP++ = *RP;
	goto _NEXT;
*/

/*-----------------------------------------------*/

_DUP:

	*SP = SP[-1];
	PUSH();
	goto _NEXT;

/*-----------------------------------------------*/

_DROP:

	POP();
	goto _NEXT;

/*-----------------------------------------------*/

_NEW:

	EXEC_new(code);
	goto _NEXT;

/*-----------------------------------------------*/

_ON_GOTO_GOSUB:

	{
		int n, m;

		m = GET_XX();
		SP--;
		VALUE_conv_integer(SP);
		n = SP->_integer.value;
		if (n < 0 || n >= m)
			PC += m + 3;
		else
		{
			PC[m + 2] = PC[n + 1] - (m - n) - 2;
			PC += m + 1;
		}
		goto _MAIN;
	}

/*-----------------------------------------------*/

_GOSUB:

	{
		STACK_check(1 + FP->stack_usage - FP->n_local);

		SP->type = T_VOID;
		SP->_void.value[0] = (intptr_t)PC;
		SP->_void.value[1] = (intptr_t)GP;

		GP = SP;

		SP++;

		val = &BP[FP->n_local];
		for (ind = 0; ind < FP->n_ctrl; ind++)
		{
			*SP++ = val[ind];
			val[ind].type = T_NULL;
		}
	}

/*-----------------------------------------------*/

_JUMP:

	PC += (signed short)PC[1] + 2;
	goto _MAIN;


/*-----------------------------------------------*/

_JUMP_IF_TRUE:

	VALUE_conv_boolean(&SP[-1]);
	SP--;
	if (SP->_boolean.value & 1)
		PC += (signed short)PC[1];

	PC += 2;
	goto _MAIN;

/*-----------------------------------------------*/

_JUMP_IF_FALSE:

	VALUE_conv_boolean(&SP[-1]);
	SP--;
	if ((SP->_boolean.value & 1) == 0)
		PC += (signed short)PC[1];

	PC += 2;
	goto _MAIN;

/*-----------------------------------------------*/

_RETURN:

	{
		static const void *return_jump[] = { &&__RETURN_GOSUB, &&__RETURN_VALUE, &&__RETURN_VOID, &&__INIT_BYTECODE_CHECK };

		goto *return_jump[GET_UX()];

	__RETURN_GOSUB:

		if (!GP)
			goto __RETURN_VOID;

		val = &BP[FP->n_local];
		GP++;

		for (ind = 0; ind < FP->n_ctrl; ind++)
		{
			RELEASE(&val[ind]);
			val[ind] = GP[ind];
		}

		GP--;

		SP = GP;
		PC = (PCODE *)GP->_void.value[0] + 2;
		GP = (VALUE *)GP->_void.value[1];

		goto _MAIN;

	__RETURN_VALUE:

		VALUE_conv(&SP[-1], FP->type);
		SP--;
		*RP = *SP;

		goto __RETURN_LEAVE;

	__RETURN_VOID:

		VALUE_default(RP, FP->type);

	__RETURN_LEAVE:

		EXEC_leave_keep();

		if (!PC)
			return;

		goto _NEXT;

	__INIT_BYTECODE_CHECK:

		_sb_jump_table = jump_table;
		_sb_jump_table_3_18_AXXX = jump_table_3_18_AXXX;
		_sb_jump_table_3_18_FXXX = jump_table_3_18_FXXX;
		return;
	}

/*-----------------------------------------------*/

_CALL:

	{
		static const void *call_jump[] =
		{
			&&__CALL_NULL, &&__CALL_NATIVE, &&__CALL_PRIVATE, &&__CALL_PUBLIC,
			&&__CALL_EVENT, &&__CALL_EXTERN, &&__CALL_UNKNOWN, &&__CALL_CALL,
			&&__CALL_SUBR
		};

		ind = GET_3X();
		val = &SP[-(ind + 1)];

		if (!TYPE_is_function(val->type))
		{
			bool defined = EXEC_object(val, &EXEC.class, (OBJECT **)&EXEC.object);

			val->type = T_FUNCTION;
			val->_function.kind = FUNCTION_CALL;
			val->_function.defined = defined;
			val->_function.class = EXEC.class;
			val->_function.object = EXEC.object;
			//goto _CALL;
		}
		else
		{
			EXEC.class = val->_function.class;
			EXEC.object = val->_function.object;
		}

		EXEC.nparam = ind;
		EXEC.use_stack = TRUE;

		if (!val->_function.defined)
			*PC |= CODE_CALL_VARIANT;

		goto *call_jump[(int)val->_function.kind];

	__CALL_NULL:

		while (ind > 0)
		{
			POP();
			ind--;
		}

		POP();

		//if (!PCODE_is_void(code))
		{
			/*VALUE_default(SP, (TYPE)(val->_function.function));*/
			VALUE_null(SP);
			SP++;
		}

		goto _NEXT;

	__CALL_NATIVE:

		EXEC.native = TRUE;
		EXEC.index = val->_function.index;
		EXEC.desc = &EXEC.class->table[EXEC.index].desc->method;
		//EXEC.use_stack = TRUE;

		goto __EXEC_NATIVE;

	__CALL_PRIVATE:

		EXEC.native = FALSE;
		EXEC.index = val->_function.index;
		EXEC.func = &EXEC.class->load->func[EXEC.index];

		goto __EXEC_ENTER;

	__CALL_PUBLIC:

		EXEC.native = FALSE;
		EXEC.desc = &EXEC.class->table[val->_function.index].desc->method;
		EXEC.index = (int)(intptr_t)(EXEC.desc->exec);
		EXEC.class = EXEC.desc->class;
		EXEC.func = &EXEC.class->load->func[EXEC.index];

		goto __EXEC_ENTER;

	__EXEC_ENTER:

		if (EXEC.func->fast && !JIT_exec(TRUE))
		{
			goto _NEXT;
		}
		else
		{
			EXEC_enter_check(val->_function.defined);
			goto _CHECK_BYTECODE;
		}

	__EXEC_NATIVE:

		EXEC_native_check(val->_function.defined);
		goto _NEXT;

	__CALL_EVENT:

		//if (OP && !strcmp(OBJECT_class(OP)->name, "Workspace"))
		//	BREAKPOINT();
		ind = GB_Raise(OP, val->_function.index, (-EXEC.nparam));

		POP(); // function

		//if (!PCODE_is_void(code))
		{
			SP->type = T_BOOLEAN;
			SP->_boolean.value = ind ? -1 : 0;
			SP++;
		}

		//EVENT_Last = old_last;

		goto _NEXT;

	__CALL_UNKNOWN:

		EXEC_unknown_name = CP->load->unknown[val->_function.index];
		EXEC.desc = CLASS_get_special_desc(EXEC.class, SPEC_UNKNOWN);
		//EXEC.use_stack = TRUE;
		goto __CALL_SPEC;

	__CALL_CALL:

		EXEC.desc = CLASS_get_special_desc(EXEC.class, SPEC_CALL);

		if (EXEC.desc)
		{
			if (!CLASS_DESC_is_static_method(EXEC.desc) && !EXEC.object)
			{
				if (!EXEC.class->auto_create)
					THROW(E_DYNAMIC, CLASS_get_name(EXEC.class), $("_call"));

				EXEC.object = EXEC_auto_create(EXEC.class, FALSE);
				EXEC.nparam = ind;
			}

			goto __CALL_SPEC;
		}

		if (!EXEC.object && EXEC.nparam == 1 && !EXEC.class->is_virtual)
		{
			SP[-2] = SP[-1];
			SP--;
			VALUE_conv_object(SP - 1, (TYPE)EXEC.class);
			goto _NEXT;
		}

	__CALL_SPEC:

		if (!EXEC.desc)
			THROW(E_NFUNC);

		EXEC.native = FUNCTION_is_native(EXEC.desc);

		if (EXEC.native)
		{
			EXEC_native();
			goto _NEXT;
		}
		else
		{
			EXEC.index = (int)(intptr_t)(EXEC.desc->exec);
			EXEC.class = EXEC.desc->class;
			EXEC.func = &EXEC.class->load->func[EXEC.index];

			if (EXEC.func->fast && !JIT_exec(TRUE))
			{
				goto _NEXT;
			}
			else
			{
				EXEC_enter();
				goto _CHECK_BYTECODE;
			}
		}

	__CALL_EXTERN:

		EXEC.index = val->_function.index;
		EXTERN_call();
		goto _NEXT;

	__CALL_SUBR:

		ind = GET_3X();
		((EXEC_FUNC_CODE_SP)(EXEC.class->table[val->_function.index].desc->method.exec))(ind, SP);
		SP -= ind;
		SP[-1] = SP[0];
		goto _NEXT;
	}

/*-----------------------------------------------*/

_CALL_QUICK:

	{
		static const void *call_jump[] =
		{
			&&__CALL_NULL, &&__CALL_NATIVE_Q, &&__CALL_PRIVATE_Q, &&__CALL_PUBLIC_Q,
			&&__CALL_EVENT, &&__CALL_EXTERN, &&__CALL_UNKNOWN, &&__CALL_CALL,
			&&__CALL_SUBR
		};

		ind = GET_3X();
		val = &SP[-(ind + 1)];

		EXEC.class = val->_function.class;
		EXEC.object = val->_function.object;
		EXEC.nparam = ind;

		if (!val->_function.defined)
			*PC |= CODE_CALL_VARIANT;

		//if (call_jump[(int)val->_function.kind] == 0)
		//  fprintf(stderr, "val->_function.kind = %d ?\n", val->_function.kind);

		goto *call_jump[(int)val->_function.kind];

	__CALL_PRIVATE_Q:

		EXEC.native = FALSE;
		EXEC.index = val->_function.index;

		goto __EXEC_ENTER_Q;

	__CALL_PUBLIC_Q:

		EXEC.native = FALSE;
		EXEC.desc = &EXEC.class->table[val->_function.index].desc->method;
		EXEC.index = (int)(intptr_t)(EXEC.desc->exec);
		EXEC.class = EXEC.desc->class;

	__EXEC_ENTER_Q:

		EXEC_enter_quick();
		goto _CHECK_BYTECODE;

	__CALL_NATIVE_Q:

		EXEC.native = TRUE;
		EXEC.index = val->_function.index;
		EXEC.desc = &EXEC.class->table[EXEC.index].desc->method;

		EXEC_native_quick();
		goto _NEXT;
	}

/*-----------------------------------------------*/

#if 0
_CALL_EASY:
{
	static const void *call_jump[] =
		{ &&__CALL_NULL, &&__CALL_NATIVE_E, &&__CALL_PRIVATE_E, &&__CALL_PUBLIC_E };

	VALUE * NO_WARNING(val);

	ind = GET_3X();
	val = &SP[-(ind + 1)];

	EXEC.class = val->_function.class;
	EXEC.object = val->_function.object;
	EXEC.nparam = ind;

	if (!val->_function.defined)
		*PC |= CODE_CALL_VARIANT;

	//if (call_jump[(int)val->_function.kind] == 0)
	//  fprintf(stderr, "val->_function.kind = %d ?\n", val->_function.kind);

	goto *call_jump[(int)val->_function.kind];

__CALL_PRIVATE_E:

	EXEC.native = FALSE;
	EXEC.index = val->_function.index;

	goto __EXEC_ENTER_E;

__CALL_PUBLIC_E:

	EXEC.native = FALSE;
	EXEC.desc = &EXEC.class->table[val->_function.index].desc->method;
	EXEC.index = (int)(intptr_t)(EXEC.desc->exec);
	EXEC.class = EXEC.desc->class;

__EXEC_ENTER_E:

	EXEC_enter_easy();
	goto _MAIN;

__CALL_NATIVE_E:

	EXEC.native = TRUE;
	EXEC.index = val->_function.index;
	EXEC.desc = &EXEC.class->table[EXEC.index].desc->method;

	EXEC_native_easy();
	goto _NEXT;
}
#endif

/*-----------------------------------------------*/

_CALL_SLOW:

	{
		static const void *call_jump[] =
		{
			&&__CALL_NULL, &&__CALL_NATIVE_S, &&__CALL_PRIVATE_S, &&__CALL_PUBLIC_S,
			&&__CALL_EVENT, &&__CALL_EXTERN, &&__CALL_UNKNOWN, &&__CALL_CALL,
			&&__CALL_SUBR
		};

		ind = GET_3X();
		val = &SP[-(ind + 1)];

		EXEC.class = val->_function.class;
		EXEC.object = val->_function.object;
		EXEC.nparam = ind;
		EXEC.use_stack = TRUE;

		if (!val->_function.defined)
			*PC |= CODE_CALL_VARIANT;

		goto *call_jump[(int)val->_function.kind];

	__CALL_PRIVATE_S:

		EXEC.native = FALSE;
		EXEC.index = val->_function.index;

		goto __EXEC_ENTER_S;

	__CALL_PUBLIC_S:

		EXEC.native = FALSE;
		EXEC.desc = &EXEC.class->table[val->_function.index].desc->method;
		EXEC.index = (int)(intptr_t)(EXEC.desc->exec);
		EXEC.class = EXEC.desc->class;

	__EXEC_ENTER_S:

		EXEC.func = &EXEC.class->load->func[EXEC.index];

		if (EXEC.func->fast && !JIT_exec(TRUE))
		{
			goto _NEXT;
		}
		else
		{
			EXEC_enter();
			goto _CHECK_BYTECODE;
		}

	__CALL_NATIVE_S:

		EXEC.native = TRUE;
		EXEC.index = val->_function.index;
		EXEC.desc = &EXEC.class->table[EXEC.index].desc->method;

		EXEC_native();
		goto _NEXT;
	}

/*-----------------------------------------------*/

_JUMP_FIRST:

	{
		static const void *const jn_jump[] =
			{
				NULL, &&_JN_INTEGER_INC, &&_JN_BYTE, &&_JN_SHORT, &&_JN_INTEGER_DEC, &&_JN_LONG, &&_JN_SINGLE, &&_JN_FLOAT
			};

		VALUE * NO_WARNING(inc);
		VALUE * NO_WARNING(end);
		TYPE type;

		ind = GET_XX();

		end = &BP[ind];
		inc = &BP[ind + 1];
		val = &BP[PC[3] & 0xFF];

		type = val->type;

		if (type < T_BYTE || type > T_FLOAT)
			THROW(E_TYPE, "Number", TYPE_get_name(type));

		if (type > T_INTEGER)
			VALUE_conv(&SP[-1], type);
		else
			VALUE_conv_integer(&SP[-1]);

		VALUE_conv(&SP[-2], type);

		_pop_ctrl(ind + 1);
		_pop_ctrl(ind);

		// loop mode is stored in the inc type. It must be strictly lower than T_STRING

		if (type == T_INTEGER && inc->_integer.value == 1 && !CP->not_3_18)
		{
			PC++;
			*PC = C_JUMP_NEXT_INTEGER | ind;
			goto _JN_INTEGER_TEST_INC;
		}
		else
		{
			if (type == T_INTEGER && inc->_integer.value > 0)
				type = 1;

			inc->type = type;

			PC++;
			*PC |= ind;

			if (type <= T_INTEGER)
			{
				if (inc->_integer.value < 0)
					goto _JN_INTEGER_TEST_DEC;
				else
					goto _JN_INTEGER_TEST_INC;
			}
			else if (type == T_LONG)
				goto _JN_LONG_TEST;
			else if (type == T_SINGLE)
				goto _JN_SINGLE_TEST;
			else //if (type == T_FLOAT)
				goto _JN_FLOAT_TEST;
		}
		
/*-----------------------------------------------*/

_JUMP_NEXT_INTEGER:

		end = &BP[GET_XX()];
		val = &BP[PC[2] & 0xFF];

		val->_integer.value++;

		if (val->_integer.value <= end->_integer.value)
			PC += 3;
		else
			PC += (signed short)PC[1] + 2;

		goto _MAIN;

/*-----------------------------------------------*/

_JUMP_NEXT:

		end = &BP[GET_XX()];
		inc = end + 1;
		val = &BP[PC[2] & 0xFF];

		goto *jn_jump[inc->type];

	_JN_BYTE:
		val->_integer.value = (unsigned char)(val->_integer.value + inc->_integer.value);
		if (inc->_integer.value < 0)
			goto _JN_INTEGER_TEST_DEC;
		else
			goto _JN_INTEGER_TEST_INC;

	_JN_SHORT:
		val->_integer.value = (short)(val->_integer.value + inc->_integer.value);
		if (inc->_integer.value < 0)
			goto _JN_INTEGER_TEST_DEC;
		else
			goto _JN_INTEGER_TEST_INC;

	_JN_INTEGER_INC:
		val->_integer.value += inc->_integer.value;

	_JN_INTEGER_TEST_INC:
		if (val->_integer.value <= end->_integer.value)
		{
			PC += 3;
			goto _MAIN;
		}
		else
			goto _JN_END;

	_JN_INTEGER_DEC:
		val->_integer.value += inc->_integer.value;

	_JN_INTEGER_TEST_DEC:
		if (val->_integer.value >= end->_integer.value)
		{
			PC += 3;
			goto _MAIN;
		}
		else
			goto _JN_END;

	_JN_LONG:
		val->_long.value += inc->_long.value;

	_JN_LONG_TEST:
		if ((inc->_long.value > 0 && val->_long.value <= end->_long.value)
			  || (inc->_long.value < 0 && val->_long.value >= end->_long.value))
		{
			PC += 3;
			goto _MAIN;
		}
		else
			goto _JN_END;

	_JN_SINGLE:
		val->_single.value += inc->_single.value;

	_JN_SINGLE_TEST:
		if ((inc->_single.value > 0 && val->_single.value <= end->_single.value)
			  || (inc->_single.value < 0 && val->_single.value >= end->_single.value))
		{
			PC += 3;
			goto _MAIN;
		}
		else
			goto _JN_END;

	_JN_FLOAT:
		val->_float.value += inc->_float.value;

	_JN_FLOAT_TEST:
		if ((inc->_float.value > 0 && val->_float.value <= end->_float.value)
			  || (inc->_float.value < 0 && val->_float.value >= end->_float.value))
		{
			PC += 3;
			goto _MAIN;
		}
		else
			goto _JN_END;

	_JN_END:
		PC += (signed short)PC[1] + 2;
 		goto _MAIN;
	}

/*-----------------------------------------------*/

_ENUM_FIRST:

	ind = GET_XX();
	_pop_ctrl(ind);
	EXEC_enum_first(code, &BP[ind], &BP[ind + 1]);
	goto _NEXT;

/*-----------------------------------------------*/

_ENUM_NEXT:

	ind = PC[-1] & 0xFF;
	if (EXEC_enum_next(code, &BP[ind], &BP[ind + 1]))
		goto _JUMP;
	else
	{
		PC += 2;
		goto _MAIN;
	}

/*-----------------------------------------------*/

_PUSH_CLASS:

	SP->type = T_CLASS;
	SP->_class.class = CP->load->class_ref[GET_7XX()];;
	SP++;
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_FUNCTION:

	/*ind = GET_7XX();*/

	SP->type = T_FUNCTION;
	SP->_function.class = CP;
	SP->_function.object = OP;
	SP->_function.kind = FUNCTION_PRIVATE;
	SP->_function.index = GET_7XX();
	SP->_function.defined = TRUE;

	OBJECT_REF_CHECK(OP);
	SP++;

	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_EXTERN:

	/*ind = GET_7XX();*/

	SP->type = T_FUNCTION;
	SP->_function.class = CP;
	SP->_function.object = NULL;
	SP->_function.kind = FUNCTION_EXTERN;
	SP->_function.index = GET_UX();
	SP->_function.defined = TRUE;

	//OBJECT_REF(OP, "exec_loop._PUSH_FUNCTION (FUNCTION)");
	SP++;

	goto _NEXT;

/*-----------------------------------------------*/

	{
		CLASS_VAR *NO_WARNING(var);
		char *NO_WARNING(addr);
		void *NO_WARNING(ref);

_PUSH_DYNAMIC:

		var = &CP->load->dyn[GET_7XX()];

		//if (OP == NULL)
		//  THROW_ILLEGAL();

		ref = OP;
		addr = &OP[var->pos];
		goto __READ;

_PUSH_STATIC:

		var = &CP->load->stat[GET_7XX()];
		addr = (char *)CP->stat + var->pos;
		ref = CP;
		goto __READ;

__READ:

		my_VALUE_class_read(CP, SP, addr, var->type, ref, PDS);
		SP++;
		goto _NEXT;


_POP_DYNAMIC:

		var = &CP->load->dyn[GET_7XX()];

		//if (OP == NULL)
		//  THROW_ILLEGAL();

		addr = &OP[var->pos];
		goto __WRITE;

_POP_STATIC:

		var = &CP->load->stat[GET_7XX()];
		addr = (char *)CP->stat + var->pos;
		goto __WRITE;

__WRITE:

		VALUE_class_write(CP, &SP[-1], addr, var->type);
		POP();

		goto _NEXT;

	}

/*-----------------------------------------------*/

_PUSH_CONST_EX:

	PC++;
	ind = *PC;
	goto _PUSH_CONSTANT;

/*-----------------------------------------------*/

_PUSH_CONST:

	ind = GET_UXX();

_PUSH_CONSTANT:

	VALUE_class_constant_inline(CP, SP, ind);
	SP++;
	goto _NEXT;

/*-----------------------------------------------*/

_PUSH_QUICK:

	SP->type = T_INTEGER;
	SP->_integer.value = GET_XXX();
	SP++;
	goto _NEXT;

/*-----------------------------------------------*/

_ADD_QUICK:

	{
		static void *_aq_jump[] = {
			&&__AQ_VOID, &&__AQ_BOOLEAN, &&__AQ_BYTE, &&__AQ_SHORT, &&__AQ_INTEGER, &&__AQ_LONG, &&__AQ_SINGLE, &&__AQ_FLOAT,
			&&__AQ_DATE, &&__AQ_STRING, &&__AQ_STRING, &&__AQ_POINTER, &&__AQ_VARIANT, &&__AQ_BOOLEAN, &&__AQ_BOOLEAN, &&__AQ_BOOLEAN
			};

		TYPE NO_WARNING(type);
		void * NO_WARNING(jump_end);

		val = SP - 1;

		jump_end = &&_NEXT;
		type = val->type;
		ind = GET_XXX();

	__AQ_JUMP:

		if (TYPE_is_object(type))
			goto __AQ_OBJECT;
		else
			goto *_aq_jump[type];

	__AQ_VOID:

		THROW(E_NRETURN);

	__AQ_BYTE:

		val->_integer.value = (unsigned char)(val->_integer.value + ind);
		goto *jump_end;

	__AQ_SHORT:

		val->_integer.value = (short)(val->_integer.value + ind);
		goto *jump_end;

	__AQ_INTEGER:

		val->_integer.value += ind;
		goto *jump_end;

	__AQ_LONG:

		val->_long.value += (int64_t)ind;
		goto *jump_end;

	__AQ_SINGLE:

		val->_single.value += (float)ind;
		goto *jump_end;

	__AQ_DATE:
	__AQ_STRING:

		VALUE_conv_float(val);

	__AQ_FLOAT:

		val->_float.value += (double)ind;
		goto *jump_end;

	__AQ_POINTER:

		val->_pointer.value += ind;
		goto *jump_end;

	__AQ_VARIANT:

		jump_end = &&__AQ_VARIANT_END;
		VARIANT_undo(val);
		type = val->type;
		goto __AQ_JUMP;

	__AQ_OBJECT:

		if (EXEC_check_operator_single(val, CO_ADDF))
		{
			EXEC_operator_object_add_quick(val, ind);
			goto *jump_end;
		}

	__AQ_BOOLEAN:

		THROW(E_TYPE, "Number", TYPE_get_name(type));

	__AQ_VARIANT_END:

		VALUE_conv_variant(val);
		goto _NEXT;
	}

/*-----------------------------------------------*/

_PUSH_ARRAY_NATIVE_INTEGER:

	{
		CARRAY *array;

		val = &SP[-2];
		array = (CARRAY *)val->_object.object;

		if (!array)
			THROW_NULL();

		//VALUE_conv_integer(&SP[-1]);
		ind = val[1]._integer.value;

		if (ind < 0 || ind >= array->count)
			THROW_BOUND();

		val->_integer.value = ((int *)(array->data))[ind];
		val->type = GB_T_INTEGER;

		OBJECT_UNREF(array);
		SP--;
		goto _NEXT;
	}

_PUSH_ARRAY_NATIVE_FLOAT:

	{
		CARRAY *array;

		val = &SP[-2];
		array = (CARRAY *)val->_object.object;

		if (!array)
			THROW_NULL();

		//VALUE_conv_integer(&SP[-1]);
		ind = val[1]._integer.value;

		if (ind < 0 || ind >= array->count)
			THROW_BOUND();

		val->_float.value = ((double *)(array->data))[ind];
		val->type = GB_T_FLOAT;

		OBJECT_UNREF(array);
		SP--;
		goto _NEXT;
	}

_PUSH_ARRAY_NATIVE_COLLECTION:

#if 0
	{
		GB_COLLECTION col;

		val = &SP[-2];
		col = (GB_COLLECTION)val->_object.object;

		if (!col)
			THROW_NULL();

		VALUE_conv_string(&val[1]);
		GB_CollectionGet(col, val[1]._string.addr + val[1]._string.start, val[1]._string.len, (GB_VARIANT *)&SP[-2]);

		RELEASE_STRING(&val[1]);
		SP--;
		BORROW(&SP[-1]);
		OBJECT_UNREF(col);
		goto _NEXT;
	}
#endif

/*-----------------------------------------------*/

_POP_ARRAY_NATIVE_INTEGER:

	{
		CARRAY *array;

		val = &SP[-2];
		array = (CARRAY *)val->_object.object;

		if (!array)
			THROW_NULL();

		CARRAY_check_not_read_only(array);

		ind = val[1]._integer.value;
		if (ind < 0 || ind >= array->count)
			THROW_BOUND();

		((int *)(array->data))[ind] = val[-1]._integer.value;

		OBJECT_UNREF(array);
		SP -= 3;
		goto _NEXT;
	}

_POP_ARRAY_NATIVE_FLOAT:

	{
		CARRAY *array;

		val = &SP[-2];
		array = (CARRAY *)val->_object.object;

		if (!array)
			THROW_NULL();

		CARRAY_check_not_read_only(array);

		//VALUE_conv_float(&SP[-3]);
		//VALUE_conv_integer(&SP[-1]);

		ind = val[1]._integer.value;
		if (ind < 0 || ind >= array->count)
			THROW_BOUND();

		((double *)(array->data))[ind] = SP[-3]._float.value;

		OBJECT_UNREF(array);
		SP -= 3;
		goto _NEXT;
	}

_POP_ARRAY_NATIVE_COLLECTION:

#if 0
	{
		GB_COLLECTION col;

		val = &SP[-2];
		col = (GB_COLLECTION)val->_object.object;
		if (!col)
			THROW_NULL();

		VALUE_conv_variant(&val[-1]);
		VALUE_conv_string(&val[1]);

		if (GB_CollectionSet((GB_COLLECTION)col, val[1]._string.addr + val[1]._string.start, val[1]._string.len, (GB_VARIANT *)&SP[-3]))
			PROPAGATE();

		RELEASE_STRING(&val[1]);
		OBJECT_UNREF(col);
		RELEASE_VARIANT(&val[-1]);
		SP -= 3;
		goto _NEXT;
	}
#endif

/*-----------------------------------------------*/

_ADD_INTEGER:

	SP--;
	SP[-1]._integer.value += SP->_integer.value;
	goto _NEXT;

_ADD_FLOAT:

	SP--;
	SP[-1]._float.value += SP->_float.value;
	goto _NEXT;

_SUB_INTEGER:

	SP--;
	SP[-1]._integer.value -= SP->_integer.value;
	goto _NEXT;

_SUB_FLOAT:

	SP--;
	SP[-1]._float.value -= SP->_float.value;
	goto _NEXT;

_MUL_INTEGER:

	SP--;
	SP[-1]._integer.value *= SP->_integer.value;
	goto _NEXT;

_MUL_FLOAT:

	SP--;
	SP[-1]._float.value *= SP->_float.value;
	goto _NEXT;

_DIV_INTEGER:

	SP--;
	VALUE_conv_float(&SP[-1]);
	VALUE_conv_float(SP);
	SP[-1]._float.value /= SP->_float.value;
	if (!isfinite(SP[-1]._float.value))
		THROW(E_ZERO);
	goto _NEXT;

_DIV_FLOAT:

	SP--;
	SP[-1]._float.value /= SP->_float.value;
	if (!isfinite(SP[-1]._float.value))
		THROW(E_ZERO);
	goto _NEXT;

/*-----------------------------------------------*/

_TRY:

	EP = SP;
	ET = EC;
	EC = PC + (signed short)PC[1] + 2;

	#if DEBUG_ERROR
	fprintf(stderr, "exec TRY %p\n", EC);
	#endif

	PC += 2;
	goto _MAIN;

/*-----------------------------------------------*/

_END_TRY:

	#if DEBUG_ERROR
	fprintf(stderr, "exec END TRY %p\n", PC);
	#endif

	// If EP was reset to null, then an error occurred
	EXEC_got_error = (EP == NULL);
	EP = NULL;
	EC = ET;
	ET = NULL;
	goto _NEXT;

/*-----------------------------------------------*/

_CATCH:

	if (EC == NULL)
		goto _NEXT;
	else
		goto __RETURN_VOID;

/*-----------------------------------------------*/

_BREAK:

	if (!EXEC_trace && !EXEC_debug)
		*PC = C_NOP;
	else
		_break(code);
	goto _NEXT;

/*-----------------------------------------------*/

_QUIT:

	EXEC_quit(code);
	goto _NEXT;

/*-----------------------------------------------*/

_BYREF:

	if (PC == FP->code)
	{
		PC += GET_UX() + 2;
		goto _MAIN;
	}

	THROW(E_BYREF);

/*-----------------------------------------------*/

_SUBR_COMPN:

	_SUBR_compe(code);
	goto _NEXT;

_SUBR_COMPE:
	
	_SUBR_compe(code);
	goto _NEXT;

#if 0
	{
		static void *jump[] = {
			&&__SC_VARIANT, &&__SC_BOOLEAN, &&__SC_BYTE, &&__SC_SHORT, &&__SC_INTEGER, &&__SC_LONG, &&__SC_SINGLE, &&__SC_FLOAT,
			&&__SC_DATE, &&__SC_STRING, &&__SC_STRING, &&__SC_POINTER, &&__SC_ERROR, &&__SC_ERROR, &&__SC_ERROR, &&__SC_NULL,
			&&__SC_OBJECT, &&__SC_OBJECT_FLOAT, &&__SC_FLOAT_OBJECT, &&__SC_OBJECT_OTHER, &&__SC_OTHER_OBJECT, &&__SC_OBJECT_OBJECT, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

			NULL, &&__SC_BOOLEAN, &&__SC_BYTE, &&__SC_SHORT, &&__SC_INTEGER, &&__SC_LONG_NC, &&__SC_SINGLE_NC, &&__SC_FLOAT_NC,
			&&__SC_DATE_NC, &&__SC_STRING_NC, &&__SC_STRING_NC, &&__SC_POINTER_NC, &&__SC_ERROR, &&__SC_ERROR, &&__SC_ERROR, &&__SC_NULL,
			&&__SC_OBJECT
			};

		char NO_WARNING(result);
		VALUE *NO_WARNING(P1);
		VALUE *NO_WARNING(P2);

_SUBR_COMPN:

		result = 1;
		goto _SUBR_COMP;

_SUBR_COMPE:

		result = 0;

_SUBR_COMP:

		P1 = SP - 2;
		P2 = SP - 1;

		goto *jump[code & 0x3F];

	__SC_BOOLEAN:
	__SC_BYTE:
	__SC_SHORT:
	__SC_INTEGER:

		result ^= P1->_integer.value == P2->_integer.value;
		goto __SC_END;

	__SC_LONG:

		VALUE_conv(P1, T_LONG);
		VALUE_conv(P2, T_LONG);

	__SC_LONG_NC:

		result ^= P1->_long.value == P2->_long.value;
		goto __SC_END;

	__SC_DATE:

		VALUE_conv(P1, T_DATE);
		VALUE_conv(P2, T_DATE);

	__SC_DATE_NC:

		result ^= DATE_comp_value(P1, P2) == 0;
		goto __SC_END;

	__SC_NULL:

		if (P2->type == T_NULL)
		{
			result ^= VALUE_is_null(P1);
			goto __SC_END_RELEASE;
		}
		else if (P1->type == T_NULL)
		{
			result ^= VALUE_is_null(P2);
			goto __SC_END_RELEASE;
		}

	__SC_STRING:

		VALUE_conv_string(P1);
		VALUE_conv_string(P2);

	__SC_STRING_NC:

		if (P1->_string.len == P2->_string.len)
			result ^= STRING_equal_same(P1->_string.addr + P1->_string.start, P2->_string.addr + P2->_string.start, P1->_string.len);

		RELEASE_STRING(P1);
		RELEASE_STRING(P2);
		goto __SC_END;

	__SC_SINGLE:

		VALUE_conv(P1, T_SINGLE);
		VALUE_conv(P2, T_SINGLE);

	__SC_SINGLE_NC:

		result ^= P1->_single.value == P2->_single.value;
		goto __SC_END;

	__SC_FLOAT:

		VALUE_conv_float(P1);
		VALUE_conv_float(P2);

	__SC_FLOAT_NC:

		result ^= P1->_float.value == P2->_float.value;
		goto __SC_END;

	__SC_POINTER:

		VALUE_conv(P1, T_POINTER);
		VALUE_conv(P2, T_POINTER);

	__SC_POINTER_NC:

		result ^= P1->_pointer.value == P2->_pointer.value;
		goto __SC_END;

	__SC_OBJECT:

		result ^= OBJECT_comp_value(P1, P2) == 0;
		//RELEASE_OBJECT(P1);
		//RELEASE_OBJECT(P2);
		goto __SC_END_RELEASE;

	__SC_OBJECT_FLOAT:

		result ^= EXEC_comparator(OP_OBJECT_FLOAT, CO_EQUALF, P1, P2);
		goto __SC_END;

	__SC_FLOAT_OBJECT:

		result ^= EXEC_comparator(OP_FLOAT_OBJECT, CO_EQUALF, P1, P2);
		goto __SC_END;

	__SC_OBJECT_OTHER:

		result ^= EXEC_comparator(OP_OBJECT_OTHER, CO_EQUALO, P1, P2);
		goto __SC_END;

	__SC_OTHER_OBJECT:

		result ^= EXEC_comparator(OP_OTHER_OBJECT, CO_EQUALO, P1, P2);
		goto __SC_END;

	__SC_OBJECT_OBJECT:

		result ^= EXEC_comparator(OP_OBJECT_OBJECT, CO_EQUAL, P1, P2);
		goto __SC_END;

	__SC_VARIANT:

		{
			bool variant = FALSE;
			TYPE type;

			if (TYPE_is_variant(P1->type))
			{
				VARIANT_undo(P1);
				variant = TRUE;
			}

			if (TYPE_is_variant(P2->type))
			{
				VARIANT_undo(P2);
				variant = TRUE;
			}

			code = EXEC_check_operator(P1, P2, CO_EQUAL);
			if (code)
			{
				code += T_OBJECT;
				if (!(variant || P1->type == T_OBJECT || P2->type == T_OBJECT))
					*PC |= code;
				goto *jump[code];
			}

			type = Max(P1->type, P2->type);

			if (TYPE_is_object_null(P1->type) && TYPE_is_object_null(P2->type))
				type = T_OBJECT;
			else if (TYPE_is_object(type))
				THROW(E_TYPE, "Object", TYPE_get_name(Min(P1->type, P2->type)));
			else if (TYPE_is_void(type))
				THROW(E_NRETURN);

			if (!variant)
			{
				if (P1->type == P2->type)
					*PC |= 0x20;
				*PC |= type;
			}

			goto *jump[type];
		}

	__SC_ERROR:

		THROW(E_TYPE, "Number, Date or String", TYPE_get_name(code & 0x1F));

	__SC_END_RELEASE:

		RELEASE(P1);
		RELEASE(P2);

	__SC_END:

		P1->type = T_BOOLEAN;
		P1->_boolean.value = -result;
		SP--;
		goto _NEXT;
	}
#endif

/*-----------------------------------------------*/

	{
		char NO_WARNING(result);
		VALUE *NO_WARNING(P1);
		VALUE *NO_WARNING(P2);

		static void *jump[] = {
			&&__SCI_VARIANT, &&__SCI_BOOLEAN, &&__SCI_BYTE, &&__SCI_SHORT, &&__SCI_INTEGER, &&__SCI_LONG, &&__SCI_SINGLE, &&__SCI_FLOAT,
			&&__SCI_DATE, &&__SCI_STRING, &&__SCI_STRING, &&__SCI_POINTER, &&__SCI_ERROR, &&__SCI_ERROR, &&__SCI_ERROR, &&__SCI_NULL,
			&&__SCI_OBJECT, &&__SCI_OBJECT_FLOAT, &&__SCI_FLOAT_OBJECT, &&__SCI_OBJECT_OTHER, &&__SCI_OTHER_OBJECT, &&__SCI_OBJECT_OBJECT, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

			NULL, &&__SCI_BOOLEAN, &&__SCI_BYTE, &&__SCI_SHORT, &&__SCI_INTEGER, &&__SCI_LONG_NC, &&__SCI_SINGLE_NC, &&__SCI_FLOAT_NC,
			&&__SCI_DATE_NC, &&__SCI_STRING_NC, &&__SCI_STRING_NC, &&__SCI_POINTER_NC, &&__SCI_ERROR, &&__SCI_ERROR, &&__SCI_ERROR, &&__SCI_NULL,
			&&__SCI_OBJECT
			};

_SUBR_COMPGT:

		P1 = SP - 2;
		P2 = SP - 1;
		result = 0;
		goto _SUBR_COMPI;

_SUBR_COMPLT:

		P1 = SP - 1;
		P2 = SP - 2;
		result = 0;
		goto _SUBR_COMPI;

_SUBR_COMPLE:

		P1 = SP - 2;
		P2 = SP - 1;
		result = 1;
		goto _SUBR_COMPI;

_SUBR_COMPGE:

		P1 = SP - 1;
		P2 = SP - 2;
		result = 1;
		goto _SUBR_COMPI;

_SUBR_COMPI:

		goto *jump[code & 0x3F];

	__SCI_BOOLEAN:
	__SCI_BYTE:
	__SCI_SHORT:
	__SCI_INTEGER:

		result ^= P1->_integer.value > P2->_integer.value;
		goto __SCI_END;

	__SCI_LONG:

		VALUE_conv(P1, T_LONG);
		VALUE_conv(P2, T_LONG);

	__SCI_LONG_NC:

		result ^= P1->_long.value > P2->_long.value;
		goto __SCI_END;

	__SCI_DATE:

		VALUE_conv(P1, T_DATE);
		VALUE_conv(P2, T_DATE);

	__SCI_DATE_NC:

		result ^= DATE_comp_value(P1, P2) > 0;
		goto __SCI_END;

	__SCI_NULL:
	__SCI_STRING:

		VALUE_conv_string(P1);
		VALUE_conv_string(P2);

	__SCI_STRING_NC:

		result ^= STRING_compare(P1->_string.addr + P1->_string.start, P1->_string.len, P2->_string.addr + P2->_string.start, P2->_string.len) > 0;

		RELEASE_STRING(P1);
		RELEASE_STRING(P2);
		goto __SCI_END;

	__SCI_SINGLE:

		VALUE_conv(P1, T_SINGLE);
		VALUE_conv(P2, T_SINGLE);

	__SCI_SINGLE_NC:

		result ^= P1->_single.value > P2->_single.value;
		goto __SCI_END;

	__SCI_FLOAT:

		VALUE_conv_float(P1);
		VALUE_conv_float(P2);

	__SCI_FLOAT_NC:

		result ^= P1->_float.value > P2->_float.value;
		goto __SCI_END;

	__SCI_POINTER:

		VALUE_conv(P1, T_POINTER);
		VALUE_conv(P2, T_POINTER);

	__SCI_POINTER_NC:

		result ^= P1->_pointer.value > P2->_pointer.value;
		goto __SCI_END;

	__SCI_OBJECT:

		result ^= OBJECT_comp_value(P1, P2) > 0;
		//RELEASE_OBJECT(P1);
		//RELEASE_OBJECT(P2);
		goto __SCI_END_RELEASE;

	__SCI_OBJECT_FLOAT:

		result ^= EXEC_comparator(OP_OBJECT_FLOAT, CO_COMPF, P1, P2) > 0;
		goto __SCI_END;

	__SCI_FLOAT_OBJECT:

		result ^= EXEC_comparator(OP_FLOAT_OBJECT, CO_COMPF, P1, P2) > 0;
		goto __SCI_END;

	__SCI_OBJECT_OTHER:

		result ^= EXEC_comparator(OP_OBJECT_OTHER, CO_COMPO, P1, P2) > 0;
		goto __SCI_END;

	__SCI_OTHER_OBJECT:

		result ^= EXEC_comparator(OP_OTHER_OBJECT, CO_COMPO, P1, P2) > 0;
		goto __SCI_END;

	__SCI_OBJECT_OBJECT:

		result ^= EXEC_comparator(OP_OBJECT_OBJECT, CO_COMP, P1, P2) > 0;
		goto __SCI_END;

	__SCI_VARIANT:

		{
			bool variant = FALSE;
			int op;
			TYPE type;

			if (TYPE_is_variant(P1->type))
			{
				VARIANT_undo(P1);
				variant = TRUE;
			}

			if (TYPE_is_variant(P2->type))
			{
				VARIANT_undo(P2);
				variant = TRUE;
			}

			op = EXEC_check_operator(P1, P2, CO_COMP);
			if (op)
			{
				op += T_OBJECT;
				if (!(variant || P1->type == T_OBJECT || P2->type == T_OBJECT))
					*PC |= op;
				goto *jump[op];
			}

			type = Max(P1->type, P2->type);

			if (type == T_NULL || TYPE_is_string(type))
			{
				TYPE typem = Min(P1->type, P2->type);
				if (!TYPE_is_string(typem))
					THROW_TYPE(typem, type);
			}
			else if (TYPE_is_object(type))
				THROW(E_TYPE, "Number, Date or String", TYPE_get_name(type));
			else if (TYPE_is_void(type))
				THROW(E_NRETURN);

			if (!variant)
			{
				if (P1->type == P2->type)
					*PC |= 0x20;
				*PC |= type;
			}

			goto *jump[type];
		}

	__SCI_ERROR:

		THROW(E_TYPE, "Number, Date or String", TYPE_get_name(code & 0x1F));

	__SCI_END_RELEASE:

		RELEASE(P1);
		RELEASE(P2);

	__SCI_END:

		SP -= 2;
		SP->type = T_BOOLEAN;
		SP->_boolean.value = -result;
		SP++;
		goto _NEXT;
	}


/*-----------------------------------------------*/

_PUSH_VARIABLE:

	{
		void *NO_WARNING(object);
		CLASS_DESC *NO_WARNING(desc);

		val = &SP[-1];
		object = val->_object.object;
		if (!object)
			THROW_NULL();
		desc = val->_object.class->table[PC[1]].desc;
		my_VALUE_class_read(desc->variable.class, val, (char *)object + desc->variable.offset, desc->variable.ctype, object, PV);
		//BORROW(&SP[-1]);
		OBJECT_UNREF(object);
	}
	goto _NEXT;

_POP_VARIABLE:

	{
		void *object = SP[-1]._object.object;
		if (!object)
			THROW_NULL();
		CLASS_DESC *desc = SP[-1]._object.class->table[PC[1]].desc;
		VALUE_write(&SP[-2], (char *)object + desc->variable.offset, desc->variable.type);
		RELEASE(&SP[-2]);
		OBJECT_UNREF(object);
		SP -= 2;
	}
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_CONV:

  VALUE_conv(SP - 1, code & 0x3F);
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_LEFT:

	SUBR_left(code);
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_RIGHT:

	SUBR_right(code);
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_MID:

	SUBR_mid(code);
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_LEN:

	SUBR_len();
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_ADD:

	_SUBR_add(code);
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_SUB:

	_SUBR_sub(code);
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_MUL:

	_SUBR_mul(code);
	goto _NEXT;

/*-----------------------------------------------*/

_SUBR_DIV:

	_SUBR_div(code);
	goto _NEXT;

}



#if 0
static void _SUBR_compn(ushort code)
{
	static void *jump[] = {
		&&__VARIANT, &&__BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG, &&__SINGLE, &&__FLOAT, &&__DATE,
		&&__STRING, &&__STRING, &&__POINTER, &&__ERROR, &&__ERROR, &&__ERROR, &&__NULL, &&__OBJECT,
		&&__OBJECT_FLOAT, &&__FLOAT_OBJECT, &&__OBJECT_OTHER, &&__OTHER_OBJECT, &&__OBJECT_OBJECT
		};

	//static void *test[] = { &&__EQ, &&__NE, &&__GT, &&__LE, &&__LT, &&__GE };

	char NO_WARNING(result);
	VALUE *P1;
	VALUE *P2;

	P1 = SP - 2;
	P2 = P1 + 1;

	goto *jump[code & 0x1F];

__BOOLEAN:
__BYTE:
__SHORT:
__INTEGER:

	result = P1->_integer.value == P2->_integer.value;
	goto __END;

__LONG:

	VALUE_conv(P1, T_LONG);
	VALUE_conv(P2, T_LONG);

	result = P1->_long.value == P2->_long.value;
	goto __END;

__DATE:

	VALUE_conv(P1, T_DATE);
	VALUE_conv(P2, T_DATE);

	result = DATE_comp_value(P1, P2) == 0;
	goto __END;

__NULL:

	if (P2->type == T_NULL)
	{
		result = VALUE_is_null(P1);
		goto __END_RELEASE;
	}
	else if (P1->type == T_NULL)
	{
		result = VALUE_is_null(P2);
		goto __END_RELEASE;
	}

__STRING:

	VALUE_conv_string(P1);
	VALUE_conv_string(P2);

	if (P1->_string.len != P2->_string.len)
		result = 0;
	else
		result = STRING_equal_same(P1->_string.addr + P1->_string.start, P2->_string.addr + P2->_string.start, P1->_string.len);

	RELEASE_STRING(P1);
	RELEASE_STRING(P2);
	goto __END;

__SINGLE:

	VALUE_conv(P1, T_SINGLE);
	VALUE_conv(P2, T_SINGLE);

	result = P1->_single.value == P2->_single.value;
	goto __END;

__FLOAT:

	VALUE_conv_float(P1);
	VALUE_conv_float(P2);

	result = P1->_float.value == P2->_float.value;
	goto __END;

__POINTER:

	VALUE_conv(P1, T_POINTER);
	VALUE_conv(P2, T_POINTER);

	result = P1->_pointer.value == P2->_pointer.value;
	goto __END;

__OBJECT:

	result = OBJECT_comp_value(P1, P2) == 0;
	//RELEASE_OBJECT(P1);
	//RELEASE_OBJECT(P2);
	goto __END_RELEASE;

__OBJECT_FLOAT:

	result = EXEC_comparator(OP_OBJECT_FLOAT, CO_EQUALF, P1, P2);
	goto __END;

__FLOAT_OBJECT:

	result = EXEC_comparator(OP_FLOAT_OBJECT, CO_EQUALF, P1, P2);
	goto __END;

__OBJECT_OTHER:

	result = EXEC_comparator(OP_OBJECT_OTHER, CO_EQUALO, P1, P2);
	goto __END;

__OTHER_OBJECT:

	result = EXEC_comparator(OP_OTHER_OBJECT, CO_EQUALO, P1, P2);
	goto __END;

__OBJECT_OBJECT:

	result = EXEC_comparator(OP_OBJECT_OBJECT, CO_EQUAL, P1, P2);
	goto __END;

__VARIANT:

	{
		bool variant = FALSE;
		TYPE type;

		if (TYPE_is_variant(P1->type))
		{
			VARIANT_undo(P1);
			variant = TRUE;
		}

		if (TYPE_is_variant(P2->type))
		{
			VARIANT_undo(P2);
			variant = TRUE;
		}

		code = EXEC_check_operator(P1, P2, CO_EQUAL);
		if (code)
		{
			code += T_OBJECT;
			if (!(variant || P1->type == T_OBJECT || P2->type == T_OBJECT))
				*PC |= code;
			goto *jump[code];
		}

		type = Max(P1->type, P2->type);

		if (TYPE_is_object_null(P1->type) && TYPE_is_object_null(P2->type))
			type = T_OBJECT;
		else if (TYPE_is_object(type))
			THROW(E_TYPE, "Object", TYPE_get_name(Min(P1->type, P2->type)));
		else if (TYPE_is_void(type))
			THROW(E_NRETURN);

		if (!variant)
			*PC |= type;

		goto *jump[type];
	}

__ERROR:

	THROW(E_TYPE, "Number, Date or String", TYPE_get_name(code & 0x1F));

__END_RELEASE:

	RELEASE(P1);
	RELEASE(P2);

__END:

	P1->type = T_BOOLEAN;
	SP--;

	P1->_boolean.value = result - 1; // ? 0 : -1;
}
#endif

#define MANAGE_VARIANT_OBJECT(_func, _op, _opcode) \
({ \
	type = Max(P1->type, P2->type); \
	if (TYPE_is_void(P1->type) || TYPE_is_void(P2->type)) \
		THROW(E_NRETURN); \
	\
	if (TYPE_is_number_date(type)) \
	{ \
		*PC |= type; \
		if (P1->type == P2->type) \
		{ \
			*PC |= 0x10; \
			if (!CP->not_3_18) \
			{ \
				if (type == T_INTEGER) \
					*PC = _opcode##_INTEGER; \
				else if (type == T_FLOAT) \
					*PC = _opcode##_FLOAT; \
			} \
		} \
		goto *jump[type]; \
	} \
	\
	code = EXEC_check_operator(P1, P2, _op); \
	if (code) \
	{ \
		code += T_DATE; \
		if (!(P1->type == T_OBJECT || P2->type == T_OBJECT)) \
			*PC |= code; \
		goto *jump[code]; \
	} \
	\
	VARIANT_undo(P1); \
	VARIANT_undo(P2); \
	\
	if (TYPE_is_string(P1->type)) \
		VALUE_conv_float(P1); \
	\
	if (TYPE_is_string(P2->type)) \
		VALUE_conv_float(P2); \
	\
	if (TYPE_is_null(P1->type) || TYPE_is_null(P2->type)) \
		type = T_NULL; \
	else \
		type = Max(P1->type, P2->type); \
	\
	if (TYPE_is_number_date(type)) \
	{ \
		(_func)(code | type); \
		VALUE_conv_variant(P1); \
		return; \
	} \
	\
	code = EXEC_check_operator(P1, P2, _op); \
	if (code) \
	{ \
		(_func)(code + T_DATE); \
		VALUE_conv_variant(P1); \
		return; \
	} \
})

#define MANAGE_VARIANT_POINTER_OBJECT(_func, _op, _opcode) \
({ \
	type = Max(P1->type, P2->type); \
	if (TYPE_is_void(P1->type) || TYPE_is_void(P2->type)) \
		THROW(E_NRETURN); \
	\
	if (TYPE_is_number_date(type) || TYPE_is_pointer(type)) \
	{ \
		*PC |= type; \
		if (P1->type == P2->type) \
		{ \
			*PC |= 0x10; \
			if (!CP->not_3_18) \
			{ \
				if (type == T_INTEGER) \
					*PC = _opcode##_INTEGER; \
				else if (type == T_FLOAT) \
					*PC = _opcode##_FLOAT; \
			} \
		} \
		goto *jump[type]; \
	} \
	\
	code = EXEC_check_operator(P1, P2, _op); \
	if (code) \
	{ \
		code += T_POINTER; \
		if (P1->type != T_OBJECT && P2->type != T_OBJECT) \
			*PC |= code; \
		goto *jump[code]; \
	} \
	\
	VARIANT_undo(P1); \
	VARIANT_undo(P2); \
	\
	if (TYPE_is_string(P1->type)) \
		VALUE_conv_float(P1); \
	\
	if (TYPE_is_string(P2->type)) \
		VALUE_conv_float(P2); \
	\
	if (TYPE_is_null(P1->type) || TYPE_is_null(P2->type)) \
		type = T_NULL; \
	else \
		type = Max(P1->type, P2->type); \
	\
	if (TYPE_is_number_date(type) || TYPE_is_pointer(type)) \
	{ \
		(_func)(code | type); \
		VALUE_conv_variant(P1); \
		return; \
	} \
	\
	code = EXEC_check_operator(P1, P2, _op); \
	if (code) \
	{ \
		(_func)(code + T_POINTER); \
		VALUE_conv_variant(P1); \
		return; \
	} \
})


static void _SUBR_add(ushort code)
{
	static void *jump[] = {
		&&__VARIANT, &&__BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG, &&__SINGLE, &&__FLOAT,
		&&__DATE, NULL, NULL, &&__POINTER, &&__OBJECT_FLOAT, &&__FLOAT_OBJECT, &&__OBJECT_OTHER, &&__OTHER_OBJECT,
		&&__OBJECT, && __BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG_NC, &&__SINGLE_NC, &&__FLOAT_NC,
		&&__DATE, NULL, NULL, &&__POINTER_NC
		};

	TYPE type;
	VALUE *P1, *P2;

	P1 = SP - 2;
	P2 = P1 + 1;

	type = code & 0x1F;
	goto *jump[type];

__BOOLEAN:

	P1->type = T_BOOLEAN;
	P1->_integer.value = P1->_integer.value | P2->_integer.value; SP--; return;

__BYTE:

	P1->type = T_BYTE;
	P1->_integer.value = (unsigned char)(P1->_integer.value + P2->_integer.value); SP--; return;

__SHORT:

	P1->type = T_SHORT;
	P1->_integer.value = (short)(P1->_integer.value + P2->_integer.value); SP--; return;

__INTEGER:

	P1->type = T_INTEGER;
	P1->_integer.value += P2->_integer.value; SP--; return;

__LONG:

	VALUE_conv(P1, T_LONG);
	VALUE_conv(P2, T_LONG);

__LONG_NC:

	P1->_long.value += P2->_long.value; SP--; return;

__SINGLE:

	VALUE_conv(P1, T_SINGLE);
	VALUE_conv(P2, T_SINGLE);

__SINGLE_NC:

	P1->_single.value += P2->_single.value; SP--; return;

__DATE:
__FLOAT:

	VALUE_conv_float(P1);
	VALUE_conv_float(P2);

__FLOAT_NC:

	P1->_float.value += P2->_float.value; SP--; return;

__POINTER:

	VALUE_conv(P1, T_POINTER);
	VALUE_conv(P2, T_POINTER);

__POINTER_NC:

	P1->_pointer.value += (intptr_t)P2->_pointer.value; SP--; return;

__OBJECT_FLOAT:

	EXEC_operator(OP_OBJECT_FLOAT, CO_ADDF, P1, P2); SP--; return;

__FLOAT_OBJECT:

	EXEC_operator(OP_FLOAT_OBJECT, CO_ADDF, P1, P2); SP--; return;

__OBJECT_OTHER:

	EXEC_operator(OP_OBJECT_OTHER, CO_ADDO, P1, P2); SP--; return;

__OTHER_OBJECT:

	EXEC_operator(OP_OTHER_OBJECT, CO_ADDO, P1, P2); SP--; return;

__OBJECT:

	EXEC_operator(OP_OBJECT_OBJECT, CO_ADD, P1, P2); SP--; return;

__VARIANT:

	MANAGE_VARIANT_POINTER_OBJECT(_SUBR_add, CO_ADD, C_ADD);
	goto __ERROR;

__ERROR:

	THROW(E_TYPE, "Number", TYPE_get_name(type));
}

static void _SUBR_sub(ushort code)
{
	static void *jump[] = {
		&&__VARIANT, &&__BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG, &&__SINGLE, &&__FLOAT,
		&&__DATE, NULL, NULL, &&__POINTER, &&__OBJECT_FLOAT, &&__FLOAT_OBJECT, &&__OBJECT_OTHER, &&__OTHER_OBJECT,
		&&__OBJECT, && __BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG_NC, &&__SINGLE_NC, &&__FLOAT_NC,
		&&__DATE, NULL, NULL, &&__POINTER_NC
		};

	TYPE type;
	VALUE *P1, *P2;

	P1 = SP - 2;
	P2 = P1 + 1;

	type = code & 0x1F;
	goto *jump[type];

__BOOLEAN:

	P1->type = T_BOOLEAN;
	P1->_integer.value = P1->_integer.value ^ P2->_integer.value; SP--; return;

__BYTE:

	P1->type = T_BYTE;
	P1->_integer.value = (unsigned char)(P1->_integer.value - P2->_integer.value); SP--; return;

__SHORT:

	P1->type = T_SHORT;
	P1->_integer.value = (short)(P1->_integer.value - P2->_integer.value); SP--; return;

__INTEGER:

	P1->type = T_INTEGER;
	P1->_integer.value -= P2->_integer.value; SP--; return;

__LONG:

	VALUE_conv(P1, T_LONG);
	VALUE_conv(P2, T_LONG);

__LONG_NC:

	P1->_long.value -= P2->_long.value; SP--; return;

__SINGLE:

	VALUE_conv(P1, T_SINGLE);
	VALUE_conv(P2, T_SINGLE);

__SINGLE_NC:

	P1->_single.value -= P2->_single.value; SP--; return;

__DATE:
__FLOAT:

	VALUE_conv_float(P1);
	VALUE_conv_float(P2);

__FLOAT_NC:

	P1->_float.value -= P2->_float.value; SP--; return;

__POINTER:

	VALUE_conv(P1, T_POINTER);
	VALUE_conv(P2, T_POINTER);

__POINTER_NC:

	P1->_pointer.value -= (intptr_t)P2->_pointer.value; SP--; return;

__OBJECT_FLOAT:

	EXEC_operator(OP_OBJECT_FLOAT, CO_SUBF, P1, P2); SP--; return;

__FLOAT_OBJECT:

	EXEC_operator(OP_FLOAT_OBJECT, CO_SUBF, P1, P2); SP--; return;

__OBJECT_OTHER:

	EXEC_operator(OP_OBJECT_OTHER, CO_SUBO, P1, P2); SP--; return;

__OTHER_OBJECT:

	EXEC_operator(OP_OTHER_OBJECT, CO_SUBO, P1, P2); SP--; return;

__OBJECT:

	EXEC_operator(OP_OBJECT_OBJECT, CO_SUB, P1, P2); SP--; return;

__VARIANT:

	MANAGE_VARIANT_POINTER_OBJECT(_SUBR_sub, CO_SUB, C_SUB);
	goto __ERROR;

__ERROR:

	THROW(E_TYPE, "Number", TYPE_get_name(type));
}

static void _SUBR_mul(ushort code)
{
	static void *jump[] = {
		&&__VARIANT, &&__BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG, &&__SINGLE, &&__FLOAT,
		&&__ERROR, &&__OBJECT_FLOAT, &&__FLOAT_OBJECT, &&__OBJECT_OTHER, &&__OTHER_OBJECT, &&__OBJECT, NULL, NULL,
		NULL, &&__BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG_NC, &&__SINGLE_NC, &&__FLOAT_NC, &&__ERROR,
		};

	TYPE type;
	VALUE *P1, *P2;

	P1 = SP - 2;
	P2 = P1 + 1;

	type = code & 0x1F;
	goto *jump[type];

__BOOLEAN:

	P1->type = T_BOOLEAN;
	P1->_integer.value = P1->_integer.value & P2->_integer.value; SP--; return;

__BYTE:

	P1->type = T_BYTE;
	P1->_integer.value = (unsigned char)(P1->_integer.value * P2->_integer.value); SP--; return;

__SHORT:

	P1->type = T_SHORT;
	P1->_integer.value = (short)(P1->_integer.value * P2->_integer.value); SP--; return;

__INTEGER:

	P1->type = T_INTEGER;
	P1->_integer.value *= P2->_integer.value; SP--; return;

__LONG:

	VALUE_conv(P1, T_LONG);
	VALUE_conv(P2, T_LONG);

__LONG_NC:

	P1->_long.value *= P2->_long.value; SP--; return;

__SINGLE:

	VALUE_conv(P1, T_SINGLE);
	VALUE_conv(P2, T_SINGLE);

__SINGLE_NC:

	P1->_single.value *= P2->_single.value; SP--; return;

__FLOAT:

	VALUE_conv_float(P1);
	VALUE_conv_float(P2);

__FLOAT_NC:

	P1->_float.value *= P2->_float.value; SP--; return;

__OBJECT_FLOAT:

	EXEC_operator(OP_OBJECT_FLOAT, CO_MULF, P1, P2); SP--; return;

__FLOAT_OBJECT:

	EXEC_operator(OP_FLOAT_OBJECT, CO_MULF, P1, P2); SP--; return;

__OBJECT_OTHER:

	EXEC_operator(OP_OBJECT_OTHER, CO_MULO, P1, P2); SP--; return;

__OTHER_OBJECT:

	EXEC_operator(OP_OTHER_OBJECT, CO_MULO, P1, P2); SP--; return;

__OBJECT:

	EXEC_operator(OP_OBJECT_OBJECT, CO_MUL, P1, P2); SP--; return;

__VARIANT:

	MANAGE_VARIANT_OBJECT(_SUBR_mul, CO_MUL, C_MUL);
	goto __ERROR;

__ERROR:

	THROW(E_TYPE, "Number", TYPE_get_name(type));
}

static void _SUBR_div(ushort code)
{
	static void *jump[] = {
		&&__VARIANT, &&__BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG, &&__SINGLE, &&__FLOAT,
		&&__ERROR, &&__OBJECT_FLOAT, &&__FLOAT_OBJECT, &&__OBJECT_OTHER, &&__OTHER_OBJECT, &&__OBJECT, NULL, NULL,
		NULL, &&__BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG, &&__SINGLE_NC, &&__FLOAT_NC, &&__ERROR,
		};

	TYPE type;
	VALUE *P1, *P2;

	P1 = SP - 2;
	P2 = P1 + 1;

	type = code & 0x1F;
	goto *jump[type];

__BOOLEAN:
__BYTE:
__SHORT:
__INTEGER:
__LONG:
__FLOAT:

	VALUE_conv_float(P1);
	VALUE_conv_float(P2);

__FLOAT_NC:

	P1->_float.value /= P2->_float.value;
	if (isfinite(P1->_float.value))
	{
		SP--;
		return;
	}

	THROW(E_ZERO);

__SINGLE:

	VALUE_conv(P1, T_SINGLE);
	VALUE_conv(P2, T_SINGLE);

__SINGLE_NC:

	P1->_single.value /= P2->_single.value;
	if (isfinite(P1->_single.value))
	{
		SP--;
		return;
	}

	THROW(E_ZERO);

__OBJECT_FLOAT:

	EXEC_operator(OP_OBJECT_FLOAT, CO_DIVF, P1, P2);
	goto __CHECK_OBJECT;

__FLOAT_OBJECT:

	EXEC_operator(OP_FLOAT_OBJECT, CO_DIVF, P1, P2);
	goto __CHECK_OBJECT;

__OBJECT_OTHER:

	EXEC_operator(OP_OBJECT_OTHER, CO_DIVO, P1, P2);
	goto __CHECK_OBJECT;

__OTHER_OBJECT:

	EXEC_operator(OP_OTHER_OBJECT, CO_DIVO, P1, P2);
	goto __CHECK_OBJECT;

__OBJECT:

	EXEC_operator(OP_OBJECT_OBJECT, CO_DIV, P1, P2);
	goto __CHECK_OBJECT;

__VARIANT:

	MANAGE_VARIANT_OBJECT(_SUBR_div, CO_DIV, C_DIV);
	goto __ERROR;

__ERROR:

	THROW(E_TYPE, "Number", TYPE_get_name(type));

__CHECK_OBJECT:

	if (P1->_object.object == NULL)
		THROW(E_ZERO);
	SP--;
}

static void _SUBR_compe(ushort code)
{
	static void *jump[] = {
		&&__SC_VARIANT, &&__SC_BOOLEAN, &&__SC_BYTE, &&__SC_SHORT, &&__SC_INTEGER, &&__SC_LONG, &&__SC_SINGLE, &&__SC_FLOAT,
		&&__SC_DATE, &&__SC_STRING, &&__SC_STRING, &&__SC_POINTER, &&__SC_ERROR, &&__SC_ERROR, &&__SC_ERROR, &&__SC_NULL,
		&&__SC_OBJECT, &&__SC_OBJECT_FLOAT, &&__SC_FLOAT_OBJECT, &&__SC_OBJECT_OTHER, &&__SC_OTHER_OBJECT, &&__SC_OBJECT_OBJECT, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

		NULL, &&__SC_BOOLEAN, &&__SC_BYTE, &&__SC_SHORT, &&__SC_INTEGER, &&__SC_LONG_NC, &&__SC_SINGLE_NC, &&__SC_FLOAT_NC,
		&&__SC_DATE_NC, &&__SC_STRING_NC, &&__SC_STRING_NC, &&__SC_POINTER_NC, &&__SC_ERROR, &&__SC_ERROR, &&__SC_ERROR, &&__SC_NULL,
		&&__SC_OBJECT
		};

	char result = code >= C_NE;
	VALUE *P1;
	VALUE *P2;

	P1 = SP - 2;
	P2 = SP - 1;

	goto *jump[code & 0x3F];

__SC_BOOLEAN:
__SC_BYTE:
__SC_SHORT:
__SC_INTEGER:

	result ^= P1->_integer.value == P2->_integer.value;
	goto __SC_END;

__SC_LONG:

	VALUE_conv(P1, T_LONG);
	VALUE_conv(P2, T_LONG);

__SC_LONG_NC:

	result ^= P1->_long.value == P2->_long.value;
	goto __SC_END;

__SC_DATE:

	VALUE_conv(P1, T_DATE);
	VALUE_conv(P2, T_DATE);

__SC_DATE_NC:

	result ^= DATE_comp_value(P1, P2) == 0;
	goto __SC_END;

__SC_NULL:

	if (P2->type == T_NULL)
	{
		result ^= VALUE_is_null(P1);
		goto __SC_END_RELEASE;
	}
	else if (P1->type == T_NULL)
	{
		result ^= VALUE_is_null(P2);
		goto __SC_END_RELEASE;
	}

__SC_STRING:

	VALUE_conv_string(P1);
	VALUE_conv_string(P2);

__SC_STRING_NC:

	if (P1->_string.len == P2->_string.len)
		result ^= STRING_equal_same(P1->_string.addr + P1->_string.start, P2->_string.addr + P2->_string.start, P1->_string.len);

	RELEASE_STRING(P1);
	RELEASE_STRING(P2);
	goto __SC_END;

__SC_SINGLE:

	VALUE_conv(P1, T_SINGLE);
	VALUE_conv(P2, T_SINGLE);

__SC_SINGLE_NC:

	result ^= P1->_single.value == P2->_single.value;
	goto __SC_END;

__SC_FLOAT:

	VALUE_conv_float(P1);
	VALUE_conv_float(P2);

__SC_FLOAT_NC:

	result ^= P1->_float.value == P2->_float.value;
	goto __SC_END;

__SC_POINTER:

	VALUE_conv(P1, T_POINTER);
	VALUE_conv(P2, T_POINTER);

__SC_POINTER_NC:

	result ^= P1->_pointer.value == P2->_pointer.value;
	goto __SC_END;

__SC_OBJECT:

	result ^= OBJECT_comp_value(P1, P2) == 0;
	//RELEASE_OBJECT(P1);
	//RELEASE_OBJECT(P2);
	goto __SC_END_RELEASE;

__SC_OBJECT_FLOAT:

	result ^= EXEC_comparator(OP_OBJECT_FLOAT, CO_EQUALF, P1, P2);
	goto __SC_END;

__SC_FLOAT_OBJECT:

	result ^= EXEC_comparator(OP_FLOAT_OBJECT, CO_EQUALF, P1, P2);
	goto __SC_END;

__SC_OBJECT_OTHER:

	result ^= EXEC_comparator(OP_OBJECT_OTHER, CO_EQUALO, P1, P2);
	goto __SC_END;

__SC_OTHER_OBJECT:

	result ^= EXEC_comparator(OP_OTHER_OBJECT, CO_EQUALO, P1, P2);
	goto __SC_END;

__SC_OBJECT_OBJECT:

	result ^= EXEC_comparator(OP_OBJECT_OBJECT, CO_EQUAL, P1, P2);
	goto __SC_END;

__SC_VARIANT:

	{
		bool variant = FALSE;
		TYPE type;

		if (TYPE_is_variant(P1->type))
		{
			VARIANT_undo(P1);
			variant = TRUE;
		}

		if (TYPE_is_variant(P2->type))
		{
			VARIANT_undo(P2);
			variant = TRUE;
		}

		code = EXEC_check_operator(P1, P2, CO_EQUAL);
		if (code)
		{
			code += T_OBJECT;
			if (!(variant || P1->type == T_OBJECT || P2->type == T_OBJECT))
				*PC |= code;
			goto *jump[code];
		}

		type = Max(P1->type, P2->type);

		if (TYPE_is_object_null(P1->type) && TYPE_is_object_null(P2->type))
			type = T_OBJECT;
		else if (TYPE_is_object(type))
			THROW_TYPE(T_OBJECT, Min(P1->type, P2->type));
		else if (TYPE_is_void(type))
			THROW(E_NRETURN);

		if (!variant)
		{
			if (P1->type == P2->type)
				*PC |= 0x20;
			*PC |= type;
		}

		goto *jump[type];
	}

__SC_ERROR:

	THROW(E_TYPE, "Number, Date or String", TYPE_get_name(code & 0x1F));

__SC_END_RELEASE:

	RELEASE(P1);
	RELEASE(P2);

__SC_END:

	P1->type = T_BOOLEAN;
	P1->_boolean.value = -result;
	SP--;
}

#if 0
static void _SUBR_compi(ushort code)
{
	static void *jump[] = {
		&&__VARIANT, &&__BOOLEAN, &&__BYTE, &&__SHORT, &&__INTEGER, &&__LONG, &&__SINGLE, &&__FLOAT, &&__DATE,
		&&__STRING, &&__STRING, &&__POINTER, &&__ERROR, &&__ERROR, &&__ERROR, &&__NULL, &&__OBJECT,
		&&__OBJECT_FLOAT, &&__FLOAT_OBJECT, &&__OBJECT_OTHER, &&__OTHER_OBJECT, &&__OBJECT_OBJECT
		};

	static void *test[] = { &&__GT, &&__LE, &&__LT, &&__GE };

	char NO_WARNING(result);
	VALUE *P1;
	VALUE *P2;
	TYPE type;

	P1 = SP - 2;
	P2 = P1 + 1;

	type = code & 0x1F;
	goto *jump[type];

__BOOLEAN:
__BYTE:
__SHORT:
__INTEGER:

	result = P1->_integer.value > P2->_integer.value ? 1 : P1->_integer.value < P2->_integer.value ? -1 : 0;
	goto __END;

__LONG:

	VALUE_conv(P1, T_LONG);
	VALUE_conv(P2, T_LONG);

	result = P1->_long.value > P2->_long.value ? 1 : P1->_long.value < P2->_long.value ? -1 : 0;
	goto __END;

__DATE:

	VALUE_conv(P1, T_DATE);
	VALUE_conv(P2, T_DATE);

	result = DATE_comp_value(P1, P2);
	goto __END;

__NULL:
__STRING:

	VALUE_conv_string(P1);
	VALUE_conv_string(P2);

	result = STRING_compare(P1->_string.addr + P1->_string.start, P1->_string.len, P2->_string.addr + P2->_string.start, P2->_string.len);

	RELEASE_STRING(P1);
	RELEASE_STRING(P2);
	goto __END;

__SINGLE:

	VALUE_conv(P1, T_SINGLE);
	VALUE_conv(P2, T_SINGLE);

	result = P1->_single.value > P2->_single.value ? 1 : P1->_single.value < P2->_single.value ? -1 : 0;
	goto __END;

__FLOAT:

	VALUE_conv_float(P1);
	VALUE_conv_float(P2);

	result = P1->_float.value > P2->_float.value ? 1 : P1->_float.value < P2->_float.value ? -1 : 0;
	goto __END;

__POINTER:

	VALUE_conv(P1, T_POINTER);
	VALUE_conv(P2, T_POINTER);

	result = P1->_pointer.value > P2->_pointer.value ? 1 : P1->_pointer.value < P2->_pointer.value ? -1 : 0;
	goto __END;

__OBJECT:

	result = OBJECT_comp_value(P1, P2);
	//RELEASE_OBJECT(P1);
	//RELEASE_OBJECT(P2);
	goto __END_RELEASE;

__OBJECT_FLOAT:

	result = EXEC_comparator(OP_OBJECT_FLOAT, CO_COMPF, P1, P2);
	goto __END;

__FLOAT_OBJECT:

	result = EXEC_comparator(OP_FLOAT_OBJECT, CO_COMPF, P1, P2);
	goto __END;

__OBJECT_OTHER:

	result = EXEC_comparator(OP_OBJECT_OTHER, CO_COMPO, P1, P2);
	goto __END;

__OTHER_OBJECT:

	result = EXEC_comparator(OP_OTHER_OBJECT, CO_COMPO, P1, P2);
	goto __END;

__OBJECT_OBJECT:

	result = EXEC_comparator(OP_OBJECT_OBJECT, CO_COMP, P1, P2);
	goto __END;

__VARIANT:

	{
		bool variant = FALSE;
		int op;

		if (TYPE_is_variant(P1->type))
		{
			VARIANT_undo(P1);
			variant = TRUE;
		}

		if (TYPE_is_variant(P2->type))
		{
			VARIANT_undo(P2);
			variant = TRUE;
		}

		op = EXEC_check_operator(P1, P2, CO_COMP);
		if (op)
		{
			op += T_OBJECT;
			if (!(variant || P1->type == T_OBJECT || P2->type == T_OBJECT))
				*PC |= op;
			goto *jump[op];
		}

		type = Max(P1->type, P2->type);

		if (type == T_NULL || TYPE_is_string(type))
		{
			TYPE typem = Min(P1->type, P2->type);
			if (!TYPE_is_string(typem))
				THROW(E_TYPE, TYPE_get_name(typem), TYPE_get_name(type));
		}
		else if (TYPE_is_object(type))
			goto __ERROR;
		else if (TYPE_is_void(type))
			THROW(E_NRETURN);

		if (!variant)
			*PC |= type;

		goto *jump[type];
	}

__ERROR:

	THROW(E_TYPE, "Number, Date or String", TYPE_get_name(type));

__END_RELEASE:

	RELEASE(P1);
	RELEASE(P2);

__END:

	P1->type = T_BOOLEAN;
	SP--;

	goto *test[(code >> 8) - (C_GT >> 8)];

__GT:
	P1->_boolean.value = result > 0 ? -1 : 0;
	return;

__GE:
	P1->_boolean.value = result >= 0 ? -1 : 0;
	return;

__LT:
	P1->_boolean.value = result < 0 ? -1 : 0;
	return;

__LE:
	P1->_boolean.value = result <= 0 ? -1 : 0;
	return;
}
#endif

void EXEC_push_array(ushort code)
{
	static const void *jump[] = {
		&&__PUSH_GENERIC, &&__PUSH_GENERIC, &&__PUSH_GENERIC, &&__PUSH_GENERIC,
		&&__PUSH_ARRAY, &&__PUSH_ARRAY, &&__PUSH_ARRAY, &&__PUSH_ARRAY,
		&&__PUSH_NATIVE_ARRAY, NULL, &&__PUSH_NATIVE_ARRAY_SIMPLE, &&__PUSH_NATIVE_ARRAY_SIMPLE,
		&&__PUSH_NATIVE_COLLECTION, &&__PUSH_NATIVE_ARRAY_INTEGER, &&__PUSH_NATIVE_ARRAY_FLOAT, &&__PUSH_NATIVE_STRING
	};

	CLASS *class;
	OBJECT *object;
	ushort np;
	int i;
	void *NO_WARNING(data);
	bool defined;
	VALUE *NO_WARNING(val);
	uint fast;
	CARRAY *array;

	goto *jump[(code & 0xF0) >> 4];

__PUSH_GENERIC:

	np = GET_3X();
	val = &SP[-np];
	np--;

	defined = EXEC_object(val, &class, &object);

	if (class->quick_array == CQA_STRING)
	{
		if (np < 1)
			THROW(E_NEPARAM);
		else if (np > 2)
			THROW(E_TMPARAM);

		if (defined)
			*PC = (*PC & 0xFF00) | (0xF1 + np);
		
		goto __PUSH_NATIVE_STRING;
	}
	
	fast = 0x41 + np;

	if (defined)
	{
		if (class->quick_array == CQA_ARRAY)
		{
			if (np == 1)
			{
				array = (CARRAY *)object;
				if (array->type == GB_T_INTEGER)
				{
					if (!CP->not_3_18 && SP[-1].type == T_INTEGER)
					{
						*PC = C_PUSH_ARRAY_NATIVE_INTEGER;
						goto __PUSH_ARRAY_2;
					}

					fast = 0xD0;
				}
				else if (array->type == GB_T_FLOAT)
				{
					if (!CP->not_3_18 && SP[-1].type == T_INTEGER)
					{
						*PC = C_PUSH_ARRAY_NATIVE_FLOAT;
						goto __PUSH_ARRAY_2;
					}
					else
					{
						fast = 0xE0;
					}
				}
				else if (TYPE_is_object(array->type))
					fast = 0xB0;
				else
					fast = 0xA0 + array->type;
			}
			else
			{
				if (np > MAX_ARRAY_DIM)
					THROW(E_TMPARAM);

				fast = 0x81 + np;
			}
		}
		else if (class->quick_array == CQA_COLLECTION)
		{
			if (np < 1)
				THROW(E_NEPARAM);
			else if (np > 1)
				THROW(E_TMPARAM);

			if (TRUE) //CP->not_3_18)
			{
				fast = 0xC0;
			}
			else
			{
				*PC = C_PUSH_ARRAY_NATIVE_COLLECTION;
				goto __PUSH_ARRAY_2;
			}
		}
		else
		{
			// Check the symbol existance, but *not virtually*
			if (object && !VALUE_is_super(val))
			{
				CLASS *nvclass = val->_object.class;

				if (nvclass->special[SPEC_GET] == NO_SYMBOL)
					THROW(E_NARRAY, CLASS_get_name(nvclass));
			}
		}
	}
	
	*PC = (*PC & 0xFF00) | fast;

	goto __PUSH_ARRAY_2;

__PUSH_NATIVE_ARRAY:

	np = GET_3X();
	val = &SP[-np];
	np--;
	EXEC_object_array(val, class, object);

	for (i = 1; i <= np; i++)
		VALUE_conv_integer(&val[i]);

	data = CARRAY_get_data_multi((CARRAY *)object, (GB_INTEGER *)&val[1], np - 1);
	if (!data)
		PROPAGATE();

	VALUE_read(val, data, ((CARRAY *)object)->type);
	goto __PUSH_NATIVE_END;

__PUSH_NATIVE_COLLECTION:

	val = &SP[-2];
	EXEC_object_array(val, class, object);

	VALUE_conv_string(&val[1]);
	//fprintf(stderr, "GB_CollectionGet: %p '%.*s'\n", val[1]._string.addr, val[1]._string.len, val[1]._string.addr + val[1]._string.start);
	GB_CollectionGet((GB_COLLECTION)object, val[1]._string.addr + val[1]._string.start, val[1]._string.len, (GB_VARIANT *)val);

	RELEASE_STRING(&val[1]);
	goto __PUSH_NATIVE_END;

__PUSH_NATIVE_STRING:

	BoxedString_get(GET_0X() - 1);
	return;

__PUSH_NATIVE_ARRAY_SIMPLE:

	val = &SP[-2];
	np = code & 0x1F;

	EXEC_object_array(val, class, object);
	array = (CARRAY *)object;

	VALUE_conv_integer(&val[1]);

	data = CARRAY_get_data_throw(array, val[1]._integer.value);

	VALUE_read_inline_type(val, data, np, array->type, __PUSH_NATIVE_END_NOREF, __PUSH_NATIVE_END);

__PUSH_NATIVE_ARRAY_INTEGER:

	val = &SP[-2];
	EXEC_object_array(val, class, object);
	array = (CARRAY *)object;

	VALUE_conv_integer(&val[1]);
	i = val[1]._integer.value;

	if (i < 0 || i >= array->count)
		THROW_BOUND();

	val->_integer.value = ((int *)(array->data))[i];
	val->type = GB_T_INTEGER;
	goto __PUSH_NATIVE_END_NOREF;

__PUSH_NATIVE_ARRAY_FLOAT:

	val = &SP[-2];
	EXEC_object_array(val, class, object);
	array = (CARRAY *)object;

	VALUE_conv_integer(&val[1]);
	i = val[1]._integer.value;

	if (i < 0 || i >= array->count)
		THROW_BOUND();

	val->_float.value = ((double *)(array->data))[i];
	val->type = GB_T_FLOAT;
	goto __PUSH_NATIVE_END_NOREF;

__PUSH_NATIVE_END_NOREF:

	SP = val + 1;
	OBJECT_UNREF(object);
	return;

__PUSH_NATIVE_END:

	SP = val;
	PUSH();
	OBJECT_UNREF(object);
	return;

__PUSH_ARRAY:

	np = GET_3X();
	val = &SP[-np];
	np--;
	defined = EXEC_object(val, &class, &object);

__PUSH_ARRAY_2:

	if (EXEC_special(SPEC_GET, class, object, np, FALSE))
		THROW(E_NARRAY, CLASS_get_name(class));

	OBJECT_UNREF(object);
	SP--;
	//SP[-1] = SP[0];
	VALUE_copy(&SP[-1], &SP[0]);

	if (!defined)
		VALUE_conv_variant(&SP[-1]);
}

void EXEC_pop_array(ushort code)
{
	static const void *jump[] = {
		&&__POP_GENERIC, &&__POP_GENERIC, &&__POP_GENERIC, &&__POP_GENERIC,
		&&__POP_ARRAY, &&__POP_ARRAY, &&__POP_ARRAY, &&__POP_ARRAY,
		&&__POP_NATIVE_ARRAY, NULL, &&__POP_NATIVE_ARRAY_SIMPLE, &&__POP_NATIVE_ARRAY_SIMPLE,
		&&__POP_NATIVE_COLLECTION, &&__POP_NATIVE_ARRAY_INTEGER, &&__POP_NATIVE_ARRAY_FLOAT, NULL
	};

	CLASS *class;
	OBJECT *object;
	ushort np;
	int i;
	void *data;
	bool defined;
	VALUE *NO_WARNING(val);
	VALUE swap;
	int fast;
	CARRAY *array;

	goto *jump[(code & 0xF0) >> 4];

__POP_GENERIC:

	np = GET_3X();
	val = &SP[-np];

	defined = EXEC_object(val, &class, &object);

	fast = 0x40 + np;

	if (defined)
	{
		if (class->quick_array == CQA_ARRAY)
		{
			if (np == 2)
			{
				array = (CARRAY *)object;
				if (array->type == GB_T_INTEGER)
				{
					if (!CP->not_3_18 && SP[-1].type == T_INTEGER && SP[-3].type == T_INTEGER)
					{
						*PC = C_POP_ARRAY_NATIVE_INTEGER;
						goto __POP_ARRAY_2;
					}
					else
					{
						fast = 0xD0;
					}
				}
				else if (array->type == GB_T_FLOAT)
				{
					if (!CP->not_3_18 && SP[-1].type == T_INTEGER && SP[-3].type == T_FLOAT)
					{
						*PC = C_POP_ARRAY_NATIVE_FLOAT;
						goto __POP_ARRAY_2;
					}
					else
					{
						fast = 0xE0;
					}
				}
				else if (TYPE_is_object(array->type))
					fast = 0xB0;
				else
					fast = 0xA0 + array->type;
			}
			else
			{
				if (np > (MAX_ARRAY_DIM + 1))
					THROW(E_TMPARAM);

				fast = 0x80 + np;
			}
		}
		else if (class->quick_array == CQA_COLLECTION)
		{
			if (np < 2)
				THROW(E_NEPARAM);
			else if (np > 2)
				THROW(E_TMPARAM);

			if (TRUE) //CP->not_3_18)
			{
				fast = 0xC0;
			}
			else
			{
				*PC = C_POP_ARRAY_NATIVE_COLLECTION;
				goto __POP_ARRAY_2;
			}
		}
		else
		{
			// Check the symbol existance, but *not virtually*
			if (object && !VALUE_is_super(val))
			{
				CLASS *nvclass = val->_object.class;

				if (nvclass->special[SPEC_PUT] == NO_SYMBOL)
					THROW(E_NARRAY, CLASS_get_name(nvclass));
			}
		}
	}

	*PC = (*PC & 0xFF00) | fast;

	goto __POP_ARRAY_2;

__POP_NATIVE_ARRAY:

	np = GET_3X();
	val = &SP[-np];

	EXEC_object_array(val, class, object);
	array = (CARRAY *)object;

	CARRAY_check_not_read_only(array);
	
	for (i = 1; i < np; i++)
		VALUE_conv_integer(&val[i]);

	data = CARRAY_get_data_multi((CARRAY *)object, (GB_INTEGER *)&val[1], np - 2);
	if (data == NULL)
		PROPAGATE();

	VALUE_write(&val[-1], data, array->type);
	goto __POP_NATIVE_END;

__POP_NATIVE_COLLECTION:

	val = &SP[-2];
	EXEC_object_array(val, class, object);

	VALUE_conv_variant(&val[-1]);
	VALUE_conv_string(&val[1]);

	if (GB_CollectionSet((GB_COLLECTION)object, val[1]._string.addr + val[1]._string.start, val[1]._string.len, (GB_VARIANT *)&val[-1]))
		PROPAGATE();

	RELEASE_STRING(&val[1]);
	OBJECT_UNREF(object)
	RELEASE_VARIANT(&val[-1]);
	SP -= 3;
	return;

__POP_NATIVE_ARRAY_SIMPLE:

	val = &SP[-2];
	EXEC_object_array(val, class, object);
	array = (CARRAY *)object;

	CARRAY_check_not_read_only(array);
	
	VALUE_conv_integer(&val[1]);

	data = CARRAY_get_data(array, val[1]._integer.value);
	if (data == NULL)
		PROPAGATE();

	VALUE_write(&val[-1], data, array->type);
	goto __POP_NATIVE_END;

__POP_NATIVE_ARRAY_INTEGER:

	val = &SP[-2];
	EXEC_object_array(val, class, object);
	array = (CARRAY *)object;

	CARRAY_check_not_read_only(array);
	
	VALUE_conv_integer(&val[-1]);
	VALUE_conv_integer(&val[1]);

	i = val[1]._integer.value;
	if (i < 0 || i >= array->count)
		THROW_BOUND();

	((int *)(array->data))[i] = val[-1]._integer.value;
	goto __POP_NATIVE_FAST_END;

__POP_NATIVE_ARRAY_FLOAT:

	val = &SP[-2];
	EXEC_object_array(val, class, object);
	array = (CARRAY *)object;

	CARRAY_check_not_read_only(array);
	
	VALUE_conv_float(&val[-1]);
	VALUE_conv_integer(&val[1]);

	i = val[1]._integer.value;
	if (i < 0 || i >= array->count)
		THROW_BOUND();

	((double *)(array->data))[i] = val[-1]._float.value;

__POP_NATIVE_FAST_END:

	SP = val + 1;
	OBJECT_UNREF(object);
	SP -= 2;
	return;

__POP_NATIVE_END:

	SP = val + 1;
	OBJECT_UNREF(object);
	RELEASE(SP - 2);
	SP -= 2;
	//OBJECT_UNREF(object);
	return;

__POP_ARRAY:

	np = GET_3X();
	val = &SP[-np];

	defined = EXEC_object(val, &class, &object);

__POP_ARRAY_2:

	/* swap object and value to be inserted */
	VALUE_copy(&swap, &val[0]);
	VALUE_copy(&val[0], &val[-1]);
	VALUE_copy(&val[-1], &swap);

	if (EXEC_special(SPEC_PUT, class, object, np, TRUE))
		THROW(E_NARRAY, CLASS_get_name(class));

	POP(); /* free the object */
}

void EXEC_quit(ushort code)
{
	switch(code & 3)
	{
		case 0: // QUIT
			EXEC_do_quit();
			break;

		case 1: // STOP
			if (EXEC_debug && CP) // && CP->component == COMPONENT_main)
				DEBUG.Breakpoint(0);
			break;

		case 2: // STOP EVENT
			GAMBAS_StopEvent = TRUE;
			break;

		case 3: // QUIT <return value>
			VALUE_conv(&SP[-1], T_BYTE);
			SP--;
			EXEC_quit_value = (uchar)SP->_integer.value;
			EXEC_do_quit();
			break;
	}
}

NOINLINE static void _break(ushort code)
{
	if (EXEC_trace)
	{
		double timer;
		int i;
		
		DATE_timer(&timer, TRUE);
		fprintf(stderr, "[%d.%06d] ", (int)timer, (int)(timer * 1000000) % 1000000);
		for (i = 0; i < STACK_frame_count; i++)
			fputs(". ", stderr);
		fputs(DEBUG_get_current_position(), stderr);
		fputc('\n', stderr);
		fflush(stderr);
	}
	
	if (EXEC_debug)
	{
		/*TC = PC + 1;
		TP = SP;*/

		//fprintf(stderr, "%s %d\n", DEBUG_get_current_position(), DEBUG_info->watch);

		if (CP && (EXEC_debug_inside || !CP->component))
		{
			if (EXEC_profile_instr)
				DEBUG.Profile.Add(CP, FP, PC);

			if (DEBUG_info->watch)
			{
				if (DEBUG.CheckWatches())
					return;
			}
			
			code = (uchar)code;

			if (code == 0)
			{
				if (!DEBUG_info->stop)
					return;

				// Return from (void stack)
				if (DEBUG_info->leave)
				{
					if (STACK_get_current()->pc)
						return;
					if (FP == DEBUG_info->fp)
						return;
					if (BP > DEBUG_info->bp)
						return;
				}
				// Forward or Return From
				else if (DEBUG_info->fp != NULL)
				{
					if (BP > DEBUG_info->bp)
						return;
				}
				// otherwise, Next
			}

			DEBUG.Breakpoint(code);
		}
	}
}

NOINLINE static void SUBR_left(ushort code)
{
	int val;

	SUBR_ENTER();

	if (!SUBR_check_string(PARAM))
	{
		if (NPARAM == 1)
			val = 1;
		else
		{
			VALUE_conv_integer(&PARAM[1]);
			val = PARAM[1]._integer.value;
		}

		if (val < 0)
			val += PARAM->_string.len;

		PARAM->_string.len = MinMax(val, 0, PARAM->_string.len);
	}

	SP -= NPARAM;
	SP++;
}

NOINLINE static void SUBR_mid(ushort code)
{
	int start;
	int len;
	bool null;

	SUBR_ENTER();

	null = SUBR_check_string(PARAM);

	VALUE_conv_integer(&PARAM[1]);
	start = PARAM[1]._integer.value - 1;

	if (start < 0)
		THROW(E_ARG);

	if (null)
		goto _SUBR_MID_FIN;

	if (start >= PARAM->_string.len)
	{
		VOID_STRING(PARAM);
		goto _SUBR_MID_FIN;
	}

	if (NPARAM == 2)
		len = PARAM->_string.len;
	else
	{
		VALUE_conv_integer(&PARAM[2]);
		len = PARAM[2]._integer.value;
	}

	if (len < 0)
		len = Max(0, PARAM->_string.len - start + len);

	len = MinMax(len, 0, PARAM->_string.len - start);

	if (len == 0)
	{
		RELEASE_STRING(PARAM);
		PARAM->_string.addr = NULL;
		PARAM->_string.start = 0;
	}
	else
		PARAM->_string.start += start;

	PARAM->_string.len = len;

_SUBR_MID_FIN:

	SP -= NPARAM;
	SP++;
}

NOINLINE static void SUBR_right(ushort code)
{
	int val;
	int new_len;

	SUBR_ENTER();

	if (!SUBR_check_string(PARAM))
	{
		if (NPARAM == 1)
			val = 1;
		else
		{
			VALUE_conv_integer(&PARAM[1]);
			val = PARAM[1]._integer.value;
		}

		if (val < 0)
			val += PARAM->_string.len;

		new_len = MinMax(val, 0, PARAM->_string.len);

		PARAM->_string.start += PARAM->_string.len - new_len;
		PARAM->_string.len = new_len;
	}

	SP -= NPARAM;
	SP++;
}


NOINLINE static void SUBR_len(void)
{
	int len;

	SUBR_GET_PARAM(1);

	if (SUBR_check_string(PARAM))
		len = 0;
	else
		len = PARAM->_string.len;

	RELEASE_STRING(PARAM);

	PARAM->type = T_INTEGER;
	PARAM->_integer.value = len;
}
