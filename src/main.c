#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "compiler.h"
#include "vm.h"
#include "loader.h"
#include "main.h"

#define COMPILE_TEST 1
#define RUN_TEST 1

#define INFILE "misc/test.rho"
#define OUTFILE "misc/test"

int main(void)
{
#if COMPILE_TEST
	FILE *src = fopen(INFILE, "r");
	FILE *out = fopen(OUTFILE RHOC_EXT, "wb");
	compile(src, out, INFILE);
	fclose(src);
	fclose(out);
#endif

#if RUN_TEST
	printf("=== START ===\n");

	Code code;
	assert(load_from_file(OUTFILE, &code) == 0);
	VM *vm = vm_new();
	vm_exec_code(vm, &code);
	vm_free(vm, true);

	printf("===  END  ===\n");
#endif
}
