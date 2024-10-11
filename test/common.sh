#!/bin/bash
#
# Copyright 2021 Hewlett Packard Enterprise Development LP. All rights reserved.
#

SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
    DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"

export PATH=$(readlink -f $DIR/../src):$PATH

function check_for_diag_messages {
    logfile=$1

    entries_found=$(grep "\[DIAG\]" $logfile | wc -l)

    if [[ $entries_found -gt 0 ]] ; then
        echo true
    else
        echo false
    fi
}

function check_for_keywords {
    keywords=$1
    logfile=$2

    entries_found=$(grep "$keywords" $logfile | wc -l)

    if [[ $entries_found -gt 0 ]] ; then
        echo true
    else
        echo false
    fi
}
