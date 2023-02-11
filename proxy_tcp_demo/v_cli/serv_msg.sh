#!/bin/bash

# 和真正的服务端连接，转发消息
# 本脚本会被调用多次，一个用户连接被调用一次

# 标准输入为真正的服务端发送的消息
# 标准输出为给真正的服务端发送消息
# 200管道读取用户发送过来的数据
# 201管道给用户写服务端发的数据

PID=$1

rm -f "user_to_serv.${PID}.fifo"
rm -f "serv_to_user.${PID}.fifo"
mkfifo "user_to_serv.${PID}.fifo"
mkfifo "serv_to_user.${PID}.fifo"

exec 200<>"user_to_serv.${PID}.fifo"
exec 201<>"serv_to_user.${PID}.fifo"

# 只有这里是要读这个管道的，无需加锁
# 并且实际上是由cli_exec帮搞过来的
# 搞过来后把头部什么的勾巴都去掉了，所以直接cat就可以了
# 大胆动手吧！
cat <&200 &

# 只有这里是写这个管道的，无需加锁
# 直接把标准输入（真正服务端的输出）重定向给用户管道
cat >&201
