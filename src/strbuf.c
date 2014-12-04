#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "str.h"
#include "strbuf.h"

#define STRBUF_DEFAULT_INIT_CAPACITY 16

static void strbuf_grow(StrBuf *sb, const size_t min_cap);

void strbuf_init(StrBuf *sb, const size_t cap)
{
	assert(cap > 0);
	sb->buf = malloc(cap);
	sb->len = 0;
	sb->cap = cap;
	sb->buf[0] = '\0';
}

void strbuf_init_default(StrBuf *sb)
{
	strbuf_init(sb, STRBUF_DEFAULT_INIT_CAPACITY);
}

void strbuf_append(StrBuf *sb, const char *str, const size_t len)
{
	size_t new_len = sb->len + len;
	if (new_len + 1 < sb->cap) {
		strbuf_grow(sb, new_len + 1);
	}
	memcpy(sb->buf + sb->len, str, len);
	sb->len = new_len;
	sb->buf[new_len] = '\0';
}

Str *strbuf_as_str(StrBuf *sb)
{
	return str_new_copy(sb->buf, sb->len);
}

void strbuf_dealloc(StrBuf *sb)
{
	free(sb->buf);
}

static void strbuf_grow(StrBuf *sb, const size_t min_cap)
{
	size_t new_cap = (sb->cap + 1) * 2;
	if (new_cap < min_cap) {
		new_cap = min_cap;
	}
	sb->buf = realloc(sb->buf, new_cap);
	sb->cap = new_cap;
}
