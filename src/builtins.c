#include <stdlib.h>
#include "nativefunc.h"
#include "object.h"
#include "strobject.h"
#include "vmops.h"
#include "exc.h"
#include "builtins.h"

#define OBJ_INIT_STATIC(class) (Object){class, -1}

#define NFUNC_INIT(func) (NativeFuncObject){OBJ_INIT_STATIC(&native_func_class), func}

static Value hash(Value *args, size_t nargs);
static Value str(Value *args, size_t nargs);
static Value len(Value *args, size_t nargs);
static Value type(Value *args, size_t nargs);

static NativeFuncObject hash_nfo = NFUNC_INIT(hash);
static NativeFuncObject str_nfo  = NFUNC_INIT(str);
static NativeFuncObject len_nfo  = NFUNC_INIT(len);
static NativeFuncObject type_nfo = NFUNC_INIT(type);

const struct builtin builtins[] = {
		{"hash", makeobj(&hash_nfo)},
		{"str",  makeobj(&str_nfo)},
		{"len",  makeobj(&len_nfo)},
		{"type", makeobj(&type_nfo)},
		{NULL,   makeempty()},
};

#define ARG_ERR(count, expected) return call_exc_args(__FUNCTION__, count, expected)
#define ARG_CHECK(count, expected) if (count != expected) ARG_ERR(count, expected)

#define TYPE_ERROR(class) return makeerr(type_error_unsupported_1(__FUNCTION__, class))

static Value hash(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	return op_hash(&args[0]);
}

static Value str(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	Str str;
	op_str(&args[0], &str);
	Value stro = strobj_make(str);
	return stro;
}

static Value len(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	Class *class = getclass(&args[0]);
	LenFunc len = resolve_len(class);

	if (!len) {
		return type_exc_unsupported_1(__FUNCTION__, class);
	}

	return makeint(len(&args[0]));
}

static Value type(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	return makeobj(getclass(&args[0]));
}
