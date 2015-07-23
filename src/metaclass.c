#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "object.h"
#include "util.h"
#include "exc.h"
#include "object.h"
#include "metaclass.h"

static void meta_class_del(Value *this)
{
	UNUSED(this);
}

static void meta_class_str(Value *this, Str *dest)
{
#define STR_MAX_LEN 50
	char buf[STR_MAX_LEN];
	Class *class = objvalue(this);
	size_t len = snprintf(buf, STR_MAX_LEN, "<class %s>", class->name);
	assert(len > 0);

	if (len > STR_MAX_LEN) {
		len = STR_MAX_LEN;
	}

	char *copy = rho_malloc(len + 1);
	strcpy(copy, buf);
	*dest = STR_INIT(copy, len, 1);
#undef STR_MAX_LEN
}

static Value meta_class_call(Value *this, Value *args, size_t nargs)
{
	Class *class = objvalue(this);
	InitFunc init = resolve_init(class);

	if (!init) {
		return type_exc_cannot_instantiate(class);
	}

	Value instance = makeobj(obj_alloc(class));
	Value init_result = init(&instance, args, nargs);

	if (iserror(&init_result)) {
		free(objvalue(&instance));  /* straight-up free; no need to
		                               go through `release` since we can
		                               be sure nobody has a reference to
		                               the newly created instance        */
		return init_result;
	} else {
		return instance;
	}
}

Class meta_class = {
	.base = CLASS_BASE_INIT(),
	.name = "MetaClass",
	.super = &meta_class,

	.instance_size = sizeof(Class),

	.init = NULL,
	.del = meta_class_del,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = meta_class_str,
	.call = meta_class_call,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL
};
