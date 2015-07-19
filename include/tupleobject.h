#ifndef TUPLEOBJECT_H
#define TUPLEOBJECT_H

#include <stdlib.h>
#include "object.h"

extern struct num_methods tuple_num_methods;
extern struct seq_methods tuple_seq_methods;
extern Class tuple_class;

typedef struct {
	Object base;
	size_t count;
	Value elements[];
} TupleObject;

Value tuple_make(Value *elements, const size_t count);

#endif /* TUPLEOBJECT_H */
