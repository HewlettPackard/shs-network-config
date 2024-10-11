#!/bin/bash
#
# Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
#
# This program executes lldptool to get the interface configuration
# from the TLVs being broadcast by the switch upstream of its HSN ports.
# HSN Interface configuration includes the algorithmically assigned MAC
# address (AMA), the IP Address, the TTL of that IP address, and the MTU
# of the interface.
# The AMA is parsed from the Chassis ID TLV.
# The IP, TTL, and MTU are parsed from an org specific TLV found
# in the Cray implementation of LLDP.

export PATH=/usr/sbin:/sbin:${PATH}

if [[ ${EUID} -ne 0 ]] ; then
    echo please run this script as root. Exiting.
    exit -1
fi

function retry_function() {
        local count=${1}
        local rc=0
        shift
        command="$@"

        while [[ $count -gt 0 ]] ; do
                out=$($command)
                rc=$?

                let count--
                if [[ $rc -eq 0 ]] ; then
                        break
                else
                        warn "command failed: \"$command\""
                        warn "output: $out"
                fi
                sleep 1
                warn "retrying command, \"$command\""
                warn "$count attempts left..."
        done

        return $rc
}

LLDP_ARGS=""
TARGET_DIR=/tmp
IN_DRACUT=false
HELP=false

if [[ -f /lib/dracut-lib.sh ]]; then
    # Add dracut-lib.sh for 'info' support
    type getarg >/dev/null 2>&1 || . /lib/dracut-lib.sh
    IN_DRACUT=true
else
    function write_to_log() {
        echo $@
    }

    function info() {
        write_to_log "INFO: $@"
    }
    function warn () {
        write_to_log "WARN: $@" 1>&2
    }

    TARGET_DIR=/var/log
fi

function config_device() {
    for DEVICE in /sys/class/net/lan* /sys/class/net/hsn*; do
        IFNAME=${DEVICE##*/}

        # Set the interface up so that that it gets an LLDP agent from lldpad.
        ip link set dev $IFNAME up

        LLDP_PID=$(lldptool -p)
        ret=$?
        if [[ $ret -ne 0 ]] ; then
            info "lldptool -p returned non-zero value: $ret"
            info "lldpad is likely dead"
        else
            # configure device to send/receive adminStatus
            retry_function 15 lldptool set-lldp -i $IFNAME adminStatus=rxtx
            if [[ $? -ne 0 ]]; then
                warn "Unable to set LLDP adminStatus for interface $IFNAME"
                EXIT=1
                continue
            fi
            info "lldpad: adminStatus TLV enabled for rx/tx for $DEVICE"
        fi

        if ${IN_DRACUT} ; then
            # Flush any stale addresses
            $first && ip addr flush dev $IFNAME

            read NAME STATE ADDR <<< $(ip -br -f inet addr show dev $IFNAME 2>/dev/null)
            if [[ -n $ADDR ]]; then
                info "Interface $IFNAME is already configured"
                continue
            fi
        fi

        # Check that TLV is present; print problematic PCI device if not
        if [[ -z $(lldptool -tni $IFNAME) ]] ; then
            PCIDEVICE=$(ethtool -i $IFNAME | grep bus | awk '{print $2}')
            warn "Unable to pull TLV for interface $IFNAME. Failed to configure PCI device $PCIDEVICE"
            EXIT=1
            continue
        fi

        LIST+=" $IFNAME"
    done
}

function usage() {
    echo -e """\
Usage: $(basename $0) [opts]

Slingshot Network LLDP Configuration tool
This program executes lldptool to get the interface configuration from the TLVs being broadcast by the switch upstream of its HSN ports.

Options:
    -c | --create-ifcfg create corresponding ifcfg file
    -d | --debug        enable debug output
    -n | --dry-run      show the commands to be run but do not run them
    -s | --skip-reload  do not cycle(link up, then link down) the interface to apply configuration
    -h | --help         print help
${ADDED_USAGE}
"""
}

# define arguments
function main() {
    SHORT_OPTS="+cdnsh"
    LONG_OPTS="create-ifcfg,debug,dry-run,skip-reload,help"
    OPTS=`getopt -o ${SHORT_OPTS} --long ${LONG_OPTS} -n 'parse-options' -- "$@"`

    if [ $? != 0 ] ; then echo "Failed parsing options." >&2 ; exit 1 ; fi

    eval set -- "$OPTS"
    while true; do
        case "$1" in
            -h | --help )
                HELP=true
                ;;
            -c | --create-ifcfg )
                LLDP_ARGS="${LLDP_ARGS} --create-ifcfg"
                ;;
            -d | --debug )
                LLDP_ARGS="${LLDP_ARGS} --debug"
                ;;
            -n | --dry-run )
                LLDP_ARGS="${LLDP_ARGS} --dry-run"
                ;;
            -s | --skip-reload )
                LLDP_ARGS="${LLDP_ARGS} --skip-reload"
                ;;
            -- )
                break
                ;;
            * )
                echo ERROR: $1
                break
                ;;
        esac
        shift
    done

    if $HELP ; then
        usage
        exit 0
    fi

    echo "Start run_slingshot_network_cfg_lldp"
    shopt -s nullglob

    EXIT=0
    LIST=""

    CFG_LLDP_TIMER_STATE=/tmp/_cfg_lldp_timer_state
    if [[ -r $CFG_LLDP_TIMER_STATE ]]; then
        let _timer=$(cat $CFG_LLDP_TIMER_STATE)
        first=false
    else
        let _timer=35
        first=true
    fi

    config_device

    if $first && [[ -n $LIST ]] ; then
        # SIGHUP cannot be sent to lldpad in dracut. If SIGHUP is sent to lldpad in dracut, it will
        #   terminate the process, rather than refresh the interface list. 
        if ! ${IN_DRACUT} ; then
            # Prod lldpad to pick up interfaces we just set up
            kill -s HUP $(lldptool ping)
        fi
        # Give the switches time to send LLDP info
        sleep $_timer
    fi

    for IFNAME in $LIST; do
        info "Configuring $IFNAME, see /tmp/slingshot-lldp.log or /var/log/slingshot-lldp.log for output" 1>&2
        retry_function 15 slingshot-network-cfg-lldp -v ${LLDP_ARGS} $IFNAME &>> ${TARGET_DIR}/slingshot-lldp.log
        if [[ $? -ne 0 ]]; then
            warn "Configuration via LLDP failed for interface $IFNAME"
            EXIT=1
            continue
        fi

        ip addr show dev $IFNAME 1>&2
    done

    let _increment=5
    let _max_timer=120
    let _timer+=$_increment

    echo $_timer > $CFG_LLDP_TIMER_STATE

    if [[ $EXIT -ne 0 && $_timer -gt $_max_timer ]]; then
        warn "Tried for too long to configure interfaces via LLDP - giving up"
        EXIT=0
    fi

    [[ $EXIT -ne 0 ]] && sleep $_increment

    echo "End run_slingshot_network_cfg_lldp"

    exit $EXIT
}

main $@
