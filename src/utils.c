/*
 * Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
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

#include <stdlib.h>
#include "utils.h"
#include "debug.h"

int hex_to_ascii(const char *cp)
{
    char buf[3] = { cp[0], cp[1], '\0' };

    return strtol(buf, NULL, 16);
}

bool run(const char *cmd, bool dry_run)
{
    VERBOSE("Command to execute: %s", cmd);
    if (dry_run)
        return true;

    if (system(cmd)) {
        ERROR("'%s' exited with error status", cmd);
        return false;
    }

    return true;
}

bool runv(const char **cmdv, bool dry_run)
{
    int i;

    for (i = 0; cmdv[i]; i++) {
        if (!run(cmdv[i], dry_run)) {
            return false;
        }
    }

    return true;
}

size_t  strlcpy(char *d, const char *s, size_t len)
{
    len--;
    strncpy(d, s, len);
    d[len] = '\0';

    return (size_t) d;
}
