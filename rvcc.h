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
  TK_ADD,    // +
  TK_SUB,    // -
  TK_MUL,    // *
  TK_DIV,    // '/'
  TK_LBR,    // '('
  TK_RBR,    // ')'
  TK_NUM,    //数字
  TK_NOT,    //!
  TK_DEQ,    //== Double EQual
  TK_NEQ,    //!= Not EQual
  TK_BGT,    //> BiGger Than
  TK_BGE,    //>= BiGger and Equal
  TK_LST,    //< LeSs Than
  TK_LSE,    //<= LeSs and Equal
  TK_SEM,    //; Semicolon
  TK_VAR,    //变量
  TK_ASS,    //赋值符号
  TK_RET,    // return
  TK_LBB,    // {
  TK_RBB,    // }
  TK_IF,     // if
  TK_ELS,    // else
  TK_FOR,    // for
  TK_WHI,    // while
  TK_DEREF,  // * 解引用
  TK_ADDR,   // & 取指符
  TK_INT,    // int
  TK_COM,    // ，
  TK_FUNC,   // 函数调用
  TK_LMB,    // [
  TK_RMB,    // ]
  TK_SIZEOF, // sizeof
  TK_CHAR,   // char
  TK_EOF     //终结符
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

// 类型种类
typedef enum {
  TY_INT, // int 整型
  TY_CHAR,  // char
  TY_PTR, // 指针
  TY_ARRAY, //数组
  TY_FUNC // 函数
} TypeKind;

// 符号表中变量
typedef struct Obj Obj;

// 结点或变量类型
typedef struct Type Type;
struct Type {
  TypeKind tyKind;    // 哪种类型
  Type* base;         // 若类型为指针则表示指针指向的数据类型
  Type* params;       // 形参
  Type* next;         // 指示下一个形参
  Obj* var;           // 记录下函数形参信息
  Type* retType;      // 函数返回类型
  int size;           // 类型所占空间，若为数组则为整个数组所占空间
};

// 声明全局变量，在 type.c 中定义
extern Type *TypeInt;
extern Type *TypeChar;

// 交给预处理器判断编译 rvcc.h 中的哪个 Obj
#if USE_HASH
// hash_size 即为 hash 桶的个数
#define HASH_SIZE 13
struct Obj {
  char* Name;        // 变量名
  int value;         // 哈希值，防止每次都调用 hash 函数
  struct Obj* next;  // 下一个对象
  int Offset;        // fp 偏移量
  Type *ty;          // 数据类型
  bool isLocal;      // 记录是否为局部变量
  bool isFuncName;       // 记录变量为 全局变量 或 函数名
};

typedef struct HashTable HashTable;
struct HashTable {
  Obj* objs[HASH_SIZE];   // hash_size 个 Obj 指针数组
  int size;               // 变量的总个数
};

unsigned int hash(char* Name, int size, int len);
Obj* insert(HashTable* hashTable, char* Name, int len);
Obj* search(HashTable* hashTable, char* Name, int len);
void remove_hash(HashTable* hashTable, char* Name);
#else
// 本地变量（符号表）思考：每个方程有不同的符号表，故可以变量重名
typedef struct Obj Obj;
struct Obj {
  char* Name;        // 变量名
  int value;         // 哈希值，防止每次都调用 hash 函数
  struct Obj* next;  // 下一个对象
  int Offset;        // fp 偏移量
  Type *ty;          // 数据类型
  bool isFuncName;       // 记录变量为 全局变量 或 函数名
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
  Node* init;  // 存储初始化结点
  Obj* Var;    // 对应变量
  Type* ty;     // 数据类型
  Node* argus;  // 函数参数
  char* funcName;   // 对于函数调用，需要存储函数名
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
  Type* params;       //形参
  int StackSize;      //栈大小
  Type* FType;        //函数类型
  char* funcName;     //函数名
  Function* next;     //下一个函数
  bool isFunction;    //是函数还是全局变量初始化    
};

// 程序
typedef struct Program Program;
struct Program {
  Function* funcs;      // 程序的所有方程
  HashTable* globals;   // 程序全局变量
};

/************************ 函数声明 *************************/
// 报错信息
void verrorAt(char* loc, char* Fmt, va_list VA);
void errorAt(char* loc, char* Fmt, ...);
void errorTok(Token* token, char* Fmt, ...);
void error(char* Fmt, ...);

// 词法分析入口
Token* tokenize(char* p);
Token* skip(Token* token, char* str);
bool equal(Token* token, char* str);
bool startsWith(char* Str, char* SubStr);

// 语法分析入口
Program* parse(Token** rest, Token* token);

// 类型分析
Type* pointerTo(Type *base);
Type* funcType(Type* ty);
void addType(Node *node);
bool isInteger(Type *ty);
bool isPtr(Type *ty);
Type* copyType(Type* ty);
Type* arrayOf(Type* ty, int cnt);

// 汇编代码生成入口
void genCode(Program* root);