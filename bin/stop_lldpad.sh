#!/bin/bash
#
# Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
#

# Add dracut-lib.sh for 'info' support
type getarg >/dev/null 2>&1 || . /lib/dracut-lib.sh

info "Start stop_lldpad"

# Daemons should not be left running in the initrd before the switch root.
# Send the KILL signal to the lldp agent daemon process.
kill -s KILL $(lldptool ping)

info "End stop_lldpad"
