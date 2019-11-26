#!/bin/bash

platform=
WORKDIR="$( cd "$( dirname "${BASH_SOURCE[0]}"   )" && pwd   )"
echo "WORKDIR: $WORKDIR "
echo "========FINAL_PATH: $FINAL_PATH"

if [ ! "$1"x == x ]; then 
    echo "$1"
    platform=$1
else
    echo -e "\033[32m       platform unknown      \033[0m"
    exit -1
fi

make
make install

