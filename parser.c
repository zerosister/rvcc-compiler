#include "rvcc.h"

// 函数调用 
static Node* stmt(Token** rest,Token* token);
static Node* exprStmt(Token** rest,Token* token);
static Status* equation(Token** rest,Token* token);
static Status* equation_Prime(Token** rest,Token* token,Node* inherit);
static Status* rela(Token** rest,Token* token);
static Status* rela_Prime(Token** rest,Token* token,Node* inherit);
static Status* add(Token** rest ,Token* token);
static Status* add_Prime(Token** rest ,Token* token,Node* inherit);
static Status* mul(Token** rest ,Token* token);
static Status* mul_Prime(Token** rest ,Token* token,Node* inherit);
static Status* primary(Token** rest ,Token* token);
static Node* mkNode(Token* token,Node* left,Node* right);
static Node* mkLeaf(Token* token);
static Status* newStatus(StatusKind kind);

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

// program = stmt*
// stmt = exprStmt
// exprStmt = expr ";"
// 暂时 expr = Equa
// Equa -> ! Equa | Rela Equa' 
// Equa' -> == Rela Equa'  | != Rela Equa' | null
// Rela -> E Rela'   
// Rela' -> < E Rela' | > E Rela' | <= E Rela' | >= E Rela' | null   
// Add -> Mul Add'
// Add' -> + Mul Add1' | - Mul Add1' | null
// Mul -> Primary Mul'
// Mul' -> * Primary Mul1' | / Primary Mul1' | null
// Primary -> num | (Equa)

static Node* stmt(Token** rest,Token* token){
    // 仅支持表达式语句
    return exprStmt(rest,token);
}

static Node* exprStmt(Token** rest,Token* token){
    Node* root = equation(rest,token)->ptr;
    if((*rest)->kind == TK_SEM){
        *rest = (*rest)->next;
        return root;
    }
    else 
        skip(*rest,";");
}

// Equa -> ! Equa | Rela Equa' 
static Status* equation(Token** rest,Token* token){
    Status* equa = newStatus(ST_Equa);
    switch (token->kind){
        case TK_LBR:
        case TK_NUM:
        case TK_ADD:
        case TK_SUB:
            // Equa -> Rela Equa'
            Status* relation = rela(rest,token);
            Status* equa_P = equation_Prime(rest,*rest,relation->ptr);
            equa->ptr = equa_P->system;
            return equa; 
        case TK_NOT:
            // Equa -> ! Equa,消耗 !
            *rest = token->next;
            equa->ptr = mkNode(token,equation(rest,*rest)->ptr,NULL);
            return equa;
        default:
            break;
    }
}

// Equa' -> == Rela Equa'  | != Rela Equa' | null
static Status* equation_Prime(Token** rest,Token* token,Node* inherit){
    Status* equa_P = newStatus(ST_EqPrim);
    switch (token->kind){
        case TK_DEQ:
        case TK_NEQ:
            // Equa' -> == Rela Equa' | != Rela Equa' 
            *rest = token->next;
            Status* relat = rela(rest,*rest);
            Status* equa_P2 = equation_Prime(rest,*rest,mkNode(token,inherit,relat->ptr));
            equa_P->system = equa_P2->system;
            return equa_P;
        case TK_RBR:
        case TK_SEM:
        case TK_EOF:    //加入是为了防止编译器在此时即报错，需要等到expression 需要;时再报错
            // Equa' -> null
            equa_P->system = inherit;
            return equa_P;
        default:
            errorTok(token,"Superman! Something wrong with semantic~");
    }
    return NULL;
}

// Rela -> E Rela'  
static Status* rela(Token** rest,Token* token){
    Status* relat = newStatus(ST_Rela);
    switch (token->kind){
        case TK_LBR:
        case TK_NUM:
        case TK_ADD:
        case TK_SUB:
            // Rela -> E Rela'  
            Status* addition = add(rest,token);
            Status* relat_P = rela_Prime(rest,*rest,addition->ptr);
            relat->ptr = relat_P->system;
            return relat;
        default:
            errorTok(token,"Spiderman!,it is not a relation equation~");
    }
    return NULL;
}

// Rela' -> < E Rela' | > E Rela' | <= E Rela' | >= E Rela' | null
static Status* rela_Prime(Token** rest,Token* token,Node* inherit){
    Status* realt_P = newStatus(ST_RePrim);
    switch (token->kind){
        case TK_LST:
        case TK_BGT:
        case TK_BGE:
        case TK_LSE:
            // Rela' -> < E Rela' | > E Rela' | <= E Rela' | >= E Rela'
            *rest = token->next;
            Status* addition = add(rest,*rest);
            Status* relat_P2 = rela_Prime(rest,*rest,mkNode(token,inherit,addition->ptr));
            realt_P->system = relat_P2->system;
            return realt_P;
        case TK_DEQ:
        case TK_NEQ:
        case TK_SEM:
        case TK_RBR:
        case TK_EOF:    //加入是为了防止编译器在此时即报错，需要等到expression 需要;时再报错
            // Rela' -> null
            realt_P->system = inherit;
            return realt_P;
        default:
            errorTok(token,"Batman!,it is not a relation equation~");
    }
    return NULL;
}

// Add -> Mul Add'
static Status* add(Token** rest ,Token* token){
    // 新建状态
    Status* add = newStatus(ST_Add);
    if(token->kind == TK_LBR || token->kind == TK_NUM || token->kind == TK_SUB || token->kind == TK_ADD){
        // printf("Add -> Mul Add'\n");
        // 递归进入Mul识别
        Status* mult = mul(rest,token);
        // 传递继承属性到Add'识别
        Status* add_P = add_Prime(rest,*rest,mult->ptr);
        add->ptr = add_P->system;
        return add;
    }
    else
        errorTok(token,"Dumpling~,it is not an addition... : )");
    return NULL;
}

// Add' -> + Mul Add1' | - Mul Add1' | null
static Status* add_Prime(Token** rest ,Token* token,Node* inherit){
    Status* add_P = newStatus(ST_AddPrim);
    switch (token->kind){
        // 识别到+ or -,Add' -> + Mul Add1' | - Mul Add1'
        case TK_ADD:
        case TK_SUB:
            // printf("Add' -> + Mul Add1' | - Mul Add1'\n");
            // 引入 rest 是为了能记录token链识别的位置
            *rest = token->next;
            Status* mult = mul(rest,*rest);
            // 此时mknode需要用到的token 为 + or -
            Status* add_P2 = add_Prime(rest,*rest,mkNode(token,inherit,mult->ptr));
            add_P->system = add_P2->system;
            return add_P;
        case TK_RBR:
        case TK_SEM:
        case TK_LST:
        case TK_LSE:
        case TK_BGE:
        case TK_BGT:
        case TK_NEQ:
        case TK_DEQ:
        case TK_EOF:    //加入是为了防止编译器在此时即报错，需要等到expression 需要;时再报错
            // Follow(E')下 Add' -> null
            add_P->system = inherit;
            return add_P;
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
        case TK_SEM:
        case TK_LST:
        case TK_LSE:
        case TK_BGE:
        case TK_BGT:
        case TK_NEQ:
        case TK_DEQ:
        case TK_ADD:
        case TK_SUB:
        case TK_EOF:    //加入是为了防止编译器在此时即报错，需要等到expression 需要;时再报错 
            // printf("Mul' -> null\n");
            mult_P->system = inherit;
            return mult_P;
        default:
            errorTok(token,"Pookie~ ,I found some problem when reducing Multiplication/Division");
            break;
    }
    return NULL;
}

// Primary -> num | (Equa) | + Primary | - Primary
static Status* primary(Token** rest ,Token* token){
    Status* prim = newStatus(ST_Primary);
    switch (token->kind){
        case TK_LBR:
            // 识别到 Primary -> (Equa)
            *rest = token->next; 
            token = token->next;
            Status* equa = equation(rest,token);
            // 同时此时需要消耗 )
            token = *rest;
            if(token->kind == TK_RBR){
                *rest = (*rest)->next;
                token = token->next;
                prim->ptr = equa->ptr;
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

// program = stmt*
Node* parse(Token** rest,Token* token){
    // 程序为语句链表
    Node head = {};
    Node* cur = &head;
    while ((*rest)->kind!=TK_EOF){
        cur->next = stmt(rest,*rest);
        cur = cur->next;
    }
    return cur;
}