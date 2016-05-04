#ifndef RHO_CONSTTAB_H
#define RHO_CONSTTAB_H

#include <stdlib.h>
#include <stdbool.h>
#include "str.h"
#include "code.h"

#define RHO_CT_CAPACITY 16
#define RHO_CT_LOADFACTOR 0.75f

typedef enum {
	RHO_CT_INT,
	RHO_CT_DOUBLE,
	RHO_CT_STRING,
	RHO_CT_CODEOBJ
} RhoConstType;

typedef struct {
	RhoConstType type;

	union {
		int i;
		double d;
		RhoStr *s;
		RhoCode *c;
	} value;
} RhoCTConst;

typedef struct rho_ct_entry {
	RhoCTConst key;

	unsigned int value;  // index of the constant
	int hash;
	struct rho_ct_entry *next;
} RhoCTEntry;

/*
 * Simple constant table
 */
typedef struct {
	RhoCTEntry **table;
	size_t table_size;
	size_t capacity;

	float load_factor;
	size_t threshold;

	unsigned int next_id;

	/*
	 * Code objects work somewhat differently in the constant
	 * indexing mechanism, so they are dealt with separately.
	 */
	RhoCTEntry *codeobjs_head;
	RhoCTEntry *codeobjs_tail;
	size_t codeobjs_size;
} RhoConstTable;

RhoConstTable *rho_ct_new(void);
unsigned int rho_ct_id_for_const(RhoConstTable *ct, RhoCTConst key);
unsigned int rho_ct_poll_codeobj(RhoConstTable *ct);
void rho_ct_free(RhoConstTable *ct);

#endif /* RHO_CONSTTAB_H */
