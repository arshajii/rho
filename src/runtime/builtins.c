#include <stdlib.h>
#include "nativefunc.h"
#include "object.h"
#include "strobject.h"
#include "vmops.h"
#include "strdict.h"
#include "exc.h"
#include "module.h"
#include "builtins.h"

static RhoValue hash(RhoValue *args, size_t nargs);
static RhoValue str(RhoValue *args, size_t nargs);
static RhoValue len(RhoValue *args, size_t nargs);
static RhoValue iter(RhoValue *args, size_t nargs);
static RhoValue next(RhoValue *args, size_t nargs);
static RhoValue type(RhoValue *args, size_t nargs);
static RhoValue safe(RhoValue *args, size_t nargs);

static RhoNativeFuncObject hash_nfo = RHO_NFUNC_INIT(hash);
static RhoNativeFuncObject str_nfo  = RHO_NFUNC_INIT(str);
static RhoNativeFuncObject len_nfo  = RHO_NFUNC_INIT(len);
static RhoNativeFuncObject iter_nfo = RHO_NFUNC_INIT(iter);
static RhoNativeFuncObject next_nfo = RHO_NFUNC_INIT(next);
static RhoNativeFuncObject type_nfo = RHO_NFUNC_INIT(type);
static RhoNativeFuncObject safe_nfo = RHO_NFUNC_INIT(safe);

const struct rho_builtin rho_builtins[] = {
		{"hash", RHO_MAKE_OBJ(&hash_nfo)},
		{"str",  RHO_MAKE_OBJ(&str_nfo)},
		{"len",  RHO_MAKE_OBJ(&len_nfo)},
		{"iter", RHO_MAKE_OBJ(&iter_nfo)},
		{"next", RHO_MAKE_OBJ(&next_nfo)},
		{"type", RHO_MAKE_OBJ(&type_nfo)},
		{"safe", RHO_MAKE_OBJ(&safe_nfo)},
		{NULL,   RHO_MAKE_EMPTY()},
};

static RhoValue hash(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("hash", nargs, 1);
	return rho_op_hash(&args[0]);
}

static RhoValue str(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("str", nargs, 1);
	return rho_op_str(&args[0]);
}

static RhoValue len(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("len", nargs, 1);
	RhoClass *class = rho_getclass(&args[0]);
	RhoUnOp len = rho_resolve_len(class);

	if (!len) {
		return rho_type_exc_unsupported_1(__func__, class);
	}

	return len(&args[0]);
}

static RhoValue iter(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("iter", nargs, 1);
	return rho_op_iter(args);
}

static RhoValue next(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("next", nargs, 1);
	return rho_op_iternext(args);
}

static RhoValue type(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("type", nargs, 1);
	return rho_makeobj(rho_getclass(&args[0]));
}

static RhoValue safe(RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK("safe", nargs, 1);

	if (!rho_isobject(args)) {
		rho_retain(args);
		return *args;
	}

	RhoObject *o = rho_objvalue(args);

	if (rho_object_set_monitor(o)) {
		rho_retain(args);
		return *args;
	} else {
		return RHO_CONC_ACCS_EXC("safe() argument already safe'd or has multiple references to it");
	}
}

/* Built-in modules */
#include "iomodule.h"
#include "mathmodule.h"

const RhoModule *rho_builtin_modules[] = {
		(RhoModule *)&rho_io_module,
		(RhoModule *)&rho_math_module,
		NULL
};
