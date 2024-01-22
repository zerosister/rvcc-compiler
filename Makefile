# C编译器参数：使用C11标准，生成debug信息，禁止将未初始化的全局变量放入到common段
CFLAGS=-std=c11 -g -fno-common
# 指定C编译器，来构建项目
RV=~/riscv/bin/riscv64-unknown-linux-gnu-gcc
QEMU=~/riscv/bin/qemu-riscv64
CC=gcc
# C源代码文件，所有以.c结尾文件
SRCS=$(wildcard *.c)
# C文件编译生成的未链接的可重定位文件，将.c文件名替换为同名的.o结尾文件名
OBJS=$(SRCS:.c=.o)
# test/文件夹的 c 测试文件
TEST_SRCS=$(wildcard test/*.c)
# test/文件夹的 c 测试文件编译出的可执行文件
TESTS=$(TEST_SRCS:.c=.exe)
# 指定 RISCV
RISCV=~/riscv

# 若make没有指定标签，默认指定makefile中第一个
# rvcc tag,表示如何构建最终二进制文件，依赖于main.o,.o文件为.c文件中间产物
rvcc: $(OBJS)
# 将多个*.o文件编译为rvcc,riscv64编译器gcc 输出为rvcc,引用源文件为*.o
# $@表示目标文件，此处为rvcc,$^表示依赖文件，此处为$(OBJS)
	$(CC) -o $@ $(CFLAGS) $^
# 所有的可重定位文件依赖于rvcc.h的头文件
$(OBJS): rvcc.h

# 测试标签，运行测试脚本
test/%.exe: rvcc test/%.c
# -o-: 将结果打印出来；-E 只进行预处理（展开宏）；-P:不输出行号标记；-C:预处理时注释不做处理
	$(RV) -o- -E -P -C test/$*.c | ./rvcc -o test/$*.s -
# $(CC) -o- -E -P -C test/$*.c | ./rvcc -o test/$*.s -
# 同时需要将 exe 加上强制以 c 编译的 test/common
# $(CC) -o $@ test/$*.s -xc test/common
	$(RV) -o $@ test/$*.s -xc test/common


test: $(TESTS)
	for i in $^; do echo $$i; $(QEMU) -L ~/riscv/sysroot ./$$i || exit 1; echo; done
	test/driver.sh

# 清理标签，清除所有非源代码文件
clean:
	rm -rf rvcc tmp* $(TESTS) test/*.s test/*.exe *.o

# 伪目标，没有实际的产出文件（rvcc标签产生了main.o）
.PHONY: test clean