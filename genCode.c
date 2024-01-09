#include "rvcc.h"

// 栈深度
static int Depth = 0;

/****************** 函数声明 *******************/
static void Push(void);
static void Pop(char* reg);

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
static void genCode_re(Node* root){
    // 深度优先遍历DFS left -> right -> root
    Token* token_root = root->token; 
    if(token_root->kind == TK_NUM){
        printf("\tli a0,%d\n",token_root->val);
        return;   
    }
    
    // 当前为操作符，递归遍历
    if(!root->LNode)
        errorTok(token_root,"Juliet~,An operand is losed when generating code~");
    genCode_re(root->LNode);
    // 左操作数入栈
    Push();

    if(!root->RNode){
        // 可能为单元运算符 - 或 !，二者操作不一样
        Pop("a0");
        if(token_root->kind == TK_SUB){
            // neg a0, a0是sub a0, x0, a0的别名, 即a0=0-a0
            printf("\tneg a0,a0\n");
            return;
        }
        else if(token_root->kind == TK_NOT){
            // !操作为，若数值非零，则置为0，若为0，则置为1
            // set equal zero,sltui a0, a0, 1,小于1当然只有0了
            printf("  seqz a0, a0\n");
            return;
        }
            errorTok(token_root,"Juliet~,An operand is losed when generating code~");
    }
    else
        // 右操作数本身会在a0
        genCode_re(root->RNode);

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
        case TK_LST:
            // a1 < a0,slt:set less than:R[rd] = (R[rs1]<R[rs2])?1:0
            printf("\tslt a0,a1,a0\n");
            return;
        case TK_BGE:
            // a1 >= a0,即为slt取反
            printf("\tslt a0,a1,a0\n");
            // 取反不能用neg,-0 = 0
            printf("\txori a0,a0,1\n");
            return;
        case TK_BGT:
            // a1 > a0,将a0,a1换位即可
            printf("\tslt a0,a0,a1\n");
            return;
        case TK_LSE:
            // a1 <= a0,换位同BGE
            printf("\tslt a0,a0,a1\n");
            printf("\txori a0,a0,1\n");
            return;
        case TK_DEQ:
            // a1 == a0
            printf("\txor a0,a0,a1\n");
            // equal zero?sltiu a0,a0,1
            printf("\tseqz a0,a0\n");
            return;
        case TK_NEQ:
            printf("\txor a0,a1,a0\n");
            // not equal zero? sltu a0,x0,a0
            printf("\tsnez a0,a0\n");
            return;
        default:
            break;
    }
    error("apple~,we met invalid expression q_q");
}

void genCode(Node* root){
    printf("\t.global main\n");
    printf("main:\n");
    genCode_re(root);
    // 栈未清零则报错
    assert(Depth == 0);
    printf("\tret\n");
}