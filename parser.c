#include "rvcc.h"

// 符号表，hash_Table 实现形式
HashTable* hashTable = NULL;

// 非递归的语法分析，发现 LL(1) 文法用栈其实无法解决本质问题
// 因为仅仅为生成产生式时有用，附加语义动作则需要在 parse 中同时进行
// so，回归课程中方法
// 性质：递归下降

// program = Compound
// Compound = { stmt* }
// stmt -> return exprStmt | exprStmt | Compund
// exprStmt = expr? ;
// 暂时 expr = assign
// Assign -> Equa Assign'
// Assign' -> = Equa Assign | null
// Equa -> ! Equa | Rela Equa'
// Equa' -> == Rela Equa'  | != Rela Equa' | null
// Rela -> Add Rela'
// Rela' -> < Add Rela' | > Add Rela' | <= Add Rela' | >= Add Rela' | null
// Add -> Mul Add'
// Add' -> + Mul Add1' | - Mul Add1' | null
// Mul -> Primary Mul'
// Mul' -> * Primary Mul1' | / Primary Mul1' | null
// Primary -> num | (expr)

static Node* compound(Token** rest, Token* token);
static Node* stmt(Token** rest, Token* token);
static Node* exprStmt(Token** rest, Token* token);
static Status* expr(Token** rest, Token* token);
static Status* assign(Token** rest, Token* token);
static Status* assign_Prime(Token** rest, Token* token, Node* inherit);
static Status* equation(Token** rest, Token* token);
static Status* equation_Prime(Token** rest, Token* token, Node* inherit);
static Status* rela(Token** rest, Token* token);
static Status* rela_Prime(Token** rest, Token* token, Node* inherit);
static Status* add(Token** rest, Token* token);
static Status* add_Prime(Token** rest, Token* token, Node* inherit);
static Status* mul(Token** rest, Token* token);
static Status* mul_Prime(Token** rest, Token* token, Node* inherit);
static Status* primary(Token** rest, Token* token);

static HashTable* getHashTable() {
  if (hashTable == NULL) hashTable = calloc(1, sizeof(HashTable));
  return hashTable;
}

#if USE_HASH
// hash 方法查找本地变量（符号）
static Obj* findVar(Token* token) {
  hashTable = getHashTable();
  return search(hashTable, token->loc, token->len);
}

// hash 方法新增变量至符号表，而变量只会从 token 中产生
static Obj* newVar(Token* token) {
  Obj* var = findVar(token);
  if (var == NULL) {
    // 未找到，新建 Obj
    var = insert(getHashTable(), token->loc, token->len);
  }
  return var;
}

#else

// 按照名称查找本地变量（符号）
static Obj* findVar(Token* token) {
  HashTable* hashTable = getHashTable();
  if (!hashTable->locals) {
    hashTable->locals = calloc(1, sizeof(Obj));
  }
  Obj* locals = hashTable->locals;
  if (locals == NULL) {
    return NULL;
  }
  Obj* cmp = locals->Next;
  while (cmp && !(startsWith(token->loc, cmp->Name) &&
      (token->len == strlen(cmp->Name)))) {
    cmp = cmp->Next;
  }
  if (cmp && startsWith(token->loc, cmp->Name) &&
      (token->len == strlen(cmp->Name)))
    return cmp;
  return NULL;
}

// 新增变量到符号表中
static Obj* newVar(Token* token) {
  // 首先检查是否已在符号表中 (性能优化？哈希表？)
  Obj* var = findVar(token);
  Obj* locals = getHashTable()->locals;
  if (var == NULL) {
    var = calloc(1, sizeof(Obj));
    var->Name = strndup(token->loc, token->len);
    // 头插法插入符号表
    if (locals->Next) {
      var->Next = locals->Next;
    }
    locals->Next = var;
  }
  return var;
}
#endif

// 生成新的内结点
static Node* mkNode(Token* token, Node* left, Node* right) {
  Node* node = calloc(1, sizeof(Node));
  node->token = token;
  node->LNode = left;
  node->RNode = right;
  return node;
}

// 生成新的叶子结点
static Node* mkLeaf(Token* token) {
  Node* node = calloc(1, sizeof(Node));
  node->token = token;
  return node;
}

// 生成新的变量结点
static Node* mkVarNode(Token* token) {
  Node* node = mkLeaf(token);
  node->Var = newVar(token);
  return node;
}

// 生成新的块结点，其中 token 为 {, body 为 {} 内部语句
static Node* mkBlockNode(Token* token, Node* body) {
  Node* node = mkLeaf(token);
  node->body = body;
  return node;
}
// 新建一个状态
static Status* newStatus(StatusKind kind) {
  Status* status = calloc(1, sizeof(Status));
  status->kind = kind;
  return status;
}

// Compound = { stmt* } 
static Node* compound(Token** rest, Token* token) {
  *rest = skip(token, "{");
  Node head = {};
  Node* cur = &head;
  // 当 token 为 "}" 时停下
  while ((*rest)->kind != TK_RBB) {
    cur->next = stmt(rest, *rest);
    // 由于可识别空语句 cur->next 可能为 NULL
    if (cur->next) cur = cur->next; 
  }
  *rest = skip(*rest, "}");
  // 返回构建的 compound 结点
  return mkBlockNode(token, head.next);
}

// stmt -> return exprStmt | exprStmt | Compund
static Node* stmt(Token** rest, Token* token) {
  if (token->kind == TK_RET) {
    *rest = token->next;
    Node* exprS = exprStmt(rest, *rest);
    return mkNode(token, exprS, NULL);
  }
  if (token->kind == TK_LBB) {
    return compound(rest, *rest);
  }
  return exprStmt(rest, token);
}

// exprStmt = expr? ;
static Node* exprStmt(Token** rest, Token* token) {
  if (token->kind == TK_SEM) {
    // 表示为空语句，吸收 ;
    *rest = token->next;
    // 在 parser 中处理空语句，避免建立过多结点
    return NULL;
  }
  Node* root = assign(rest, token)->ptr;
  *rest = skip(*rest, ";");
  return root;
}

// 暂时 expr = assign
static Status* expr(Token** rest, Token* token) { return assign(rest, token); }

// Assign -> Equa Assign'
static Status* assign(Token** rest, Token* token) {
  Status* ass = newStatus(ST_Assign);
  switch (token->kind) {
    case TK_LBR:
    case TK_NUM:
    case TK_ADD:
    case TK_SUB:
    case TK_NOT:
    case TK_VAR:
      // Assign -> Equa Assign'
      Status* equa = equation(rest, token);
      Status* ass_P = assign_Prime(rest, *rest, equa->ptr);
      ass->ptr = ass_P->system;
      return ass;
    default:
      errorTok(token, "Emily~,Something wrong when reducing an assignment");
  }
}

// Assign' -> = Assign | null
static Status* assign_Prime(Token** rest, Token* token, Node* inherit) {
  Status* ass_P = newStatus(ST_AssPrim);
  switch (token->kind) {
    case TK_ASS:
      // Assign' -> = Assign
      *rest = token->next;
      // 其他单变元语句为了遍历方便，是把数值放在了右部，
      Status* ass = assign(rest, *rest);
      ass_P->system = mkNode(token, inherit, ass->ptr);
      return ass_P;
    case TK_RBR:
    case TK_SEM:
    case TK_EOF:
      // Assign' -> = null
      ass_P->system = inherit;
      return ass_P;
    default:
      errorTok(token, "Ava~,maybe we need an '='");
  }
}
// Equa -> ! Equa | Rela Equa'
static Status* equation(Token** rest, Token* token) {
  Status* equa = newStatus(ST_Equa);
  switch (token->kind) {
    case TK_LBR:
    case TK_NUM:
    case TK_ADD:
    case TK_SUB:
    // 新增变量等价于 NUM
    case TK_VAR:
      // Equa -> Rela Equa'
      Status* relation = rela(rest, token);
      Status* equa_P = equation_Prime(rest, *rest, relation->ptr);
      equa->ptr = equa_P->system;
      return equa;
    case TK_NOT:
      // Equa -> ! Equa，消耗 !
      *rest = token->next;
      equa->ptr = mkNode(token, equation(rest, *rest)->ptr, NULL);
      return equa;
    default:
      errorTok(token, "Lily~,Something wrong when reducing equation");
  }
}

// Equa' -> == Rela Equa'  | != Rela Equa' | null
static Status* equation_Prime(Token** rest, Token* token, Node* inherit) {
  Status* equa_P = newStatus(ST_EqPrim);
  switch (token->kind) {
    case TK_DEQ:
    case TK_NEQ:
      // Equa' -> == Rela Equa' | != Rela Equa'
      *rest = token->next;
      Status* relat = rela(rest, *rest);
      Status* equa_P2 =
          equation_Prime(rest, *rest, mkNode(token, inherit, relat->ptr));
      equa_P->system = equa_P2->system;
      return equa_P;
    case TK_RBR:
    case TK_SEM:
    case TK_ASS:
    case TK_EOF:  //加入是为了防止编译器在此时即报错，需要等到 expression
                  //需要;时再报错
      // Equa' -> null
      equa_P->system = inherit;
      return equa_P;
    default:
      errorTok(token, "Superman! Something wrong with semantic~");
  }
  return NULL;
}

// Rela -> E Rela'
static Status* rela(Token** rest, Token* token) {
  Status* relat = newStatus(ST_Rela);
  switch (token->kind) {
    case TK_LBR:
    case TK_NUM:
    case TK_ADD:
    case TK_SUB:
    case TK_VAR:
      // Rela -> E Rela'
      Status* addition = add(rest, token);
      Status* relat_P = rela_Prime(rest, *rest, addition->ptr);
      relat->ptr = relat_P->system;
      return relat;
    default:
      errorTok(token, "Spiderman!,it is not a relation equation~");
  }
  return NULL;
}

// Rela' -> < E Rela' | > E Rela' | <= E Rela' | >= E Rela' | null
static Status* rela_Prime(Token** rest, Token* token, Node* inherit) {
  Status* realt_P = newStatus(ST_RePrim);
  switch (token->kind) {
    case TK_LST:
    case TK_BGT:
    case TK_BGE:
    case TK_LSE:
      // Rela' -> < E Rela' | > E Rela' | <= E Rela' | >= E Rela'
      *rest = token->next;
      Status* addition = add(rest, *rest);
      Status* relat_P2 =
          rela_Prime(rest, *rest, mkNode(token, inherit, addition->ptr));
      realt_P->system = relat_P2->system;
      return realt_P;
    case TK_DEQ:
    case TK_NEQ:
    case TK_SEM:
    case TK_RBR:
    case TK_ASS:
    case TK_EOF:  //加入是为了防止编译器在此时即报错，需要等到 expression
                  //需要;时再报错
      // Rela' -> null
      realt_P->system = inherit;
      return realt_P;
    default:
      errorTok(token, "Batman!,it is not a relation equation~");
  }
  return NULL;
}

// Add -> Mul Add'
static Status* add(Token** rest, Token* token) {
  // 新建状态
  Status* add = newStatus(ST_Add);
  switch (token->kind) {
    case TK_LBR:
    case TK_NUM:
    case TK_ADD:
    case TK_SUB:
    case TK_VAR:
      // 递归进入 Mul 识别
      Status* mult = mul(rest, token);
      // 传递继承属性到 Add'识别
      Status* add_P = add_Prime(rest, *rest, mult->ptr);
      add->ptr = add_P->system;
      return add;
    default:
      errorTok(token, "Dumpling~,it is not an addition... : )");
  }
}

// Add' -> + Mul Add1' | - Mul Add1' | null
static Status* add_Prime(Token** rest, Token* token, Node* inherit) {
  Status* add_P = newStatus(ST_AddPrim);
  switch (token->kind) {
    // 识别到 + or -,Add' -> + Mul Add1' | - Mul Add1'
    case TK_ADD:
    case TK_SUB:
      // printf("Add' -> + Mul Add1' | - Mul Add1'\n");
      // 引入 rest 是为了能记录 token 链识别的位置
      *rest = token->next;
      Status* mult = mul(rest, *rest);
      // 此时 mknode 需要用到的 token 为 + or -
      Status* add_P2 =
          add_Prime(rest, *rest, mkNode(token, inherit, mult->ptr));
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
    case TK_ASS:
    case TK_EOF:  //加入是为了防止编译器在此时即报错，需要等到 expression
                  //需要;时再报错
      // Follow(E') 下 Add' -> null
      add_P->system = inherit;
      return add_P;
    default:
      errorTok(
          token,
          "Sugar~~,I found some problem when reducing Addition/Subtraction");
      break;
  }
  return NULL;
}

// Mul -> Primary Mul'
static Status* mul(Token** rest, Token* token) {
  // 新建状态
  Status* mult = newStatus(ST_Mul);
  // 进入数字识别
  switch (token->kind) {
    case TK_LBR:
    case TK_NUM:
    case TK_ADD:
    case TK_SUB:
    case TK_VAR:
      Status* prim = primary(rest, token);
      Status* mult_P = mul_Prime(rest, *rest, prim->ptr);
      mult->ptr = mult_P->system;
      return mult;
    default:
      errorTok(token, "Soulmate~,May be not a multipler");
  }
}

// Mul' -> * Primary Mul1' | / Primary Mul1' | null
static Status* mul_Prime(Token** rest, Token* token, Node* inherit) {
  Status* mult_P = newStatus(ST_MPrim);
  switch (token->kind) {
    // 识别到* or / ,Mul' -> * Primary Mul1' | / Primary Mul1'
    case TK_MUL:
    case TK_DIV:
      // printf("Mul' -> * Primary Mul1' | / Primary Mul1'\n");
      *rest = token->next;
      Status* prim = primary(rest, *rest);
      Status* mult_P2 =
          mul_Prime(rest, *rest, mkNode(token, inherit, prim->ptr));
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
    case TK_ASS:
    case TK_EOF:  //加入是为了防止编译器在此时即报错，需要等到 expression
                  //需要;时再报错
      // printf("Mul' -> null\n");
      mult_P->system = inherit;
      return mult_P;
    default:
      errorTok(token,
               "Pookie~ ,I found some problem when reducing "
               "Multiplication/Division");
      break;
  }
  return NULL;
}

// Primary -> num | (Expr) | + Primary | - Primary
static Status* primary(Token** rest, Token* token) {
  Status* prim = newStatus(ST_Primary);
  switch (token->kind) {
    case TK_LBR:
      // 识别到 Primary -> (Equa)
      *rest = token->next;
      token = token->next;
      Status* equa = expr(rest, token);
      // 同时此时需要消耗 )
      token = *rest;
      *rest = skip(token, ")");
      prim->ptr = equa->ptr;
      return prim;
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
      prim->ptr = mkNode(token, primary(rest, *rest)->ptr, NULL);
      return prim;
    case TK_ADD:
      // 不需要 genCode，只需要消耗 + 即可
      while (token->kind == TK_ADD) {
        // 最终会在种别为 TK_NUM 时停止
        *rest = token->next;
        token = *rest;
      }
      return primary(rest, *rest);
    case TK_VAR:
      // 识别到变量
      prim->ptr = mkVarNode(token);
      *rest = token->next;
      return prim;
    default:
      break;
  }
  // 非法的
  errorTok(token, "boo boo,a num is expected~");
  return NULL;
}

// program = compound
Function* parse(Token** rest, Token* token) {
  Node* body = compound(rest, token);

  // 函数体存储语句的 AST，locals 存储变量
  Function* prog = calloc(1, sizeof(Function));
  prog->Body = body;
  prog->Locals = getHashTable();
  return prog;
}