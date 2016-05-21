#ifndef RHO_LISTOBJECT_H
#define RHO_LISTOBJECT_H

#include <stdlib.h>
#include "object.h"
#include "iter.h"

extern struct rho_num_methods rho_list_num_methods;
extern struct rho_seq_methods rho_list_seq_methods;
extern RhoClass rho_list_class;

typedef struct {
	RhoObject base;
	RhoValue *elements;
	size_t count;
	size_t capacity;
} RhoListObject;

RhoValue rho_list_make(RhoValue *elements, const size_t count);
RhoValue rho_list_get(RhoListObject *list, const size_t idx);
void rho_list_append(RhoListObject *list, RhoValue *v);
void rho_list_clear(RhoListObject *list);
void rho_list_trim(RhoListObject *list);

extern RhoClass rho_list_iter_class;

typedef struct {
	RhoIter base;
	RhoListObject *source;
	size_t index;
} RhoListIter;

#endif /* RHO_LISTOBJECT_H */
