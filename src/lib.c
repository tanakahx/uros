#include "uros.h"

void *memset(void *b, int c, size_t len)
{
	char *p = (char *)b;

	while (len--)
		*p++ = c;
	return b;
}

void *memcpy(void *dst, const void *src, size_t n)
{
	char *p1 = (char *)dst;
	const char *p2 = (const char *)src;

	while (n--)
		*p1++ = *p2++;
	return dst;
}
