#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<stdarg.h>
#include "stdbool.h"
#include<string.h>
#include<assert.h>

/************************ 数据结构声明 *************************/
//单词种别
typedef enum{
    TK_ADD,     // +
    TK_SUB,     // -
    TK_MUL,     // *
    TK_DIV,     // '/'
    TK_LBR,     // '('
    TK_RBR,     // ')'
    TK_NUM,     //数字
    TK_NOT,     //!
    TK_DEQ,     //== Double EQual
    TK_NEQ,     //!= Not EQual
    TK_BGT,     //> BiGger Than
    TK_BGE,     //>= BiGger and Equal
    TK_LST,     //< LeSs Than
    TK_LSE,     //<= LeSs and Equal 
    TK_SEM,     //; Semicolon
    TK_EOF      //终结符
} TokenKind;
 
// 单词结构体,typedef为结构体取别名
typedef struct Token Token;
struct Token{
    TokenKind kind;     //类型
    Token* next;        //指向下一个终结符
    int val;            //值
    char* loc;          //字符串中位置
    int len;            //长度
};

// AST(抽象语法树)节点
typedef struct Node Node;
struct Node{
    // 左右子节点
    Node* LNode;
    Node* RNode;
    // 当前token
    Token* token;
    Node* next;
};

// 表示状态的种类
typedef enum{
    ST_Add,        //E
    ST_AddPrim,       //E'
    ST_Mul,         //T
    ST_MPrim,       //T'
    ST_Primary,     //F
    ST_Equa,        //Equa
    ST_EqPrim,      //Equa'
    ST_Rela,        //Rela
    ST_RePrim      //Rela'
} StatusKind;

// 状态
typedef struct Status Status;
struct Status{
    StatusKind kind;
    Node* inherit;  //继承属性
    Node* ptr;      //自身所指向结点
    Node* system;   //综合属性

    // 递归则不需要保存栈
    // Status* next;   //下一个状态
};

/************************ 函数声明 *************************/
// 报错信息
void verrorAt(char* loc,char* Fmt,va_list VA);
void errorAt(char* loc,char* Fmt,...);
void errorTok(Token* token,char* Fmt,...);
void error(char* Fmt,...);

// 词法分析入口
Token* tokenize(char* p);
Token* skip(Token* token,char* str);

// 语法分析入口
Node* parse(Token** rest,Token* token);

// 汇编代码生成入口
void genCode(Node* root);