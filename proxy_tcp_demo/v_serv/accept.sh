#!/bin/bash

# 当mnc_exec.sh启动后，有用户连接时mnc会fork执行本脚本
# 用于重定向输入输出到虚拟客户端
# 本脚本会被调用多次，一个用户连接就会被调用一次

T_PORT=$1

./mnc -a '127.0.0.1' -p "$T_PORT"
