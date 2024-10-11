/*
 * Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
 *
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "debug.h"

char *log_level_labels[DEBUG_LVL_MAX] = {
	[DEBUG_LVL_DEBUG] = "DEBUG",
	[DEBUG_LVL_VERBOSE] = "VERBOSE",
	[DEBUG_LVL_WARN] = "WARN",
	[DEBUG_LVL_ERROR] = "ERROR",
	[DEBUG_LVL_DIAG] = "DIAG",
	[DEBUG_LVL_CRITICAL] = "CRITICAL",
	[DEBUG_LVL_FATAL] = "FATAL",
};

int debug_level = DEBUG_LVL_ERROR;

void debug_fprintf(const char *file, const char *func, const int line, FILE* fp, char *fmt, ...) {
	char buf[1024];
	char *buf_ptr;
	int bc = 0;
	va_list ap;
	time_t t;
	struct tm *tmp;


	t = time(NULL);
	tmp = localtime(&t);
	buf_ptr = buf + strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S %Z ", tmp);

	va_start(ap, fmt);
	buf_ptr += vsprintf(buf_ptr, fmt, ap);
	va_end(ap);

	bc = sprintf(buf_ptr, " - [%s:%s():%d]", file, func, line);
	if (bc < 0) {
	    fprintf(stderr, "could not print debug to stderr, ret=%d\n", bc);
	}
	fprintf(fp, "%s\n", buf);

	fflush(fp);
}
