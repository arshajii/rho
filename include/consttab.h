#ifndef CONSTTAB_H
#define CONSTTAB_H

#include <stdlib.h>
#include <stdbool.h>
#include "str.h"
#include "code.h"

#define CT_CAPACITY 16
#define CT_LOADFACTOR 0.75f

typedef enum {
	CT_INT,
	CT_DOUBLE,
	CT_STRING,
	CT_CODEOBJ
} ConstType;

typedef struct {
	ConstType type;

	union {
		int i;
		double d;
		Str *s;
		Code *c;
	} value;
} CTConst;

typedef struct CTEntry {
	CTConst key;

	unsigned int value;  // index of the constant
	int hash;
	struct CTEntry *next;
} CTEntry;

/*
 * Simple constant table
 */
typedef struct {
	CTEntry **table;
	size_t table_size;
	size_t capacity;

	float load_factor;
	size_t threshold;

	unsigned int next_id;

	/*
	 * Code objects work somewhat differently in the constant
	 * indexing mechanism, so they are dealt with separately.
	 */
	CTEntry *codeobjs_head;
	CTEntry *codeobjs_tail;
	size_t codeobjs_size;
} ConstTable;

ConstTable *ct_new(void);
unsigned int ct_id_for_const(ConstTable *ct, CTConst key);
unsigned int ct_poll_codeobj(ConstTable *ct);
void ct_free(ConstTable *ct);

#endif /* CONSTTAB_H */
