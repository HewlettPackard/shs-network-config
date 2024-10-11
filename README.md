# To build this:

autogen.sh

configure --prefix=$(pwd)/install

make

make install

# To execute this:
Input: the name of the HSN interface to be configured.

Routine:
Ensure that the LLDP daemon is running with e.g.,
`lldpad -d`.
Allow up to 30 seconds for the agent to become operational.
Or loop, checking the output of
`lldptool get-tlv -i [name of interface to configure ]`
perhaps every second until it eventually succeeds and
produces a non-error output.

Then,
`Usage: slingshot-network-cfg-lldp [-h|--help] [-c|--create-ifcfg] [-n|--dry-run] [-r|--remove-ip-addrs] [-v|--verbose] <interface>`

will use lldptool to obtain the TLV broadcast by the upstream Rosetta.
It will then parse the TLV and use `ip` to put the parsed configuration down onto the specified interface, if both are valid.

