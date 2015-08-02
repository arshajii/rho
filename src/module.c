#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "exc.h"
#include "str.h"
#include "util.h"
#include "module.h"

Value module_make(const char *name, StrDict *contents)
{
	Module *mod = obj_alloc(&module_class);
	mod->name = name;
	mod->contents = *contents;
	return makeobj(mod);
}

static void module_free(Value *this)
{
	Module *mod = objvalue(this);
	strdict_dealloc(&mod->contents);
	obj_class.del(this);
}

static Value module_attr_get(Value *this, const char *attr)
{
	Module *mod = objvalue(this);
	Str key = STR_INIT(attr, strlen(attr), 0);
	Value v = strdict_get(&mod->contents, &key);

	if (isempty(&v)) {
		return attr_exc_not_found(&module_class, attr);
	}

	retain(&v);
	return v;
}

static Value module_attr_set(Value *this, const char *attr, Value *v)
{
	UNUSED(this);
	UNUSED(attr);
	UNUSED(v);
	return ATTR_EXC("cannot re-assign module attributes");
}

struct num_methods module_num_methods = {
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

struct seq_methods module_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
};

Class module_class = {
	.base = CLASS_BASE_INIT(),
	.name = "Module",
	.super = &obj_class,

	.instance_size = sizeof(Module),

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
	.seq_methods  = &module_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = module_attr_get,
	.attr_set = module_attr_set
};
