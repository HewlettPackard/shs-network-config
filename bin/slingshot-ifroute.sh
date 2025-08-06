#!/bin/bash
#
# Copyright 2021-2025 Hewlett Packard Enterprise Development LP. All rights reserved.
#
# The slingshot-ifroute script is designed to manage network routing configurations for high-speed
# interfaces, typically used in clustered or high-performance computing environments.
# It can be triggered either manually or automatically:
# By systemd service (cm-slingshot-ifroute.service)  runs the script without arguments,
# typically during system boot or reboot.
# By a dispatcher  runs the script with two arguments: the network interface name and its status
# (up or down), allowing dynamic route adjustments based on interface state changes.
#

SCRIPT_NAME=$(basename "$0")

function usage() {
    echo -e """
Usage: $SCRIPT_NAME [INTERFACE] [STATUS]

This script configures routing rules and tables for network interfaces it is typically triggered by
network events or reboot. The script expects the interface/s to be already created and configured.

Arguments:
  INTERFACE   Name of the network interface/range of interfaces (example hsn0, hsn0,hsn1 or hsn[0-1]).
              If omitted, applies to all matching interfaces.
  STATUS      Interface status (e.g., up). Required if INTERFACE is specified.

Examples:
  $SCRIPT_NAME
  $SCRIPT_NAME hsn0 up
  $SCRIPT_NAME hsn0,hsn1,hsn2 up
  $SCRIPT_NAME hsn[0-2] up
    """
    exit 1
}

function dec2ip () {
    local ip dec=$@
    for e in {3..0}
    do
        ((octet = dec / (256 ** e) ))
        ((dec -= octet * 256 ** e))
        ip+=$delim$octet
        delim=.
    done
    printf '%s\n' "$ip"
}

function ip2dec () {
    local a b c d ip=$@
    IFS=. read -r a b c d <<< "$ip"
    printf '%d\n' "$((a * 256 ** 3 + b * 256 ** 2 + c * 256 + d))"
}

function network_id {
    ip_address=$1
    netmask=$2
    let mask=2**32-1
    let mask=$((mask >> (32-${netmask})))
    let mask=$((mask << (32-${netmask})))

    val=$(ip2dec ${ip_address})
    let val=$((val & mask))
    ip=$(dec2ip ${val})

    echo $ip
}
function add_rule_if_not_present {
    rule="$@"
    rule_definition="$(echo ${rule} | sed -e 's/pref .*//g')"
    if [[ $(ip rule | grep "${rule_definition}" | wc -l) -eq 0 ]] ; then
        ip rule add ${rule}
    else
        echo rule "${rule}" is already present. skipping
    fi
}

function apply_routes_from_file {
    routing_table=$1
    device=$2
    ip=$3
    route_file=$4

    while read -r line; do
        # we cannot apply route which attempt to set a source address. raise a warning
        if [[ $(echo $line | egrep 'src ([0-9]{1,3}\.){3}[0-9]{1,3}' | wc -l) -gt 0 ]] ; then
            echo skipping route "$line" due to source address routing constraints
            continue
        fi

        if [[ -z ${line} ]] ; then
            continue
        fi
        # strip the device name out so we can rewrite the rule as needed for
        # source addressing requirements
        line=$(echo $line | sed -e 's/\n//g')
        ip route replace table ${routing_table} ${line} src ${ip}
    done < ${route_file}
}

if [ -f /etc/iproute2/rt_tables ] ; then
    RT_TABLES=/etc/iproute2/rt_tables
elif [ -f /usr/share/iproute2/rt_tables ] ; then
    RT_TABLES=/usr/share/iproute2/rt_tables
else
    echo 'iproute2 rt_tables not found.'
    exit 1
fi

NET_DIR=/sys/class/net
DEV_PREFIX=hsn
RT_PREFIX=rt_

# -------------------------------
# Parse arguments and determine mode
# -------------------------------
if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    usage
fi

# Parse arguments
ARG_INTERFACE="$1"
ARG_STATUS="$2"

# Check if INTERFACE is provided but STATUS is missing
if [[ -n "$ARG_INTERFACE" && -z "$ARG_STATUS" ]]; then
    echo "Error: STATUS argument is missing for interface '$ARG_INTERFACE'."
    usage
fi


#Expand interface input formats: hsn[1-2] or hsn0,hsn1
if [[ "$ARG_INTERFACE" =~ ^hsn\[[0-9]+-[0-9]+\]$ ]]; then
    range_part=$(echo "$ARG_INTERFACE" | sed -E 's/hsn\[([0-9]+)-([0-9]+)\]/\1 \2/')
    start=$(echo $range_part | awk '{print $1}')
    end=$(echo $range_part | awk '{print $2}')
    INTERFACES=""
    for ((i=start; i<=end; i++)); do
        INTERFACES+="hsn$i "
    done
    INTERFACES=$(echo $INTERFACES)
elif [[ "$ARG_INTERFACE" == *","* ]]; then
    IFS=',' read -ra ADDR <<< "$ARG_INTERFACE"
    INTERFACES="${ADDR[@]}"
elif [[ -n "$ARG_INTERFACE" ]]; then
    INTERFACES="$ARG_INTERFACE"
else
    echo "Running for all hsn interfaces"
    INTERFACES=$(ls ${NET_DIR} | grep ${DEV_PREFIX})
fi


# create routing tables for the devices
_index=0
for device in ${INTERFACES} ; do
    label=${RT_PREFIX}${device}
    found=$(grep "$label" ${RT_TABLES} | wc -l)
    unit=${device#${DEV_PREFIX}}
    let index=200+${unit}

    if [[ ${found} -eq 1 ]] ; then
        echo "${label} already exists: $(grep "$label" ${RT_TABLES})"
    else
        if [[ ${found} -eq 0 ]] ; then
            echo "adding entry for ${label} in ${RT_TABLES}"
            echo "${index} ${label}" >> ${RT_TABLES}
        else
            echo "Error: Multiple entries found for ${label} in ${RT_TABLES}"
            exit 1
        fi
    fi
done

# check to see if the local table is already a lower priority
expected_local_priority=10
actual_local_priority=$(ip rule | grep "from all lookup local" | awk -F: '{print $1}')
if [[ ${actual_local_priority} -lt ${expected_local_priority} ]] ; then
    # move local table to a slightly lower priority
    ip rule add lookup local pref ${expected_local_priority}
    # delete the lowest priority local rule
    ip rule del lookup local
fi

local_loopback_priority=0
outbound_loc_device_priority=1
outbound_rem_device_priority=2

# ALL_HSNS - all HSN interfaces created in the system
ALL_HSNS=$(ls ${NET_DIR} | grep ${DEV_PREFIX})

#Iterate through interfaces passed by the script (Which could be a sub set of all HSNs interfaces)
for device in ${INTERFACES} ; do
    label=${RT_PREFIX}${device}
    device_cidr=$(ip a show $device | grep "inet " | awk '{print $2}' | head -n1)
    device_ip=$(echo $device_cidr | awk -F/ '{print $1}')
    device_netmask=$(echo $device_cidr | awk -F/ '{print $2}')
    if [[ -z ${device_ip} || -z ${device_netmask} ]]; then
        echo "Error: Unable to determine IP or Mask for ${device}"
        continue
    fi
    device_network=$(network_id $device_ip $device_netmask)
    # for each target hsn interface on the local host
    for target in ${ALL_HSNS} ; do
        target_cidr=$(ip a show $device | grep "inet " | awk '{print $2}' | head -n1)
        target_ip=$(echo $target_cidr | awk -F/ '{print $1}')
        target_netmask=$(echo $target_cidr | awk -F/ '{print $2}')
        if [[ -z ${target_ip} || -z ${target_netmask} ]]; then
            echo "Error: Unable to determine IP or Mask for device ${target}"
            continue
        fi
        target_network=$(network_id $target_ip $target_netmask)
        rule="from $device_ip iif $target lookup local pref ${outbound_loc_device_priority}"
        if [[ "$target" == "$device" ]] ; then
            rule="from $device_ip to $device_ip lookup local pref ${local_loopback_priority}"
        fi

        add_rule_if_not_present "${rule}"
    done

    # add local routing policy for outbound devices
    add_rule_if_not_present "from $device_ip lookup ${label} pref ${outbound_rem_device_priority}"

    # add local routing rules specific to the device
    ip route replace table ${label} ${device_network}/${device_netmask} dev ${device} proto kernel scope host src ${device_ip}
done

# set sysctl values
for device in ${INTERFACES} ; do
    sysctl -w net.ipv4.conf.${device}.accept_local=1
    sysctl -w net.ipv4.conf.${device}.arp_accept=1
    sysctl -w net.ipv4.conf.${device}.arp_ignore=1
    sysctl -w net.ipv4.conf.${device}.arp_filter=1
    sysctl -w net.ipv4.conf.${device}.arp_announce=2
    sysctl -w net.ipv4.conf.${device}.rp_filter=0
done

# flush the ARP cache
ip neigh flush all

# flush the routing cache
ip route flush cache
