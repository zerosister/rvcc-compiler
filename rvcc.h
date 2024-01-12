// 使用 POSIX.1 标准
// 使用了 strndup 函数
#define _POSIX_C_SOURCE 200809L

// 规定是否使用 hash 桶的宏
#define USE_HASH 1

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdbool.h"

/************************ 数据结构声明 *************************/
//单词种别
typedef enum {
  TK_ADD,  // +
  TK_SUB,  // -
  TK_MUL,  // *
  TK_DIV,  // '/'
  TK_LBR,  // '('
  TK_RBR,  // ')'
  TK_NUM,  //数字
  TK_NOT,  //!
  TK_DEQ,  //== Double EQual
  TK_NEQ,  //!= Not EQual
  TK_BGT,  //> BiGger Than
  TK_BGE,  //>= BiGger and Equal
  TK_LST,  //< LeSs Than
  TK_LSE,  //<= LeSs and Equal
  TK_SEM,  //; Semicolon
  TK_VAR,  //变量
  TK_ASS,  //赋值符号
  TK_RET,  // return
  TK_LBB,  // Left Big Brakect
  TK_RBB,  // Right Big Brakect
  TK_IF,   // if
  TK_ELS,  // else
  TK_EOF   //终结符
} TokenKind;

// 单词结构体，typedef 为结构体取别名
typedef struct Token Token;
struct Token {
  TokenKind kind;  //类型
  Token* next;     //指向下一个终结符
  int val;         //值
  char* loc;       //字符串中位置
  int len;         //长度
};

// 交给预处理器判断编译 rvcc.h 中的哪个 Obj
#if USE_HASH
// hash_size 即为 hash 桶的个数
#define HASH_SIZE 13
typedef struct Obj Obj;
struct Obj {
  char* Name;        //变量名
  int value;         //哈希值，防止每次都调用 hash 函数
  struct Obj* next;  //下一个对象
  int Offset;        // fp 偏移量
};

typedef struct HashTable HashTable;
struct HashTable {
  // hash_size 个 Obj 指针数组
  Obj* objs[HASH_SIZE];
  int size;
};

unsigned int hash(char* Name, int size, int len);
Obj* insert(HashTable* hashTable, char* Name, int len);
Obj* search(HashTable* hashTable, char* Name, int len);
void remove_hash(HashTable* hashTable, char* Name);
#else
// 本地变量（符号表）思考：每个方程有不同的符号表，故可以变量重名
typedef struct Obj Obj;
struct Obj {
  Obj* Next;   //指向下一对象
  char* Name;  //变量名
  int Offset;  // fp 的偏移量
};

// 方便之后对比，统一用上 hash_table
typedef struct HashTable HashTable;
struct HashTable {
  Obj* locals;
};
#endif

// AST(抽象语法树) 节点
typedef struct Node Node;
struct Node {
  // 左右子节点
  Node* LNode;
  Node* RNode;
  Token* token;  // 当前 token
  Node* next;
  Node* body;  // 若为 Compound 结点，对应的语句链表
  Obj* Var;    // 对应变量
};

// 表示状态的种类
typedef enum {
  ST_Add,      // E
  ST_AddPrim,  // E'
  ST_Mul,      // T
  ST_MPrim,    // T'
  ST_Primary,  // F
  ST_Equa,     // Equa
  ST_EqPrim,   // Equa'
  ST_Rela,     // Rela
  ST_RePrim,   // Rela'
  ST_Assign,   // Assign
  ST_AssPrim   // Assign'
} StatusKind;

// 状态
typedef struct Status Status;
struct Status {
  StatusKind kind;
  Node* inherit;  //继承属性
  Node* ptr;      //自身所指向结点
  Node* system;   //综合属性
};

// 函数
typedef struct Function Function;
struct Function {
  Node* Body;         //函数体
  HashTable* Locals;  //本地变量
  int StackSize;      //栈大小
};

/************************ 函数声明 *************************/
// 报错信息
void verrorAt(char* loc,char* Fmt,va_list VA);
void errorAt(char* loc, char* Fmt, ...);
void errorTok(Token* token, char* Fmt, ...);
void error(char* Fmt, ...);

// 词法分析入口
Token* tokenize(char* p);
Token* skip(Token* token, char* str);
bool equal(Token* token, char* str);
bool startsWith(char* Str, char* SubStr);

// 语法分析入口
Function* parse(Token** rest, Token* token);

// 汇编代码生成入口
void genCode(Function* root);