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

RhoValue rho_exc_make(RhoClass *exc_class, bool active, const char *msg_format, ...)
{
#define EXC_MSG_BUF_SIZE 200

	char msg_static[EXC_MSG_BUF_SIZE];

	RhoException *exc = rho_obj_alloc(exc_class);
	rho_tb_manager_init(&exc->tbm);

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

	return active ? rho_makeexc(exc) : rho_makeobj(exc);

#undef EXC_MSG_BUF_SIZE
}

void rho_exc_traceback_append(RhoException *e,
                              const char *fn,
                              const unsigned int lineno)
{
	rho_tb_manager_add(&e->tbm, fn, lineno);
}

void rho_exc_traceback_print(RhoException *e, FILE *out)
{
	rho_tb_manager_print(&e->tbm, out);
}

void rho_exc_print_msg(RhoException *e, FILE *out)
{
	if (e->msg != NULL) {
		fprintf(out, "%s: %s\n", e->base.class->name, e->msg);
	} else {
		fprintf(out, "%s\n", e->base.class->name);
	}
}

/* Base Exception */

static RhoValue exc_init(RhoValue *this, RhoValue *args, size_t nargs)
{
	if (nargs > 1) {
		return rho_makeerr(rho_err_new(RHO_ERR_TYPE_TYPE,
		                               "Exception constructor takes at most 1 argument (got %lu)",
		                               nargs));
	}

	RhoException *e = rho_objvalue(this);
	rho_tb_manager_init(&e->tbm);

	if (nargs == 0) {
		e->msg = NULL;
	} else {
		if (!rho_is_a(&args[0], &rho_str_class)) {
			RhoClass *class = rho_getclass(&args[0]);
			return rho_makeerr(rho_err_new(RHO_ERR_TYPE_TYPE,
			                               "Exception constructor takes a Str argument, not a %s",
			                               class->name));
		}

		RhoStrObject *str = rho_objvalue(&args[0]);
		char *str_copy = rho_malloc(str->str.len + 1);
		strcpy(str_copy, str->str.value);
		e->msg = str_copy;
	}

	return *this;
}

static void exc_free(RhoValue *this)
{
	RhoException *exc = rho_objvalue(this);
	RHO_FREE(exc->msg);
	rho_tb_manager_dealloc(&exc->tbm);
	rho_obj_class.del(this);
}

RhoClass rho_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Exception",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoException),

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

static RhoValue sub_exc_init(RhoValue *this, RhoValue *args, size_t nargs)
{
	return exc_init(this, args, nargs);
}

static void sub_exc_free(RhoValue *this)
{
	rho_exception_class.del(this);
}

RhoClass rho_index_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "IndexException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoIndexException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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

RhoClass rho_type_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "TypeException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoTypeException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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

RhoClass rho_io_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "IOException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoIOException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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

RhoClass rho_attr_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "AttributeException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoAttributeException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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

RhoClass rho_import_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "ImportException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoImportException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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

RhoClass rho_isc_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "IllegalStateChangeException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoIllegalStateChangeException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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

RhoClass rho_seq_exp_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "SequenceExpandException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoSequenceExpandException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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

RhoClass rho_actor_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "ActorException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoActorException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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

RhoClass rho_conc_access_exception_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "ConcurrentAccessException",
	.super = &rho_exception_class,

	.instance_size = sizeof(RhoConcurrentAccessException),

	.init = sub_exc_init,
	.del = sub_exc_free,

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


/* Common exceptions */

RhoValue rho_type_exc_unsupported_1(const char *op, const RhoClass *c1)
{
	return RHO_TYPE_EXC("unsupported operand type for %s: '%s'", op, c1->name);
}

RhoValue rho_type_exc_unsupported_2(const char *op, const RhoClass *c1, const RhoClass *c2)
{
	return RHO_TYPE_EXC("unsupported operand types for %s: '%s' and '%s'",
	                    op,
	                    c1->name,
	                    c2->name);
}

RhoValue rho_type_exc_cannot_index(const RhoClass *c1)
{
	return RHO_TYPE_EXC("type '%s' does not support indexing", c1->name);
}

RhoValue rho_type_exc_cannot_apply(const RhoClass *c1)
{
	return RHO_TYPE_EXC("type '%s' does not support function application", c1->name);
}

RhoValue rho_type_exc_cannot_instantiate(const RhoClass *c1)
{
	return RHO_TYPE_EXC("class '%s' cannot be instantiated", c1->name);
}

RhoValue rho_type_exc_not_callable(const RhoClass *c1)
{
	return RHO_TYPE_EXC("object of type '%s' is not callable", c1->name);
}

RhoValue rho_type_exc_not_iterable(const RhoClass *c1)
{
	return RHO_TYPE_EXC("object of type '%s' is not iterable", c1->name);
}

RhoValue rho_type_exc_not_iterator(const RhoClass *c1)
{
	return RHO_TYPE_EXC("object of type '%s' is not an iterator", c1->name);
}

RhoValue rho_type_exc_hint_mismatch(const RhoClass *got, const RhoClass *expected)
{
	return RHO_TYPE_EXC("hint mismatch: %s is not a %s", got->name, expected->name);
}


RhoValue rho_call_exc_num_args(const char *fn, unsigned int got, unsigned int expected)
{
	return RHO_TYPE_EXC("function %s(): expected %u arguments, got %u",
	                    fn,
	                    expected,
	                    got);
}

RhoValue rho_call_exc_num_args_at_most(const char *fn, unsigned int got, unsigned int expected)
{
	return RHO_TYPE_EXC("function %s(): expected at most %u arguments, got %u",
	                    fn,
	                    expected,
	                    got);
}

RhoValue rho_call_exc_num_args_between(const char *fn, unsigned int got, unsigned int min, unsigned int max)
{
	return RHO_TYPE_EXC("function %s(): expected %u-%u arguments, got %u",
	                    fn,
	                    min, max,
	                    got);
}

RhoValue rho_call_exc_named_args(const char *fn)
{
	return RHO_TYPE_EXC("function %s(): got unexpected named arguments", fn);
}

RhoValue rho_call_exc_dup_arg(const char *fn, const char *name)
{
	return RHO_TYPE_EXC("function %s(): duplicate argument for parameter '%s'",
	                fn,
	                name);
}

RhoValue rho_call_exc_unknown_arg(const char *fn, const char *name)
{
	return RHO_TYPE_EXC("function %s(): unknown parameter name '%s'",
	                fn,
	                name);
}

RhoValue rho_call_exc_missing_arg(const char *fn, const char *name)
{
	return RHO_TYPE_EXC("function %s(): missing argument for parameter '%s'",
	                fn,
	                name);
}

RhoValue rho_call_exc_native_named_args(void)
{
	return RHO_TYPE_EXC("native functions do not take named arguments");
}

RhoValue rho_call_exc_constructor_named_args(void)
{
	return RHO_TYPE_EXC("constructors do not take named arguments");
}

RhoValue rho_io_exc_cannot_open_file(const char *filename, const char *mode)
{
	return RHO_IO_EXC("cannot open file '%s' in mode '%s'", filename, mode);
}

RhoValue rho_io_exc_cannot_read_file(const char *filename)
{
	return RHO_IO_EXC("cannot read from file '%s'", filename);
}

RhoValue rho_io_exc_cannot_write_file(const char *filename)
{
	return RHO_IO_EXC("cannot write to file '%s'", filename);
}

RhoValue rho_io_exc_file_closed(const char *filename)
{
	return RHO_IO_EXC("file '%s' has been closed", filename);
}

RhoValue rho_attr_exc_not_found(const RhoClass *type, const char *attr)
{
	return RHO_ATTR_EXC("object of type '%s' has no attribute '%s'",
	                type->name,
	                attr);
}

RhoValue rho_attr_exc_readonly(const RhoClass *type, const char *attr)
{
	return RHO_ATTR_EXC("attribute '%s' of type '%s' object is read-only",
	                attr,
	                type->name);
}

RhoValue rho_attr_exc_mismatch(const RhoClass *type, const char *attr, const RhoClass *assign_type)
{
	return RHO_ATTR_EXC("cannot assign '%s' to attribute '%s' of '%s' object",
	                assign_type->name,
	                attr,
	                type->name);
}

RhoValue rho_import_exc_not_found(const char *name)
{
	return RHO_IMPORT_EXC("cannot find module '%s'", name);
}

RhoValue rho_seq_exp_exc_inconsistent(const unsigned int got, const unsigned int expected)
{
	if (got > expected) {
		return RHO_SEQ_EXP_EXC("too many values to expand (got %u, expected %u)", got, expected);
	} else {
		return RHO_SEQ_EXP_EXC("too few values to expand (got %u, expected %u)", got, expected);
	}
}
