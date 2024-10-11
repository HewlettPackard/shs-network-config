#!/bin/bash
#
# Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
#

# Add dracut-lib.sh for 'info' support
type getarg >/dev/null 2>&1 || . /lib/dracut-lib.sh

info "Start start_lldpad"

# Make sure the config directory exists.
mkdir -p /var/lib/lldpad

# Executes the LLDP protocol for supported network interfaces.
# -d in order to run lldpad as a daemon
lldpad -d

ret=255
while [[ $ret -ne 0 ]] ; do
  pid=$(lldptool -p)
  ret=$?
  if [[ $ret -ne 0 ]] ; then
    info "failed to get pid for lldpad"
    sleep 1
  fi
done

info "End start_lldpad"
