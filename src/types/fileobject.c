#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "object.h"
#include "strobject.h"
#include "iter.h"
#include "exc.h"
#include "str.h"
#include "strbuf.h"
#include "util.h"
#include "fileobject.h"

#define isopen(file)   ((file->flags) & RHO_FILE_FLAG_OPEN)
#define close(file)    ((file->flags) &= ~RHO_FILE_FLAG_OPEN)
#define canread(file)  ((file->flags) & (RHO_FILE_FLAG_READ | RHO_FILE_FLAG_UPDATE))
#define canwrite(file) ((file->flags) & (RHO_FILE_FLAG_WRITE | RHO_FILE_FLAG_UPDATE))

static int parse_mode(const char *mode)
{
	int flags = 0;

	if (mode[0] == '\0') {
		return -1;
	}

	switch (mode[0]) {
	case 'r': flags |= RHO_FILE_FLAG_READ; break;
	case 'w': flags |= RHO_FILE_FLAG_WRITE; break;
	case 'a': flags |= RHO_FILE_FLAG_APPEND; break;
	default: return -1;
	}

	if (mode[1] == '\0') {
		return flags;
	} else if (mode[2] != '\0' || mode[1] != '+') {
		return -1;
	}

	flags |= RHO_FILE_FLAG_UPDATE;
	return flags;
}

RhoValue rho_file_make(const char *filename, const char *mode)
{
	const int flags = parse_mode(mode);

	if (flags == -1) {
		return RHO_TYPE_EXC("invalid file mode '%s'", mode);
	}

	FILE *file = fopen(filename, mode);

	if (file == NULL) {
		return rho_io_exc_cannot_open_file(filename, mode);
	}

	RhoFileObject *fileobj = rho_obj_alloc(&rho_file_class);
	fileobj->file = file;
	fileobj->name = rho_util_str_dup(filename);
	fileobj->flags = flags | RHO_FILE_FLAG_OPEN;
	return rho_makeobj(fileobj);
}

static void rho_file_free(RhoValue *this)
{
	RhoFileObject *file = rho_objvalue(this);

	if (isopen(file)) {
		fclose(file->file);
	}

	RHO_FREE(file->name);
	rho_obj_class.del(this);
}

static bool is_newline_or_eof(const int c)
{
	return c == '\n' || c == EOF;
}

RhoValue rho_file_readline(RhoFileObject *fileobj)
{
	if (!isopen(fileobj)) {
		return rho_io_exc_file_closed(fileobj->name);
	}

	if (!canread(fileobj)) {
		return rho_io_exc_cannot_read_file(fileobj->name);
	}

	FILE *file = fileobj->file;

	if (feof(file)) {
		return rho_makenull();
	}

	char buf[255];
	char *next = &buf[0];
	int c;
	RhoStrBuf aux;
	aux.buf = NULL;
	buf[sizeof(buf)-1] = '\0';

	while (!is_newline_or_eof((c = fgetc(file)))) {
		if (next == &buf[sizeof(buf) - 1]) {
			if (aux.buf == NULL) {
				rho_strbuf_init(&aux, 2*sizeof(buf));
				rho_strbuf_append(&aux, buf, sizeof(buf) - 1);
			}

			const char str = c;
			rho_strbuf_append(&aux, &str, 1);
		} else {
			*next++ = c;
		}
	}

	if (ferror(file)) {
		fclose(file);
		close(fileobj);
		rho_strbuf_dealloc(&aux);
		return rho_io_exc_cannot_read_file(fileobj->name);
	}

	if (aux.buf == NULL) {
		*next = '\0';
		return rho_strobj_make_direct(buf, next - buf);
	} else {
		RhoStr str;
		rho_strbuf_trim(&aux);
		rho_strbuf_to_str(&aux, &str);
		str.freeable = 1;
		return rho_strobj_make(str);
	}
}

RhoValue rho_file_write(RhoFileObject *fileobj, const char *str, const size_t len)
{
	if (!isopen(fileobj)) {
		return rho_io_exc_file_closed(fileobj->name);
	}

	if (!canwrite(fileobj)) {
		return rho_io_exc_cannot_write_file(fileobj->name);
	}

	FILE *file = fileobj->file;
	fwrite(str, 1, len, file);

	if (ferror(file)) {
		fclose(file);
		close(fileobj);
		return rho_io_exc_cannot_write_file(fileobj->name);
	}

	return rho_makenull();
}

void rho_file_rewind(RhoFileObject *fileobj)
{
	rewind(fileobj->file);
}

bool rho_file_close(RhoFileObject *fileobj)
{
	if (isopen(fileobj)) {
		fclose(fileobj->file);
		close(fileobj);
		return true;
	} else {
		return false;
	}
}

static RhoValue file_readline(RhoValue *this,
                              RhoValue *args,
                              RhoValue *args_named,
                              size_t nargs,
                              size_t nargs_named)
{
#define NAME "readline"

	RHO_UNUSED(args);
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 0);

	RhoFileObject *fileobj = rho_objvalue(this);
	return rho_file_readline(fileobj);

#undef NAME
}

static RhoValue file_write(RhoValue *this,
                           RhoValue *args,
                           RhoValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "write"

	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 1);

	if (!rho_is_a(&args[0], &rho_str_class)) {
		RhoClass *class = rho_getclass(&args[0]);
		return RHO_TYPE_EXC("can only write strings to a file, not %s instances", class->name);
	}

	RhoFileObject *fileobj = rho_objvalue(this);
	RhoStrObject *str = rho_objvalue(&args[0]);

	return rho_file_write(fileobj, str->str.value, str->str.len);

#undef NAME
}

static RhoValue file_rewind(RhoValue *this,
                            RhoValue *args,
                            RhoValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "rewind"

	RHO_UNUSED(args);
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 0);

	RhoFileObject *fileobj = rho_objvalue(this);
	rho_file_rewind(fileobj);
	return rho_makenull();

#undef NAME
}

static RhoValue file_close(RhoValue *this,
                           RhoValue *args,
                           RhoValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "close"

	RHO_UNUSED(args);
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 0);

	RhoFileObject *fileobj = rho_objvalue(this);
	return rho_makebool(rho_file_close(fileobj));

#undef NAME
}

static RhoValue file_iter(RhoValue *this)
{
	RhoFileObject *fileobj = rho_objvalue(this);
	rho_retaino(fileobj);
	return *this;
}

static RhoValue file_iternext(RhoValue *this)
{
	RhoFileObject *fileobj = rho_objvalue(this);
	RhoValue next = rho_file_readline(fileobj);
	return rho_isnull(&next) ? rho_get_iter_stop() : next;
}

struct rho_num_methods rho_file_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	NULL,    /* add */
	NULL,    /* sub */
	NULL,    /* mul */
	NULL,    /* div */
	NULL,    /* mod */
	NULL,    /* pow */

	NULL,    /* bitnot */
	NULL,    /* bitand */
	NULL,    /* bitor */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	NULL,    /* iadd */
	NULL,    /* isub */
	NULL,    /* imul */
	NULL,    /* idiv */
	NULL,    /* imod */
	NULL,    /* ipow */

	NULL,    /* ibitand */
	NULL,    /* ibitor */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	NULL,    /* radd */
	NULL,    /* rsub */
	NULL,    /* rmul */
	NULL,    /* rdiv */
	NULL,    /* rmod */
	NULL,    /* rpow */

	NULL,    /* rbitand */
	NULL,    /* rbitor */
	NULL,    /* rxor */
	NULL,    /* rshiftl */
	NULL,    /* rshiftr */

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct rho_seq_methods rho_file_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

struct rho_attr_method file_methods[] = {
	{"readline", file_readline},
	{"write", file_write},
	{"rewind", file_rewind},
	{"close", file_close},
	{NULL, NULL}
};

RhoClass rho_file_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "File",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoFileObject),

	.init = NULL,
	.del = rho_file_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = file_iter,
	.iternext = file_iternext,

	.num_methods = &rho_file_num_methods,
	.seq_methods = &rho_file_seq_methods,

	.members = NULL,
	.methods = file_methods,

	.attr_get = NULL,
	.attr_set = NULL
};
