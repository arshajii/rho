#ifndef STROBJECT_H
#define STROBJECT_H

#include <stdbool.h>
#include "str.h"
#include "object.h"

extern struct num_methods str_num_methods;
extern struct seq_methods str_seq_methods;
extern Class str_class;

typedef struct {
	Object base;
	Str str;
	bool freeable;
} StrObject;

Value strobj_make(Str value);

#endif /* STROBJECT_H */
