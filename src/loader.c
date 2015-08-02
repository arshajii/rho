#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "code.h"
#include "compiler.h"
#include "err.h"
#include "loader.h"

int load_from_file(const char *name, Code *dest)
{
	char filename_buf[FILENAME_MAX];
	strcpy(filename_buf, name);
	strcat(filename_buf, RHOC_EXT);

	FILE *compiled = fopen(filename_buf, "rb");

	if (compiled == NULL) {
		return 1;
	}

	fseek(compiled, 0L, SEEK_END);
	const size_t code_size = ftell(compiled) - magic_size;
	fseek(compiled, 0L, SEEK_SET);

	/* verify file signature */
	for (size_t i = 0; i < magic_size; i++) {
		const byte c = fgetc(compiled);
		if (c != magic[i]) {
			fatal_error("invalid file signature");
			return 1;
		}
	}

	code_init(dest, code_size);
	fread(dest->bc, 1, code_size, compiled);
	dest->size = code_size;
	fclose(compiled);
	return 0;
}
