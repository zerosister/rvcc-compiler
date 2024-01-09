# C编译器参数：使用C11标准，生成debug信息，禁止将未初始化的全局变量放入到common段
CFLAGS=-std=c11 -g -fno-common
# 指定C编译器，来构建项目
# CC=~/riscv/bin/riscv64-unknown-linux-gnu-gcc
CC=gcc

main.o:
# Compile and assemble,do not link
	$(CC) -o main.o -c rvcc.c
# 若make没有指定标签，默认指定makefile中第一个
# rvcc tag,表示如何构建最终二进制文件，依赖于main.o,.o文件为.c文件中间产物
rvcc: main.o
# 将多个*.o文件编译为rvcc,riscv64编译器gcc 输出为rvcc,引用源文件为main.o
	$(CC) -o rvcc $(CFLAGS) main.o -g

# 测试标签，运行测试脚本
test: rvcc
	./test.sh

# 清理标签，清除所有非源代码文件
clean:
	rm -f rvcc *.o *.s tmp.* a.out tmp

# 伪目标，没有实际的产出文件（rvcc标签产生了main.o）
.PHONY: test clean