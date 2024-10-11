#!/bin/bash
# module-setup.sh for slingshot-network-config

check() {
    return 0
}

depends() {
    echo network
    return 0
}

install() {
    local bindir=/opt/slingshot/slingshot-network-config/default/bin
    inst_multiple lldpad lldptool slingshot-network-cfg-lldp ethtool
    inst "$bindir/slingshot-ifroute.sh"
    inst "/etc/iproute2/rt_tables"
    inst "/usr/bin/head"
    inst "/sbin/sysctl"
    inst "/usr/bin/wc"
    inst_hook pre-trigger 20 "$bindir/start_lldpad.sh"
    inst_hook initqueue/finished 15 "$bindir/run_slingshot_network_cfg_lldp.sh"
    inst_hook initqueue/finished 18 "$bindir/slingshot-ifroute.sh"
    inst_hook pre-pivot 20 "$bindir/stop_lldpad.sh"
    inst_hook pre-pivot 21 "$bindir/copy_logfile_to_rootfs.sh"
}
