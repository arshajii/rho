#ifndef RHO_LOADER_H
#define RHO_LOADER_H

#include "code.h"

#define RHO_EXT  ".rho"
#define RHOC_EXT ".rhoc"

enum {
	RHO_LOAD_ERR_NONE,
	RHO_LOAD_ERR_NOT_FOUND,
	RHO_LOAD_ERR_INVALID_SIGNATURE
};

int rho_load_from_file(const char *name, const bool name_has_ext, RhoCode *dest);

#endif /* RHO_LOADER_H */
