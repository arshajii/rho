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

Value exc_make(Class *exc_class, bool active, const char *msg_format, ...)
{
#define EXC_MSG_BUF_SIZE 200

	char msg_static[EXC_MSG_BUF_SIZE];

	Exception *exc = obj_alloc(exc_class);
	tb_manager_init(&exc->tbm);

	va_list args;
	va_start(args, msg_format);
	int size = vsnprintf(msg_static, EXC_MSG_BUF_SIZE, msg_format, args);
	assert(size >= 0);
	va_end(args);

	if (size >= EXC_MSG_BUF_SIZE)
		size = EXC_MSG_BUF_SIZE;

	char *msg = rho_malloc(size + 1);
	strcpy(msg, msg_static);
	exc->msg = msg;

	return active ? makeexc(exc) : makeobj(exc);

#undef EXC_MSG_BUF_SIZE
}

void exc_traceback_append(Exception *e,
                          const char *fn,
                          const unsigned int lineno)
{
	tb_manager_add(&e->tbm, fn, lineno);
}

void exc_traceback_print(Exception *e, FILE *out)
{
	tb_manager_print(&e->tbm, out);
}

void exc_print_msg(Exception *e, FILE *out)
{
	if (e->msg != NULL) {
		fprintf(out, "%s: %s\n", e->base.class->name, e->msg);
	} else {
		fprintf(out, "%s\n", e->base.class->name);
	}
}

/* Base Exception */

static Value exc_init(Value *this, Value *args, size_t nargs)
{
	if (nargs > 1) {
		return makeerr(error_new(ERR_TYPE_TYPE,
		                         "Exception constructor takes at most 1 argument (got %lu)",
		                         nargs));
	}

	Exception *e = objvalue(this);
	tb_manager_init(&e->tbm);

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
		char *str_copy = rho_malloc(str->str.len + 1);
		strcpy(str_copy, str->str.value);
		e->msg = str_copy;
	}

	return *this;
}

static void exc_free(Value *this)
{
	Exception *exc = objvalue(this);
	FREE(exc->msg);
	tb_manager_dealloc(&exc->tbm);
	obj_class.del(this);
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
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* IndexException */

static Value index_exc_init(Value *this, Value *args, size_t nargs)
{
	return exc_init(this, args, nargs);
}

static void index_exc_free(Value *this)
{
	exception_class.del(this);
}

Class index_exception_class = {
	.base = CLASS_BASE_INIT(),
	.name = "IndexException",
	.super = &exception_class,

	.instance_size = sizeof(IndexException),

	.init = index_exc_init,
	.del = index_exc_free,

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
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* TypeException */

static Value type_exc_init(Value *this, Value *args, size_t nargs)
{
	return exc_init(this, args, nargs);
}

static void type_exc_free(Value *this)
{
	exception_class.del(this);
}

Class type_exception_class = {
	.base = CLASS_BASE_INIT(),
	.name = "TypeException",
	.super = &exception_class,

	.instance_size = sizeof(TypeException),

	.init = type_exc_init,
	.del = type_exc_free,

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
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* AttributeException */

static Value attr_exc_init(Value *this, Value *args, size_t nargs)
{
	return exc_init(this, args, nargs);
}

static void attr_exc_free(Value *this)
{
	exception_class.del(this);
}

Class attr_exception_class = {
	.base = CLASS_BASE_INIT(),
	.name = "AttributeException",
	.super = &exception_class,

	.instance_size = sizeof(AttributeException),

	.init = attr_exc_init,
	.del = attr_exc_free,

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
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* ImportException */

static Value import_exc_init(Value *this, Value *args, size_t nargs)
{
	return exc_init(this, args, nargs);
}

static void import_exc_free(Value *this)
{
	exception_class.del(this);
}

Class import_exception_class = {
	.base = CLASS_BASE_INIT(),
	.name = "ImportException",
	.super = &exception_class,

	.instance_size = sizeof(ImportException),

	.init = import_exc_init,
	.del = import_exc_free,

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
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* Common Exceptions */

Value type_exc_unsupported_1(const char *op, const Class *c1)
{
	return TYPE_EXC("unsupported operand type for %s: '%s'", op, c1->name);
}

Value type_exc_unsupported_2(const char *op, const Class *c1, const Class *c2)
{
	return TYPE_EXC("unsupported operand types for %s: '%s' and '%s'",
	                op,
	                c1->name,
	                c2->name);
}

Value type_exc_cannot_index(const Class *c1)
{
	return TYPE_EXC("type '%s' does not support indexing", c1->name);
}

Value type_exc_cannot_instantiate(const Class *c1)
{
	return TYPE_EXC("class '%s' cannot be instantiated", c1->name);
}

Value type_exc_not_callable(const Class *c1)
{
	return TYPE_EXC("object of type '%s' is not callable", c1->name);
}

Value type_exc_not_iterable(const Class *c1)
{
	return TYPE_EXC("object of type '%s' is not iterable", c1->name);
}

Value type_exc_not_iterator(const Class *c1)
{
	return TYPE_EXC("object of type '%s' is not an iterator", c1->name);
}

Value call_exc_args(const char *fn, unsigned int expected, unsigned int got)
{
	return TYPE_EXC("function %s(): expected %u arguments, got %u",
	                fn,
	                expected,
	                got);
}

Value attr_exc_not_found(const Class *type, const char *attr)
{
	return ATTR_EXC("object of type '%s' has no attribute '%s'",
	                type->name,
	                attr);
}

Value attr_exc_readonly(const Class *type, const char *attr)
{
	return ATTR_EXC("attribute '%s' of type '%s' object is read-only",
	                attr,
	                type->name);
}

Value attr_exc_mismatch(const Class *type, const char *attr, const Class *assign_type)
{
	return ATTR_EXC("cannot assign '%s' to attribute '%s' of '%s' object",
	                assign_type->name,
	                attr,
	                type->name);
}

Value import_exc_not_found(const char *name)
{
	return IMPORT_EXC("cannot find module '%s'", name);
}
