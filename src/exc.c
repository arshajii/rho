#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "err.h"
#include "exc.h"

static Value exc_init(Value *this, Value *args, size_t nargs)
{
	if (nargs > 1) {
		return makeerr(error_new(ERR_TYPE_TYPE,
		                         "Exception constructor takes at most 1 argument (got %lu)",
		                         nargs));
	}

	Exception *e = objvalue(this);
	if (nargs == 0) {
		e->msg = NULL;
	} else {
		if (!is_a(&args[0], &str_class)) {
			Class *class = getclass(&args[0]);
			return makeerr(error_new(ERR_TYPE_TYPE,
			                         "Exception constructor takes a Str argument, not a %s",
			                         class->name));
		}

		StrObject *str = objvalue(&args[0]);
		char *str_copy = malloc(str->str.len + 1);
		strcpy(str_copy, str->str.value);
		e->msg = str_copy;
	}

	return *this;
}

Value exc_make(Class *exc_class, bool active, const char *msg_format, ...)
{
#define EXC_MSG_BUF_SIZE 200

	char msg_static[EXC_MSG_BUF_SIZE];

	Exception *exc = obj_alloc(exc_class);

	va_list args;
	va_start(args, msg_format);
	int size = vsnprintf(msg_static, EXC_MSG_BUF_SIZE, msg_format, args);
	assert(size >= 0);
	va_end(args);

	if (size >= EXC_MSG_BUF_SIZE)
		size = EXC_MSG_BUF_SIZE;

	char *msg = malloc(size + 1);
	strcpy(msg, msg_static);
	exc->msg = msg;

	return active ? makeexc(exc) : makeobj(exc);

#undef EXC_MSG_BUF_SIZE
}

static void exc_free(Value *this)
{
	Exception *exc = objvalue(this);
	free((char *)exc->msg);
	exc->base.class->super->del(this);
}

Class exception_class = {
	.base = CLASS_BASE_INIT(),
	.name = "Exception",
	.super = &obj_class,

	.instance_size = sizeof(Exception),

	.init = exc_init,
	.del = exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL
};
