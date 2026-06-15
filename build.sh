#!/usr/bin/env bash
#
# Copyright 2026 Hewlett Packard Enterprise Development LP. All rights reserved.
#

set -Eeuox pipefail

export CE_BUILD_SCRIPT_REPO=hpc-shs-ce-devops

if [ -d ${CE_BUILD_SCRIPT_REPO} ]; then
    echo "Using existing '${CE_BUILD_SCRIPT_REPO}' directory"
else
    git clone https://$HPE_GITHUB_TOKEN@github.hpe.com/hpe/${CE_BUILD_SCRIPT_REPO}.git
fi

. ${CE_BUILD_SCRIPT_REPO}/build/sh/rpmbuild/build-common.sh
