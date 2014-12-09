#ifndef LISTOBJECT_H
#define LISTOBJECT_H

#include <stdlib.h>
#include "object.h"

extern struct num_methods list_num_methods;
extern struct seq_methods list_seq_methods;
extern Class list_class;

typedef struct {
	Object base;
	Value *elements;
	size_t count;
	size_t capacity;
} ListObject;

Value list_make(Value *elements, const size_t count);

#endif /* LISTOBJECT_H */