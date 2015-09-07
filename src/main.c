#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "loader.h"
#include "util.h"
#include "main.h"

#define COMPILE_TEST 1
#define RUN_TEST 1

#define INFILE "misc/test.rho"
#define OUTFILE "misc/test"

int main(void)
{
#if COMPILE_TEST
	char *src = file_to_str(INFILE);
	Parser *p = parser_new(src, strlen(src), INFILE);

	if (PARSER_ERROR(p)) {
		fprintf(stderr, "%s\n", p->error_msg);
		FREE(src);
		parser_free(p);
		return EXIT_FAILURE;
	}

	Program *prog = parse(p);

	if (PARSER_ERROR(p)) {
		fprintf(stderr, "%s\n", p->error_msg);
		FREE(src);
		parser_free(p);
		return EXIT_FAILURE;
	}

	parser_free(p);
	FREE(src);

	FILE *out_file = fopen(OUTFILE RHOC_EXT, "wb");
	compile(INFILE, prog, out_file);
	fclose(out_file);
	ast_list_free(prog);
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
