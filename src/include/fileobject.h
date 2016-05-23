#ifndef RHO_FILEOBJECT_H
#define RHO_FILEOBJECT_H

#include <stdio.h>
#include <stdbool.h>
#include "object.h"

extern struct rho_num_methods rho_file_num_methods;
extern struct rho_seq_methods rho_file_seq_methods;
extern RhoClass rho_file_class;

#define RHO_FILE_FLAG_OPEN   (1 << 0)
#define RHO_FILE_FLAG_READ   (1 << 1)
#define RHO_FILE_FLAG_WRITE  (1 << 2)
#define RHO_FILE_FLAG_APPEND (1 << 3)
#define RHO_FILE_FLAG_UPDATE (1 << 4)

typedef struct {
	RhoObject base;
	FILE *file;
	const char *name;
	int flags;
} RhoFileObject;

RhoValue rho_file_make(const char *filename, const char *mode);
RhoValue rho_file_write(RhoFileObject *fileobj, const char *str, const size_t len);
RhoValue rho_file_readline(RhoFileObject *fileobj);
void rho_file_rewind(RhoFileObject *fileobj);
bool rho_file_close(RhoFileObject *fileobj);

#endif /* RHO_FILEOBJECT_H */
