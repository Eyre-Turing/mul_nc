#!/bin/bash

# 当接受虚拟客户端连接后，本脚本会被调用
# 处理真正用户连接的事件
# 本脚本只会被调用一次

R_PORT=$1
T_PORT=$2

rm -f user_to_serv.fifo
rm -f serv_to_user.fifo
mkfifo user_to_serv.fifo
mkfifo serv_to_user.fifo

rm -f serv_to_user.temp
touch serv_to_user.temp # 缓存，当user进程不小心读取了不是发送给自己的数据时写到这里，后面的user进程会优先看这个文件里的数据

./mnc -ldp "$R_PORT" -c "cd '$(pwd)'; ./accept.sh $T_PORT"

exec 200<>user_to_serv.fifo
exec 201<>serv_to_user.fifo

# 标准输入为服务端发送的数据
# 标准输出为给服务端发数据
# 200管道接收用户发送的数据
# 201管道给用户发送数据

# 注意：给虚拟客户端发送数据等价于给服务端发送数据

# 把用户发送的数据给打印出来，因为调用本脚本的是监听虚拟客户端（也就是服务端）的服务，所以打印出来就会发送给虚拟客户端（服务端）
cat <&200 &

# 把标准输入写入管道，因为调用本脚本的是监听虚拟客户端的服务，所以标准输入是虚拟客户端发送的消息，写入管道即发送给用户
cat >&201
