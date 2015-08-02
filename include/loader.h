#ifndef LOADER_H
#define LOADER_H

#include "code.h"

#define RHO_EXT  ".rho"
#define RHOC_EXT ".rhoc"

int load_from_file(const char *name, Code *dest);

#endif /* LOADER_H */
