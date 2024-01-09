#include "rvcc.h"

// 输入的字符串
static char* currentInput;

/**************************** 函数调用**************************/
static bool equal(Token* token, char* str);
static int getNumber(Token* token);
static Token* newToken(TokenKind kind,int val,char* loc,int len);
static int specify_puntc(char* p);

/************************** 错误处理 **************************/
// 运行程序时参数出错
void error(char* Fmt,...){
    va_list VA;
    va_start(VA,Fmt);
    vfprintf(stderr,Fmt,VA);
    fprintf(stderr,"\n");
    va_end(VA);
    exit(1);
}

// 输出错误出现位置，并退出
void verrorAt(char* loc,char* Fmt,va_list VA){
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
void errorAt(char* loc,char* Fmt,...){
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(loc,Fmt,VA);
}

// 语法分析出错
void errorTok(Token* token,char* Fmt,...){
    va_list VA;
    va_start(VA,Fmt);
    verrorAt(token->loc,Fmt,VA);
}

/********************* 分词 ***********************/
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
    switch (*p){
    case '+':
        return TK_ADD;
    case '-':
        return TK_SUB;
    case '*':
        return TK_MUL;
    case '/':
        return TK_DIV;
    case '(':
        return TK_LBR;
    case ')':
        return TK_RBR;
    case '!':
        if(*(p+1) == '=')
            return TK_NEQ;
        else 
            return TK_NOT;
    case '=':
        if(*(p+1) == '=')
            return TK_DEQ;
        else
            errorAt(p,"Beauty~,Now I only able to deal with == ~");
    case '>':
        if(*(p+1) == '=')
            return TK_BGE;
        else 
            return TK_BGT;
    case '<':
        if(*(p+1) == '=')
            return TK_LSE;
        else    
            return TK_LST;
    case ';':
        return TK_SEM;
    default:
        errorAt(p,"Darling T.T ~~ I can't deal with this punctuation");
    }
    return 1;
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
Token* skip(Token* token,char* str){
    if(!equal(token,str))
        errorTok(token,"Sweety~,expected %s",str);
    return token->next;
}

// 终结符解析
Token* tokenize(char* p){
    // 使用一个链表进行存储各个Token
    // head表示链表头
    currentInput = p;
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
            int tk_kind = specify_puntc(p);
            int len = 1;
            switch (tk_kind){
                case TK_DEQ:
                case TK_NEQ:
                case TK_BGE:
                case TK_LSE:
                    len = 2;
                    break;
                default:
                    break;
            }
            cur->next = newToken(tk_kind,0,p,len);
            cur = cur->next;
            p+=cur->len;
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