# cmake 最低版本号
cmake_minimum_required( VERSION 3.10.0 )

# 项目名称
project( rvcc C)

# 可执行文件 rvcc 的依赖文件
add_executable(rvcc
    rvcc.c
    parser.c
    tokenize.c
    genCode.c
)

# 编译参数
SET( CMAKE_C_FLAGS "-std=c11 -g -fno-common")

# build 目录位置
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)