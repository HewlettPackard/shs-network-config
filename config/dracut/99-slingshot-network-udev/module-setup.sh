#!/bin/bash

check() {
	return 0
}

depends() {
	echo network
    return 0
}

install() {
	inst_rules 99-slingshot-network.rules
	inst_script /usr/bin/slingshot-ifname /usr/bin/slingshot-ifname
}
