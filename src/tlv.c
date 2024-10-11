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
#include <string.h>

#include "tlv.h"
#include "utils.h"

#define ORG_TLV_HEADER "\tOUI: 0x000eab, Subtype: 1, Info: "

void parse_mac_addr(const char *buf, char *mac_addr)
{
    /* Get our MAC address from the Chassis ID or Port ID TLV */
    /* It is in both of them */
    const char *payload = buf + 6;

    strlcpy(mac_addr, payload, MAC_ADDR_SIZE);

    /* Mask off the second octet of the parsed address */
    mac_addr[3] = '0';
    mac_addr[4] = '0';
}

void parse_org_tlv(const char *buf, char *org_tlv)
{
    const char *payload = buf + strlen(ORG_TLV_HEADER);
    int length = strlen(payload);
    int i, j;

    /* N.B.,
        OUI is unchanging for us (it is the mfg id for Cray)
        We may increment subtype if ever we modify the contents
        of the tlv payload.
        Info denotes that the tlv payload follows */
    for (i = 0, j = 0; i < length; i += 2, j++) {
        org_tlv[j] = hex_to_ascii(payload + i);
    }
    org_tlv[j] = '\0';
}
