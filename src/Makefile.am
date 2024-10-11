AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS = -I../external/cJSON -I../include
AM_CFLAGS = -Wall -Werror

bin_PROGRAMS = slingshot-network-cfg-lldp

slingshot_network_cfg_lldp_SOURCES = slingshot-network-cfg-lldp.c \
    debug.c \
    tlv.c \
    utils.c \
    validation.c \
    ../external/cJSON/cJSON.c
 
