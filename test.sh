#!/bin/bash 
#告诉操作系统应当用/bin/bash来处理此脚本

# 查看脚本输出内容 
# set -x

# 声明一个函数
assert(){
    # 短路执行,若运行失败直接退出
    # 程序运行 期待值 为参数1
    RISCV=~/riscv/
    expected="$1"
    # 输入值为 参数2
    input="$2"
    ./rvcc "$input" > tmp.s|| exit
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

# assert 期待值 输入值
# [1] 返回指定数值
assert 0 '0;'
assert 42 '42;'

# [2] 支持+ -运算符
assert 34 '12-34+56;'

# [3] 支持空格
assert 41 ' 12 + 34 - 5 ;'

# [5] 支持* / ()运算符
assert 47 '5+6*7;'
assert 15 '5*(9-6);'
assert 17 '1-8/(2*2)+3*6;'

# [6] 支持一元运算的+ -
assert 10 '-10+20;'
assert 10 '- -10;'
assert 10 '- - +10;'
assert 48 '------12*+++++----++++++++++4;'

# [7] 支持条件运算符
assert 0 '0==1;'
assert 1 '42==42;'
assert 1 '0!=1;'
assert 0 '42!=42;'
assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'
assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'
assert 1 '5==2+3;'
assert 0 '6==4+3;'
assert 1 '0*9+5*2==4+4*(6/3)-2;'
assert 1 '!(78==(34+44-1));'

# [9] 支持;分割语句
assert 3 '1; 2; 3;'
assert 12 '12+23;12+99/3;78-66;'

# 如果运行正常未提前退出，程序将显示OK
echo OK