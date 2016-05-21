#ifndef RHO_ATTR_H
#define RHO_ATTR_H

enum rho_attr_type {
	RHO_ATTR_T_CHAR,
	RHO_ATTR_T_BYTE,
	RHO_ATTR_T_SHORT,
	RHO_ATTR_T_INT,
	RHO_ATTR_T_LONG,
	RHO_ATTR_T_UBYTE,
	RHO_ATTR_T_USHORT,
	RHO_ATTR_T_UINT,
	RHO_ATTR_T_ULONG,
	RHO_ATTR_T_SIZE_T,
	RHO_ATTR_T_BOOL,
	RHO_ATTR_T_FLOAT,
	RHO_ATTR_T_DOUBLE,
	RHO_ATTR_T_STRING,
	RHO_ATTR_T_OBJECT
};

/*
 * A class's member attributes are delineated via
 * an array of this structure:
 */
struct rho_attr_member {
	const char *name;
	const enum rho_attr_type type;
	const size_t offset;
	const int flags;
};

#define RHO_ATTR_FLAG_READONLY    (1 << 2)
#define RHO_ATTR_FLAG_TYPE_STRICT (1 << 3)

struct rho_value;
typedef struct rho_value (*MethodFunc)(struct rho_value *this,
                                       struct rho_value *args,
                                       struct rho_value *args_named,
                                       size_t nargs,
                                       size_t nargs_named);

/*
 * A class's method attributes are delineated via
 * an array of this structure:
 */
struct rho_attr_method {
	const char *name;
	const MethodFunc meth;
};

typedef struct rho_attr_dict_entry {
	const char *key;
	unsigned int value;
	int hash;
	struct rho_attr_dict_entry *next;
} RhoAttrDictEntry;

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
typedef struct rho_attr_dict {
	RhoAttrDictEntry **table;
	size_t table_size;
	size_t table_capacity;
} RhoAttrDict;

#define RHO_ATTR_DICT_FLAG_FOUND  (1 << 0)
#define RHO_ATTR_DICT_FLAG_METHOD (1 << 1)

void rho_attr_dict_init(RhoAttrDict *d, const size_t max_size);
unsigned int rho_attr_dict_get(RhoAttrDict *d, const char *key);
void rho_attr_dict_register_members(RhoAttrDict *d, struct rho_attr_member *members);
void rho_attr_dict_register_methods(RhoAttrDict *d, struct rho_attr_method *methods);

#endif /* RHO_ATTR_H */
