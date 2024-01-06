#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>

int main(int argc,char** argv){
    //参数异常判断
    if(argc != 2){
        //并非形如 rvcc 48,并非正常结束程序，返回非零

        //fprintf,格式化文件输出，向文件中写入字符串
        //需要向stderr中输入异常信息，stderr可向屏幕输出异常信息
        fprintf(stderr,"%s:expected 2 arguments but got %d\n",argv[0],argc);
        return 1;
    }
    // lesson2 to identify + - automachine way
    char *ch;
    ch = argv[1];
    // state 自动机 的 状态
    int state = 0;
    // stack 规约式的继承属性 +为 1，-为 -1
    int stack = 1;
    // add 表示增加或减少增量
    int add = 0;
    printf("\t.global main\n");
    printf("main:\n");

    // 逐字符识别不对，如果数字长度是大于1的呢？
    // 且若数字为小数呢？（应当暂时不考虑小数，因为riscv汇编代码不能直接处理小数）
    // solution:运用函数strtol string to long
    add = strtol(ch,&ch,10);
    printf("\tli a0,%ld\n",add);
    /*
        采用如下产生式 E->digit + E | digit - E | digit
        rvcc课程方法先将digit识别
    */

   // 若此时为while(ch)则存在一个问题，因为指针不为NULL，故会死循环
    while (*ch)
    {
        if(*ch == '+'){
            // 连着符号一起to long
            printf("\taddi a0,a0,%ld\n",strtol(ch,&ch,10));
        }
        else if (*ch == '-'){
            printf("\taddi a0,a0,%ld\n",strtol(ch,&ch,10));
        }
    }
    printf("\tret\n");
    return 0;
    //向tmp.s生成汇编代码
    // FILE *fp = NULL;
    // fp = fopen("tmp.s","w+");
    // // 不能随意乱指定，因为ret为 jalr x0,x1,0别名指令，返回子程序
    // fprintf(fp,"\t.global main\nmain:\n\tli a0,%d\n\tret",atoi(argv[1]));
    // fclose(fp);
    return 0;
}
