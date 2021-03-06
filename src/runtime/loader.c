#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "code.h"
#include "compiler.h"
#include "err.h"
#include "util.h"
#include "loader.h"

int rho_load_from_file(const char *name, const bool name_has_ext, RhoCode *dest)
{
	FILE *compiled;

	if (name_has_ext) {
		compiled = fopen(name, "rb");
	} else {
		char *filename_buf = rho_malloc(strlen(name) + strlen(RHOC_EXT) + 1);
		strcpy(filename_buf, name);
		strcat(filename_buf, RHOC_EXT);
		compiled = fopen(filename_buf, "rb");
		free(filename_buf);
	}

	if (compiled == NULL) {
		return RHO_LOAD_ERR_NOT_FOUND;
	}

	fseek(compiled, 0L, SEEK_END);
	const size_t code_size = ftell(compiled) - rho_magic_size;
	fseek(compiled, 0L, SEEK_SET);

	/* verify file signature */
	for (size_t i = 0; i < rho_magic_size; i++) {
		const byte c = fgetc(compiled);
		if (c != rho_magic[i]) {
			return RHO_LOAD_ERR_INVALID_SIGNATURE;
		}
	}

	rho_code_init(dest, code_size);
	fread(dest->bc, 1, code_size, compiled);
	dest->size = code_size;
	fclose(compiled);
	return RHO_LOAD_ERR_NONE;
}
