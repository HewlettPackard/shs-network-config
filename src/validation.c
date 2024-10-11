/*
 * Copyright 2020-2021 Hewlett Packard Enterprise Development LP. All rights reserved.
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

// system includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <stdbool.h>

// local includes
#include "validation.h"

// external library includes
#include "cJSON.h"

#define BUFSIZE       1000
#define MAC_ADDR_SIZE 24
#define IP_ADDR_SIZE  32
#define MTU_SIZE      16
#define TTL_SIZE      16

#define BASE_DEC 10
#define BASE_HEX 16

#define IP_PREFIX_MIN 0
#define IP_PREFIX_MAX 32

#define OCTET_MIN 0
#define OCTET_MAX 255

bool valid_number(const char *str)
{
    char *ep = NULL;
    long val = strtol(str, &ep, BASE_DEC);

    if (val < 0 ||
            ep == str ||
            *ep) {
        return false;
    }

    return true;
}

bool valid_octet(const char **str, int base, char term)
{
    char *ep = NULL;
    long val = strtol(*str, &ep, base);

    if (val < OCTET_MIN ||
            val > OCTET_MAX ||
            ep == *str ||
            *ep != term) {
        return false;
    }

    // why do we do this?
    *str = ep + 1;

    return true;
}

bool valid_mac_addr(const char *mac_addr)
{
    const char term[6] = { ':', ':', ':', ':', ':', '\0' };
    const char *str = mac_addr;
    int i;

    for (i = 0; i < 6; i++) {
        if (!valid_octet(&str, BASE_HEX, term[i])) {
            return false;
        }
    }

    return true;
}

bool valid_ip_addr(const char *ip_addr)
{
    /* Return whether or not ip_addr is a valid dotted quad IPv4 Address. */
    const char term[4] = { '.', '.', '.', '/' };
    const char *str = ip_addr;
    int i;
    char *ep = NULL;
    long prefix;

    for (i = 0; i < 4; i++) {
        if (!valid_octet(&str, BASE_DEC, term[i])) {
            return false;
        }
    }

    prefix = strtol(str, &ep, BASE_DEC);
    if (prefix < IP_PREFIX_MIN ||
            prefix > IP_PREFIX_MAX ||
            ep == str ||
            *ep) {
        return false;
    }

    return true;
}

bool valid_mtu(const char *mtu)
{
    return valid_number(mtu);
}

bool valid_ttl(const char *ttl)
{
    if (!strcmp(ttl, "forever")) {
        return true;
    }

    return valid_number(ttl);
}

void chomp(char *buf)
{
    char *cp = strrchr(buf, '\n');

    if (cp) {
        *cp = '\0';
    }
}
