#ifndef RHO_COMPILER_H
#define RHO_COMPILER_H

#include <stdio.h>
#include "code.h"
#include "symtab.h"
#include "consttab.h"
#include "opcodes.h"

extern const byte rho_magic[];
extern const size_t rho_magic_size;

/*
 * The following structure is used for
 * continue/break bookkeeping.
 */
struct rho_loop_block_info {
	size_t start_index;     // start of loop body

	size_t *break_indices;  // indexes of break statements
	size_t break_indices_size;
	size_t break_indices_capacity;

	struct rho_loop_block_info *prev;
};

typedef struct {
	const char *filename;
	RhoCode code;
	struct rho_loop_block_info *lbi;
	RhoSymTable *st;
	RhoConstTable *ct;

	unsigned int try_catch_depth;
	unsigned int try_catch_depth_max;

	RhoCode lno_table;
	unsigned int first_lineno;
	unsigned int first_ins_on_line_idx;
	unsigned int last_ins_idx;
	unsigned int last_lineno;

	unsigned in_generator : 1;
} RhoCompiler;

void rho_compile(const char *name, RhoProgram *prog, FILE *out);

int rho_opcode_arg_size(RhoOpcode opcode);

#endif /* RHO_COMPILER_H */
