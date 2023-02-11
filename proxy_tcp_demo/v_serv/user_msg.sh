#!/bin/bash

# 与用户交互
# 本脚本会被调用多次，一个用户连接就会被调用一次
# 所以这个脚本里必须处理好用户对应关系

# 用户和服务端转发协议
# ```
# PID（就是本脚本执行的进程ID，用这个唯一标识用户）
# length（消息的长度，好截断），连接到时给服务端发-1，断开发0
# info（消息内容，当length > 0时，长度是length）
# ```

# 标准输入为用户发送的数据
# 标准输出为给用户发数据
# 200管道给服务端发送数据
# 201管道接收服务端发送的数据

MY_PID=$$

(
    flock -x 200
    echo "$MY_PID" >&200
    echo "-1" >&200     # 发-1表示有新用户连接
) 200<>user_to_serv.fifo

while :; do
    (
        # 这里从201管道读入服务端发送的数据，并且写到标准输出（用户）
        flock -x 201
        PID=$(cat serv_to_user.temp)
        [ -z "$PID" ] && read -u 201 PID
        if [ "$MY_PID" == "$PID" ]; then
            read -u 201 length
            if ((length > 0)); then
                dd bs=1 "count=$length" <&201
            else
                # 真正的服务端断开了连接
                kill "$MY_PID"
            fi
            >serv_to_user.temp
        else
            echo "$PID" >serv_to_user.temp
        fi
    ) 201<>serv_to_user.fifo
done &

while :; do
    (
        # 这里从标准输入读数据，把读到的数据发送给服务端（200管道）
        tmp_file=$(mktemp)
        timeout 0.01 dd "of=$tmp_file" bs=1M count=10

        if [ "$(stat -c %s "$tmp_file")" != "0" ]; then
            # 当读到信息后开始使用文件锁做同步操作
            flock -x 200
            echo "$MY_PID" >&200            # 把当前PID发送给服务端
            stat -c %s "$tmp_file" >&200    # 输出当前消息长度
            cat "$tmp_file" >&200           # 把信息发送给服务端
        fi
        rm -f "$tmp_file"
    ) 200<>user_to_serv.fifo
done
