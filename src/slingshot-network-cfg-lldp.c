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

/* system includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <stdbool.h>

/* local includes */
#include "debug.h"
#include "validation.h"
#include "utils.h"
#include "tlv.h"

/* external library includes */
#include "cJSON.h"

/* global definitions */
#define BUFSIZE       1000
#define MAC_ADDR_SIZE 24
#define IP_ADDR_SIZE  32
#define MTU_SIZE      16
#define TTL_SIZE      16

#define DEVICE_NOT_UP "Device not found or inactive"

/* macros */
#define ALLOC_CMD_BUFFER(cmds, idx, err_label, fmt, args...) \
    do { \
        cmds[idx] = calloc(BUFSIZE, sizeof(char)); \
        if (!cmds[idx]) { \
            goto err_label; \
        } \
        snprintf(cmds[idx], BUFSIZE * sizeof(char), \
            fmt, ##args); \
        idx++; \
    } while (0)

/* structs */
struct program_options {
    bool create_ifcfg;
    bool dry_run;
    bool remove_ip_addrs;
    char *input_file;
    bool skip_reload;
};

typedef struct fabric_config {
    char *ifname;
    char mac_addr[MAC_ADDR_SIZE];
    char ip_addr[IP_ADDR_SIZE];
    char mtu[MTU_SIZE];
    char ttl[TTL_SIZE];
} fabric_config_t;

/* forward declarations */
bool reload_interface(fabric_config_t *fc);

/* global variables */
struct program_options options = {
    .create_ifcfg = false,
    .dry_run = false,
    .remove_ip_addrs = false,
    .skip_reload = false,
};

char *get_lldp_tlv(fabric_config_t *fc) {
    char buf[BUFSIZE];
    char *org_tlv;
    FILE *fp;

    org_tlv = calloc(BUFSIZE, sizeof(*org_tlv));
    if (!org_tlv) {
        ERROR("failed to allocate memory for TLV data");
        goto error;
    }

    snprintf(buf, sizeof(buf), "lldptool get-tlv -i %s -n", fc->ifname);

    if (!options.input_file) {
    fp = popen(buf, "r");
    if (!fp) {
        ERROR("opening pipe to lldptool failed!");
        goto free_tlv;
    }
    } else {
        fp = fopen(options.input_file, "r");
    }

    while (fgets(buf, sizeof(buf), fp)) {
        /* Process output of lldptool (now in buf) */
        chomp(buf);

        DEBUG("%s", buf);

        /* read MAC data from non-org TLV and store it */
        if (!fc->mac_addr[0] && !strncmp(buf, "\tMAC: ", 6)) {
            parse_mac_addr(buf, fc->mac_addr);
        }

        /* read org TLV */
        if (!org_tlv[0] && !strncmp(buf, "\tOUI: ", 6)) {
            parse_org_tlv(buf, org_tlv);
        }

        if (!strncmp(buf, DEVICE_NOT_UP, strlen(DEVICE_NOT_UP))) {
            ERROR("device not found or inactive according to lldp");
            DIAG("check local adapter state. The adapter may be down");
            DIAG("carrier signal may also be absent.");
            pclose(fp);
            goto free_tlv;
        }
    } /* end while */

    if (pclose(fp)) {
        ERROR("lldptool exited with error status");
        goto free_tlv;
    }

    if (!strlen(fc->mac_addr)) {
        ERROR("lldpad not receiving any data from switch");
        DIAG("check local LLDPAD configuration for administrative status");
        DIAG("check Rosetta switch to see if it advertising TLVs on other adapters");
        goto free_tlv;
    }

    if (!org_tlv[0]) {
        ERROR("Missing Org TLV in lldptool output");
        DIAG("check Rosetta switch configuration for LLDP. CrayTLV not advertised.");
        DIAG("Fabric configuration is not active or not advertised");
        goto free_tlv;
    }

    return org_tlv;

free_tlv:
    free(org_tlv);
error:
    return NULL;
}

bool is_valid_tlv_data(fabric_config_t *fc) {
    VERBOSE("Parsed:");
    VERBOSE("ifname:   %s", fc->ifname);
    VERBOSE("mac_addr: %s", fc->mac_addr);
    VERBOSE("ip_addr:  %s", fc->ip_addr);
    VERBOSE("MTU:      %s", fc->mtu);
    VERBOSE("TTL:      %s", fc->ttl);

    /* Ensure that the items were parsed off as expected. */
    if (!valid_mac_addr(fc->mac_addr)) {
        ERROR("Invalid MAC addr: '%s'", fc->mac_addr);
        DIAG("MAC address is malformed. Check LLDP output");
        return false;
    }

    if (!valid_ip_addr(fc->ip_addr)) {
        ERROR("Invalid IP addr: '%s'", fc->ip_addr);
        DIAG("CrayTLV is malformed. Expected a valid IP address");
        DIAG("Check Rosetta LLDP configuration");
        return false;
    }

    if (!valid_mtu(fc->mtu)) {
        ERROR("Invalid MTU: '%s'", fc->mtu);
        DIAG("CrayTLV is malformed. Expected a valid MTU");
        DIAG("Check Rosetta LLDP configuration");
        return false;
    }

    if (!valid_ttl(fc->ttl)) {
        ERROR("Invalid TTL: '%s'", fc->ttl);
        DIAG("CrayTLV is malformed. Expected a valid TTL");
        DIAG("Check Rosetta LLDP configuration");
        return false;
    }

    return true;
}

bool parse_tlv(fabric_config_t *fc)
{
    char *org_tlv;

    fc->mac_addr[0] = '\0';
    fc->ip_addr[0] = '\0';
    fc->mtu[0] = '\0';
    fc->ttl[0] = '\0';

    VERBOSE("Begin parse_tlv");

    /* fetch TLV from LLDP */
    org_tlv = get_lldp_tlv(fc);
    if (!org_tlv) {
        CRITICAL("failed to get response from LLDP, or "
                    "could not find CrayTLV");
        return false;
    }

    /* parse JSON payload from Org TLV */
    DEBUG("Org TLV json: '%s'", org_tlv);
    cJSON *org_json = cJSON_Parse(org_tlv);

    /* free TLV string now that we are done with it */
    free(org_tlv);

    /* read json values from JSON object */
    const cJSON *ip_addr_ptr = cJSON_GetObjectItemCaseSensitive(org_json, "ip_addr");
    if (cJSON_IsString(ip_addr_ptr)) {
        strlcpy(fc->ip_addr, ip_addr_ptr->valuestring, IP_ADDR_SIZE);
    }

    const cJSON *mtu_ptr = cJSON_GetObjectItemCaseSensitive(org_json, "mtu");
    if (cJSON_IsNumber(mtu_ptr)) {
        snprintf(fc->mtu, MTU_SIZE, "%d", mtu_ptr->valueint);
    }

    const cJSON *ttl_ptr = cJSON_GetObjectItemCaseSensitive(org_json, "ttl");
    if (cJSON_IsNumber(ttl_ptr)) {
        snprintf(fc->ttl, TTL_SIZE, "%d", ttl_ptr->valueint);
    } else if (cJSON_IsString(ttl_ptr)) {
        strlcpy(fc->ttl, ttl_ptr->valuestring, TTL_SIZE);
    }

    cJSON_Delete(org_json);

    return is_valid_tlv_data(fc);
}

bool write_ifcfg(fabric_config_t *fc)
{
    char file[BUFSIZE];
    FILE *fp;
    bool ret = true;

    if (!fc) {
        FATAL("null fc passed to function");
    }

    snprintf(file, sizeof(file), "/etc/sysconfig/network/ifcfg-%s", fc->ifname);

    if (options.dry_run) {
        fp = stdout;
        VERBOSE("open '%s' for writing", file);
    } else {
        fp = fopen(file, "w");
        if (!fp) {
            ERROR("Unable to create ifcfg file %s: %s", file, strerror(errno));
            return false;
        }
    }

    fprintf(fp, "NAME=%s\n", fc->ifname);
    fprintf(fp, "STARTMODE=auto\n");
    fprintf(fp, "BOOTPROTO=static\n");
    fprintf(fp, "LLADDR=%s\n", fc->mac_addr);
    fprintf(fp, "IPADDR=%s\n", fc->ip_addr);
    fprintf(fp, "MTU=%s\n", fc->mtu);

    /* What to do about ttl? */
    fprintf(fp, "POST_UP_SCRIPT=wicked:/etc/sysconfig/network/if-up.d\n");

    if (options.dry_run) {
        VERBOSE("close");
    } else {
        fclose(fp);
    }

    if (!options.skip_reload) {
        ret = reload_interface(fc);
    }

    return ret;
}

bool reload_interface(fabric_config_t *fc)
{
    char *commands[3] = {};
    int index = 0;
    bool result = false;

    ALLOC_CMD_BUFFER(commands, index,
            free_commands, "wicked ifdown %s", fc->ifname);
    ALLOC_CMD_BUFFER(commands, index,
            free_commands, "wicked ifup %s", fc->ifname);
    commands[index] = NULL;

    result = runv((const char **) commands, options.dry_run);

    if (!result) {
        ERROR("a command in the queue failed");
    }

free_commands:
    for (int i = 0; i < 3; i++) {
        if (commands[i])
            free(commands[i]);
    }

    return result;
}

bool do_ip_cmds(fabric_config_t *fc)
{
    char *commands[7] = {};
    int index = 0;
    bool result = false;

    if (options.remove_ip_addrs) {
        ALLOC_CMD_BUFFER(commands, index, free_commands,
                            "ip addr flush dev %s", fc->ifname);
    }

    if (!options.skip_reload) {
        ALLOC_CMD_BUFFER(commands, index, free_commands,
                            "ip link set dev %s down", fc->ifname);
    }

    ALLOC_CMD_BUFFER(commands, index, free_commands,
                        "ip link set dev %s addr %s", fc->ifname, fc->mac_addr);

    if (!options.skip_reload) {
        ALLOC_CMD_BUFFER(commands, index, free_commands,
                            "ip link set dev %s up", fc->ifname);
    }

    ALLOC_CMD_BUFFER(commands, index, free_commands,
                        "ip addr add %s dev %s valid_lft %s preferred_lft %s",
                        fc->ip_addr, fc->ifname, fc->ttl, fc->mtu);
    ALLOC_CMD_BUFFER(commands, index, free_commands,
                        "ip link set dev %s mtu %s", fc->ifname, fc->mtu);

    commands[index] = NULL;

    result = runv((const char **) commands, options.dry_run);

    if (!result) {
        ERROR("a command in the queue failed");
    }

free_commands:
    for (int i = 0; i < 7; i++) {
        if (commands[i])
            free(commands[i]);
    }

    return result;
}

/* usage */
void usage_brief(const char *prog, FILE *fp)
{
    fprintf(fp, "Usage: %s [-h|--help] [-c|--create-ifcfg] [-d|--debug] "
            "\n\t\t[-n|--dry-run] [-r|--remove-ip-addrs] [-v|--verbose] "
            "\n\t\t<interface>\n", prog);
}

void usage_full(const char *prog, FILE *fp)
{
    usage_brief(prog, fp);

    fprintf(fp, "\n");

    fprintf(fp, "\t-h|--help             show this helpful text\n");
    fprintf(fp, "\t-c|--create-ifcfg     create corresponding ifcfg file\n");
    fprintf(fp, "\t-d|--debug            enable debug output\n");
    fprintf(fp, "\t-n|--dry-run          show the commands to be run but do not run them\n");
    fprintf(fp, "\t-r|--remove-ip-addrs  remove any existing ip addresses\n");
    fprintf(fp, "\t-s|--skip-reload      do not cycle(link up, then link down) the interface to apply configuration\n");
    fprintf(fp, "\t-v|--verbose          enable verbose output\n");
    fprintf(fp, "\t<interface>           the name of the interface to configure\n");
}

/* driver */
int main(int argc, char *argv[])
{
    int opt;
    bool ret;
    fabric_config_t *fc;

    fc = calloc(1, sizeof(*fc));
    if (!fc) {
        FATAL("could not allocate a fabric config object");
    }

    while (1) {
        const struct option long_options[] = {
            {"help",            no_argument, NULL, 'h'},
            {"create-ifcfg",    no_argument, NULL, 'c'},
            {"debug",           no_argument, NULL, 'd'},
            {"dry-run",         no_argument, NULL, 'n'},
            {"input-file",      required_argument, NULL, 'f'},
            {"remove-ip-addrs", no_argument, NULL, 'r'},
            {"skip-reload",     no_argument, NULL, 's'},
            {"verbose",         no_argument, NULL, 'v'},
            { }
        };

        opt = getopt_long(argc, argv, "cdf:hnrv", long_options, NULL);
        if (opt == -1) {
            break;
        }

        switch(opt) {
            case 'h':
                usage_full(argv[0], stdout);
                return EXIT_SUCCESS;
            case 'c':
                options.create_ifcfg = true;
                break;
            case 'd':
                if (debug_level > DEBUG_LVL_DEBUG)
                    debug_level = DEBUG_LVL_DEBUG;
                break;
            case 'f':
                options.input_file = strdup(optarg);
                break;
            case 'n':
                options.dry_run = true;
                break;
            case 'r':
                options.remove_ip_addrs = true;
                break;
            case 's':
                options.skip_reload = true;
                break;
            case 'v':
                if (debug_level > DEBUG_LVL_VERBOSE)
                    debug_level = DEBUG_LVL_VERBOSE;
                break;
            case '?':
                usage_brief(argv[0], stderr);
                return EXIT_FAILURE;
            default:
                abort();
        }
    }

    if (argc - optind != 1) {
        usage_brief(argv[0], stderr);
        return EXIT_FAILURE;
    }

    DEBUG("options.input_file: %s", options.input_file ? options.input_file : "");

    fc->ifname = argv[optind];

    if (!parse_tlv(fc)) {
        FATAL("failed to parse TLV provided by LLDP");
    }

    if (options.create_ifcfg) {
        ret = write_ifcfg(fc);
    } else {
        ret = do_ip_cmds(fc);
    }

    free(fc);

    return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
