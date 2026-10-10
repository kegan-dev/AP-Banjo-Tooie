/* Minimal libc routines so the soft-float bundle does not pull hard-float newlib objects. */
#include <stddef.h>
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)); return r; }
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *dd = d; const unsigned char *ss = s; while (n--) *dd++ = *ss++; return d; }
