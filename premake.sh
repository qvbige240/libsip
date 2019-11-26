#!/bin/bash
# qing.zou

WORKDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
echo "WORKDIR: $WORKDIR "


function usage_exit()
{ 
        echo "  Usge: $0 platform [rebuild]"
        echo "  platform list:"
        echo -e "\033[32m               nt966x \033[0m"
        echo -e "\033[32m               x86    \033[0m"
        exit -1
}

function proj_path_get()
{
    if [ -d "${WORKDIR}/../$1" ]; then
        echo "${WORKDIR}/../$1"
    elif [ -d "${WORKDIR}/../../$1/workspace" ]; then
        echo "${WORKDIR}/../../$1/workspace"
    else
        echo "$1 not exist"
        exit -1
    fi
}

function prepare()
{
    if [ -f ${WORKDIR}/prepare.sh ]; then
        ${WORKDIR}/prepare.sh
    fi
}

function premake_init()
{
    #DIR_PREMAKE=$(proj_path_get premake)
    #echo "dir premake: ${DIR_PREMAKE}"
    dir_premake=$(proj_path_get premake)
    echo "dir premake: ${dir_premake}"
    source ${dir_premake}/init.sh
    premake_dir_set
    platform_init $1

    echo "DIR PREMAKE: ${DIR_PREMAKE}"
}

function start()
{
    if [ ! -n "$1" ]; then
        usage_exit
    fi

    if [ "$2" == 1 ]; then
        prepare
    fi

    premake_init $1
    premake_make ${WORKDIR} 1 true
}

# $1: nt966x/x86, $2: reserved, $3: true/false (make clean)
start $1 $2 $3

