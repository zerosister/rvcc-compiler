#!/bin/bash 
#告诉操作系统应当用/bin/bash来处理此脚本

# 声明一个函数
assert(){
    # 短路执行,若运行失败直接退出
    # 程序运行 期待值 为参数1
    RISCV=~/riscv/
    expected="$1"
    # 输入值为 参数2
    input="$2"
    ./rvcc $input || exit
    #编译rvcc产生的汇编文件
    $RISCV/bin/riscv64-unknown-linux-gnu-gcc -static tmp.s -o tmp
    #运行生成文件
    $RISCV/bin/qemu-riscv64 -L $RISCV/sysroot ./tmp
    actual="$?"
    if [ "$actual" == "$expected" ];then
        echo "$input => $actual"
    else
        echo "$input => $expected expected but get $actual"
        exit 1
    fi
}

assert 0 0
assert 90 90
echo "OK"