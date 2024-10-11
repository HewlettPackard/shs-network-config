#!/bin/bash
#
# Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
#

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
        #   source addressing requirements
        line=$(echo $line | sed -e 's/\n//g')
        ip route replace table ${routing_table} ${line} src ${ip}
    done < ${route_file}
}

RT_TABLES=/etc/iproute2/rt_tables
NET_DIR=/sys/class/net
DEV_PREFIX=hsn
RT_PREFIX=rt_

INTERFACES=$(ls ${NET_DIR} | grep ${DEV_PREFIX})

# create routing tables for the devices
_index=0
for device in ${INTERFACES} ; do
    label=${RT_PREFIX}${device}
    found=$(grep "$label" ${RT_TABLES} | wc -l)
    unit=${device#${DEV_PREFIX}}
    let index=200+${unit}

    # TODO: either a case statement or if...elif...else where error should indicate bad rt tables file with more than one entry for the label. you may want to check that index is also not already used.

    if [[ ${found} -eq 1 ]] ; then
        echo "${label} already exists: $(grep "$label" ${RT_TABLES})"
    else
        if [[ ${found} -eq 0 ]] ; then
            echo "adding entry for ${label} in ${RT_TABLES}"
            echo "${index} ${label}" >> ${RT_TABLES}
        else
            error
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

# for each local hsn interface on the local host
for device in ${INTERFACES} ; do
    label=${RT_PREFIX}${device}
    device_cidr=$(ip a show $device | grep "inet " | awk '{print $2}' | head -n1)
    device_ip=$(echo $device_cidr | awk -F/ '{print $1}')
    device_netmask=$(echo $device_cidr | awk -F/ '{print $2}')
    device_network=$(network_id $device_ip $device_netmask)

    # for each target hsn interface on the local host
    for target in ${INTERFACES} ; do
        target_cidr=$(ip a show $device | grep "inet " | awk '{print $2}' | head -n1)
        target_ip=$(echo $target_cidr | awk -F/ '{print $1}')
        target_netmask=$(echo $target_cidr | awk -F/ '{print $2}')
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
