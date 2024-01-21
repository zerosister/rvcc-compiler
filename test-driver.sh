#!/bin/bash

# 创建一个临时文件夹，-d 代表为文件夹 XXXXXX会被替换为随机字符串
tmp=`mktemp -d /tmp/rvcc-test-XXXXXX`
# 清理工作 trap 规定了对应信号的处理方式
# 在接收到 中断（ctrl+c），终止，挂起（ssh掉线，用户退出），退出 信号时
# 执行rm命令，删除掉新建的临时文件夹
trap 'rm -rf $tmp' INT TERM HUP EXIT
# 在临时文件夹内，新建一个空文件，名为empty.c
echo > $tmp/empty.c

# 判断返回值是否为0来判断程序是否成功执行
check() {
  if [ $? -eq 0 ]; then
    echo "testing $1 ... passed"
  else
    echo "testing $1 ... failed"
    exit 1
  fi
}

# -o
# 清理掉$tmp中的out文件
rm -f $tmp/out
# 编译生成out文件
./rvcc -o $tmp/out $tmp/empty.c
# 条件判断，是否存在out文件
# -f 测试运算符 表示是否存在且为文件
[ -f $tmp/out ]
# 将-o传入check函数
check -o

# --help
# 将--help的结果传入到grep进行 行过滤
# -q 表示 grep 不输出，只判断是否匹配到存在rvcc字符串的行结果
# 2:stderr,1:stdout,>&:重定向
./rvcc --help 2>&1 | grep -q rvcc
# 将--help传入check函数
check --help

echo OK
