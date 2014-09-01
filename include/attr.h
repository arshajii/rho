#ifndef ATTR_H
#define ATTR_H

enum attr_type {
	ATTR_T_CHAR,
	ATTR_T_BYTE,
	ATTR_T_SHORT,
	ATTR_T_INT,
	ATTR_T_LONG,
	ATTR_T_UBYTE,
	ATTR_T_USHORT,
	ATTR_T_UINT,
	ATTR_T_ULONG,
	ATTR_T_SIZE_T,
	ATTR_T_BOOL,
	ATTR_T_FLOAT,
	ATTR_T_DOUBLE,
	ATTR_T_STRING,
	ATTR_T_OBJECT
};

/*
 * A class's member attributes are delineated via
 * an array of this structure:
 */
struct attr_member {
	const char *name;
	const enum attr_type type;
	const size_t offset;
	const int flags;
};

struct value;
typedef struct value (*Method)(struct value *this, struct value **args, size_t nargs);

/*
 * A class's method attributes are delineated via
 * an array of this structure:
 */
struct attr_method {
	const char *name;
	const Method *meth;
};

typedef struct attr_dict_entry {
	const char *key;
	unsigned int value;
	int hash;
	struct attr_dict_entry *next;
} AttrDictEntry;

/*
 * Mapping of attribute names to attribute info.
 *
 * Maps key (char *) to value (unsigned int) where the first
 * bit of the value is an error bit indicating if a given key
 * was found in the dict (1) or not (0) and the second bit
 * is a flag indicating whether the attribute is a member (0)
 * or a method (1) and the remaining bits contain the index of
 * the attribute in the corresponding array.
 *
 * Attribute dictionaries should never be modified after they
 * are initialized. Also, the same key should never be added
 * twice (even if the value is the same both times).
 */
typedef struct attr_dict {
	AttrDictEntry **table;
	size_t table_size;
	size_t table_capacity;
} AttrDict;

#define ATTR_DICT_FLAG_FOUND   (1 << 0)
#define ATTR_DICT_FLAG_METHOD  (1 << 1)

void attr_dict_init(AttrDict *d, const size_t max_size);
unsigned int attr_dict_get(AttrDict *d, const char *key);
void attr_dict_register_members(AttrDict *d, struct attr_member *members);
void attr_dict_register_methods(AttrDict *d, struct attr_method *methods);

#endif /* ATTR_H */
