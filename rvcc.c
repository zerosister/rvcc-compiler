#include<stdio.h>
#include<stdlib.h>

int main(int argc,char** argv){
    //参数异常判断
    if(argc != 2){
        //并非形如 rvcc 48,并非正常结束程序，返回非零

        //fprintf,格式化文件输出，向文件中写入字符串
        //需要向stderr中输入异常信息，stderr可向屏幕输出异常信息
        fprintf(stderr,"%s:expected 2 arguments but got %d\n",argv[0],argc);
        return 1;
    }
    //向tmp.s生成汇编代码
    FILE *fp = NULL;
    fp = fopen("tmp.s","w+");
    // 不能随意乱指定，因为ret为 jalr x0,x1,0别名指令，返回子程序
    fprintf(fp,"\t.global main\nmain:\n\tli a0,%d\n\tret",atoi(argv[1]));
    fclose(fp);
    return 0;
}
