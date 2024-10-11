#! /bin/sh
#
# Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
#

if test ! -d .git && test ! -f src/slingshot-network-cfg-lldp.c; then
    echo You really need to run this script in the top-level directory
    exit 1
fi

set -x

autoreconf -ivf
