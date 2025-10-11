#include <stdio.h>
#include <stdarg.h>

void nd500_log(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}


