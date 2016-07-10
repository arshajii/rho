#ifndef RHO_DICT_H
#define RHO_DICT_H

#include <stdlib.h>
#include "object.h"

extern struct rho_num_methods rho_dict_num_methods;
extern struct rho_seq_methods rho_dict_seq_methods;
extern RhoClass rho_dict_class;

struct rho_dict_entry {
	RhoValue key;
	RhoValue value;
	int hash;
	struct rho_dict_entry *next;
};

typedef struct {
	RhoObject base;
	struct rho_dict_entry **entries;
	size_t count;
	size_t capacity;
	size_t threshold;
	unsigned state_id;
	RHO_SAVED_TID_FIELD
} RhoDictObject;

/*
 * Elements should be given in `entries` in the format
 *   [key1, value1, key2, value2, ..., keyN, valueN]
 * where the `size` parameter is the length of this list.
 */
RhoValue rho_dict_make(RhoValue *entries, const size_t size);

RhoValue rho_dict_get(RhoDictObject *dict, RhoValue *key, RhoValue *dflt);
RhoValue rho_dict_put(RhoDictObject *dict, RhoValue *key, RhoValue *value);
RhoValue rho_dict_remove_key(RhoDictObject *dict, RhoValue *key);
RhoValue rho_dict_contains_key(RhoDictObject *dict, RhoValue *key);
RhoValue rho_dict_eq(RhoDictObject *dict, RhoDictObject *other);
size_t rho_dict_len(RhoDictObject *dict);

extern RhoClass rho_dict_iter_class;

typedef struct {
	RhoIter base;
	RhoDictObject *source;
	unsigned saved_state_id;
	size_t current_index;
	struct rho_dict_entry *current_entry;
} RhoDictIter;

#endif /* RHO_DICT_H */
