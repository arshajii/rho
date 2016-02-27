#include <stdlib.h>
#include "nativefunc.h"
#include "object.h"
#include "strobject.h"
#include "vmops.h"
#include "strdict.h"
#include "exc.h"
#include "module.h"
#include "builtins.h"

#define NFUNC_INIT(func_) { .base = OBJ_INIT_STATIC(&native_func_class), .func = (func_) }

static Value hash(Value *args, size_t nargs);
static Value str(Value *args, size_t nargs);
static Value len(Value *args, size_t nargs);
static Value iter(Value *args, size_t nargs);
static Value next(Value *args, size_t nargs);
static Value type(Value *args, size_t nargs);

static NativeFuncObject hash_nfo = NFUNC_INIT(hash);
static NativeFuncObject str_nfo  = NFUNC_INIT(str);
static NativeFuncObject len_nfo  = NFUNC_INIT(len);
static NativeFuncObject iter_nfo = NFUNC_INIT(iter);
static NativeFuncObject next_nfo = NFUNC_INIT(next);
static NativeFuncObject type_nfo = NFUNC_INIT(type);

const struct builtin builtins[] = {
		{"hash", MAKE_OBJ(&hash_nfo)},
		{"str",  MAKE_OBJ(&str_nfo)},
		{"len",  MAKE_OBJ(&len_nfo)},
		{"iter", MAKE_OBJ(&iter_nfo)},
		{"next", MAKE_OBJ(&next_nfo)},
		{"type", MAKE_OBJ(&type_nfo)},
		{NULL,   MAKE_EMPTY()},
};

#define ARG_ERR(count, expected) return call_exc_num_args(__func__, count, expected)
#define ARG_CHECK(count, expected) if (count != expected) ARG_ERR(count, expected)

#define TYPE_ERROR(class) return makeerr(type_error_unsupported_1(__func__, class))

static Value hash(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	return op_hash(&args[0]);
}

static Value str(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	return makeobj(op_str(&args[0]));
}

static Value len(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	Class *class = getclass(&args[0]);
	LenFunc len = resolve_len(class);

	if (!len) {
		return type_exc_unsupported_1(__func__, class);
	}

	return makeint(len(&args[0]));
}

static Value iter(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	return op_iter(args);
}

static Value next(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	return op_iternext(args);
}

static Value type(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	return makeobj(getclass(&args[0]));
}

/* Built-in Modules */

/* math */
#include <math.h>

static Value rho_cos(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	if (!(isint(args) || isfloat(args))) {
		return type_exc_unsupported_1("cos", getclass(args));
	}
	const double d = isint(args) ? (double)intvalue(args) : floatvalue(args);
	return makefloat(cos(d));
}

static Value rho_sin(Value *args, size_t nargs)
{
	ARG_CHECK(nargs, 1);
	if (!(isint(args) || isfloat(args))) {
		return type_exc_unsupported_1("sin", getclass(args));
	}
	const double d = isint(args) ? (double)intvalue(args) : floatvalue(args);
	return makefloat(sin(d));
}

static NativeFuncObject cos_nfo = NFUNC_INIT(rho_cos);
static NativeFuncObject sin_nfo = NFUNC_INIT(rho_sin);

#define PI 3.14159265358979323846
#define E  2.71828182845904523536

const struct builtin math_builtins[] = {
		{"pi",  MAKE_FLOAT(PI)},
		{"e",   MAKE_FLOAT(E)},
		{"cos", MAKE_OBJ(&cos_nfo)},
		{"sin", MAKE_OBJ(&sin_nfo)},
		{NULL,  MAKE_EMPTY()},
};

static void init_math_module(BuiltInModule *mod)
{
	StrDict *dict = &mod->base.contents;
	for (size_t i = 0; math_builtins[i].name != NULL; i++) {
		strdict_put(dict, math_builtins[i].name, (Value *)&math_builtins[i].value, false);
	}
}

BuiltInModule rho_math_module = BUILTIN_MODULE_INIT_STATIC("math", init_math_module);

const Module *builtin_modules[] = {
		(Module *)&rho_math_module,
		NULL
};
