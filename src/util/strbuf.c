#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "str.h"
#include "util.h"
#include "strbuf.h"

#define STRBUF_DEFAULT_INIT_CAPACITY 16

static void strbuf_grow(RhoStrBuf *sb, const size_t min_cap);

void rho_strbuf_init(RhoStrBuf *sb, const size_t cap)
{
	assert(cap > 0);
	sb->buf = rho_malloc(cap);
	sb->len = 0;
	sb->cap = cap;
	sb->buf[0] = '\0';
}

void rho_strbuf_init_default(RhoStrBuf *sb)
{
	rho_strbuf_init(sb, STRBUF_DEFAULT_INIT_CAPACITY);
}

void rho_strbuf_append(RhoStrBuf *sb, const char *str, const size_t len)
{
	size_t new_len = sb->len + len;
	if (new_len + 1 > sb->cap) {
		strbuf_grow(sb, new_len + 1);
	}
	memcpy(sb->buf + sb->len, str, len);
	sb->len = new_len;
	sb->buf[new_len] = '\0';
}

void rho_strbuf_to_str(RhoStrBuf *sb, RhoStr *dest)
{
	*dest = RHO_STR_INIT(sb->buf, sb->len, 0);
}

void rho_strbuf_trim(RhoStrBuf *sb)
{
	const size_t new_cap = sb->len + 1;
	sb->buf = rho_realloc(sb->buf, new_cap);
	sb->cap = new_cap;
}

void rho_strbuf_dealloc(RhoStrBuf *sb)
{
	free(sb->buf);
}

static void strbuf_grow(RhoStrBuf *sb, const size_t min_cap)
{
	size_t new_cap = (sb->cap + 1) * 2;
	if (new_cap < min_cap) {
		new_cap = min_cap;
	}

	sb->buf = rho_realloc(sb->buf, new_cap);
	sb->cap = new_cap;
}
