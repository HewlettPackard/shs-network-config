#!/bin/bash

check() {
    return 0
}

depends() {
    return 0
}

install() {
	inst /etc/modprobe.d/99-slingshot-network.conf
}
