#!/bin/bash

# 编译 .proto 文件生成 C++ 代码
# 注意：这要求你的 Linux 环境中已经能够直接使用 protoc 命令
~/MooncRPC/vcpkg/installed/x64-linux/tools/protobuf/protoc --cpp_out=. message.proto

# 检查上一条命令是否执行成功
if [ $? -eq 0 ]; then
    echo "Protobuf compilation completed!"
else
    echo "Protobuf compilation failed! 请检查是否正确安装了 protobuf 编译器。"
    exit 1
fi