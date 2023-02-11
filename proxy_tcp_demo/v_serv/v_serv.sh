#!/bin/bash

# 作为代理的虚假服务端，把这个脚本放到能被用户访问的机器上执行

cd "$(dirname "$0")"

usage()
{
cat << EOF
Usage: $0 <-r <port> > <-v <port> > <-t <port> >
    -r <port>       Spec real user connect port.
    -v <port>       Spec virtual client connect port.
    -t <port>       Spec temp connect port.

[ user ] -> real-port -> [ accept ] -> temp-port -> [ mnc_exec ] <- virtual-port <- [ v_cli ]
EOF
}

ARGS=$(getopt -o 'hr:v:t:' -- "$@")
[ $? -ne 0 ] && exit 1
eval "set -- $ARGS"

R_PORT=""
V_PORT=""
T_PORT=""

while [ $# -gt 0 ]; do
    case "$1" in
        "-h")
            usage
            exit 0
            ;;
        "-r")
            R_PORT=$2
            shift
            ;;
        "-v")
            V_PORT=$2
            shift
            ;;
        "-t")
            T_PORT=$2
            shift
            ;;
    esac
    shift
done

[ -z "$R_PORT" ] && echo "please spec real port." && exit 1
[ -z "$V_PORT" ] && echo "please spec virtual port." && exit 1
[ -z "$T_PORT" ] && echo "please spec temp port." && exit 1

echo "$R_PORT" | grep -q '[^0-9]' && echo "real port must be a 0~65535 int." && exit 1
echo "$V_PORT" | grep -q '[^0-9]' && echo "virtual port must be a 0~65535 int." && exit 1
echo "$T_PORT" | grep -q '[^0-9]' && echo "temp port must be a 0~65535 int." && exit 1

((R_PORT > 65535)) && echo "real port must be a 0~65535 int." && exit 1
((V_PORT > 65535)) && echo "virtual port must be a 0~65535 int." && exit 1
((T_PORT > 65535)) && echo "temp port must be a 0~65535 int." && exit 1

((R_PORT == V_PORT)) && echo "real port should not equal to virtual port." && exit 1
((R_PORT == T_PORT)) && echo "real port should not equal to temp port." && exit 1
((V_PORT == T_PORT)) && echo "virtual port should not equal to temp port." && exit 1

# 启动mnc服务监听虚拟客户端，当虚拟客户端有连接则执行mnc_exec.sh
./mnc -ldp "$V_PORT" -c "cd '$(pwd)'; ./mnc_exec.sh $R_PORT $T_PORT"

# 启动mnc服务监听temp port
./mnc -ldp "$T_PORT" -c "cd '$(pwd)'; ./user_msg.sh"
