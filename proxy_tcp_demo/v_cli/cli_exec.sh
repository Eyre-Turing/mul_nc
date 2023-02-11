#!/bin/bash

# 处理用户连接
# 本脚本只会被调用一次

# 标准输入为代理服务器（虚拟服务器）发送过来的消息，代理服务器可以直接理解为用户
# 标准输出为给代理服务器发消息

R_ADDR=$1
R_PORT=$2

# 转发数据给代理服务器
while :; do
    for pid_file in $(\ls serv_to_user.*.fifo); do
        # 把真正的服务端给每个虚拟用户发送的消息转发给代理服务器
        PID=$(echo "$pid_file" | sed -r 's/serv_to_user\.(.*)\.fifo/\1/g')
        tmp_file=$(mktemp)
        timeout 0.01 dd "of=$tmp_file" bs=1M count=10 <"$pid_file"
        if [ "$(stat -c %s "$tmp_file")" != "0" ]; then
            echo "$PID"
            stat -c %s "$tmp_file"
            cat "$tmp_file"
        fi
        rm -f "$tmp_file"
    done
done &

# 当有一个用户连接，就创建一个虚假的用户连接到真正的服务器，并且建立两边通信
while :; do
    read PID
    read length
    if [ "$length" == "-1" ]; then
        # 新用户连接
        ./mnc -d -a "$R_ADDR" -p "$R_PORT" -c "cd '$(pwd)'; ./serv_msg.sh '$PID'"
        continue
    fi
    if [ "$length" == "0" ]; then
        # 用户断开连接
        continue
    fi

    # 转发给真正的服务端
    dd bs=1 "count=$length" >"user_to_serv.${PID}.fifo"
done
