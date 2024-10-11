#!/bin/bash
#
# Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
#

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <netif>" 1>&2
    exit 1
fi

MATCH=$1
IFNAME=$MATCH
PATTERN="????:??:??.?"
HSN_PREFIX=hsn
NETWORK_CLASS=0x020000
VENDOR_MELLANOX=0x15b3
DEVICE_CONNECTX_5=0x1017

cd /sys/devices

let hsn_index=-1

# get an orderd list of the PCI network devices
for PCIDEV in $(ls -1d pci*/$PATTERN pci*/$PATTERN/$PATTERN 2>/dev/null); do
    [[ -r $PCIDEV/class ]] || continue
    [[ -r $PCIDEV/vendor ]] || continue
    [[ -r $PCIDEV/device ]] || continue
    [[ -r $PCIDEV/net ]] || continue
    CLASS=$(cat $PCIDEV/class)
    [[ $CLASS == $NETWORK_CLASS ]] || continue
    VENDOR=$(cat $PCIDEV/vendor)
    DEVICE=$(cat $PCIDEV/device)

    # change the name if this is a high speed connection
    case $VENDOR:$DEVICE in
        $VENDOR_MELLANOX:$DEVICE_CONNECTX_5)
            PREFIX=${HSN_PREFIX}
            NETDEV=$(ls -1d $PCIDEV/net/* $PCIDEV/*/net/* 2>/dev/null)
            [[ -e $NETDEV ]] || NETDEV=""
            NETIF=${NETDEV##*/}
            let hsn_index=${hsn_index}+1
            if [[ $NETIF == $MATCH ]]; then
                IFNAME=$PREFIX${hsn_index}
                break
            fi
            ;;
    esac
done

echo $IFNAME
exit 0
