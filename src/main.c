#include <stdlib.h>
#include <stdio.h>
#include "compiler.h"
#include "vm.h"
#include "main.h"

#define COMPILE_TEST 1
#define RUN_TEST 1

#define INFILE "misc/test.rho"
#define OUTFILE "misc/test.rhoc"

int main(void)
{
#if COMPILE_TEST
	FILE *src = fopen(INFILE, "rb");
	FILE *out = fopen(OUTFILE, "wb");
	compile(src, out, INFILE);
	fclose(src);
	fclose(out);
#endif

#if RUN_TEST
	printf("=== START ===\n");
	FILE *compiled = fopen(OUTFILE, "rb");
	execute(compiled);
	fclose(compiled);
	printf("===  END  ===\n");
#endif
}
