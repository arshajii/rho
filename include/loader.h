#ifndef LOADER_H
#define LOADER_H

#include "code.h"

#define RHO_EXT  ".rho"
#define RHOC_EXT ".rhoc"

enum {
	LOAD_ERR_NONE,
	LOAD_ERR_NOT_FOUND,
	LOAD_ERR_INVALID_SIGNATURE
};

int load_from_file(const char *name, Code *dest);

#endif /* LOADER_H */
