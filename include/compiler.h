#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include "code.h"
#include "symtab.h"
#include "consttab.h"
#include "opcodes.h"

extern const byte magic[];
extern const size_t magic_size;

/*
 * The following structure is used for
 * continue/break bookkeeping.
 */
struct loop_block_info {
	size_t start_index;     // start of loop body

	size_t *break_indices;  // indexes of break statements
	size_t break_indices_size;
	size_t break_indices_capacity;

	struct loop_block_info *prev;
};

typedef struct {
	const char *filename;
	Code code;
	struct loop_block_info *lbi;
	SymTable *st;
	ConstTable *ct;

	unsigned int try_catch_depth;
	unsigned int try_catch_depth_max;

	Code lno_table;
	unsigned int first_lineno;
	unsigned int first_ins_on_line_idx;
	unsigned int last_ins_idx;
	unsigned int last_lineno;
} Compiler;

void compile(FILE *src, FILE *out, const char *name);

int arg_size(Opcode opcode);

#endif /* COMPILER_H */
