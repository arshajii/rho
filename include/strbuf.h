#ifndef RHO_STRBUF_H
#define RHO_STRBUF_H

#include <stdlib.h>
#include "str.h"

typedef struct {
	char *buf;
	size_t len;
	size_t cap;
} RhoStrBuf;

void rho_strbuf_init(RhoStrBuf *sb, const size_t cap);
void rho_strbuf_init_default(RhoStrBuf *sb);
void rho_strbuf_append(RhoStrBuf *sb, const char *str, const size_t len);
void rho_strbuf_to_str(RhoStrBuf *sb, RhoStr *dest);
void rho_strbuf_dealloc(RhoStrBuf *sb);

#endif /* RHO_STRBUF_H */
