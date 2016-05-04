#ifndef RHO_TUPLEOBJECT_H
#define RHO_TUPLEOBJECT_H

#include <stdlib.h>
#include "object.h"

extern struct rho_num_methods rho_tuple_num_methods;
extern struct rho_seq_methods rho_tuple_seq_methods;
extern RhoClass rho_tuple_class;

typedef struct {
	RhoObject base;
	size_t count;
	RhoValue elements[];
} RhoTupleObject;

RhoValue rho_tuple_make(RhoValue *elements, const size_t count);

#endif /* RHO_TUPLEOBJECT_H */
