#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<stdarg.h>
#include "stdbool.h"
#include<string.h>
#include<assert.h>

/*************************** 全局变量 **************************/
// 输入的字符串
static char* currentInput;
// 栈深度
static int Depth = 0;


/************************ 数据结构声明 *************************/
//单词种别
typedef enum{
    TK_ADD,    // +
    TK_SUB,    // -
    TK_MUL,    // *
    TK_DIV,    // '/'
    TK_LBR,    // '('
    TK_RBR,    // ')'
    TK_NUM,    //数字
    TK_EOF     //终结符
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
};

// 表示状态的种类
typedef enum{
    ST_Expr,       //E
    ST_EPrim,      //E'
    ST_Mul,        //T
    ST_MPrim,      //T'
    ST_Primary,    //F
    ST_Head        //$栈顶 因为使用头插法
} StatusKind;

// 状态
typedef struct Status{
    StatusKind kind;
    Node* inherit;  //继承属性
    Node* ptr;      //自身所指向结点
    Node* system;   //综合属性

    // 递归则不需要保存栈
    // Status* next;   //下一个状态
}Status;

/************************ 函数声明 *************************/
// 报错信息
static void verrorAt(char* loc,char* Fmt,va_list VA);
static void errorAt(char* loc,char* Fmt,...);
static void errorTok(Token* token,char* Fmt,...);
static void error(char* Fmt,...);
// 生成函数
static Node* mkNode(Token* token,Node* left,Node* right);
static Node* mkLeaf(Token* token);
static Status* newStatus(StatusKind kind);
static Token* newToken(TokenKind kind,int val,char* loc,int len);
// 词法分析
static int specify_puntc(char* p);
static Token *tokenize();
static bool equal(Token* token, char* str);
static int getNumber(Token* token);
static Token* skip(Token* token,char* str);
// 语法分析递归调用
static Status* expr(Token** rest ,Token* token);
static Status* expr_Prime(Token** rest ,Token* token,Node* inherit);
static Status* mul(Token** rest ,Token* token);
static Status* mul_Prime(Token** rest ,Token* token,Node* inherit);
static Status* primary(Token** rest ,Token* token);
// 汇编代码生成
static void Push(void);
static void Pop(char* reg);
static void genCode(Node* root);
/************************** 主体部分 ***************************/
// 生成新的内结点
static Node* mkNode(Token* token,Node* left,Node* right){
    Node* node = calloc(1,sizeof(Node));
    node->token = token;
    node->LNode = left;
    node->RNode = right;
    return node;
}

// 生成新的叶子结点
static Node* mkLeaf(Token* token){
    Node* node = calloc(1,sizeof(Node));
    node->token = token;
    return node;
}

// 所用LL(1)文法
// E -> TE'
// E' -> +TE' | -TE' | NULL
// T -> FT'
// T' -> *FT' | NULL
// F -> (E) | num

// 新建一个状态
static Status* newStatus(StatusKind kind){
    Status* status = calloc(1,sizeof(Status));
    status->kind = kind;
    return status;
}


// 非递归的语法分析,发现LL(1)文法用栈其实无法解决本质问题
// 因为仅仅为生成产生式时有用，附加语义动作则需要在parse中同时进行
// so，回归课程中方法

// 性质：递归下降
// Expr = Mul (+ Mul | - Mul)*
// Mul = Primary (* Primary | / Primary)*
// Primary = (Expr) | num

// Expr -> Mul Expr'
// Expr' -> + Mul Expr1' | - Mul Expr1' | null
// Mul -> Primary Mul'
// Mul' -> * Primary Mul1' | / Primary Mul1' | null
// Primary -> num | (Expr)

// Expr -> Mul Expr'
static Status* expr(Token** rest ,Token* token){
    // 新建状态
    Status* expr = newStatus(ST_Expr);
    if(token->kind == TK_LBR || token->kind == TK_NUM || token->kind == TK_SUB || token->kind == TK_ADD){
        // printf("Expr -> Mul Expr'\n");
        // 递归进入Mul识别
        Status* mult = mul(rest,token);
        // 传递继承属性到Expr'识别
        Status* expr_P = expr_Prime(rest,*rest,mult->ptr);
        expr->ptr = expr_P->system;
        return expr;
    }
    else
        errorTok(token,"Dumpling~,it is not an expression... : )");
    return NULL;
}

// Expr' -> + Mul Expr1' | - Mul Expr1' | null
static Status* expr_Prime(Token** rest ,Token* token,Node* inherit){
    Status* expr_P = newStatus(ST_EPrim);
    switch (token->kind){
        // 识别到+ or -,Expr' -> + Mul Expr1' | - Mul Expr1'
        case TK_ADD:
        case TK_SUB:
            // printf("Expr' -> + Mul Expr1' | - Mul Expr1'\n");
            // 引入 rest 是为了能记录token链识别的位置
            *rest = token->next;
            Status* mult = mul(rest,*rest);
            // 此时mknode需要用到的token 为 + or -
            Status* expr_P2 = expr_Prime(rest,*rest,mkNode(token,inherit,mult->ptr));
            expr_P->system = expr_P2->system;
            return expr_P;
            break;
        // 识别到 ) or $ ,Expr' -> null
        case TK_RBR:
        case TK_EOF:
            // printf("Expr' -> null\n");
            expr_P->system = inherit;
            return expr_P;
        default:
            errorTok(token,"Sugar~~,I found some problem when reducing Addition/Subtraction");
            break;
    }
    return NULL;
}

// Mul -> Primary Mul'
static Status* mul(Token** rest ,Token* token){
    // 新建状态
    Status* mult = newStatus(ST_Mul);
    // 进入数字识别
    if(token->kind == TK_LBR || token->kind == TK_NUM || token->kind == TK_SUB || token->kind == TK_ADD){
        // printf("Mul -> Primary Mul'\n");
        Status* prim = primary(rest,token);
        Status* mult_P = mul_Prime(rest,*rest,prim->ptr);
        mult->ptr = mult_P->system;
        return mult;
    }
    else
        errorTok(token,"Soulmate~,May be not a multipler");
    return NULL;
}

// Mul' -> * Primary Mul1' | / Primary Mul1' | null
static Status* mul_Prime(Token** rest ,Token* token,Node* inherit){
    Status* mult_P = newStatus(ST_MPrim);
    switch (token->kind){
        // 识别到* or / ,Mul' -> * Primary Mul1' | / Primary Mul1'
        case TK_MUL:
        case TK_DIV:
            // printf("Mul' -> * Primary Mul1' | / Primary Mul1'\n");
            *rest = token->next;
            Status* prim = primary(rest,*rest);
            Status* mult_P2 = mul_Prime(rest,*rest,mkNode(token,inherit,prim->ptr));
            mult_P->system = mult_P2->system;
            return mult_P;
            break;
        // 识别到 ) or + or - or $,Mul' -> null
        case TK_RBR:
        case TK_ADD:
        case TK_SUB: 
        case TK_EOF:
            // printf("Mul' -> null\n");
            mult_P->system = inherit;
            return mult_P;
        default:
            errorTok(token,"Pookie~ ,I found some problem when reducing Multiplication/Division");
            break;
    }
    return NULL;
}

// Primary -> num | (Expr) | + Primary | - Primary
static Status* primary(Token** rest ,Token* token){
    Status* prim = newStatus(ST_Primary);
    switch (token->kind){
        case TK_LBR:
            // 识别到 Primary -> (Expr)
            // printf("Primary -> (Expr)\n");
            *rest = token->next; 
            token = token->next;
            Status* Expr = expr(rest,token);
            // 同时此时需要消耗 )
            token = *rest;
            if(token->kind == TK_RBR){
                *rest = (*rest)->next;
                token = token->next;
                prim->ptr = Expr->ptr;
                return prim;
            }
        case TK_NUM:
            // 识别到 Primary -> num
            // printf("Primary -> num\n");
            prim->ptr = mkLeaf(token);
            *rest = token->next;
            return prim;
        case TK_SUB:
            // 消耗 - 
            *rest = token->next;
            // 因为为从左到右遍历二叉树，所以需要先从左结点得到数值，再进行单元运算
            // 所以课程中的从右到左遍历二叉树好处就是，单运算符时，运算符在左结点
            prim->ptr = mkNode(token,primary(rest,*rest)->ptr,NULL);
            return prim;
        case TK_ADD:
            // 不需要genCode，只需要消耗+即可
            while (token->kind == TK_ADD){
                // 最终会在种别为TK_NUM时停止
                *rest = token->next;
                token = *rest;
            }
            return primary(rest,*rest);
        default:
            break;
    }
    // 非法的
    errorTok(token,"boo boo,a num is expected~");
    return NULL;
}

// 运行程序时参数出错
static void error(char* Fmt,...){
    va_list VA;
    va_start(VA,Fmt);
    vfprintf(stderr,Fmt,VA);
    fprintf(stderr,"\n");
    va_end(VA);
    exit(1);
}

// 输出错误出现位置，并退出
static void verrorAt(char* loc,char* Fmt,va_list VA){
    // 输出源信息
    fprintf(stderr,"%s\n",currentInput);

    // 输出出错信息
    // 计算出错位置，loc为出错位置指针，currentInput为当前输入首地址
    int pos = loc - currentInput;
    // 将字符串补齐pos位，%*s允许传递一个整数参数输出相应长度空格:)
    fprintf(stderr,"%*s",pos,"");
    fprintf(stderr,"^ ");
    // 报错信息
    vfprintf(stderr,Fmt,VA);
    fprintf(stderr,"\n");
    va_end(VA);
    exit(1);
}

// 分词(tokenize)时出错
static void errorAt(char* loc,char* Fmt,...){
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(loc,Fmt,VA);
}

// 语法分析出错
static void errorTok(Token* token,char* Fmt,...){
    va_list VA;
    va_start(VA,Fmt);
    verrorAt(token->loc,Fmt,VA);
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

// 确定究竟为哪一种punctuation
static int specify_puntc(char* p){
    switch (*p)
    {
    case '+':
        return TK_ADD;
        break;
    case '-':
        return TK_SUB;
        break;
    case '*':
        return TK_MUL;
        break;
    case '/':
        return TK_DIV;
        break;
    case '(':
        return TK_LBR;
        break;
    case ')':
        return TK_RBR;
        break;
    default:
        errorAt(p,"Darling T.T ~~ I can't deal with this punctuation");
    }
    return 1;
}
// 终结符解析
static Token *tokenize(){
    // 使用一个链表进行存储各个Token
    // head表示链表头
    char* p = currentInput;
    // 也可以用 Token head = {}; 相当于赋初值全为0
    // Token* head = newToken(2,0,0,0);
    Token head = {};
    Token* cur = &head;
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
        else if (ispunct(*p)){
            // 识别运算符
            cur->next = newToken(specify_puntc(p),0,p,1);
            cur = cur->next;
            p++;
            continue;
        }
        else{
            // 识别到非法字符
            errorAt(p,"Baby~,invalid input");
        }
    }
    cur->next = newToken(TK_EOF,0,p,0);

    // 因为头节点不存储信息故返回head->next
    return head.next;
}

// 判断token值是否为指定值
static bool equal(Token* token, char* str){
    // LHS为左字符串，RHS为右字符串
    // memcmp 按照字典序比较 LHS<RHS返回负值，=返回0,>返回正值
    return memcmp(token->loc,str,token->len) == 0 && str[token->len] == '\0';
}

// 期待得到一个数字，否则报错
static int getNumber(Token* token){
    if(token->kind != TK_NUM){
        errorTok(token,"Honey~,here expecte a number");
    }
    else return token->val;
    return 1;
}

// skip期待得到指定符号
static Token* skip(Token* token,char* str){
    if(!equal(token,str))
        errorTok(token,"Sweety~,expected %s",str);
    return token->next;
}

// 将a0中值压入sp栈中
static void Push(void){
    // riscv64压栈为8个字节
    printf("\taddi sp,sp,-8\n");
    printf("\tsd a0,0(sp)\n");
    Depth++;
}

// 弹出sp栈中值到reg中
static void Pop(char* reg){
    printf("\tld %s,0(sp)\n",reg);
    printf("\taddi sp,sp,8\n");
    Depth--;
}

// 代码生成：遍历结点
static void genCode(Node* root){
    // 深度优先遍历DFS left -> right -> root
    Token* token_root = root->token; 
    if(token_root->kind == TK_NUM){
        printf("\tli a0,%d\n",token_root->val);
        return;   
    }
    
    // 当前为操作符，递归遍历
    if(!root->LNode)
        errorTok(token_root,"Juliet~,An operand is losed when generating code~");
    genCode(root->LNode);
    // 左操作数入栈
    Push();

    // 右操作数本身会在a0
    if(!root->RNode){
        // 可能为单元运算符 -
        if(token_root->kind == TK_SUB){
            // 数据存入a1
            Pop("a1");
            printf("\tsub a0,x0,a1\n");
            return;
        }
        else
            errorTok(token_root,"Juliet~,An operand is losed when generating code~");
    }
    else
        genCode(root->RNode);

    // 将左结点值pop至a1中
    Pop("a1");
    switch (token_root->kind){
        case TK_ADD:
            printf("\tadd a0,a1,a0\n");
            return;
        case TK_SUB:
            printf("\tsub a0,a1,a0\n");
            return;
        case TK_MUL:
            printf("\tmul a0,a1,a0\n");
            return;
        case TK_DIV:
            printf("\tdiv a0,a1,a0\n");
            return;
        default:
            break;
    }
    error("apple~,we met invalid expression q_q");
}

int main(int argc,char** argv){
    // argv[0]为程序名称，argv[1]为传入第一个参数...依此类推
    if(argc != 2){
        // 参数异常处理
        error("%s:  invalid number of arguments",argv[0]);
    }
    // 分词
    currentInput = argv[1];
    Token* token = tokenize();
    // 语法分析
    // 因为rest是用来记录的，所以需要新的指针
    Token** rest = &token;
    Status* expretion = expr(rest,token);
    assert((*rest)->kind == TK_EOF);
    // 代码生成
    printf("\t.global main\n");
    printf("main:\n");
    genCode(expretion->ptr);
    // 栈未清零则报错
    assert(Depth == 0);
    printf("\tret\n");
    return 0;
}

