#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "strobject.h"
#include "iter.h"
#include "util.h"
#include "setobject.h"

#define EMPTY_SIZE  16
#define LOAD_FACTOR 0.75f

typedef struct rho_set_entry Entry;

static Entry **make_empty_table(const size_t capacity);
static void set_resize(RhoSetObject *set, const size_t new_capacity);
static void set_free_entries(RhoSetObject *set);
static void set_free(RhoValue *this);

RhoValue rho_set_make(RhoValue *elements, const size_t size)
{
	RhoSetObject *set = rho_obj_alloc(&rho_set_class);
	RHO_INIT_SAVED_TID_FIELD(set);

	const size_t capacity = (size == 0) ? EMPTY_SIZE : rho_smallest_pow_2_at_least(size);

	set->entries = make_empty_table(capacity);
	set->count = 0;
	set->capacity = capacity;
	set->threshold = (size_t)(capacity * LOAD_FACTOR);
	set->state_id = 0;

	for (size_t i = 0; i < size; i++) {
		RhoValue *value = &elements[i];
		RhoValue v = rho_set_add(set, value);

		if (rho_iserror(&v)) {
			RhoValue set_v = rho_makeobj(set);
			set_free(&set_v);

			for (size_t j = i; j < size; j++) {
				rho_release(&elements[j]);
			}

			return v;
		}

		rho_release(value);
	}

	return rho_makeobj(set);
}

static Entry *make_entry(RhoValue *element, const int hash)
{
	Entry *entry = rho_malloc(sizeof(Entry));
	entry->element = *element;
	entry->hash = hash;
	entry->next = NULL;
	return entry;
}

RhoValue rho_set_add(RhoSetObject *set, RhoValue *element)
{
	Entry **entries = set->entries;
	const size_t capacity = set->capacity;
	const RhoValue hash_v = rho_op_hash(element);

	if (rho_iserror(&hash_v)) {
		return hash_v;
	}

	const int hash = rho_util_hash_secondary(rho_intvalue(&hash_v));
	const size_t index = hash & (capacity - 1);

	/* every value should have a valid `eq` */
	const RhoBinOp eq = rho_resolve_eq(rho_getclass(element));

	for (Entry *entry = entries[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash) {
			RhoValue eq_v = eq(element, &entry->element);

			if (rho_iserror(&eq_v)) {
				return eq_v;
			}

			if (rho_boolvalue(&eq_v)) {
				return rho_makefalse();
			}
		}
	}

	rho_retain(element);

	Entry *entry = make_entry(element, hash);
	entry->next = entries[index];
	entries[index] = entry;

	if (set->count++ >= set->threshold) {
		const size_t new_capacity = (2 * capacity);
		set_resize(set, new_capacity);
		set->threshold = (size_t)(new_capacity * LOAD_FACTOR);
	}

	++set->state_id;
	return rho_maketrue();
}

RhoValue rho_set_remove(RhoSetObject *set, RhoValue *element)
{
	const RhoValue hash_v = rho_op_hash(element);

	if (rho_iserror(&hash_v)) {
		return hash_v;
	}

	const int hash = rho_util_hash_secondary(rho_intvalue(&hash_v));

	/* every value should have a valid `eq` */
	const RhoBinOp eq = rho_resolve_eq(rho_getclass(element));

	Entry **entries = set->entries;
	const size_t idx = hash & (set->capacity - 1);
	Entry *entry = entries[idx];
	Entry *prev = entry;

	while (entry != NULL) {
		Entry *next = entry->next;

		if (hash == entry->hash) {
			RhoValue eq_v = eq(element, &entry->element);

			if (rho_iserror(&eq_v)) {
				return eq_v;
			}

			if (rho_boolvalue(&eq_v)) {
				if (prev == entry) {
					entries[idx] = next;
				} else {
					prev->next = next;
				}

				rho_release(&entry->element);
				free(entry);
				--set->count;
				++set->state_id;
				return rho_maketrue();
			}
		}

		prev = entry;
		entry = next;
	}

	return rho_makefalse();
}

RhoValue rho_set_contains(RhoSetObject *set, RhoValue *element)
{
	Entry **entries = set->entries;
	const size_t capacity = set->capacity;
	RhoValue hash_v = rho_op_hash(element);

	if (rho_iserror(&hash_v)) {
		rho_release(&hash_v);
		return rho_makefalse();
	}

	const int hash = rho_util_hash_secondary(rho_intvalue(&hash_v));
	const size_t index = hash & (capacity - 1);

	/* every value should have a valid `eq` */
	const RhoBinOp eq = rho_resolve_eq(rho_getclass(element));

	for (Entry *entry = entries[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash) {
			RhoValue eq_v = eq(element, &entry->element);

			if (rho_iserror(&eq_v)) {
				return eq_v;
			}

			if (rho_boolvalue(&eq_v)) {
				return rho_maketrue();
			}
		}
	}

	return rho_makefalse();
}

RhoValue rho_set_eq(RhoSetObject *set, RhoSetObject *other)
{
	if (set->count != other->count) {
		return rho_makefalse();
	}

	RhoSetObject *s1, *s2;

	if (set->capacity < other->capacity) {
		s1 = set;
		s2 = other;
	} else {
		s1 = other;
		s2 = set;
	}

	Entry **entries = s1->entries;
	size_t capacity = s1->capacity;
	for (size_t i = 0; i < capacity; i++) {
		for (Entry *entry = entries[i];
		     entry != NULL;
		     entry = entry->next) {

			RhoValue contains = rho_set_contains(s2, &entry->element);

			if (rho_iserror(&contains)) {
				return contains;
			}

			if (!rho_boolvalue(&contains)) {
				return rho_makefalse();
			}
		}
	}

	return rho_maketrue();
}

size_t rho_set_len(RhoSetObject *set)
{
	return set->count;
}

static Entry **make_empty_table(const size_t capacity)
{
	Entry **table = rho_malloc(capacity * sizeof(Entry *));
	for (size_t i = 0; i < capacity; i++) {
		table[i] = NULL;
	}
	return table;
}

static void set_resize(RhoSetObject *set, const size_t new_capacity)
{
	const size_t old_capacity = set->capacity;
	Entry **old_entries = set->entries;
	Entry **new_entries = make_empty_table(new_capacity);

	for (size_t i = 0; i < old_capacity; i++) {
		Entry *entry = old_entries[i];
		while (entry != NULL) {
			Entry *next = entry->next;
			const size_t idx = entry->hash & (new_capacity - 1);
			entry->next = new_entries[idx];
			new_entries[idx] = entry;
			entry = next;
		}
	}

	free(old_entries);
	set->entries = new_entries;
	set->capacity = new_capacity;
	++set->state_id;
}

static RhoValue set_contains(RhoValue *this, RhoValue *element)
{
	RhoSetObject *set = rho_objvalue(this);
	RHO_CHECK_THREAD(set);
	return rho_set_contains(set, element);
}

static RhoValue set_eq(RhoValue *this, RhoValue *other)
{
	RhoSetObject *set = rho_objvalue(this);
	RHO_CHECK_THREAD(set);

	if (!rho_is_a(other, &rho_set_class)) {
		return rho_makefalse();
	}

	RhoSetObject *other_set = rho_objvalue(other);
	return rho_set_eq(set, other_set);
}

static RhoValue set_len(RhoValue *this)
{
	RhoSetObject *set = rho_objvalue(this);
	RHO_CHECK_THREAD(set);
	return rho_makeint(rho_set_len(set));
}

static RhoValue set_str(RhoValue *this)
{
	RhoSetObject *set = rho_objvalue(this);
	RHO_CHECK_THREAD(set);

	const size_t capacity = set->capacity;

	if (set->count == 0) {
		return rho_strobj_make_direct("{}", 2);
	}

	RhoStrBuf sb;
	rho_strbuf_init(&sb, 16);
	rho_strbuf_append(&sb, "{", 1);

	Entry **entries = set->entries;

	bool first = true;
	for (size_t i = 0; i < capacity; i++) {
		for (Entry *e = entries[i]; e != NULL; e = e->next) {
			if (!first) {
				rho_strbuf_append(&sb, ", ", 2);
			}
			first = false;

			RhoValue *element = &e->element;

			if (rho_isobject(element) && rho_objvalue(element) == set) {
				rho_strbuf_append(&sb, "{...}", 5);
			} else {
				RhoValue str_v = rho_op_str(element);

				if (rho_iserror(&str_v)) {
					rho_strbuf_dealloc(&sb);
					return str_v;
				}

				RhoStrObject *str = rho_objvalue(&str_v);
				rho_strbuf_append(&sb, str->str.value, str->str.len);
				rho_releaseo(str);
			}
		}
	}
	rho_strbuf_append(&sb, "}", 1);

	RhoStr dest;
	rho_strbuf_to_str(&sb, &dest);
	dest.freeable = 1;

	return rho_strobj_make(dest);
}

static RhoValue iter_make(RhoSetObject *set);

static RhoValue set_iter(RhoValue *this)
{
	RhoSetObject *set = rho_objvalue(this);
	RHO_CHECK_THREAD(set);
	return iter_make(set);
}

static RhoValue set_add_method(RhoValue *this,
                               RhoValue *args,
                               RhoValue *args_named,
                               size_t nargs,
                               size_t nargs_named)
{
#define NAME "add"
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 1);

	RhoSetObject *set = rho_objvalue(this);
	RHO_CHECK_THREAD(set);
	return rho_set_add(set, &args[0]);
#undef NAME
}

static RhoValue set_remove_method(RhoValue *this,
                                  RhoValue *args,
                                  RhoValue *args_named,
                                  size_t nargs,
                                  size_t nargs_named)
{
#define NAME "remove"
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 1);

	RhoSetObject *set = rho_objvalue(this);
	RHO_CHECK_THREAD(set);
	return rho_set_remove(set, &args[0]);
#undef NAME
}

static RhoValue set_init(RhoValue *this, RhoValue *args, size_t nargs)
{
	RHO_ARG_COUNT_CHECK_AT_MOST("Set", nargs, 1);
	rho_obj_class.init(this, NULL, 0);

	RhoSetObject *set = rho_objvalue(this);
	set->entries = make_empty_table(EMPTY_SIZE);
	set->count = 0;
	set->capacity = EMPTY_SIZE;
	set->threshold = (size_t)(EMPTY_SIZE * LOAD_FACTOR);
	set->state_id = 0;

	if (nargs > 0) {
		RhoValue iter = rho_op_iter(&args[0]);

		if (rho_iserror(&iter)) {
			set_free_entries(set);
			return iter;
		}

		while (true) {
			RhoValue next = rho_op_iternext(&iter);

			if (rho_is_iter_stop(&next)) {
				break;
			} else if (rho_iserror(&next)) {
				set_free_entries(set);
				rho_release(&iter);
				return next;
			}

			RhoValue v = rho_set_add(set, &next);

			if (rho_iserror(&v)) {
				set_free_entries(set);
				rho_release(&next);
				rho_release(&iter);
				return v;
			}
		}

		rho_release(&iter);
	}

	return *this;
}

static void set_free_entries(RhoSetObject *set)
{
	Entry **entries = set->entries;
	const size_t capacity = set->capacity;

	for (size_t i = 0; i < capacity; i++) {
		Entry *entry = entries[i];
		while (entry != NULL) {
			Entry *temp = entry;
			entry = entry->next;
			rho_release(&temp->element);
			free(temp);
		}
	}

	free(entries);
}

static void set_free(RhoValue *this)
{
	RhoSetObject *set = rho_objvalue(this);
	set_free_entries(set);
	rho_obj_class.del(this);
}

struct rho_num_methods rho_set_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	NULL,    /* add */
	NULL,    /* sub */
	NULL,    /* mul */
	NULL,    /* div */
	NULL,    /* mod */
	NULL,    /* pow */

	NULL,    /* bitnot */
	NULL,    /* bitand */
	NULL,    /* bitor */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	NULL,    /* iadd */
	NULL,    /* isub */
	NULL,    /* imul */
	NULL,    /* idiv */
	NULL,    /* imod */
	NULL,    /* ipow */

	NULL,    /* ibitand */
	NULL,    /* ibitor */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	NULL,    /* radd */
	NULL,    /* rsub */
	NULL,    /* rmul */
	NULL,    /* rdiv */
	NULL,    /* rmod */
	NULL,    /* rpow */

	NULL,    /* rbitand */
	NULL,    /* rbitor */
	NULL,    /* rxor */
	NULL,    /* rshiftl */
	NULL,    /* rshiftr */

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct rho_seq_methods rho_set_seq_methods = {
	set_len,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	set_contains,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

struct rho_attr_method set_methods[] = {
	{"add", set_add_method},
	{"remove", set_remove_method},
	{NULL, NULL}
};

RhoClass rho_set_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Set",
	.super = &rho_obj_class,

	.instance_size = sizeof(RhoSetObject),

	.init = set_init,
	.del = set_free,

	.eq = set_eq,
	.hash = NULL,
	.cmp = NULL,
	.str = set_str,
	.call = NULL,

	.print = NULL,

	.iter = set_iter,
	.iternext = NULL,

	.num_methods = &rho_set_num_methods,
	.seq_methods = &rho_set_seq_methods,

	.members = NULL,
	.methods = set_methods,

	.attr_get = NULL,
	.attr_set = NULL
};


/* set iterator */

static RhoValue iter_make(RhoSetObject *set)
{
	RhoSetIter *iter = rho_obj_alloc(&rho_set_iter_class);
	rho_retaino(set);
	iter->source = set;
	iter->saved_state_id = set->state_id;
	iter->current_entry = NULL;
	iter->current_index = 0;
	return rho_makeobj(iter);
}

static RhoValue iter_next(RhoValue *this)
{
	RhoSetIter *iter = rho_objvalue(this);
	RHO_CHECK_THREAD(iter->source);

	if (iter->saved_state_id != iter->source->state_id) {
		return RHO_ISC_EXC("set changed state during iteration");
	}

	Entry **entries = iter->source->entries;
	Entry *entry = iter->current_entry;
	size_t idx = iter->current_index;
	const size_t capacity = iter->source->capacity;

	if (idx >= capacity) {
		return rho_get_iter_stop();
	}

	if (entry == NULL) {
		for (size_t i = idx; i < capacity; i++) {
			if (entries[i] != NULL) {
				entry = entries[i];
				idx = i+1;
				break;
			}
		}

		if (entry == NULL) {
			return rho_get_iter_stop();
		}
	}

	RhoValue next = entry->element;
	rho_retain(&next);

	iter->current_index = idx;
	iter->current_entry = entry->next;

	return next;
}

static void iter_free(RhoValue *this)
{
	RhoSetIter *iter = rho_objvalue(this);
	rho_releaseo(iter->source);
	rho_iter_class.del(this);
}

struct rho_seq_methods set_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_set_iter_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "SetIter",
	.super = &rho_iter_class,

	.instance_size = sizeof(RhoSetIter),

	.init = NULL,
	.del = iter_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = iter_next,

	.num_methods = NULL,
	.seq_methods = &set_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
