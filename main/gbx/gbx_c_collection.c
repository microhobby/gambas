/***************************************************************************

  gbx_c_collection.c

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

#define __GBX_C_COLLECTION_C

#include "gbx_info.h"

#ifndef GBX_INFO

#include "gb_common.h"
#include "gb_error.h"

#include "gbx_variant.h"
#include "gambas.h"
#include "gbx_api.h"
#include "gbx_class.h"
#include "gbx_object.h"

#include "gbx_c_collection.h"


static void clear(CCOLLECTION *col)
{
	VARIANT *value;
	HASH_ENUM iter;

	CLEAR(&iter);
	col->locked = TRUE;

	for(;;)
	{
		value = HASH_TABLE_next(col->hash_table, &iter, FALSE);
		if (value == NULL)
			break;

		VARIANT_free(value);
		value->type = T_NULL;
	}

	HASH_TABLE_delete(&col->hash_table);
	col->locked = FALSE;
}

#define get_key(_col, _key, _len, _set_last) HASH_TABLE_lookup((_col)->hash_table, (_key), (_len), _set_last)

#define add_key(_col, _key, _len) ((_len) == 0 ? (GB_Error((char *)E_VKEY), NULL) : HASH_TABLE_insert(((CCOLLECTION *)(_col))->hash_table, (_key), (_len)))

static void remove_key(CCOLLECTION *col, const char *key, int len)
{
	void *value;
	HASH_NODE *last;
	HASH_NODE *save;
	void *save_enum;

	if (len == 0)
	{
		GB_Error((char *)E_VKEY);
		return;
	}

	save = col->hash_table->last;
	
	value = HASH_TABLE_lookup(col->hash_table, key, len, TRUE);
	
	last = col->hash_table->last;
	col->hash_table->last = save;
	
	if (value == NULL)
		return;

	if (last)
	{
		save_enum = GB_BeginEnum(col);
		while (!GB_NextEnum())
		{
			HASH_ENUM *iter = (HASH_ENUM *)GB_GetEnum();
			if (iter->next == last)
				iter->next = iter->next->snext;
		}
		GB_EndEnum(save_enum);
	}

	VARIANT_free((VARIANT *)value);

	if (!col->locked)
		HASH_TABLE_remove(col->hash_table, key, len);
	else // Prevent the freed variant to be freed twice if the collection is locked (i.e. being destroyed)
		((VARIANT *)value)->type = T_NULL;
}


BEGIN_METHOD(Collection_new, GB_INTEGER mode)

	int mode = MISSING(mode) ? 0 : VARG(mode);

	HASH_TABLE_create(&THIS->hash_table, TYPE_sizeof(T_VARIANT), mode);
	THIS->last = NULL;
	THIS->default_value.type = GB_T_NULL;

END_METHOD


BEGIN_METHOD_VOID(Collection_free)

	clear(THIS);
	GB_StoreVariant(NULL, POINTER(&THIS->default_value));

END_METHOD


BEGIN_PROPERTY(Collection_Count)

	GB_ReturnInt(CCOLLECTION_get_count(THIS));

END_PROPERTY


BEGIN_PROPERTY(Collection_Key)

	char *key;
	int len;

	if (READ_PROPERTY)
	{
		if (HASH_TABLE_get_last_key(THIS->hash_table, &key, &len))
			GB_ReturnVoidString();
		else
			GB_ReturnNewString(key, len);
	}
	else
		HASH_TABLE_set_last_key(THIS->hash_table, PSTRING(), PLENGTH());

END_PROPERTY


BEGIN_METHOD(Collection_Add, GB_VARIANT value; GB_STRING key)

	void *data;

	data = add_key(THIS, STRING(key), LENGTH(key));
	if (!data)
		return;

	GB_StoreVariant(ARG(value), data);

END_METHOD


BEGIN_METHOD(Collection_Exist, GB_STRING key)

	GB_ReturnBoolean(get_key(THIS, STRING(key), LENGTH(key), FALSE) != NULL);

END_METHOD


BEGIN_METHOD(Collection_Remove, GB_STRING key)

	remove_key(THIS, STRING(key), LENGTH(key));

END_METHOD


BEGIN_METHOD_VOID(Collection_Clear)

	int mode = THIS->hash_table->mode;

	/* Stops all iterators */
	GB_StopAllEnum(THIS);

	clear(THIS);
	HASH_TABLE_create(&THIS->hash_table, TYPE_sizeof(T_VARIANT), mode);

END_METHOD


BEGIN_METHOD_VOID(Collection_next)

	void *value;
	HASH_TABLE *hash_table = OBJECT(CCOLLECTION)->hash_table;
	HASH_ENUM *iter = (HASH_ENUM *)GB_GetEnum();

	value = HASH_TABLE_next(hash_table, iter, TRUE);

	if (value == NULL)
		GB_StopEnum();
	else
		GB_ReturnVariant(value);

END_METHOD


BEGIN_METHOD(Collection_get, GB_STRING key)

	void *value = get_key(THIS, STRING(key), LENGTH(key), TRUE);
	if (!value)
	{
		if (THIS->has_default)
			value = (void *)&THIS->default_value;
	}

	GB_ReturnVariant(value);

END_METHOD


BEGIN_METHOD(Collection_put, GB_VARIANT value; GB_STRING key)

	void *data;

	if (VARIANT_is_null((VARIANT *)&VARG(value)))
		remove_key(THIS, STRING(key), LENGTH(key));
	else
	{
		data = add_key(THIS, STRING(key), LENGTH(key));
		if (!data)
			return;
		GB_StoreVariant(ARG(value), data);
	}

END_METHOD


BEGIN_METHOD_VOID(Collection_Copy)

	GB_COLLECTION col;
	GB_COLLECTION_ITER iter;;
	GB_VARIANT value;
	char *key;
	int len;

	GB_CollectionNew(&col, THIS->mode);

	CLEAR(&iter);

	for(;;)
	{
		if (GB_CollectionEnum(THIS, &iter, &value, &key, &len))
			break;

		GB_CollectionSet(col, key, len, &value);
	}

	GB_ReturnObject(col);

END_METHOD


BEGIN_PROPERTY(Collection_Keys)

	GB_ARRAY keys;
	GB_COLLECTION_ITER iter;;
	char *key;
	int len;
	int i;

	GB_ArrayNew(&keys, GB_T_STRING, CCOLLECTION_get_count(THIS));

	CLEAR(&iter);

	for(i = 0;; i++)
	{
		if (GB_CollectionEnum(THIS, &iter, NULL, &key, &len))
			break;

		*((char **)GB_ArrayGet(keys, i)) = STRING_new(key, len);
	}

	GB_ReturnObject(keys);

END_METHOD


BEGIN_PROPERTY(Collection_Default)

	if (READ_PROPERTY)
		GB_ReturnVariant(&THIS->default_value);
	else
	{
		GB_StoreVariant(PROP(GB_VARIANT), POINTER(&THIS->default_value));
		THIS->has_default = THIS->default_value.type != GB_T_NULL;
	}

END_PROPERTY

static void return_node_key(HASH_TABLE *hash_table, HASH_NODE *node)
{
	char *key;
	int len;
	
	HASH_TABLE_get_key(hash_table, node, &key, &len);
	if (len)
		GB_ReturnNewString(key, len);
	else
		GB_ReturnNull();

}

BEGIN_PROPERTY(Collection_First)

	return_node_key(THIS->hash_table, THIS->hash_table->sfirst);

END_PROPERTY

BEGIN_PROPERTY(Collection_Last)

	return_node_key(THIS->hash_table, THIS->hash_table->slast);

END_PROPERTY

#endif


GB_DESC NATIVE_Collection[] =
{
	GB_DECLARE("Collection", sizeof(CCOLLECTION)),

	GB_METHOD("_new", NULL, Collection_new, "[(Mode)i]"),
	GB_METHOD("_free", NULL, Collection_free, NULL),

	GB_PROPERTY_READ("Count", "i", Collection_Count),
	GB_PROPERTY_READ("Length", "i", Collection_Count),
	GB_PROPERTY_READ("First", "s", Collection_First),
	GB_PROPERTY_READ("Last", "s", Collection_Last),
	GB_PROPERTY("Key", "s", Collection_Key),
	GB_PROPERTY("Default", "v", Collection_Default),
	GB_PROPERTY_READ("Keys", "String[]", Collection_Keys),

	GB_METHOD("Add", NULL, Collection_Add, "(Value)v(Key)s"),
	GB_METHOD("Exist", "b", Collection_Exist, "(Key)s"),
	GB_METHOD("Remove", NULL, Collection_Remove, "(Key)s"),
	GB_METHOD("Clear", NULL, Collection_Clear, NULL),
	/*GB_METHOD("_first", NULL, collection_first, NULL),*/
	GB_METHOD("_next", "v", Collection_next, NULL),
	GB_METHOD("_get", "v", Collection_get, "(Key)s"),
	GB_METHOD("_put", NULL, Collection_put, "(Value)v(Key)s"),
	GB_METHOD("Copy", "Collection", Collection_Copy, NULL),

	GB_END_DECLARE
};

#ifndef GBX_INFO

void GB_CollectionNew(GB_COLLECTION *col, int mode)
{
	VALUE param;

	param._integer.type = GB_T_INTEGER;
	param._integer.value = mode;

	*col = OBJECT_create_native(CLASS_Collection, &param);
}

int GB_CollectionCount(GB_COLLECTION col)
{
	return HASH_TABLE_size(((CCOLLECTION *)col)->hash_table);
}

bool GB_CollectionSet(GB_COLLECTION col, const char *key, int len, GB_VARIANT *value)
{
	VARIANT *data;

	if (VARIANT_is_null((VARIANT *)&value->value))
		remove_key((CCOLLECTION *)col, key, len);
	else
	{
		data = (VARIANT *)add_key((CCOLLECTION *)col, key, len);
		if (!data)
			return TRUE;
		VALUE_write_variant((VALUE *)value, data);
	}
	return FALSE;
}

bool GB_CollectionGet(GB_COLLECTION col, const char *key, int len, GB_VARIANT *value)
{
	CCOLLECTION *_object = (CCOLLECTION *)col;
	VARIANT *var;
	bool ret;

	value->type = T_VARIANT;

	var = (VARIANT *)get_key(THIS, key, len, TRUE);

	if (!var)
	{
		if (!THIS->has_default)
		{
			value->value.type = GB_T_NULL;
			return TRUE;
		}

		var = (VARIANT *)&THIS->default_value;
		ret = TRUE;
	}
	else
	{
		ret = FALSE;
	}

	value->value.type = var->type;
	value->value.value.data = var->value.data;
	return ret;
}

bool GB_CollectionEnum(GB_COLLECTION col, GB_COLLECTION_ITER *iter, GB_VARIANT *value, char **key, int *len)
{
	VARIANT *val;
	HASH_TABLE *hash_table = ((CCOLLECTION *)col)->hash_table;

	if (!key)
	{
		CLEAR(iter);
		return FALSE;
	}

	val = HASH_TABLE_next(hash_table, (HASH_ENUM *)iter, TRUE);
	if (!val)
		return TRUE;

	if (value)
	{
		value->type = T_VARIANT;
		value->value.type = val->type;
		value->value.value.data = val->value.data;
	}

	HASH_TABLE_get_last_key(hash_table, key, len);
	return FALSE;
}

#endif
