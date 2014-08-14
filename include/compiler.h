#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include "code.h"
#include "symtab.h"
#include "consttab.h"

extern const byte magic[];
extern const size_t magic_size;

#define DEFAULT_BC_CAPACITY 100

typedef struct {
	Code code;
	SymTable *st;
	ConstTable *ct;
} Compiler;

void compile(FILE *src, FILE *out, const char *name);

int read_int(byte *bytes);
double read_double(byte *bytes);

#endif /* COMPILER_H */
