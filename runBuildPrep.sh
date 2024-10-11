#!/bin/bash
#
# Copyright 2021-2023 Hewlett Packard Enterprise Development LP. All rights reserved.
#

BRANCH="git branch --show-current || git rev-parse --abbrev-ref HEAD"


if [ -d hpc-shs-version ]; then
    git -C hpc-shs-version pull
else
    if [[ -n "${SHS_LOCAL_BUILD}" ]]; then
        git clone git@github.hpe.com:hpe/hpc-shs-version.git
    else
    	git clone https://$HPE_GITHUB_TOKEN@github.hpe.com/hpe/hpc-shs-version.git
    fi
fi

. hpc-shs-version/scripts/get-shs-version.sh
. hpc-shs-version/scripts/get-shs-label.sh

PRODUCT_VERSION=$(get_shs_version)
PRODUCT_LABEL=$(get_shs_label)


sed -i -e "s/Release:.*/Release: ${PRODUCT_LABEL}${PRODUCT_VERSION}_%(echo \\\${BUILD_METADATA:-1})/g" slingshot-network-config.spec
