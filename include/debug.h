/*
 * Copyright 2021 Hewlett Packard Enterprise Development LP
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

#ifndef INCLUDE_DEBUG_H_
#define INCLUDE_DEBUG_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum {
	DEBUG_LVL_DEBUG,
	DEBUG_LVL_VERBOSE,
	DEBUG_LVL_WARN,
	DEBUG_LVL_ERROR,
    DEBUG_LVL_DIAG,
	DEBUG_LVL_CRITICAL,
	DEBUG_LVL_FATAL,
	DEBUG_LVL_MAX,
};

extern int debug_level;
extern char *log_level_labels[DEBUG_LVL_MAX];

void debug_fprintf(const char *file, const char *func, const int line, FILE* fp, char *fmt, ...);

static inline char *get_filename(char *fname)
{
	char *v = strrchr(fname, '/');
	return (v ? v + 1 : fname);
}

#define WRITE_TO_LOG(fp, dbg_lvl, fmt, args...) \
	do { \
		debug_fprintf(get_filename(__FILE__), __func__, __LINE__, fp, \
                "[%s] - " fmt "", \
				log_level_labels[dbg_lvl], ##args); \
		fflush(fp); \
	} while (0)

#define COND_WRITE_TO_LOG(fp,dbg_lvl, fmt, args...) \
	do { \
		if (debug_level <= dbg_lvl) \
			WRITE_TO_LOG(fp, dbg_lvl, fmt, ##args); \
	} while (0)

#define DEBUG(fmt, args...) \
	COND_WRITE_TO_LOG(stderr, DEBUG_LVL_DEBUG, fmt, ##args)
#define VERBOSE(fmt, args...) \
	COND_WRITE_TO_LOG(stderr, DEBUG_LVL_VERBOSE, fmt, ##args)
#define WARN(fmt, args...) \
	COND_WRITE_TO_LOG(stderr, DEBUG_LVL_WARN, fmt, ##args)
#define ERROR(fmt, args...) \
	COND_WRITE_TO_LOG(stderr, DEBUG_LVL_ERROR, fmt, ##args)
#define DIAG(fmt, args...) \
	COND_WRITE_TO_LOG(stderr, DEBUG_LVL_DIAG, fmt, ##args)
#define CRITICAL(fmt, args...) \
	COND_WRITE_TO_LOG(stderr, DEBUG_LVL_FATAL, fmt, ##args)

#define FATAL(fmt, args...) \
    do { \
	    WRITE_TO_LOG(stderr, DEBUG_LVL_FATAL, fmt, ##args); \
        exit(EXIT_FAILURE); \
    } while (0)

#endif /* INCLUDE_DEBUG_H_ */


