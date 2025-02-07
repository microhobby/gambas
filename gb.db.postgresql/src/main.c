/***************************************************************************

  main.c

  (c) 2000-2017 Benoît Minisini <benoit.minisini@gambas-basic.org>
  (c) 2011-2012 Bruce Bruen <bbruen@paddys-hill.net>

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

  --------------------------------------------------------------------------

  In addition, as a special exception, the copyright holders give
  permission to link the code of portions of this program with the
  OpenSSL library under certain conditions as described in each
  individual source file, and distribute linked combinations
  including the two.
  You must obey the GNU General Public License in all respects
  for all of the code used other than OpenSSL. If you modify
  file(s) with this exception, you may extend this exception to
  your version of the file(s), but you are not obligated to do so.
  If you do not wish to do so, delete this exception statement
  from your version. If you delete this exception statement from
  all source files in the program, then also delete it here.

***************************************************************************/

#define __MAIN_C

#include <libpq-fe.h>

#ifdef fprintf
	#undef fprintf
	#undef snprintf
	#undef sprintf
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef Max
	#undef Max
#endif

#ifdef Min
	#undef Min
#endif

#ifdef snprintf
	#undef snprintf
#endif

#ifdef pg_snprintf
	#undef pg_snprintf
#endif

#include "gb.db.proto.h"
#include "main.h"


GB_INTERFACE GB EXPORT;
DB_INTERFACE DB EXPORT;


static char _buffer[32];
static DB_DRIVER _driver;
static int _last_error;
/*static int _print_query = FALSE;*/

// PostgreSQL datatypes

enum { OID_BOOL, OID_INT2, OID_INT4, OID_INT8, OID_NUMERIC, OID_FLOAT4, OID_FLOAT8, OID_ABSTIME,
	OID_RELTIME, OID_DATE, OID_TIME, OID_TIMESTAMP, OID_DATETIME, OID_TIMESTAMPTZ, OID_BYTEA, OID_CHAR,
	OID_BPCHAR, OID_VARCHAR, OID_TEXT, OID_NAME, OID_CASH,
	OID_COUNT };

const char *_oid_names[] = {
	"bool", "int2", "int4", "int8", "numeric", "float4", "float8", "abstime", 
	"reltime", "date", "time", "timestamp", "datetime", "timestamptz", "bytea", "char",
	"bpchar", "varchar", "text", "name", "cash", NULL
};

int _oid[OID_COUNT] = { 0 };


// Get the SQL expression returning the default value of a field

static const char *get_default_value(const char *old_way)
{
	if (DB.GetCurrentDatabase()->version >= 90600)
		return "pg_get_expr(adbin, adrelid) AS adsrc";
	else
		return old_way;
}

/* Internal function to check the result of a query */

static int check_result(PGresult *res, const char *err)
{
	_last_error = 0;

	if (!res)
	{
		GB.Error("Out of memory");
		return TRUE;
	}

	_last_error = PQresultStatus(res);
	switch (_last_error)
	{
		case PGRES_COMMAND_OK:
		case PGRES_TUPLES_OK:
			return FALSE;

		default:
			if (err)
				GB.Error(err, PQresultErrorMessage(res));
			PQclear(res);
			return TRUE;
	}
}


/* internal function to quote a value stored as a string */

static void quote_string(const char *data, int len, DB_FORMAT_CALLBACK add)
{
	int i;
	unsigned char c;
	char buffer[8];

	if (DB.GetCurrentDatabase()->version >= 80200)
		(*add)("E", 1);

	(*add)("'", 1);
	for (i = 0; i < len; i++)
	{
		c = (unsigned char)data[i];
		if (c == '\\')
			(*add)("\\\\", 2);
		else if (c == '\'')
			(*add)("''", 2);
		else if (c < 32 || c > 127)
		{
			buffer[0] = '\\';
			buffer[1] = '0' + ((c >> 6) & 0x7);
			buffer[2] = '0' + ((c >> 3) & 0x7);
			buffer[3] = '0' + (c & 0x7);
			(*add)(buffer, 4);
		}
		else
			(*add)((const char *)&c, 1);
	}
	(*add)("'", 1);
}

/* Quote a string and returns the result as a temporary string */

static char *get_quote_string(const char *str, int len, char quote)
{
	char *res, *p, c;
	int len_res;
	int i;

	len_res = len;
	for (i = 0; i < len; i++)
	{
		c = str[i];
		if (c == quote || c == '\\' || c == 0)
			len_res++;
	}

	res = GB.TempString(NULL, len_res);

	p = res;
	for (i = 0; i < len; i++)
	{
		c = str[i];
		if (c == '\\' || c == quote)
		{
			*p++ = c;
			*p++ = c;
		}
		/*else if (c == 0)
		{
			*p++ = '\\';
			*p++ = '0';
		}*/
		else
			*p++ = c;
	}
	*p = 0;

	return res;
}

/* Quote a table name so that schema work, and return a temporary string */

static char *get_quoted_table(const char *table)
{
	int len;
	char *point;
	char *res;

	if (!table || !*table)
		return "";

	len = strlen(table);
	point = index(table, '.');

	if (!point)
	{
		res = GB.TempString(NULL, len + 2);
		sprintf(res, "\"%s\"", table);
	}
	else
	{
		res = GB.TempString(NULL, len + 4);
		sprintf(res, "\"%.*s\".\"%s\"", (int)(point - table), table, point + 1);
	}

	return res;
}

static bool get_table_schema(const char **table, char **schema)
{
	char *point;
	int len;

	//fprintf(stderr, "get_table_schema: %s\n", *table);

	*schema = NULL;

	if (!*table || !**table)
	{
		//fprintf(stderr, "get_table_schema: -> NULL\n");
		return TRUE;
	}

	point = strchr(*table, '.');
	if (!point)
	{
		*schema = "public";
	}
	else
	{
		len = point - *table;
		if (len >= 3 && **table == '"' && (*table)[len - 1] == '"')
			*schema = GB.TempString(*table + 1, len - 2);
		else
			*schema = GB.TempString(*table, len);
		
		*table = point + 1;
	}
		
	return FALSE;
}

/* internal function to quote a value stored as a blob */

static void quote_blob(const char *data, int len, DB_FORMAT_CALLBACK add)
{
	int i;
	unsigned char c;
	char buffer[8];

	if (DB.GetCurrentDatabase()->version >= 80200)
		(*add)("E", 1);

	(*add)("'", 1);
	for (i = 0; i < len; i++)
	{
		c = (unsigned char)data[i];
		if (c == '\\')
			(*add)("\\\\\\\\", 4);
		else if (c == '\'')
			(*add)("\\'", 2);
		else if (c < 32 || c > 127)
		{
			buffer[0] = '\\';
			buffer[1] = '\\';
			buffer[2] = '0' + ((c >> 6) & 0x7);
			buffer[3] = '0' + ((c >> 3) & 0x7);
			buffer[4] = '0' + (c & 0x7);
			(*add)(buffer, 5);
		}
		else
			(*add)((const char *)&c, 1);
	}
	(*add)("'", 1);
}

/* internal function to unquote a value stored as a string */

static int unquote_string(const char *data, int len, DB_FORMAT_CALLBACK add)
{
	int i;
	char c;

	if (!data || *data != '\'')
		return TRUE;

	for (i = 1;; i++)
	{
		c = data[i];
		if (c == '\'')
			break;
		if (c == '\\')
			i++;
		(*add)(&data[i], 1);
	}

	return FALSE;
}

/* internal function to unquote a value stored as a blob */

static int unquote_blob(const char *data, int len, DB_FORMAT_CALLBACK add)
{
	int i;
	char c;

	//if (!data || *data != '\'')
	//  return TRUE;

	for (i = 0; i < len; i++)
	{
		c = data[i];
		if (c == '\\')
		{
			i++;
			c = data[i];
			if (c >= '0' && c <= '9' && i < (len - 2))
			{
				c = ((data[i] - '0') << 6) + ((data[i + 1] - '0') << 3) + (data[i + 2] - '0');
				i += 2;
				(*add)(&c, 1);
				continue;
			}
		}
		(*add)(&data[i], 1);
	}

	return FALSE;
}

/* Internal function to convert a database type into a Gambas type */

static GB_TYPE conv_type(Oid type)
{
	if (type == _oid[OID_BOOL])
		return GB_T_BOOLEAN;
	
	if (type == _oid[OID_INT2] || type == _oid[OID_INT4])
		return GB_T_INTEGER;

	if (type == _oid[OID_INT8])
		return GB_T_LONG;

	if (type == _oid[OID_NUMERIC] || type == _oid[OID_FLOAT4] || type == _oid[OID_FLOAT8])
			return GB_T_FLOAT;

	if (type == _oid[OID_ABSTIME] || type == _oid[OID_RELTIME] || type == _oid[OID_DATE]
	    || type == _oid[OID_TIME] || type == _oid[OID_TIMESTAMP] || type == _oid[OID_DATETIME]
	    || type == _oid[OID_TIMESTAMPTZ])
		return GB_T_DATE;

	if (type == _oid[OID_BYTEA])
		return DB_T_BLOB;

	return GB_T_STRING;
}


/* Internal function to convert a database boolean value  */

static int conv_boolean(const char *data)
{
	return strcasecmp(data, "t") == 0 || strcasecmp(data, "'t'") == 0;
}

/* Internal function to convert a database value into a Gambas variant value */

static void conv_data(const char *data, int len, GB_VARIANT_VALUE *val, Oid type)
{
	GB_VALUE conv;
	GB_DATE_SERIAL date;
	double sec;
	bool bc;

	if (type == _oid[OID_BOOL])
	{
		val->type = GB_T_BOOLEAN;
		val->value._boolean = conv_boolean(data) ? -1 : 0;
	}
	else if (type == _oid[OID_INT2] || type == _oid[OID_INT4])
	{
		GB.NumberFromString(GB_NB_READ_INTEGER, data, strlen(data), &conv);

		val->type = GB_T_INTEGER;
		val->value._integer = conv._integer.value;
	}
	else if (type == _oid[OID_INT8])
	{
		GB.NumberFromString(GB_NB_READ_LONG, data, strlen(data), &conv);

		val->type = GB_T_LONG;
		val->value._long = conv._long.value;
	}
	else if (type == _oid[OID_NUMERIC] || type == _oid[OID_FLOAT4] || type == _oid[OID_FLOAT8])
	{
		GB.NumberFromString(GB_NB_READ_FLOAT, data, strlen(data), &conv);

		val->type = GB_T_FLOAT;
		val->value._float = conv._float.value;
	}
	else if (type == _oid[OID_ABSTIME] || type == _oid[OID_RELTIME] || type == _oid[OID_DATE]
	         || type == _oid[OID_TIME] || type == _oid[OID_TIMESTAMP] || type == _oid[OID_DATETIME]
	         || type == _oid[OID_TIMESTAMPTZ])
	{
		memset(&date, 0, sizeof(date));

		if (len > 3 && strcmp(&data[len - 2], "BC") == 0)
			bc = TRUE;
		else
			bc = FALSE;

		if (type == _oid[OID_ABSTIME] || type == _oid[OID_RELTIME] || type == _oid[OID_DATE])
		{
			sscanf(data, "%4d-%2d-%2d", &date.year, &date.month, &date.day);
		}
		else if (type == _oid[OID_TIME])
		{
			sscanf(data, "%2d:%2d:%lf", &date.hour, &date.min, &sec);
			date.sec = (short)sec;
			date.msec = (short)((sec - date.sec) * 1000 + 0.5);
		}
		else
		{
			sscanf(data, "%4d-%2d-%2d %2d:%2d:%lf", &date.year, &date.month, &date.day, &date.hour, &date.min, &sec);
			date.sec = (short)sec;
			date.msec = (short)((sec - date.sec) * 1000 + 0.5);
		}

		if (bc)
			date.year = (-date.year);

		// 4713-01-01 BC is used for null dates

		if (date.year == -4713 && date.month == 1 && date.day == 1)
			date.year = date.month = date.day = 0;

		GB.MakeDate(&date, (GB_DATE *)&conv);

		val->type = GB_T_DATE;
		val->value._date.date = conv._date.value.date;
		val->value._date.time = conv._date.value.time;
	}
	else if (type == _oid[OID_BYTEA])
	{
		// The BLOB are read by the blob_read() driver function
		// You must set NULL there.
		val->type = GB_T_NULL;
	}
	else
	{
		val->type = GB_T_CSTRING;
		val->value._string = (char *)data;
	}
}


/* Internal function to substitute the table name into a query */

static char *query_param[4];

static void query_get_param(int index, char **str, int *len, char quote)
{
	if (index > 4)
		return;

	index--;
	*str = query_param[index];
	*len = strlen(*str);

	if (quote == '\'')
	{
		*str = get_quote_string(*str, *len, quote);
		*len = GB.StringLength(*str);
	}
}

/* Internal function to run a query */

static int do_query(DB_DATABASE *db, const char *error, PGresult **pres, const char *qtemp, int nsubst, ...)
{
	PGconn *conn = (PGconn *)db->handle;
	va_list args;
	int i;
	const char *query;
	PGresult *res;
	int ret;

	if (nsubst)
	{
		va_start(args, nsubst);
		if (nsubst > 4)
			nsubst = 4;
		for (i = 0; i < nsubst; i++)
			query_param[i] = va_arg(args, char *);

		query = DB.SubstString(qtemp, 0, query_get_param);
	}
	else
		query = qtemp;

	DB.Debug("gb.db.postgresql", "%p: %s", db, query);

	res = PQexec(conn, query);
	ret = check_result(res, error);
	if (!ret)
	{
		if (pres)
			*pres = res;
		else
			PQclear(res);
	}

	db->error = _last_error;
	return ret;
}

/* Internal function to check database version number */

static int db_version(DB_DATABASE *db)
{
	unsigned int verMain = 0, verMajor = 0, verMinor = 0;
	const char *query = "select version()";
	const char *version;
	int dbversion = 0;
	PGresult *res;

	if (!do_query(db, NULL, &res, query, 0))
	{
		version = PQgetvalue(res, 0, 0);
		
		while (*version && !isdigit(*version))
			version++;
		
		if (*version)
		{
			sscanf(version, "%2u.%2u.%2u", &verMain, &verMajor, &verMinor);
			dbversion = ((verMain * 10000) + (verMajor * 100) + verMinor);
		}
		
		PQclear(res);
	}
	
	return dbversion;
}

/* Internal function that fills field information from a schema request.

   The result columns must be ordered that way (from the specified column index):
     - atttypid::int
     - atttypmod
     - attnotnull
     - adsrc
     - atthasdef
*/

static void fill_field_info(DB_DATABASE *db, DB_FIELD *info, PGresult *res, int row, int col)
{
	char *val;
	Oid type;
	GB_VARIANT def;

	info->name = NULL;

	type = atoi(PQgetvalue(res, row, col));
	info->type = conv_type(type);

	info->length = 0;
	if (info->type == GB_T_STRING)
	{
		info->length = atoi(PQgetvalue(res, row, col + 1));
		if (info->length < 0)
			info->length = 0;
		else
			info->length -= 4;
	}

	info->def.type = GB_T_NULL;

	if (conv_boolean(PQgetvalue(res, row, col + 4)) && conv_boolean(PQgetvalue(res, row, col + 2)))
	{
		def.type = GB_T_VARIANT;
		def.value.type = GB_T_NULL;

		val = PQgetvalue(res, row, col + 3);
		if (val && *val)
		{
			if (strncmp(val, "nextval(", 8) == 0)
			{
				if (info->type == GB_T_LONG)
					info->type = DB_T_SERIAL;
			}
			else
			{
				switch(info->type)
				{
					case GB_T_BOOLEAN:
						def.value.type = GB_T_BOOLEAN;
						def.value.value._boolean = (val[1] == 't');
						break;

					default:

						DB.Query.Init();
						if (!unquote_string(val, PQgetlength(res, row, col + 3), DB.Query.AddLength))
							val = DB.Query.Get();

						conv_data(val, -1, &def.value, type);
				}

				GB.StoreVariant(&def, &info->def);
			}
		}
	}

	if (db->flags.no_collation)
		info->collation = NULL;
	else
		info->collation = GB.NewZeroString(PQgetvalue(res, row, col + 5));
}

// Load datatypes

static bool init_datatypes(DB_DATABASE *db)
{
	const char *oid;
	const char *query = "select oid from pg_type where typname = '&1'";
	PGresult *res;
	int i;
	
	for(i = 0;; i++)
	{
		oid = _oid_names[i];
		if (!oid)
			break;
		if (do_query(db, "Unable to initialize datatypes", &res, query, 1, oid))
			return TRUE;
		if (PQntuples(res) == 1)
			_oid[i] = atoi(PQgetvalue(res, 0, 0));

		DB.Debug("gb.db.postgresql", "%p: --> %d", db, _oid[i]);
		PQclear(res);
	}
	
	return FALSE;
}


/*****************************************************************************

	get_quote()

	Returns the character used for quoting object names.

*****************************************************************************/

static const char *get_quote(void)
{
	return QUOTE_STRING;
}

/*****************************************************************************

	open_database()

	Connect to a database.

	<desc> points at a structure describing each connection parameter.
	<db> points at the DB_DATABASE structure that must be initialized.

	This function must return TRUE if the connection has failed.

	The name of the database can be NULL, meaning a default database.

*****************************************************************************/

static const char **_options_keys;
static const char **_options_values;

static void add_option(const char *key, const char *value)
{
	*(const char **)GB.Add(&_options_keys) = key;
	*(const char **)GB.Add(&_options_values) = value;
}

static void add_option_value(const char *key, GB_VALUE *value)
{
	if (GB.Conv(value, GB_T_STRING))
		return;
	
	add_option(key, value->_string.value.addr);
}

static int open_database(DB_DESC *desc, DB_DATABASE *db)
{
	const char *query =
		"show client_encoding";

	PGconn *conn;
	PGresult *res;
	int status;
	char *name;
	char buffer[16];

	if (desc->name)
		name = desc->name;
	else
		name = "template1";

	//fprintf(stderr, "gb.db.postgresql: host = `%s` port = `%s` dbnname = `%s` user = `%s` password = `%s`\n", desc->host, desc->port, dbname, desc->user, desc->password);

	GB.NewArray(&_options_keys, sizeof(char *), 0);
	GB.NewArray(&_options_values, sizeof(char *), 0);
	
	add_option("host", desc->host);
	add_option("port", desc->port);
	add_option("dbname", name);
	add_option("user", desc->user);
	add_option("password", desc->password);
	sprintf(buffer, "%d", db->timeout);
	add_option("connect_timeout", buffer);
	
	DB.GetOptions(add_option_value);
	
	add_option(NULL, NULL);
	
	conn = PQconnectdbParams((const char *const *)_options_keys, (const char *const *)_options_values, FALSE);
	
	GB.FreeArray(&_options_keys);
	GB.FreeArray(&_options_values);

	if (!conn)
	{
		GB.Error("Out of memory");
		return TRUE;
	}

	if (PQstatus(conn) == CONNECTION_BAD)
	{
		GB.Error("Cannot open database: &1", PQerrorMessage(conn));
		PQfinish(conn);
		return TRUE;
	}

	res = PQexec(conn, "set datestyle=ISO");
	status = PQresultStatus(res);

	if (status != PGRES_COMMAND_OK)
	{
		GB.Error("Cannot set 'datestyle' to 'ISO': &1", PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		return TRUE;
	}

	/* get version */

	db->handle = conn;
	db->version = db_version(db);
	db->data = (void *)0; // transaction level

	if (db->version >= 90000)
	{
		res = PQexec(conn, "set bytea_output=escape");
		status = PQresultStatus(res);

		if (status != PGRES_COMMAND_OK)
		{
			GB.Error("Cannot set 'bytea_output' to 'escape': &1", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return TRUE;
		}
	}

	// datatypes
	
	if (init_datatypes(db))
	{
		PQfinish(conn);
		return TRUE;
	}

	/* flags */

	db->flags.no_table_type = TRUE;
	//db->flags.no_nest = TRUE;
	//db->flags.no_case = TRUE;
	db->flags.schema = TRUE;
	db->flags.no_collation = db->version < 90100;

	/* encoding */

	if (PQsetClientEncoding(conn, GB.System.Charset()))
		fprintf(stderr, "gb.db.postgresql: warning: cannot set encoding to %s\n", GB.System.Charset());

	if (!do_query(db, NULL, &res, query, 0))
	{
		db->charset = GB.NewZeroString(PQgetvalue(res, 0, 0));
		PQclear(res);
	}
	else
		db->charset = NULL;
	
	return FALSE;
}

/*****************************************************************************

	close_database()

	Terminates the database connection.

	<db> contains the database handle.

*****************************************************************************/

static void close_database(DB_DATABASE *db)
{
	PGconn *conn = (PGconn *)db->handle;

	if (conn)
		PQfinish(conn);
}


/*****************************************************************************

	get_collations()

	Return the available collations as a Gambas string array.

*****************************************************************************/

static GB_ARRAY get_collations(DB_DATABASE *db)
{
	const char *query =
		"select collname from pg_collation;";

	GB_ARRAY array;
	PGresult *res;
	int i;

	if (db->flags.no_collation)
		return NULL;

	if (do_query(db, "Unable to get collations: &1", &res, query, 0))
		return NULL;

	GB.Array.New(&array, GB_T_STRING, PQntuples(res));

	for (i = 0; i < PQntuples(res); i++)
		*((char **)GB.Array.Get(array, i)) = GB.NewZeroString(PQgetvalue(res, i, 0));

	PQclear(res);

	return array;
}

/*****************************************************************************

	format_value()

	This function transforms a gambas value into a string value that can
	be inserted into a SQL query.

	<arg> points to the value.
	<add> is a callback called to insert the string into the query.

	This function must return TRUE if it translates the value, and FALSE if
	it does not.

	If the value is not translated, then a default translation is used.

*****************************************************************************/

static int format_value(GB_VALUE *arg, DB_FORMAT_CALLBACK add)
{
	int l;
	GB_DATE_SERIAL *date;
	bool bc;

	switch (arg->type)
	{
		case GB_T_BOOLEAN:

			if (VALUE((GB_BOOLEAN *)arg))
				add("TRUE", 4);
			else
				add("FALSE", 5);

			return TRUE;

		case GB_T_STRING:
		case GB_T_CSTRING:

			quote_string(VALUE((GB_STRING *)arg).addr + VALUE((GB_STRING *)arg).start, VALUE((GB_STRING *)arg).len, add);
			return TRUE;

		case GB_T_DATE:

			date = GB.SplitDate((GB_DATE *)arg);

			// Gambas year is between -4801 and +9999
			// PostgreSQL year is between -4713 and +294276
			// So let's use -4713 for representing null dates.

			if (date->year == 0)
			{
				l = sprintf(_buffer, "'4713-01-01 %02d:%02d:%02d BC'", date->hour, date->min, date->sec);
				add(_buffer, l);
			}
			else
			{
				bc = date->year < 0;

				l = sprintf(_buffer, "'%04d-%02d-%02d %02d:%02d:%02d",
					abs(date->year), date->month, date->day,
					date->hour, date->min, date->sec);

				add(_buffer, l);

				if (date->msec)
				{
					l = sprintf(_buffer, ".%03d", date->msec);
					add(_buffer, l);
				}

				if (bc)
					add(" BC", 3);

				add("'", 1);
			}

			return TRUE;

		default:
			return FALSE;
	}
}


/*****************************************************************************

	format_blob()

	This function transforms a blob value into a string value that can
	be inserted into a SQL query.

	<blob> points to the DB_BLOB structure.
	<add> is a callback called to insert the string into the query.

*****************************************************************************/

static void format_blob(DB_BLOB *blob, DB_FORMAT_CALLBACK add)
{
	quote_blob(blob->data, blob->length, add);
}


/*****************************************************************************

	exec_query()

	Send a query to the server and gets the result.

	<db> is the database handle, as returned by open_database()
	<query> is the query string.
	<result> will receive the result handle of the query.
	<err> is an error message used when the query failed.

	<result> can be NULL, when we don't care getting the result.

*****************************************************************************/

static int exec_query(DB_DATABASE *db, const char *query, DB_RESULT *result, const char *err)
{
	return do_query(db, err, (PGresult **)result, query, 0);
}


/*****************************************************************************

	get_last_insert_id()

	Return the value of the last serial field used in an INSERT statement

	<db> is the database handle, as returned by open_database()

*****************************************************************************/

static int64_t get_last_insert_id(DB_DATABASE *db)
{
	PGresult *res;

	if (do_query(db, "Unable to retrieve last insert id: &1", &res, "select lastval()", 0))
		return -1;

	return atoll(PQgetvalue(res, 0, 0));
}


/*****************************************************************************

	query_init()

	Initialize an info structure from a query result.

	<result> is the handle of the query result.
	<info> points to the info structure.
	<count> will receive the number of records returned by the query.

	This function must initialize the info->nfield field with the number of
	field in the query result.

*****************************************************************************/

static void query_init(DB_RESULT result, DB_INFO *info, int *count)
{
	PGresult *res = (PGresult *)result;

	*count = PQntuples(res);

	info->nfield = PQnfields(res);
}


/*****************************************************************************

	query_release()

	Free the info structure filled by query_init() and the result handle.

	<result> is the handle of the query result.
	<info> points to the info structure.
	<invalid> tells if the associated connection has been closed.

*****************************************************************************/

static void query_release(DB_RESULT result, DB_INFO *info, bool invalid)
{
	PQclear((PGresult *)result);
}


/*****************************************************************************

	query_fill()

	Fill a result buffer with the value of each field of a record.

	<db> is the database handle, as returned by open_database()
	<result> is the handle of the result.
	<pos> is the index of the record in the result.
	<buffer> points to an array having one element for each field in the
	result.
	<next> is a boolean telling if we want the next row.

	This function must return DB_OK, DB_ERROR or DB_NO_DATA

	This function must use GB.StoreVariant() to store the value in the
	buffer.

*****************************************************************************/

static int query_fill(DB_DATABASE *db, DB_RESULT result, int pos, GB_VARIANT_VALUE *buffer, int next)
{
	PGresult *res = (PGresult *)result;
	int i;
	char *data;
	GB_VARIANT value;

	for (i = 0; i < PQnfields(res); i++)
	{
		data = PQgetvalue(res, pos, i);

		value.type = GB_T_VARIANT;
		value.value.type = GB_T_NULL;

		if (!PQgetisnull(res, pos, i))
			conv_data(data, PQgetlength(res, pos, i), &value.value, PQftype(res, i));

		GB.StoreVariant(&value, &buffer[i]);
	}

	return DB_OK;
}


/*****************************************************************************

	blob_read()

	Returns the value of a BLOB field.

	<result> is the handle of the result.
	<pos> is the index of the record in the result.
	<blob> points at a DB_BLOB structure that will receive a pointer to the
	data and its length.

*****************************************************************************/

static void blob_read(DB_RESULT result, int pos, int field, DB_BLOB *blob)
{
	PGresult *res = (PGresult *)result;
	char *data;
	int len;

	data = PQgetvalue(res, pos, field);
	len = PQgetlength(res, pos, field);

	DB.Query.Init();
	if (!unquote_blob(data, len, DB.Query.AddLength))
	{
		len = DB.Query.Length();
		data = DB.Query.GetNew();
	}
	else
		blob->constant = TRUE;

	blob->data = data;
	blob->length = len;
}


/*****************************************************************************

	field_name()

	Return the name of a field in a result from its index.

	<result> is the result handle.
	<field> is the field index.

*****************************************************************************/

static char *field_name(DB_RESULT result, int field)
{
	return PQfname((PGresult *)result, field);
}


/*****************************************************************************

	field_index()

	Return the index of a field in a result from its name.

	<result> is the result handle.
	<name> is the field name.
	<db> is needed by this driver to enable table.field syntax

*****************************************************************************/

static int field_index(DB_RESULT result, const char *name, DB_DATABASE *db)
{
	PGresult *res = (PGresult *)result;

	char *fld;
	int index;
	char *table = NULL;
	int numfields, oid;
	char *qfield =
		"select oid from pg_class where relname = '&1' "
		"and ((relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')))";

	numfields = PQnfields(res);
	fld = strrchr(name, (int)FLD_SEP);

	if (fld)
	{
		//Includes table identity.
		//This feature is only available for 7.4 onwards
		/* check version */

		if (db->version > 70399)
		{ // version 7.4.?
			fld[0] = '.';
			table = GB.NewString(name, fld - name);
			fld = fld + 1;

			/* Need to find the OID for the table */
			PGresult *oidres;

			if (do_query(db, "Unable to get OID for table &1", &oidres, qfield, 1, table))
			{
				GB.FreeString(&table);
				return -1;
			}

			if ( PQntuples(oidres) != 1)
			{
				/* Not unique table identifier */
				GB.Error("Table &1 not unique in pg_class", table);
				PQclear(oidres);
				GB.FreeString(&table);
				return -1;
			}

			oid = atoi(PQgetvalue(oidres, 0, 0));
			PQclear(oidres);
			index = PQfnumber(res, fld);

			if (PQftable(res, index) != oid)
			{
				while ( ++index < numfields)
				{
					if (strcasecmp(PQfname(res, index), fld) == 0)
					{ //Check Fieldname
						if (PQftable(res, index) == oid)
						{ //check oid
							break; // is the required table oid
						}
					}
				}

				if ( index == numfields )
				{
					/* field not found for OID */
					GB.Error("Field &1.&2 not found", table, fld);
					GB.FreeString(&table);
					return -1;
				}

			}
		
			GB.FreeString(&table);
		}
		else
		{
			/* Using tablename and fieldname in a non supported version */
			GB.Error("Field &1.&2 not supported below 7.4.1", table, fld);
			return -1;
		}
	}
	else 
	{
		for (index = 0; index < numfields; index++)
		{
			if (strcasecmp(PQfname(res, index), name) == 0)
				break;
		}
		if (index >= numfields)
			index = -1;
	}

	return index;
}


/*****************************************************************************

	field_type()

	Return the Gambas type of a field in a result from its index.

	<result> is the result handle.
	<field> is the field index.

*****************************************************************************/

static GB_TYPE field_type(DB_RESULT result, int field)
{
	return conv_type(PQftype((PGresult *)result, field));
}


/*****************************************************************************

	field_length()

	Return the length of a field in a result from its index.

	<result> is the result handle.
	<field> is the field index.

*****************************************************************************/

static int field_length(DB_RESULT result, int field)
{
	GB_TYPE type = conv_type(PQftype((PGresult *)result, field));
	int len;

	if (type != GB_T_STRING)
		return 0;

	len = PQfmod((PGresult *)result, field);
	if (len < 0)
		len = 0;
	else
		len -= 4;

	return len;
}


/*****************************************************************************

	begin_transaction()

	Begin a transaction.

	<db> is the database handle.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int begin_transaction(DB_DATABASE *db)
{
	int trans = (int)(intptr_t)(db->data) + 1;
	db->data = (void *)(intptr_t)trans;

	if (trans == 1)
	{
		return do_query(db, "Unable to begin transaction: &1", NULL, "BEGIN", 0);
	}
	else
	{
		char buffer[16];
		sprintf(buffer, "%d", trans - 1);
		return do_query(db, "Unable to begin transaction: Unable to define savepoint: &1", NULL, "SAVEPOINT t&1", 1, buffer);
	}
}


/*****************************************************************************

	commit_transaction()

	Commit a transaction.

	<db> is the database handle.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int commit_transaction(DB_DATABASE *db)
{
	int trans = (int)(intptr_t)(db->data) - 1;
	db->data = (void *)(intptr_t)trans;

	if (trans == 0)
	{
		return do_query(db, "Unable to commit transaction: &1", NULL, "COMMIT", 0);
	}
	else
	{
		char buffer[16];
		sprintf(buffer, "%d", trans);
		return do_query(db, "Unable to commit transaction: Unable to release savepoint: &1", NULL, "RELEASE SAVEPOINT t&1", 1, buffer);
	}
}


/*****************************************************************************

	rollback_transaction()

	Rolllback a transaction.

	<db> is the database handle.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int rollback_transaction(DB_DATABASE *db)
{
	int trans = (int)(intptr_t)(db->data) - 1;
	db->data = (void *)(intptr_t)trans;

	if (trans == 0)
	{
		return do_query(db, "Unable to rollback transaction: &1", NULL, "ROLLBACK", 0);
	}
	else
	{
		char buffer[16];
		sprintf(buffer, "%d", trans);
		return do_query(db, "Unable to begin transaction: &1", NULL, "ROLLBACK TO SAVEPOINT t&1", 1, buffer);
	}
}


/*****************************************************************************

	table_init()

	Initialize an info structure from table fields.

	<db> is the database handle.
	<table> is the table name.
	<info> points at the info structure.

	This function must initialize the following info fields:
	- info->nfield must contain the number of fields in the table.
	- info->field is an array of DB_FIELD, one element for each field.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int field_info(DB_DATABASE *db, const char *table, const char *field, DB_FIELD *info);

static int table_init(DB_DATABASE *db, const char *table, DB_INFO *info)
{
	const char *qfield_all;
	char *qfield_schema_all;

	if (db->flags.no_collation)
	{
		qfield_all=
				"SELECT col.attname, col.atttypid::int, col.atttypmod, "
						"col.attnotnull, &1, col.atthasdef "
				"FROM pg_catalog.pg_class tbl, pg_catalog.pg_attribute col "
								"LEFT JOIN pg_catalog.pg_attrdef def ON (def.adnum = col.attnum AND def.adrelid = col.attrelid) "
				"WHERE tbl.relname = '&2' AND "
						"col.attrelid = tbl.oid AND "
						"col.attnum > 0 AND "
						"not col.attisdropped "
				"ORDER BY col.attnum ASC;";

		qfield_schema_all =
			"select pg_attribute.attname, pg_attribute.atttypid::int, pg_attribute.atttypmod, "
							"pg_attribute.attnotnull, &1, pg_attribute.atthasdef "
					"from pg_class, pg_attribute "
							"LEFT JOIN pg_catalog.pg_attrdef  ON (pg_attrdef.adnum = pg_attribute.attnum AND pg_attrdef.adrelid = pg_attribute.attrelid) "
					"where pg_class.relname = '&2' "
							"and (pg_class.relnamespace in (select oid from pg_namespace where nspname = '&3')) "
							"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
							"and pg_attribute.attrelid = pg_class.oid ";
	}
	else
	{
		qfield_all=
				"SELECT col.attname, col.atttypid::int, col.atttypmod, "
						"col.attnotnull, &1, col.atthasdef, pg_collation.collname "
				"FROM pg_catalog.pg_class tbl, pg_catalog.pg_attribute col "
								"LEFT JOIN pg_catalog.pg_attrdef def ON (def.adnum = col.attnum AND def.adrelid = col.attrelid) "
								"LEFT JOIN pg_collation ON (pg_collation.oid = col.attcollation) "
				"WHERE tbl.relname = '&2' AND "
						"col.attrelid = tbl.oid AND "
						"col.attnum > 0 AND "
						"not col.attisdropped "
				"ORDER BY col.attnum ASC;";

		qfield_schema_all =
			"select pg_attribute.attname, pg_attribute.atttypid::int, pg_attribute.atttypmod, "
							"pg_attribute.attnotnull, &1, pg_attribute.atthasdef, pg_collation.collname "
					"from pg_class, pg_attribute "
							"LEFT JOIN pg_catalog.pg_attrdef  ON (pg_attrdef.adnum = pg_attribute.attnum AND pg_attrdef.adrelid = pg_attribute.attrelid) "
								"LEFT JOIN pg_collation ON (pg_collation.oid = pg_attribute.attcollation) "
					"where pg_class.relname = '&2' "
							"and (pg_class.relnamespace in (select oid from pg_namespace where nspname = '&3')) "
							"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
							"and pg_attribute.attrelid = pg_class.oid ";
	}

	PGresult *res;
	int i, n;
	DB_FIELD *f;
	char *schema;

	// Table name
	info->table = GB.NewZeroString(table);

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db,"Unable to get table fields: &1", &res, qfield_all, 2, get_default_value("def.adsrc"), table))
			return TRUE;
	}
	else
	{
		if (do_query(db, "Unable to get table fields: &1", &res, qfield_schema_all, 3, get_default_value("pg_attrdef.adsrc"), table, schema))
			return TRUE;
	}

	info->nfield = n = PQntuples(res);
	if (n == 0)
	{
		PQclear(res);
		return TRUE;
	}

	GB.Alloc(POINTER(&info->field), sizeof(DB_FIELD) * n);

	for (i = 0; i < n; i++)
	{
		f = &info->field[i];

		fill_field_info(db, f, res, i, 1);

		f->name = GB.NewZeroString(PQgetvalue(res, i, 0));
	}

	PQclear(res);
	return FALSE;
}


/*****************************************************************************

	table_index()

	Initialize an info structure from table primary index.

	<db> is the database handle.
	<table> is the table name.
	<info> points at the info structure.

	This function must initialize the following info fields:
	- info->nindex must contain the number of fields in the primary index.
	- info->index is a int[] giving the index of each index field in
		info->fields.

	This function must be called after table_init().

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int table_index(DB_DATABASE *db, const char *table, DB_INFO *info)
{
	const char *qindex;
	const char *qindex_schema;

	PGresult *res;
	int i, j, n;
	const char *fulltable = table;
	char *schema;

	/* Index primaire */

	if (db->version < 80200)
	{
		qindex = "select pg_att1.attname, pg_att1.atttypid::int, pg_cl.relname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_class pg_cl, pg_index pg_ind "
				"where pg_cl.relname = '&1_pkey' AND pg_cl.oid = pg_ind.indexrelid "
					"and (pg_cl.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
					"and pg_att2.attrelid = pg_ind.indexrelid "
					"and pg_att1.attrelid = pg_ind.indrelid "
					"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
				"order by pg_att2.attnum";
		qindex_schema = "select pg_att1.attname, pg_att1.atttypid::int, pg_cl.relname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_class pg_cl, pg_index pg_ind "
				"where pg_cl.relname = '&1_pkey' AND pg_cl.oid = pg_ind.indexrelid "
					"and (pg_cl.relnamespace in (select oid from pg_namespace where nspname = '&2')) "
					"and pg_att2.attrelid = pg_ind.indexrelid "
					"and pg_att1.attrelid = pg_ind.indrelid "
					"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
				"order by pg_att2.attnum";
	}
	else
	{
		qindex = "select pg_att1.attname, pg_att1.atttypid::int, pg_cl.relname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_class pg_cl, pg_index pg_ind, pg_class pg_table "
				"where pg_table.relname = '&1' AND pg_table.oid = pg_att1.attrelid AND pg_cl.oid = pg_ind.indexrelid "
						"and (pg_cl.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
						"and pg_ind.indisprimary "
						"and pg_att2.attrelid = pg_ind.indexrelid "
						"and pg_att1.attrelid = pg_ind.indrelid "
						"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
					"order by pg_att2.attnum";
		qindex_schema = "select pg_att1.attname, pg_att1.atttypid::int, pg_cl.relname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_class pg_cl, pg_index pg_ind, pg_class pg_table "
				"where pg_table.relname = '&1' AND pg_table.oid = pg_att1.attrelid AND pg_cl.oid = pg_ind.indexrelid "
						"and (pg_cl.relnamespace in (select oid from pg_namespace where nspname = '&2')) "
						"and pg_ind.indisprimary "
						"and pg_att2.attrelid = pg_ind.indexrelid "
						"and pg_att1.attrelid = pg_ind.indrelid "
						"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
					"order by pg_att2.attnum";
	}

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to get primary index: &1", &res, qindex, 1, table))
			return TRUE;
	}
	else
	{
		if (do_query(db, "Unable to get primary index: &1", &res, qindex_schema, 2, table, schema))
			return TRUE;
	}

	n = info->nindex = PQntuples(res);

	if (n <= 0)
	{
		GB.Error("Table '&1' has no primary index", fulltable);
		PQclear(res);
		return TRUE;
	}

	GB.Alloc(POINTER(&info->index), sizeof(int) * n);

	for (i = 0; i < n; i++)
	{
		for (j = 0; j < info->nfield; j++)
		{
			if (strcmp(info->field[j].name, PQgetvalue(res, i, 0)) == 0)
			{
				info->index[i] = j;
				/*printf("index[%d] = %d\n", i, j);*/
				break;
			}
		}
	}

	PQclear(res);

	return FALSE;
}


/*****************************************************************************

	table_release()

	Free the info structure filled by table_init() and/or table_index()

	<db> is the database handle.
	<info> points at the info structure.

*****************************************************************************/

static void table_release(DB_DATABASE *db, DB_INFO *info)
{
	/* All is done outside the driver */
}


/*****************************************************************************

	table_exist()

	Returns if a table exists

	<db> is the database handle.
	<table> is the table name.

	This function returns TRUE if the table exists, and FALSE if not.

*****************************************************************************/

static int table_exist(DB_DATABASE *db, const char *table)
{
	const char *query =
		"select relname from pg_class where (relkind = 'r' or relkind = 'v'or relkind = 'm') "
		"and (relname = '&1') "
		"and (relnamespace not in (select oid from pg_namespace where nspname = 'information_schema'))";

	const char *query_schema =
		"select relname from pg_class where (relkind = 'r' or relkind = 'v' or relkind = 'm') "
		"and (relname = '&1') "
		"and (relnamespace in (select oid from pg_namespace where nspname = '&2'))";

		/*"select pg_class.relname,pg_namespace.nspname from pg_class,pg_namespace where (pg_class.relkind = 'r' or pg_class.relkind = 'v') "
		"and (pg_namespace.oid = pg_class.relnamespace) "
		"and (pg_class.relname = '&1') "
		"and (pg_namespace.nspname = '&2') "
		"and (pg_namespace.oid not in (select oid from pg_namespace where nspname = 'information_schema'))";*/

	PGresult *res;
	int exist;
	char *schema;

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to check table: &1", &res, query, 1, table))
			return FALSE;
	}
	else
	{
		if (do_query(db, "Unable to check table: &1", &res, query_schema, 2, table, schema))
			return FALSE;
	}

	exist = PQntuples(res) == 1;

	PQclear(res);

	return exist;
}



/*****************************************************************************

	table_list()

	Returns an array containing the name of each table in the database

	<db> is the database handle.
	<tables> points to a variable that will receive the char* array.

	This function returns the number of tables, or -1 if the command has
	failed.

	Be careful: <tables> can be NULL, so that just the count is returned.

*****************************************************************************/

static int table_list_73(DB_DATABASE *db, char ***tables)
{
	const char *query =
		"select pg_class.relname,pg_namespace.nspname from pg_class,pg_namespace where (pg_class.relkind = 'r' or pg_class.relkind = 'v' or pg_class.relkind = 'm') "
		"and (pg_namespace.oid = pg_class.relnamespace) "
		"and (pg_namespace.oid not in (select oid from pg_namespace where nspname = 'information_schema'))";

	PGresult *res;
	int i;
	int count;
	char *schema;

	if (do_query(db, "Unable to get tables: &1", &res, query, 0))
		return -1;

	if (tables)
	{
		GB.NewArray(tables, sizeof(char *), PQntuples(res));

		for (i = 0; i < PQntuples(res); i++)
		{
			schema = PQgetvalue(res, i, 1);
			if (!strcmp(schema, "public")) // || !strcmp(schema, "pg_catalog"))
				(*tables)[i] = GB.NewZeroString(PQgetvalue(res, i, 0));
			else
			{
				(*tables)[i] = GB.NewZeroString(schema);
				(*tables)[i] = GB.AddChar((*tables)[i], '.');
				(*tables)[i] = GB.AddString((*tables)[i], PQgetvalue(res, i, 0), 0);
			}
		}
	}

	count = PQntuples(res);

	PQclear(res);

	return count;
}

#if 0
static int table_list_74(DB_DATABASE handle, char ***tables)
{
	const char *query1 =
		"select tablename from pg_tables where schemaname <> 'information_schema'";

	const char *query2 =
		"select viewname from pg_views where schemaname <> 'information_schema'";

	PGconn *conn = (PGconn *)handle;
	PGresult *res1, *res2;
	int i, j;
	int count;

	if (do_query(conn, "Unable to get tables: &1", &res1, query1, 0))
		return -1;

	if (do_query(conn, "Unable to get tables: &1", &res2, query2, 0))
		return -1;

	count = PQntuples(res1) + PQntuples(res2);

	if (tables)
	{
		GB.NewArray(tables, sizeof(char *), count);

		j = 0;

		for (i = 0; i < PQntuples(res1); i++, j++)
			GB.NewString(&((*tables)[j]), PQgetvalue(res1, i, 0), 0);

		for (i = 0; i < PQntuples(res2); i++, j++)
			GB.NewString(&((*tables)[j]), PQgetvalue(res2, i, 0), 0);
	}

	PQclear(res1);
	PQclear(res2);

	return count;
}
#endif

static int table_list(DB_DATABASE *db, char ***tables)
{
	//if (version < 70400)
		return table_list_73(db, tables);
	//else
	//  return table_list_74(handle, tables);
}



/*****************************************************************************

	table_primary_key()

	Returns a string representing the primary key of a table.

	<db> is the database handle.
	<table> is the table name.
	<key> points to a string that will receive the primary key.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int table_primary_key(DB_DATABASE *db, const char *table, char ***primary)
{
	const char *query;
	const char *query_schema;
	PGresult *res;
	int i;
	char *schema;

	if (db->version < 80200)
	{
		query = "select pg_att1.attname, pg_att1.atttypid::int, pg_cl.relname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_class pg_cl, pg_index pg_ind "
				"where pg_cl.relname = '&1_pkey' AND pg_cl.oid = pg_ind.indexrelid "
					"and (pg_cl.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
					"and pg_att2.attrelid = pg_ind.indexrelid "
					"and pg_att1.attrelid = pg_ind.indrelid "
					"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
				"order by pg_att2.attnum";
		query_schema = "select pg_att1.attname, pg_att1.atttypid::int, pg_cl.relname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_class pg_cl, pg_index pg_ind "
				"where pg_cl.relname = '&1_pkey' AND pg_cl.oid = pg_ind.indexrelid "
					"and (pg_cl.relnamespace in (select oid from pg_namespace where nspname = '&2')) "
					"and pg_att2.attrelid = pg_ind.indexrelid "
					"and pg_att1.attrelid = pg_ind.indrelid "
					"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
				"order by pg_att2.attnum";
	}
	else
	{
		query = "select pg_att1.attname, pg_att1.atttypid::int, pg_cl.relname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_class pg_cl, pg_index pg_ind, pg_class pg_table "
				"where pg_table.relname = '&1' AND pg_table.oid = pg_att1.attrelid AND pg_cl.oid = pg_ind.indexrelid "
					"and (pg_cl.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
					"and pg_ind.indisprimary "
					"and pg_att2.attrelid = pg_ind.indexrelid "
					"and pg_att1.attrelid = pg_ind.indrelid "
					"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
				"order by pg_att2.attnum";
		query_schema = "select pg_att1.attname, pg_att1.atttypid::int, pg_cl.relname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_class pg_cl, pg_index pg_ind, pg_class pg_table "
				"where pg_table.relname = '&1' AND pg_table.oid = pg_att1.attrelid AND pg_cl.oid = pg_ind.indexrelid "
					"and (pg_cl.relnamespace in (select oid from pg_namespace where nspname = '&2')) "
					"and pg_ind.indisprimary "
					"and pg_att2.attrelid = pg_ind.indexrelid "
					"and pg_att1.attrelid = pg_ind.indrelid "
					"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
				"order by pg_att2.attnum";
	}

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to get primary key: &1", &res, query, 1, table))
			return TRUE;
	}
	else
	{
		if (do_query(db, "Unable to get primary key: &1", &res, query_schema, 2, table, schema))
			return TRUE;
	}

	GB.NewArray(primary, sizeof(char *), PQntuples(res));

	for (i = 0; i < PQntuples(res); i++)
		(*primary)[i] = GB.NewZeroString(PQgetvalue(res, i, 0));

	PQclear(res);

	return FALSE;
}


/*****************************************************************************

	table_is_system()

	Returns if a table is a system table.

	<db> is the database handle.
	<table> is the table name.

	This function returns TRUE if the table is a system table, and FALSE if
	not.

*****************************************************************************/

static int table_is_system(DB_DATABASE *db, const char *table)
{
	const char *query =
		"select 1 from pg_class where (relkind = 'r' or relkind = 'v'or relkind = 'm') "
		"and (relname = '&1') "
		"and (relnamespace in (select oid from pg_namespace where nspname = 'pg_catalog'))";

	//const char *query1 =
	//  "select 1 from pg_tables where tablename = lower('&1') and schemaname = 'public'";

	const char *query2 =
		"select 1 from pg_views where viewname = '&1' and schemaname = 'pg_catalog'";

	PGresult *res;
	int exist;
	char *schema;

	get_table_schema(&table, &schema);

	if (schema)
		return !strcmp(schema, "pg_catalog");

	if (do_query(db, "Unable to check table: &1", &res, query, 1, table))
		return TRUE;

	exist = PQntuples(res) == 1;
	PQclear(res);

	if (exist)
		return TRUE;

	if (do_query(db, "Unable to check table: &1", &res, query2, 1, table))
		return TRUE;

	exist = PQntuples(res) == 1;
	PQclear(res);

	if (exist)
		return TRUE;

	return FALSE;
}


/*****************************************************************************

	table_delete()

	Deletes a table.

	<db> is the database handle.
	<table> is the table name.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int table_delete(DB_DATABASE *db, const char *table)
{
	return
		do_query(db, "Unable to delete table: &1", NULL,
			"drop table &1", 1, get_quoted_table(table));
}


/*****************************************************************************

	table_create()

	Creates a table.

	<db> is the database handle.
	<table> is the table name.
	<fields> points to a linked list of field descriptions.
	<key> is the primary key.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int table_create(DB_DATABASE *db, const char *table, DB_FIELD *fields, char **primary, const char *not_used)
{
	DB_FIELD *fp;
	int comma;
	char *type;
	int i;

	DB.Query.Init();

	DB.Query.Add("CREATE TABLE ");
	DB.Query.Add(get_quoted_table(table));
	DB.Query.Add(" ( ");

	comma = FALSE;
	for (fp = fields; fp; fp = fp->next)
	{
		if (comma)
			DB.Query.Add(", ");
		else
			comma = TRUE;

		DB.Query.Add(QUOTE_STRING);
		DB.Query.Add(fp->name);
		DB.Query.Add(QUOTE_STRING);

		if (fp->type == DB_T_SERIAL)
		{
			DB.Query.Add(" BIGSERIAL ");
		}
		else if (fp->type == DB_T_BLOB)
		{
			DB.Query.Add(" BYTEA ");
		}
		else
		{
			switch (fp->type)
			{
				case GB_T_BOOLEAN: type = "BOOL"; break;
				case GB_T_INTEGER: type = "INT"; break;
				case GB_T_LONG: type = "BIGINT"; break;
				case GB_T_FLOAT: type = "FLOAT8"; break;
				case GB_T_DATE: type = "TIMESTAMP"; break;
				case GB_T_STRING:

					if (fp->length <= 0)
						type = "TEXT";
					else
					{
						sprintf(_buffer, "VARCHAR(%d)", fp->length);
						type = _buffer;
					}

					break;

				default: type = "TEXT"; break;
			}

			DB.Query.Add(" ");
			DB.Query.Add(type);

			if (fp->collation && *fp->collation)
			{
				DB.Query.Add(" COLLATE \"");
				DB.Query.Add(fp->collation);
				DB.Query.Add("\"");
			}

			if (fp->def.type != GB_T_NULL)
			{
				DB.Query.Add(" NOT NULL DEFAULT ");
				DB.FormatVariant(&_driver, &fp->def, DB.Query.AddLength);
			}
			else if (DB.StringArray.Find(primary, fp->name) >= 0)
			{
				DB.Query.Add(" NOT NULL ");
			}
		}
	}

	if (primary)
	{
		DB.Query.Add(", PRIMARY KEY (");

		for (i = 0; i < GB.Count(primary); i++)
		{
			if (i > 0)
				DB.Query.Add(",");

			DB.Query.Add(QUOTE_STRING);
			DB.Query.Add(primary[i]);
			DB.Query.Add(QUOTE_STRING);
		}

		DB.Query.Add(")");
	}

	DB.Query.Add(" )");

	return do_query(db, "Cannot create table: &1", NULL, DB.Query.Get(), 0);
}


/*****************************************************************************

	field_exist()

	Returns if a field exists in a given table

	<db> is the database handle.
	<table> is the table name.
	<field> is the field name.

	This function returns TRUE if the field exists, and FALSE if not.

*****************************************************************************/

static int field_exist(DB_DATABASE *db, const char *table, const char *field)
{
	const char *query = "select pg_attribute.attname from pg_class, pg_attribute "
		"where pg_class.relname = '&1' "
		"and (pg_class.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
		"and pg_attribute.attname = '&2' "
		"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
		"and pg_attribute.attrelid = pg_class.oid ";

	const char *query_schema = "select pg_attribute.attname from pg_class, pg_attribute "
		"where pg_class.relname = '&1' "
		"and (pg_class.relnamespace in (select oid from pg_namespace where nspname = '&3')) "
		"and pg_attribute.attname = '&2' "
		"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
		"and pg_attribute.attrelid = pg_class.oid ";

	PGresult *res;
	int exist;
	char *schema;

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to check field: &1", &res, query, 2, table, field))
			return FALSE;
	}
	else
	{
		if (do_query(db, "Unable to check field: &1", &res, query_schema, 3, table, field, schema))
			return FALSE;
	}

	exist = PQntuples(res) == 1;

	PQclear(res);

	return exist;
}


/*****************************************************************************

	field_list()

	Returns an array containing the name of each field in a given table

	<db> is the database handle.
	<table> is the table name.
	<fields> points to a variable that will receive the char* array.

	This function returns the number of fields, or -1 if the command has
	failed.

	Be careful: <fields> can be NULL, so that just the count is returned.

*****************************************************************************/

static int field_list(DB_DATABASE *db, const char *table, char ***fields)
{
	const char *query = "select pg_attribute.attname from pg_class, pg_attribute "
		"where pg_class.relname = '&1' "
		"and (pg_class.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
		"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
		"and pg_attribute.attrelid = pg_class.oid";

	const char *query_schema = "select pg_attribute.attname from pg_class, pg_attribute "
		"where pg_class.relname = '&1' "
		"and (pg_class.relnamespace in (select oid from pg_namespace where nspname = '&2')) "
		"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
		"and pg_attribute.attrelid = pg_class.oid";

	PGresult *res;
	int i;
	int count;
	char *schema;

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to get fields: &1", &res, query, 1, table))
			return -1;
	}
	else
	{
		if (do_query(db, "Unable to get fields: &1", &res, query_schema, 2, table, schema))
			return -1;
	}

	if (fields)
	{
		GB.NewArray(fields, sizeof(char *), PQntuples(res));

		for (i = 0; i < PQntuples(res); i++)
			(*fields)[i] = GB.NewZeroString(PQgetvalue(res, i, 0));
	}

	count = PQntuples(res);

	PQclear(res);
	return count;
}


/*****************************************************************************

	field_info()

	Get field description

	<db> is the database handle.
	<table> is the table name.
	<field> is the field name.
	<info> points to a structure filled by the function.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int field_info(DB_DATABASE *db, const char *table, const char *field, DB_FIELD *info)
{
	const char *query;
	const char *query_schema;

	if (db->flags.no_collation)
	{
		query =
			"select pg_attribute.attname, pg_attribute.atttypid::int, "
			"pg_attribute.atttypmod, pg_attribute.attnotnull, &1, pg_attribute.atthasdef "
			"from pg_class, pg_attribute "
			"left join pg_attrdef on (pg_attrdef.adrelid = pg_attribute.attrelid and pg_attrdef.adnum = pg_attribute.attnum) "
			"where pg_class.relname = '&2' "
			"and (pg_class.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
			"and pg_attribute.attname = '&3' "
			"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
			"and pg_attribute.attrelid = pg_class.oid";

		query_schema =
			"select pg_attribute.attname, pg_attribute.atttypid::int, "
			"pg_attribute.atttypmod, pg_attribute.attnotnull, &1, pg_attribute.atthasdef "
			"from pg_class, pg_attribute "
			"left join pg_attrdef on (pg_attrdef.adrelid = pg_attribute.attrelid and pg_attrdef.adnum = pg_attribute.attnum) "
			"where pg_class.relname = '&2' "
			"and (pg_class.relnamespace in (select oid from pg_namespace where nspname = '&4')) "
			"and pg_attribute.attname = '&3' "
			"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
			"and pg_attribute.attrelid = pg_class.oid";
	}
	else
	{
		query =
			"select pg_attribute.attname, pg_attribute.atttypid::int, "
			"pg_attribute.atttypmod, pg_attribute.attnotnull, &1, pg_attribute.atthasdef, pg_collation.collname "
			"from pg_class, pg_attribute "
			"left join pg_attrdef on (pg_attrdef.adrelid = pg_attribute.attrelid and pg_attrdef.adnum = pg_attribute.attnum) "
			"left join pg_collation on (pg_collation.oid = pg_attribute.attcollation) "
			"where pg_class.relname = '&2' "
			"and (pg_class.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
			"and pg_attribute.attname = '&3' "
			"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
			"and pg_attribute.attrelid = pg_class.oid";

		query_schema =
			"select pg_attribute.attname, pg_attribute.atttypid::int, "
			"pg_attribute.atttypmod, pg_attribute.attnotnull, &1, pg_attribute.atthasdef, pg_collation.collname "
			"from pg_class, pg_attribute "
			"left join pg_attrdef on (pg_attrdef.adrelid = pg_attribute.attrelid and pg_attrdef.adnum = pg_attribute.attnum) "
			"left join pg_collation on (pg_collation.oid = pg_attribute.attcollation) "
			"where pg_class.relname = '&2' "
			"and (pg_class.relnamespace in (select oid from pg_namespace where nspname = '&4')) "
			"and pg_attribute.attname = '&3' "
			"and pg_attribute.attnum > 0 and not pg_attribute.attisdropped "
			"and pg_attribute.attrelid = pg_class.oid";
	}

	PGresult *res;
	char *schema;
	const char *fulltable = table;

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to get field info: &1", &res, query, 3, get_default_value("pg_attrdef.adsrc"), table, field))
			return TRUE;
	}
	else
	{
		if (do_query(db, "Unable to get field info: &1", &res, query_schema, 4, get_default_value("pg_attrdef.adsrc"), table, field, schema))
			return TRUE;
	}

	if (PQntuples(res) != 1)
	{
		GB.Error("Unable to find field &1.&2", fulltable, field);
		return TRUE;
	}

	fill_field_info(db, info, res, 0, 1);

	PQclear(res);
	return FALSE;
}


/*****************************************************************************

	index_exist()

	Returns if an index exists in a given table

	<db> is the database handle.
	<table> is the table name.
	<field> is the index name.

	This function returns TRUE if the index exists, and FALSE if not.

*****************************************************************************/

static int index_exist(DB_DATABASE *db, const char *table, const char *index)
{
	const char *query =
		"select pg_class.relname from pg_class, pg_index, pg_class pg_class2 "
		"where pg_class2.relname = '&1' "
		"and (pg_class2.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
		"and pg_index.indrelid = pg_class2.oid "
		"and pg_index.indexrelid = pg_class.oid "
		"and pg_class.relname = '&2'";

	const char *query_schema =
		"select pg_class.relname from pg_class, pg_index, pg_class pg_class2 "
		"where pg_class2.relname = '&1' "
		"and (pg_class2.relnamespace in (select oid from pg_namespace where nspname = '&3')) "
		"and pg_index.indrelid = pg_class2.oid "
		"and pg_index.indexrelid = pg_class.oid "
		"and pg_class.relname = '&2'";

	/*const char *query = "select relname from pg_class, pg_index "
		"where pg_class.relname = '&1' "
		"and pg_index.indexrelid = pg_class.oid ";*/

	PGresult *res;
	int exist;
	char *schema;

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to check index: &1", &res, query, 2, table, index))
			return FALSE;
	}
	else
	{
		if (do_query(db, "Unable to check index: &1", &res, query_schema, 3, table, index, schema))
			return FALSE;
	}

	exist = PQntuples(res) == 1;

	PQclear(res);

	return exist;
}


/*****************************************************************************

	index_list()

	Returns an array containing the name of each index in a given table

	<db> is the database handle.
	<table> is the table name.
	<indexes> points to a variable that will receive the char* array.

	This function returns the number of indexes, or -1 if the command has
	failed.

	Be careful: <indexes> can be NULL, so that just the count is returned.

*****************************************************************************/

static int index_list(DB_DATABASE *db, const char *table, char ***indexes)
{
	const char *query = "select pg_class.relname from pg_class, pg_index, pg_class pg_class2 "
		"where pg_class2.relname = '&1' "
		"and (pg_class2.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
		"and pg_index.indrelid = pg_class2.oid "
		"and pg_index.indexrelid = pg_class.oid ";

	const char *query_schema = "select pg_class.relname from pg_class, pg_index, pg_class pg_class2 "
		"where pg_class2.relname = '&1' "
		"and (pg_class2.relnamespace in (select oid from pg_namespace where nspname = '&2')) "
		"and pg_index.indrelid = pg_class2.oid "
		"and pg_index.indexrelid = pg_class.oid ";

	PGresult *res;
	int i;
	int count;
	char *schema;

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to get indexes: &1", &res, query, 1, table))
			return TRUE;
	}
	else
	{
		if (do_query(db, "Unable to get indexes: &1", &res, query_schema, 2, table, schema))
			return TRUE;
	}

	if (indexes)
	{
		GB.NewArray(indexes, sizeof(char *), PQntuples(res));

		for (i = 0; i < PQntuples(res); i++)
			(*indexes)[i] = GB.NewZeroString(PQgetvalue(res, i, 0));
	}

	count = PQntuples(res);
	PQclear(res);

	return count;
}


/*****************************************************************************

	index_info()

	Get index description

	<db> is the database handle.
	<table> is the table name.
	<field> is the index name.
	<info> points to a structure filled by the function.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int index_info(DB_DATABASE *db, const char *table, const char *index, DB_INDEX *info)
{
	const char *query =
		"select indisunique, indisprimary, indexrelid from pg_class, pg_index, pg_class pg_class2 "
		"where pg_class2.relname = '&1' "
		"and (pg_class2.relnamespace not in (select oid from pg_namespace where nspname = 'information_schema')) "
		"and pg_index.indrelid = pg_class2.oid "
		"and pg_index.indexrelid = pg_class.oid "
		"and pg_class.relname = '&2'";

	const char *query_schema =
		"select indisunique, indisprimary, indexrelid from pg_class, pg_index, pg_class pg_class2 "
		"where pg_class2.relname = '&1' "
		"and (pg_class2.relnamespace in (select oid from pg_namespace where nspname = '&3')) "
		"and pg_index.indrelid = pg_class2.oid "
		"and pg_index.indexrelid = pg_class.oid "
		"and pg_class.relname = '&2'";

	const char *query_field =
		"select pg_att1.attname "
			"from pg_attribute pg_att1, pg_attribute pg_att2, pg_index pg_ind "
				"where pg_ind.indexrelid = &1 "
					"and pg_att2.attrelid = pg_ind.indexrelid "
					"and pg_att1.attrelid = pg_ind.indrelid "
					"and pg_att1.attnum = pg_ind.indkey[pg_att2.attnum-1] "
				"order by pg_att2.attnum";

	PGresult *res;
	char indexrelid[16];
	int i;
	char *schema;
	const char *fulltable = table;

	if (get_table_schema(&table, &schema))
	{
		if (do_query(db, "Unable to get index info: &1", &res, query, 2, table, index))
			return TRUE;
	}
	else
	{
		if (do_query(db, "Unable to get index info: &1", &res, query_schema, 3, table, index, schema))
			return TRUE;
	}

	if (PQntuples(res) != 1)
	{
		GB.Error("Unable to find index &1.&2", fulltable, index);
		return TRUE;
	}

	info->name = NULL;
	info->unique = conv_boolean(PQgetvalue(res, 0, 0));
	info->primary = conv_boolean(PQgetvalue(res, 0, 1));

	strcpy(indexrelid, PQgetvalue(res, 0, 2));

	PQclear(res);

	if (do_query(db, "Unable to get index info: &1", &res, query_field, 1, indexrelid))
		return TRUE;

	DB.Query.Init();

	for (i = 0; i < PQntuples(res); i++)
	{
		if (i > 0)
			DB.Query.Add(",");
		DB.Query.Add(PQgetvalue(res, i, 0));
	}

	PQclear(res);

	info->fields = DB.Query.GetNew();

	return FALSE;
}


/*****************************************************************************

	index_delete()

	Deletes an index.

	<db> is the database handle.
	<table> is the table name.
	<index> is the index name.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int index_delete(DB_DATABASE *db, const char *table, const char *index)
{
	return
		do_query(db, "Unable to delete index: &1", NULL,
			"drop index &1", 1, get_quoted_table(index));
}


/*****************************************************************************

	index_create()

	Creates an index.

	<db> is the database handle.
	<table> is the table name.
	<index> is the index name.
	<info> points to a structure describing the index.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int index_create(DB_DATABASE *db, const char *table, const char *index, DB_INDEX *info)
{
	DB.Query.Init();

	DB.Query.Add("CREATE ");
	if (info->unique)
		DB.Query.Add("UNIQUE ");
	DB.Query.Add("INDEX \"");
	DB.Query.Add(index);
	DB.Query.Add("\" ON ");
	DB.Query.Add(get_quoted_table(table));
	DB.Query.Add(" ( ");
	DB.Query.Add(info->fields);
	DB.Query.Add(" )");

	return do_query(db, "Cannot create index: &1", NULL, DB.Query.Get(), 0);
}


/*****************************************************************************

	database_exist()

	Returns if a database exists

	<db> is the database handle.
	<name> is the database name.

	This function returns TRUE if the database exists, and FALSE if not.

*****************************************************************************/

static int database_exist(DB_DATABASE *db, const char *name)
{
	const char *query =
		"select datname from pg_database where (datallowconn = 't') "
		"and (datname = '&1')";

	PGresult *res;
	int exist;

	if (do_query(db, "Unable to check database: &1", &res, query, 1, name))
		return FALSE;

	exist = PQntuples(res) == 1;

	PQclear(res);

	return exist;
}



/*****************************************************************************

	database_list()

	Returns an array containing the name of each database

	<db> is the database handle.
	<databases> points to a variable that will receive the char* array.

	This function returns the number of databases, or -1 if the command has
	failed.

	Be careful: <databases> can be NULL, so that just the count is returned.

*****************************************************************************/

static int database_list(DB_DATABASE *db, char ***databases)
{
	const char *query =
		"select datname from pg_database where datallowconn and datname <> 'template1'";

	PGresult *res;
	int i;
	int count;

	if (do_query(db, "Unable to get databases: &1", &res, query, 0))
		return -1;

	if (databases)
	{
		GB.NewArray(databases, sizeof(char *), PQntuples(res));

		for (i = 0; i < PQntuples(res); i++)
			(*databases)[i] = GB.NewZeroString(PQgetvalue(res, i, 0));
	}

	count = PQntuples(res);

	PQclear(res);

	return count;
}


/*****************************************************************************

	database_is_system()

	Returns if a database is a system database.

	<db> is the database handle.
	<name> is the database name.

	This function returns TRUE if the database is a system database, and
	FALSE if not.

*****************************************************************************/

static int database_is_system(DB_DATABASE *db, const char *name)
{
	const char *query =
		"select datname from pg_database where datallowconn "
		"and (datname = '&1') and datistemplate";
		//"and (datname = lower('&1')) and datistemplate";

	PGresult *res;
	int system;

	if (do_query(db, "Unable to check database: &1", &res, query, 1, name))
		return TRUE;

	system = PQntuples(res) == 1;

	PQclear(res);

	return system;
}

/*****************************************************************************

	table_type()

	Returns or sets the table type.

	<db> is the database handle.
	<table> is the table name.
	<newtype> is the new type

	This function returns the table type.

	NOTE: Only mySQL seems to have table types at the moment.

*****************************************************************************/

static char *table_type(DB_DATABASE *db, const char *table, const char *newtype)
{
	if (newtype)
		GB.Error("PostgreSQL does not have any table types");

	return NULL;
}

/*****************************************************************************

	database_delete()

	Deletes a database.

	<db> is the database handle.
	<name> is the database name.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int database_delete(DB_DATABASE *db, const char *name)
{
	return
		do_query(db, "Unable to delete database: &1", NULL,
			"drop database \"&1\"", 1, name);
}


/*****************************************************************************

	database_create()

	Creates a database.

	<db> is the database handle.
	<name> is the database name.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int database_create(DB_DATABASE *db, const char *name)
{
	return
		do_query(db, "Unable to create database: &1", NULL,
			"create database \"&1\"", 1, name);
}


/*****************************************************************************

	user_exist()

	Returns if a user exists.

	<db> is the database handle.
	<name> is the user name.

	This function returns TRUE if the user exists, and FALSE if not.

*****************************************************************************/

static int user_exist(DB_DATABASE *db, const char *name)
{
	const char *query = "select usename from pg_user "
		"where usename = '&1' ";

	PGresult *res;
	int exist;

	if (do_query(db, "Unable to check user: &1", &res, query, 1, name))
		return FALSE;

	exist = PQntuples(res) == 1;

	PQclear(res);

	return exist;
}


/*****************************************************************************

	user_list()

	Returns an array containing the name of each user.

	<db> is the database handle.
	<users> points to a variable that will receive the char* array.

	This function returns the number of users, or -1 if the command has
	failed.

	Be careful: <users> can be NULL, so that just the count is returned.

*****************************************************************************/

static int user_list(DB_DATABASE *db, char ***users)
{
	const char *query = "select usename from pg_user ";

	PGresult *res;
	int i;
	int count;

	if (do_query(db, "Unable to get users: &1", &res, query, 0))
		return -1;

	if (users)
	{
		GB.NewArray(users, sizeof(char *), PQntuples(res));

		for (i = 0; i < PQntuples(res); i++)
			(*users)[i] = GB.NewZeroString(PQgetvalue(res, i, 0));
	}

	count = PQntuples(res);

	PQclear(res);
	return count;
}


/*****************************************************************************

	user_info()

	Get user description

	<db> is the database handle.
	<name> is the user name.
	<info> points to a structure filled by the function.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int user_info(DB_DATABASE *db, const char *name, DB_USER *info)
{
	const char *query =
		"select usecreatedb, usesuper from pg_user "
		"where usename = '&1' ";

	const char *query_passwd =
		"select passwd from pg_shadow "
		"where usename = '&1' ";

	PGresult *res;

	if (do_query(db, "Unable to get user info: &1", &res, query, 1, name))
		return TRUE;

	if (PQntuples(res) != 1)
	{
		GB.Error("Unable to find user &1", name);
		return TRUE;
	}

	info->name = NULL;
	info->admin = conv_boolean(PQgetvalue(res, 0, 1));
	/* conv_boolean(PQgetvalue(res, 0, 0)) || */

	PQclear(res);

	if (!do_query(db, NULL, &res, query_passwd, 1, name))
	{
		if (*PQgetvalue(res, 0, 0))
			info->password = GB.NewString("***", 3);
	}

	return FALSE;
}


/*****************************************************************************

	user_delete()

	Deletes a user.

	<db> is the database handle.
	<name> is the user name.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int user_delete(DB_DATABASE *db, const char *name)
{
	return
		do_query(db, "Unable to delete user: &1", NULL,
			"drop user \"&1\"", 1, name);
}


/*****************************************************************************

	user_create()

	Creates a user.

	<db> is the database handle.
	<name> is the user name.
	<info> points to a structure describing the user.

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int user_create(DB_DATABASE *db, const char *name, DB_USER *info)
{
	DB.Query.Init();

	DB.Query.Add("CREATE USER ");
	DB.Query.Add(QUOTE_STRING);
	DB.Query.Add(name);
	DB.Query.Add(QUOTE_STRING);

	if (info->admin)
		DB.Query.Add(" CREATEDB CREATEUSER");
	else
		DB.Query.Add(" NOCREATEDB NOCREATEUSER");

	if (info->password && *info->password)
	{
		DB.Query.Add(" PASSWORD '");
		DB.Query.Add(info->password);
		DB.Query.Add("'");
	}

	/*_print_query = TRUE;*/
	return do_query(db, "Cannot create user: &1", NULL, DB.Query.Get(), 0);
}


/*****************************************************************************

	user_set_password()

	Change the user password.

	<db> is the database handle.
	<name> is the user name.
	<password> is the new password

	This function returns TRUE if the command has failed, and FALSE if
	everything was OK.

*****************************************************************************/

static int user_set_password(DB_DATABASE *db, const char *name, const char *password)
{
	DB.Query.Init();

	DB.Query.Add("ALTER USER \"");
	DB.Query.Add(name);
	DB.Query.Add("\" PASSWORD '");
	DB.Query.Add(password);
	DB.Query.Add("'");

	return
		do_query(db, "Cannot change user password: &1",
			NULL, DB.Query.Get(), 0);
}

/*****************************************************************************

	The driver interface

*****************************************************************************/

DECLARE_DRIVER(_driver, "postgresql");

/*****************************************************************************

	The component entry and exit functions.

*****************************************************************************/

int EXPORT GB_INIT(void)
{
	GB.GetInterface("gb.db", DB_INTERFACE_VERSION, &DB);
	DB.Register(&_driver);

	return 0;
}

void EXPORT GB_EXIT()
{
}
