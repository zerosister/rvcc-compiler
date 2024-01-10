#include "rvcc.h"

// 栈深度
static int Depth = 0;

// 将a0中值压入sp栈中
static void Push(void){
    // riscv64压栈为8个字节
    printf("\taddi sp,sp,-8\t#push\n");
    printf("\tsd a0,0(sp)\n");
    Depth++;
}

// 弹出sp栈中值到reg中
static void Pop(char* reg){
    printf("\tld %s,0(sp)\t#pop\n",reg);
    printf("\taddi sp,sp,8\n");
    Depth--;
}

// 对齐到align的整数倍
static int alignTo(int n, int align){
    // (0,align]返回align
    return (n + align - 1) / align * align;
}

// 计算给定结点的绝对地址
// 若报错，说明结点不在内存
static void genAddr(Node* node){
    if (node->token->kind == TK_VAR){
        // 偏移量相对于fp
        printf("\taddi a0,fp,%d\t#store var\n",node->Var->Offset);
        return;
    }
    error("Da Zhang Wei says: not a variable");
}

// 代码生成：遍历结点
static void genCode_re(Node* root){
    // 深度优先遍历DFS right -> left -> root
    Token* token_root = root->token; 
    if(token_root->kind == TK_NUM){
        printf("\tli a0,%d\t#load num\n",token_root->val);
        return;   
    }
    if (token_root->kind == TK_VAR){
        // 将变量地址存入a0
        genAddr(root);
        // 将变量load至a0
        printf("\tld a0,0(a0)\n");
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
        case TK_ASS:
            // 右值当前在a0，需要移动到a1
            printf("\tmv a1,a0\n");
            // 将左结点地址放入a0寄存器
            genAddr(root->LNode);
            // 将右值放入左结点变量中
            printf("\tsd a1,0(a0)\n");
            // 因为最后有返回值，即右结点值
            printf("\tmv a0,a1\n");
            return;
        default:
            break;
    }
    error("apple~,we met invalid expression q_q");
}

// 根据变量链表计算出偏移量
static void assignVarOffsets(Function *prog){
    int offset = 0;
#if USE_HASH
    // 读取所有变量
    for (int i=0; i<HASH_SIZE; i++){
        Obj* var = prog->Locals->objs[i];
        while (var){
            offset += 8;
            var->Offset = -offset;
            var = var->next;
        }
    }
#else
    // 读取所有变量
    for (Obj* var=prog->Locals->locals; var; var = var->Next){
        // 每个变量分配8字节
        offset += 8;
        // 记录下每个变量的栈中地址
        var->Offset = -offset;
    }
#endif
    // 将栈对齐到16字节
    prog->StackSize = alignTo(offset,16);
}

void genCode(Function* prog){
    // 每条语句一个结点，需要遍历每条语句
    Node* stmt = prog->Body;
    assignVarOffsets(prog);
    printf("\t.global main\n");
    printf("main:\n");
    
    // 栈布局
    //-------------------------------// sp
    //              fp
    //-------------------------------// fp = sp-8
    //             变量
    //-------------------------------// sp = sp-8-StackSize
    //           表达式计算
    //-------------------------------//
    
    // pre process
    // 将fp指针压栈,此时fp应当为上一级值
    printf("\taddi sp,sp,-8\n");
    printf("\tsd fp,0(sp)\n");
    // 将fp置为当前sp值
    printf("\tmv fp,sp\n");
    printf("\taddi sp,sp,-%d\n",prog->StackSize);
    while (stmt){
        genCode_re(stmt);
        stmt = stmt->next;   
    }

    // post process
    // 将栈复原
    printf("\taddi sp,sp,%d\n",prog->StackSize);
    // 恢复fp
    printf("\tld fp,0(sp)\n");
    printf("\taddi sp,sp,8\n");
    // 栈未清零则报错
    assert(Depth == 0);
    printf("\tret\n");
}