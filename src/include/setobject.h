#ifndef RHO_SET_H
#define RHO_SET_H

#include <stdlib.h>
#include "object.h"

extern struct rho_num_methods rho_set_num_methods;
extern struct rho_seq_methods rho_set_seq_methods;
extern RhoClass rho_set_class;

struct rho_set_entry {
	RhoValue element;
	int hash;
	struct rho_set_entry *next;
};

typedef struct {
	RhoObject base;
	struct rho_set_entry **entries;
	size_t count;
	size_t capacity;
	size_t threshold;
	unsigned state_id;
} RhoSetObject;

RhoValue rho_set_make(RhoValue *elements, const size_t size);
RhoValue rho_set_add(RhoSetObject *set, RhoValue *element);
RhoValue rho_set_remove(RhoSetObject *set, RhoValue *element);
bool rho_set_contains(RhoSetObject *set, RhoValue *element);
bool rho_set_eq(RhoSetObject *set, RhoSetObject *other);
size_t rho_set_len(RhoSetObject *set);

extern RhoClass rho_set_iter_class;

typedef struct {
	RhoIter base;
	RhoSetObject *source;
	unsigned saved_state_id;
	size_t current_index;
	struct rho_set_entry *current_entry;
} RhoSetIter;

#endif /* RHO_SET_H */
