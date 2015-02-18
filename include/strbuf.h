#ifndef STRBUF_H
#define STRBUF_H

#include <stdlib.h>
#include "str.h"

typedef struct {
	char *buf;
	size_t len;
	size_t cap;
} StrBuf;

void strbuf_init(StrBuf *sb, const size_t cap);
void strbuf_init_default(StrBuf *sb);
void strbuf_append(StrBuf *sb, const char *str, const size_t len);
void strbuf_to_str(StrBuf *sb, Str *dest);
void strbuf_dealloc(StrBuf *sb);

#endif /* STRBUF_H */
