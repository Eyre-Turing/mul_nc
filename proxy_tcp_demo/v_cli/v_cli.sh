#!/bin/bash

# 作为虚拟客户端，把这个脚本放到真正的服务端所在的机器上执行，模拟用户给真正的服务端交互

cd "$(dirname "$0")"

usage()
{
cat << EOF
Usage: $0 [-a <addr>] <-p <port> > [-A <addr>] <-P <port> >
    -a <addr>       Set virtual server addr, default is 127.0.0.1
    -p <port>       Set virtual server port.
    -A <addr>       Set real server addr, default is 127.0.0.1
    -P <port>       Set real server port.
EOF
}

ARGS=$(getopt -o 'ha:p:A:P:' -- "$@")
[ $? -ne 0 ] && exit 1
eval "set -- $ARGS"

V_ADDR="127.0.0.1"
V_PORT=""
R_ADDR="127.0.0.1"
R_PORT=""

while [ $# -gt 0 ]; do
    case "$1" in
        "-h")
            usage
            exit 0
            ;;
        "-a")
            V_ADDR=$2
            shift
            ;;
        "-p")
            V_PORT=$2
            shift
            ;;
        "-A")
            R_ADDR=$2
            shift
            ;;
        "-P")
            R_PORT=$2
            shift
            ;;
    esac
    shift
done

[ -z "$V_ADDR" ] && echo "need spec virtual server addr(aka ip)." && exit 1
[ -z "$V_PORT" ] && echo "need spec virtual server port." && exit 1
[ -z "$R_ADDR" ] && echo "need spec real server addr(aka ip)." && exit 1
[ -z "$R_PORT" ] && echo "need spec real server port." && exit 1

# 连接到代理服务端
./mnc -d -a "$V_ADDR" -p "$V_PORT" -c "cd '$(pwd)'; ./cli_exec.sh '$R_ADDR' '$R_PORT'"
