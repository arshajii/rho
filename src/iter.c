#include <stdlib.h>
#include "object.h"
#include "iter.h"

static void iter_free(Value *this)
{
	obj_class.del(this);
}

static Value iter_iter(Value *this)
{
	retain(this);
	return *this;
}

Class iter_class = {
	.base = CLASS_BASE_INIT(),
	.name = "Iter",
	.super = &obj_class,

	.instance_size = sizeof(Iter),

	.init = NULL,
	.del = iter_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = iter_iter,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

static void iter_stop_free(Value *this)
{
	obj_class.del(this);
}

Class iter_stop_class = {
	.base = CLASS_BASE_INIT(),
	.name = "IterStop",
	.super = &obj_class,

	.instance_size = sizeof(IterStop),

	.init = NULL,
	.del = iter_stop_free,

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

Value get_iter_stop(void)
{
	static IterStop iter_stop = (IterStop){OBJ_INIT_STATIC(&iter_stop_class)};
	return makeobj(&iter_stop);
}
