#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "loader.h"
#include "err.h"
#include "util.h"
#include "main.h"

#define RHO_VERSION "0.0.0"

#define ANSI_CLR_RED     "\x1b[31m"
#define ANSI_CLR_GREEN   "\x1b[32m"
#define ANSI_CLR_YELLOW  "\x1b[33m"
#define ANSI_CLR_BLUE    "\x1b[34m"
#define ANSI_CLR_MAGENTA "\x1b[35m"
#define ANSI_CLR_CYAN    "\x1b[36m"
#define ANSI_CLR_INFO    "\x1b[1;36m"
#define ANSI_CLR_WARNING "\x1b[1;33m"
#define ANSI_CLR_ERROR   "\x1b[1;31m"
#define ANSI_CLR_RESET   "\x1b[0m"

#define INFO_HEADER    ANSI_CLR_INFO    "info:    " ANSI_CLR_RESET
#define WARNING_HEADER ANSI_CLR_WARNING "warning: " ANSI_CLR_RESET
#define ERROR_HEADER   ANSI_CLR_ERROR   "error:   " ANSI_CLR_RESET

enum cmd_flags {
	FLAG_UNKNOWN     = 1 << 0,
	FLAG_NOT_OPT     = 1 << 1,
	FLAG_HELP        = 1 << 2,
	FLAG_VERSION     = 1 << 3,
	FLAG_COMPILE     = 1 << 4,
	FLAG_DISASSEMBLE = 1 << 5
};

static const struct {
	const char short_opt;
	const char *long_opt;
	enum cmd_flags mask;
	const char *description;
} options[] = {
	{'h', "help",        FLAG_HELP,        "print this message and exit"},
	{'V', "version",     FLAG_VERSION,     "print version number and exit"},
	{'c', "compile",     FLAG_COMPILE,     "compile (rho ==> rhoc)"},
	{'d', "disassemble", FLAG_DISASSEMBLE, "dump disassembled bytecode"},
	{'\0', NULL, 0, NULL}
};

static void print_usage_and_exit(const char *argv0, const int status)
{
	fprintf(stderr, "usage: %s [options] <file>\n", argv0);
	for (int i = 0; options[i].mask != 0; i++) {
		fprintf(stderr, "-%c : %s\n", options[i].short_opt, options[i].description);
	}
	exit(status);
}

static void print_version_and_exit(void)
{
	fprintf(stderr, "Rho " RHO_VERSION "\n");
	exit(EXIT_SUCCESS);
}

static void print_not_implemented_and_exit(const enum cmd_flags mask)
{
	for (int i = 0; options[i].mask != 0; i++) {
		if (options[i].mask == mask) {
			fprintf(stderr,
					ERROR_HEADER "-%c/--%s not yet implemented\n",
			        options[i].short_opt,
			        options[i].long_opt);
			exit(EXIT_FAILURE);
		}
	}
	INTERNAL_ERROR();
}

static int process_arg(const char *arg)
{
#define UNKNOWN_OPT_FMT_SHORT "Unknown option: -%c\n"
#define UNKNOWN_OPT_FMT_LONG  "Unknown option: --%s\n"

	if (arg[0] == '-') {
		if (arg[1] == '-') {
			for (int i = 0; options[i].mask != 0; i++) {
				if (strcmp(&arg[2], options[i].long_opt) == 0) {
					return options[i].mask;
				}
			}

			fprintf(stderr, UNKNOWN_OPT_FMT_LONG, &arg[2]);
			return FLAG_UNKNOWN;
		} else {
			enum cmd_flags opts = 0;

			for (int i = 1; arg[i] != '\0'; i++) {
				bool found = false;
				for (int j = 0; options[j].mask != 0; j++) {
					if (options[j].short_opt == arg[i]) {
						opts |= options[j].mask;
						found = true;
					}
				}

				if (!found) {
					fprintf(stderr, UNKNOWN_OPT_FMT_SHORT, arg[i]);
					return FLAG_UNKNOWN;
				}
			}

			return opts;
		}
	} else {
		return FLAG_NOT_OPT;
	}

#undef UNKNOWN_OPT_FMT_SHORT
#undef UNKNOWN_OPT_FMT_LONG
}

int main(int argc, char *argv[])
{
	enum cmd_flags opts = 0;
	char *filename = NULL;
	for (int i = 1; i < argc; i++) {
		const int flags = process_arg(argv[i]);
		if (flags == FLAG_NOT_OPT) {
			filename = argv[i];
			break;
		} else if (flags == FLAG_UNKNOWN) {
			print_usage_and_exit(argv[0], EXIT_FAILURE);
		} else {
			opts |= flags;
		}
	}

	if (opts & FLAG_HELP) {
		print_usage_and_exit(argv[0], EXIT_SUCCESS);
	}

	if (opts & FLAG_VERSION) {
		print_version_and_exit();
	}

	if (opts & FLAG_DISASSEMBLE) {
		print_not_implemented_and_exit(FLAG_DISASSEMBLE);
	}

	if (filename == NULL) {
		fprintf(stderr, ERROR_HEADER "no input files\n");
		exit(EXIT_FAILURE);
	}

	const char *ext = strrchr(filename, '.');

	if (ext == NULL || !(strcmp(ext, ".rho") == 0 || strcmp(ext, ".rhoc") == 0)) {
		fprintf(stderr, ERROR_HEADER "unknown file type\n");
		fprintf(stderr, INFO_HEADER "input file should be either Rho source (.rho) or compiled bytecode (.rhoc)\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(ext, ".rho") == 0) {
		char *src = file_to_str(filename);

		if (src == NULL) {
			fprintf(stderr, ERROR_HEADER "can't open file '%s'\n", filename);
			exit(EXIT_FAILURE);
		}

		Parser *p = parser_new(src, filename);

		if (PARSER_ERROR(p)) {
			fprintf(stderr, ERROR_HEADER "%s\n", p->error_msg);
			FREE(src);
			parser_free(p);
			exit(EXIT_FAILURE);
		}

		Program *prog = parse(p);

		if (PARSER_ERROR(p)) {
			fprintf(stderr, ERROR_HEADER "%s\n", p->error_msg);
			FREE(src);
			parser_free(p);
			exit(EXIT_FAILURE);
		}

		parser_free(p);
		FREE(src);

		char *out_filename_buf = rho_malloc(strlen(filename) + 2);
		strcpy(out_filename_buf, filename);
		strcat(out_filename_buf, "c");
		FILE *out_file = fopen(out_filename_buf, "w");

		if (out_file == NULL) {
			fprintf(stderr, ERROR_HEADER "can't open file '%sc' for writing\n", out_filename_buf);
			free(out_filename_buf);
			exit(EXIT_FAILURE);
		}

		compile(filename, prog, out_file);
		fclose(out_file);
		ast_list_free(prog);

		if (!(opts & FLAG_COMPILE)) {
			Code code;
			assert(load_from_file(out_filename_buf, true, &code) == 0);
			VM *vm = vm_new();
			set_current_vm(vm);
			vm_exec_code(vm, &code);
			vm_free(vm);
		}

		free(out_filename_buf);
		exit(EXIT_SUCCESS);
	} else if (strcmp(ext, ".rhoc") == 0) {
		if (opts & FLAG_COMPILE) {
			fprintf(stderr, INFO_HEADER "nothing to do\n");
			exit(EXIT_SUCCESS);
		}

		Code code;

		switch (load_from_file(filename, true, &code)) {
		case LOAD_ERR_NONE:
			break;
		case LOAD_ERR_NOT_FOUND:
			fprintf(stderr, ERROR_HEADER "can't open file '%s'\n", filename);
			exit(EXIT_FAILURE);
			break;
		case LOAD_ERR_INVALID_SIGNATURE:
			fprintf(stderr, ERROR_HEADER "rhoc file '%s' had an invalid signature\n", filename);
			exit(EXIT_FAILURE);
			break;
		default:
			INTERNAL_ERROR();
		}

		VM *vm = vm_new();
		set_current_vm(vm);
		vm_exec_code(vm, &code);
		vm_free(vm);
		exit(EXIT_SUCCESS);
	} else {
		INTERNAL_ERROR();
	}
}
