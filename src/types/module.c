#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "exc.h"
#include "str.h"
#include "util.h"
#include "module.h"

RhoValue rho_module_make(const char *name, RhoStrDict *contents)
{
	RhoModule *mod = rho_obj_alloc(&rho_module_class);
	mod->name = name;
	mod->contents = *contents;
	return rho_makeobj(mod);
}

static void module_free(RhoValue *this)
{
	RhoModule *mod = rho_objvalue(this);
	rho_strdict_dealloc(&mod->contents);
	rho_obj_class.del(this);
}

static RhoValue module_attr_get(RhoValue *this, const char *attr)
{
	RhoModule *mod = rho_objvalue(this);
	RhoStr key = RHO_STR_INIT(attr, strlen(attr), 0);
	RhoValue v = rho_strdict_get(&mod->contents, &key);

	if (rho_isempty(&v)) {
		return rho_attr_exc_not_found(&rho_module_class, attr);
	}

	rho_retain(&v);
	return v;
}

static RhoValue module_attr_set(RhoValue *this, const char *attr, RhoValue *v)
{
	RHO_UNUSED(attr);
	RHO_UNUSED(v);
	RhoModule *mod = rho_objvalue(this);
	return RHO_ATTR_EXC("cannot re-assign attributes of module '%s'", mod->name);
}

struct rho_num_methods module_num_methods = {
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

struct rho_seq_methods module_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_module_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Module",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoModule),

	.init = NULL,
	.del = module_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &module_num_methods,
	.seq_methods = &module_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = module_attr_get,
	.attr_set = module_attr_set
};

static RhoValue builtin_module_attr_get(RhoValue *this, const char *attr)
{
	RhoBuiltInModule *mod = rho_objvalue(this);

	if (!mod->initialized) {
		rho_strdict_init(&mod->base.contents);
		mod->init_func(mod);
		mod->initialized = true;
	}

	return rho_module_class.attr_get(this, attr);
}

static void builtin_module_free(RhoValue *this)
{
	rho_module_class.del(this);
}

struct rho_num_methods builtin_module_num_methods = {
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

struct rho_seq_methods builtin_module_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_builtin_module_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "BuiltInModule",
	.super = &rho_module_class,

	.instance_size = sizeof(RhoBuiltInModule),

	.init = NULL,
	.del = builtin_module_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &builtin_module_num_methods,
	.seq_methods = &builtin_module_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = builtin_module_attr_get,
	.attr_set = NULL   /* inherit from Module */
};
