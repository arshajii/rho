#ifndef STROBJECT_H
#define STROBJECT_H

#include "object.h"

extern struct num_methods str_arith_methods;
extern struct cmp_methods str_cmp_methods;
extern struct seq_methods str_misc_methods;
extern Class str_class;

typedef struct {
	Object base;
	Str str;
	bool freeable;
} StrObject;

Value strobj_make(Str value);

#endif /* STROBJECT_H */
