#!/bin/bash

# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


: "${FMI_EXE:=/usr/local/bin/fmi}"

function print_usage () {
    echo "FMI Tools"
    echo ""
    echo "  fmi <command> [command options,]"
    echo ""
    echo "Commands:"
    echo "  [fmi]  (fmi commands)"
    echo ""
    echo "Example:"
    echo "  docker run --rm -v $(pwd):/sim fmi"
    exit 1
}

if [ $# -eq 0 ]; then print_usage; fi


#echo "Command is $1"
CMD="$FMI_EXE $1"

# Run the command
if [ -z ${CMD+x} ]; then print_usage; fi
shift
#echo $CMD "$@"
$CMD "$@"
