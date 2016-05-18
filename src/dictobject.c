#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "strobject.h"
#include "tupleobject.h"
#include "iter.h"
#include "util.h"
#include "dictobject.h"

#define EMPTY_SIZE  16
#define LOAD_FACTOR 0.75f

typedef struct rho_dict_entry Entry;

#define KEY_EXC(key) RHO_INDEX_EXC("dict has no key '%s'", (key));

static Entry **make_empty_table(const size_t capacity);
static void dict_resize(RhoDictObject *dict, const size_t new_capacity);
static void dict_free(RhoValue *this);

RhoValue rho_dict_make(RhoValue *entries, const size_t size)
{
	RhoDictObject *dict = rho_obj_alloc(&rho_dict_class);
	const size_t capacity = (size == 0) ? EMPTY_SIZE : rho_smallest_pow_2_at_least(size);

	dict->entries = make_empty_table(capacity);
	dict->count = 0;
	dict->capacity = capacity;
	dict->threshold = (size_t)(capacity * LOAD_FACTOR);
	dict->state_id = 0;

	for (size_t i = 0; i < size; i += 2) {
		RhoValue *key = &entries[i];
		RhoValue *value = &entries[i+1];
		RhoValue v = rho_dict_put(dict, key, value);

		if (rho_iserror(&v)) {
			RhoValue dict_v = rho_makeobj(dict);
			dict_free(&dict_v);

			for (size_t j = i; j < size; j++) {
				rho_release(&entries[j]);
			}

			return v;
		} else {
			rho_release(&v);
		}

		rho_release(key);
		rho_release(value);
	}

	return rho_makeobj(dict);
}

static Entry *make_entry(RhoValue *key, RhoValue *value, const int hash)
{
	Entry *entry = rho_malloc(sizeof(Entry));
	entry->key = *key;
	entry->value = *value;
	entry->hash = hash;
	entry->next = NULL;
	return entry;
}

RhoValue rho_dict_get(RhoDictObject *dict, RhoValue *key, RhoValue *dflt)
{
	const RhoValue hash_v = rho_op_hash(key);

	if (rho_iserror(&hash_v)) {
		return hash_v;
	}

	const int hash = rho_util_hash_secondary(rho_intvalue(&hash_v));

	/* every value should have a valid `eq` */
	const BoolBinOp eq = rho_resolve_eq(rho_getclass(key));

	for (Entry *entry = dict->entries[hash & (dict->capacity - 1)];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash && eq(key, &entry->key)) {
			rho_retain(&entry->value);
			return entry->value;
		}
	}

	if (dflt != NULL) {
		rho_retain(dflt);
		return *dflt;
	} else {
		RhoStrObject *str = rho_op_str(key);
		RhoValue exc = KEY_EXC(str->str.value);
		rho_releaseo(str);
		return exc;
	}
}

RhoValue rho_dict_put(RhoDictObject *dict, RhoValue *key, RhoValue *value)
{
	Entry **table = dict->entries;
	const size_t capacity = dict->capacity;
	const RhoValue hash_v = rho_op_hash(key);

	if (rho_iserror(&hash_v)) {
		return hash_v;
	}

	++dict->state_id;
	const int hash = rho_util_hash_secondary(rho_intvalue(&hash_v));
	const size_t index = hash & (capacity - 1);

	/* every value should have a valid `eq` */
	const BoolBinOp eq = rho_resolve_eq(rho_getclass(key));

	for (Entry *entry = table[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash && eq(key, &entry->key)) {
			rho_retain(value);
			RhoValue old = entry->value;
			entry->value = *value;
			return old;
		}
	}

	rho_retain(key);
	rho_retain(value);

	Entry *entry = make_entry(key, value, hash);
	entry->next = table[index];
	table[index] = entry;

	if (dict->count++ >= dict->threshold) {
		const size_t new_capacity = (2 * capacity);
		dict_resize(dict, new_capacity);
		dict->threshold = (size_t)(new_capacity * LOAD_FACTOR);
	}

	return rho_makeempty();
}

RhoValue rho_dict_remove_key(RhoDictObject *dict, RhoValue *key)
{
	const RhoValue hash_v = rho_op_hash(key);

	if (rho_iserror(&hash_v)) {
		return hash_v;
	}

	const int hash = rho_util_hash_secondary(rho_intvalue(&hash_v));

	/* every value should have a valid `eq` */
	const BoolBinOp eq = rho_resolve_eq(rho_getclass(key));

	Entry **entries = dict->entries;
	const size_t idx = hash & (dict->capacity - 1);
	Entry *entry = entries[idx];
	Entry *prev = entry;

	while (entry != NULL) {
		Entry *next = entry->next;

		if (hash == entry->hash && eq(key, &entry->key)) {
			RhoValue value = entry->value;

			if (prev == entry) {
				entries[idx] = next;
			} else {
				prev->next = next;
			}

			rho_release(&entry->key);
			free(entry);
			--dict->count;
			++dict->state_id;
			return value;
		}

		prev = entry;
		entry = next;
	}

	return rho_makeempty();
}

bool rho_dict_contains_key(RhoDictObject *dict, RhoValue *key)
{
	Entry **entries = dict->entries;
	const size_t capacity = dict->capacity;
	RhoValue hash_v = rho_op_hash(key);

	if (rho_iserror(&hash_v)) {
		rho_release(&hash_v);
		return false;
	}

	const int hash = rho_util_hash_secondary(rho_intvalue(&hash_v));
	const size_t idx = hash & (capacity - 1);

	/* every value should have a valid `eq` */
	const BoolBinOp eq = rho_resolve_eq(rho_getclass(key));

	for (Entry *entry = entries[idx];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash && eq(key, &entry->key)) {
			return true;
		}
	}

	return false;
}

bool rho_dict_eq(RhoDictObject *dict, RhoDictObject *other)
{
	if (dict->count != other->count) {
		return false;
	}

	RhoDictObject *d1, *d2;

	if (dict->capacity < other->capacity) {
		d1 = dict;
		d2 = other;
	} else {
		d1 = other;
		d2 = dict;
	}

	Entry **entries = d1->entries;
	size_t capacity = d1->capacity;
	static RhoValue empty = rho_makeempty();

	for (size_t i = 0; i < capacity; i++) {
		for (Entry *entry = entries[i];
		     entry != NULL;
		     entry = entry->next) {

			RhoValue v1 = entry->value;
			const BoolBinOp eq = rho_resolve_eq(rho_getclass(&v1));
			RhoValue v2 = rho_dict_get(d2, &entry->key, &empty);

			if (rho_isempty(&v2) || !eq(&v1, &v2)) {
				return false;
			}
		}
	}

	return true;
}

size_t rho_dict_len(RhoDictObject *dict)
{
	return dict->count;
}

static Entry **make_empty_table(const size_t capacity)
{
	Entry **table = rho_malloc(capacity * sizeof(Entry *));
	for (size_t i = 0; i < capacity; i++) {
		table[i] = NULL;
	}
	return table;
}

static void dict_resize(RhoDictObject *dict, const size_t new_capacity)
{
	const size_t old_capacity = dict->capacity;
	Entry **old_entries = dict->entries;
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
	dict->entries = new_entries;
	dict->capacity = new_capacity;
	++dict->state_id;
}

static RhoValue dict_get(RhoValue *this, RhoValue *key)
{
	RhoDictObject *dict = rho_objvalue(this);
	return rho_dict_get(dict, key, NULL);
}

static RhoValue dict_set(RhoValue *this, RhoValue *key, RhoValue *value)
{
	RhoDictObject *dict = rho_objvalue(this);
	return rho_dict_put(dict, key, value);
}

static bool dict_contains(RhoValue *this, RhoValue *key)
{
	RhoDictObject *dict = rho_objvalue(this);
	return rho_dict_contains_key(dict, key);
}

static bool dict_eq(RhoValue *this, RhoValue *other)
{
	if (!rho_is_a(other, &rho_dict_class)) {
		return false;
	}

	RhoDictObject *dict = rho_objvalue(this);
	RhoDictObject *other_dict = rho_objvalue(other);
	return rho_dict_eq(dict, other_dict);
}

static size_t dict_len(RhoValue *this)
{
	RhoDictObject *dict = rho_objvalue(this);
	return rho_dict_len(dict);
}

static RhoStrObject *dict_str(RhoValue *this)
{
	RhoDictObject *dict = rho_objvalue(this);
	const size_t capacity = dict->capacity;

	if (dict->count == 0) {
		RhoValue ret = rho_strobj_make_direct("{}", 2);
		return (RhoStrObject *)rho_objvalue(&ret);
	}

	RhoStrBuf sb;
	rho_strbuf_init(&sb, 16);
	rho_strbuf_append(&sb, "{", 1);

	Entry **entries = dict->entries;

	bool first = true;
	for (size_t i = 0; i < capacity; i++) {
		for (Entry *e = entries[i]; e != NULL; e = e->next) {
			if (!first) {
				rho_strbuf_append(&sb, ", ", 2);
			}
			first = false;

			RhoValue *key = &e->key;
			RhoValue *value = &e->value;

			if (rho_isobject(key) && rho_objvalue(key) == dict) {
				rho_strbuf_append(&sb, "{...}", 5);
			} else {
				RhoStrObject *str = rho_op_str(key);
				rho_strbuf_append(&sb, str->str.value, str->str.len);
				rho_releaseo(str);
			}

			rho_strbuf_append(&sb, ": ", 2);

			if (rho_isobject(value) && rho_objvalue(value) == dict) {
				rho_strbuf_append(&sb, "{...}", 5);
			} else {
				RhoStrObject *str = rho_op_str(value);
				rho_strbuf_append(&sb, str->str.value, str->str.len);
				rho_releaseo(str);
			}
		}
	}
	rho_strbuf_append(&sb, "}", 1);

	RhoStr dest;
	rho_strbuf_to_str(&sb, &dest);
	dest.freeable = 1;

	RhoValue ret = rho_strobj_make(dest);
	return (RhoStrObject *)rho_objvalue(&ret);
}

static RhoValue iter_make(RhoDictObject *dict);

static RhoValue dict_iter(RhoValue *this)
{
	RhoDictObject *dict = rho_objvalue(this);
	return iter_make(dict);
}

static RhoValue dict_get_method(RhoValue *this,
                                RhoValue *args,
                                RhoValue *args_named,
                                size_t nargs,
                                size_t nargs_named)
{
#define NAME "get"
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK_BETWEEN(NAME, nargs, 1, 2);

	RhoDictObject *dict = rho_objvalue(this);
	return rho_dict_get(dict, &args[0], (nargs == 2) ? &args[1] : NULL);
#undef NAME
}

static RhoValue dict_put_method(RhoValue *this,
                                RhoValue *args,
                                RhoValue *args_named,
                                size_t nargs,
                                size_t nargs_named)
{
#define NAME "put"
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 2);

	RhoDictObject *dict = rho_objvalue(this);
	RhoValue old = rho_dict_put(dict, &args[0], &args[1]);
	return rho_isempty(&old) ? rho_makenull() : old;
#undef NAME
}

static RhoValue dict_remove_method(RhoValue *this,
                                   RhoValue *args,
                                   RhoValue *args_named,
                                   size_t nargs,
                                   size_t nargs_named)
{
#define NAME "remove"
	RHO_UNUSED(args_named);
	RHO_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	RHO_ARG_COUNT_CHECK(NAME, nargs, 1);

	RhoDictObject *dict = rho_objvalue(this);
	RhoValue v = rho_dict_remove_key(dict, &args[0]);
	return rho_isempty(&v) ? rho_makenull() : v;
#undef NAME
}

static void dict_free(RhoValue *this)
{
	RhoDictObject *dict = rho_objvalue(this);
	Entry **entries = dict->entries;
	const size_t capacity = dict->capacity;

	for (size_t i = 0; i < capacity; i++) {
		Entry *entry = entries[i];
		while (entry != NULL) {
			Entry *temp = entry;
			entry = entry->next;
			rho_release(&temp->key);
			rho_release(&temp->value);
			free(temp);
		}
	}

	free(entries);
	obj_class.del(this);
}

struct rho_num_methods rho_dict_num_methods = {
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

struct rho_seq_methods rho_dict_seq_methods = {
	dict_len,    /* len */
	dict_get,    /* get */
	dict_set,    /* set */
	dict_contains,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

struct rho_attr_method dict_methods[] = {
	{"get", dict_get_method},
	{"put", dict_put_method},
	{"remove", dict_remove_method},
	{NULL, NULL}
};

RhoClass rho_dict_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "Dict",
	.super = &obj_class,

	.instance_size = sizeof(RhoDictObject),

	.init = NULL,
	.del = dict_free,

	.eq = dict_eq,
	.hash = NULL,
	.cmp = NULL,
	.str = dict_str,
	.call = NULL,

	.print = NULL,

	.iter = dict_iter,
	.iternext = NULL,

	.num_methods = &rho_dict_num_methods,
	.seq_methods = &rho_dict_seq_methods,

	.members = NULL,
	.methods = dict_methods,

	.attr_get = NULL,
	.attr_set = NULL
};


/* dict iterator */

static RhoValue iter_make(RhoDictObject *dict)
{
	RhoDictIter *iter = rho_obj_alloc(&rho_dict_iter_class);
	rho_retaino(dict);
	iter->source = dict;
	iter->saved_state_id = dict->state_id;
	iter->current_entry = NULL;
	iter->current_index = 0;
	return rho_makeobj(iter);
}

static RhoValue iter_next(RhoValue *this)
{
	RhoDictIter *iter = rho_objvalue(this);

	if (iter->saved_state_id != iter->source->state_id) {
		return RHO_ISC_EXC("dict changed state during iteration");
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

	RhoValue pair[] = {entry->key, entry->value};
	rho_retain(&pair[0]);
	rho_retain(&pair[1]);

	iter->current_index = idx;
	iter->current_entry = entry->next;

	return rho_tuple_make(pair, 2);
}

static void iter_free(RhoValue *this)
{
	RhoDictIter *iter = rho_objvalue(this);
	rho_releaseo(iter->source);
	rho_iter_class.del(this);
}

struct rho_seq_methods dict_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

RhoClass rho_dict_iter_class = {
	.base = RHO_CLASS_BASE_INIT(),
	.name = "DictIter",
	.super = &rho_iter_class,

	.instance_size = sizeof(RhoDictIter),

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
	.seq_methods = &dict_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
