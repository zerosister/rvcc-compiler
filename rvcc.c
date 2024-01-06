#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<stdarg.h>
#include "stdbool.h"
#include<string.h>

//单词种别
typedef enum{
    TK_PUNCT,   //操作符 + -
    TK_NUM,     //数字
    TK_EOF      //
} TokenKind;
 

// 单词结构体
typedef struct Token Token;
struct Token{
    TokenKind kind;     //类型
    Token* next;        //指向下一个终结符
    int val;            //值
    char* loc;          //字符串中位置
    int len;            //长度
};
// 增加错误信息
// static文件内可访问的函数
// Fmt为传入字符串，...为可变参数，表示Fmt后面所有参数
static void error(char *Fmt, ...){
    // def 一个 va_list变量
    va_list VA;
    // VA获取Fmt后面所有参数
    va_start(VA,Fmt);

    // vfprintf可输出va_list类型参数
    //需要向stderr中输入异常信息，stderr可向屏幕输出异常信息
    vfprintf(stderr,Fmt,VA);
    
    //fprintf,格式化文件输出，向文件中写入字符串
    // 结尾加上换行符
    fprintf(stderr,"\n");
    
    // 清除VA
    va_end(VA);
    
    // 终止程序
    exit(1);
}

// 生成新的Token
static Token* newToken(TokenKind kind,int val,char* loc,int len){
    // calloc有一个特性为会将内存初始化为0，所以token->next自动置为NULL
    Token* token = calloc(1,sizeof(Token));
    token->kind = kind;
    token->val = val;
    token->loc = loc;
    token->len = len;
    return token;
}

// 终结符解析
static Token *tokenize(char *p){
    // 使用一个链表进行存储各个Token
    // head表示链表头
    Token* head = newToken(2,0,0,0);
    Token* cur = head;
    while (*p){
        if(isspace(*p)){
            p++;
            continue;
        }
        else if(isdigit(*p)){
            char* startloc = p;
            // 获得数值的大小(absulute)
            int val = strtol(p,&p,10);
            // token长度
            int len = p - startloc + 1;
            cur->next = newToken(TK_NUM,val,startloc,len);
            cur = cur->next;
            continue;
        }
        // 不同之处，在此处将+,-区分开来与在main中+,-区分开
        // 此处为词法分析，尽量不在此处区分开+，-
        else if ((*p == '+') || (*p == '-')){
            cur->next = newToken(TK_PUNCT,0,p,1);
            cur = cur->next;
            p++;
            continue;
        }
    }
    cur->next = newToken(TK_EOF,0,p,0);

    // 因为头节点不存储信息故返回head->next
    return head->next;
}

// 判断token值是否为指定值
static bool equal(Token* token, char* str){
    // LHS为左字符串，RHS为右字符串
    // memcmp 按照字典序比较 LHS<RHS返回负值，=返回0,>返回正值
    return memcmp(token->loc,str,token->len) == 0 && str[token->len] == '\0';
}
int main(int argc,char** argv){
    // argv[0]为程序名称，argv[1]为传入第一个参数...依此类推
    if(argc != 2){
        // 参数异常处理
        error("%s:  invalid number of arguments",argv[0]);
    }
    // 分词
    Token* token = tokenize(argv[1]);
    // 
    printf("\t.global main\n");
    printf("main:\n");

    printf("\tli a0,%ld\n",token->val);
    token = token->next;
    while (token->kind != TK_EOF){
        if(token->kind == TK_PUNCT){
            // 用字符串比较替换为 用equal函数，方便拓展
            // if(*(token->loc) == '+')
            if(equal(token,"+"))
                printf("\taddi a0,a0,%ld\n",token->next->val);
            else{
                // 下面注释掉的这行代码会出问题，因为 %ld 会打印出其补数 (待考证)
                // printf("\taddi a0,a0,%ld\n",(-(token->next->val)));  
                
                // 但是strtol函数却可以处理负值
                // char** p0 = NULL;
                // printf("\t test: addi a0,a0,%d\n",strtol("-1003",p0,10));
                
                printf("\taddi a0,a0,-%ld\n",token->next->val);
            }
        }
        token = token->next;
    }
    printf("\tret\n");
    return 0;
    // 逐字符识别不对，如果数字长度是大于1的呢？
    // 且若数字为小数呢？（应当暂时不考虑小数，因为riscv汇编代码不能直接处理小数）
    // solution:运用函数strtol string to long
    
   // 若此时为while(ch)则存在一个问题，因为指针不为NULL，故会死循环
}

