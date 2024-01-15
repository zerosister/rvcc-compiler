#include "rvcc.h"

// 符号表，hash_Table 实现形式
HashTable* hashTable = NULL;

// 自行构建 token 变量
static Token multiplyToken = {TK_MUL};
static Token divisionToken = {TK_DIV};

// 非递归的语法分析，发现 LL(1) 文法用栈其实无法解决本质问题
// 因为仅仅为生成产生式时有用，附加语义动作则需要在 parse 中同时进行
// so，回归课程中方法
// 性质：递归下降

// program = Compound
// Compound = { (Decla | Stmt)* }
// Decla = 
//        Declspec (Declarator ('=' expr)? (',' Declarator ('=' expr)?)*)? ';'
// Declspec: 数据类型
// Declarator: '*'* ident  可以为多重指针
// Stmt -> return ExprStmt
//        | ExprStmt
//        | if '(' Expr ')' Stmt (else Stmt)?
//        | for '(' ExprStmt Expr? ; Expr? ')' Stmt
//        | "while" "(" expr ")" stmt
//        | Compound
// ExprStmt = Expr? ;
// 暂时 Expr = assign
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
// Primary -> num | (Expr) | + Primary | - Primary | * Primary | & Primary | Var ('(' ')')?

static Node* compound(Token** rest, Token* token);
static Node* decla(Token** rest, Token* token);
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
  // 变为符号表后此处有更改，变为 find
  node->Var = findVar(token);
  return node;
}

// 生成新的块结点，其中 token 为 {, body 为 {} 内部语句
static Node* mkBlockNode(Token* token, Node* body) {
  Node* node = mkLeaf(token);
  node->body = body;
  return node;
}

// 生成新的 IF 结点，用 LNode 存储条件成立时 Compound
// 用 RNode 存储条件不成立时 Compound（此处先置为 NULL）, body 存储判断条件
static Node* mkIfNode(Token* token, Node* cond, Node* yes) {
  Node* node = mkLeaf(token);
  node->body = cond;
  node->LNode = yes;
  return node;
}

// 生成新的 For 结点，init 存储初始化结点，Body 存储条件结点，
// LNode 存储递增结点，RNode 存储条件成立执行结点
static Node* mkForNode(Token* token, Node* init, Node* cond, Node* incre,
                       Node* exe) {
  Node* node = mkLeaf(token);
  node->init = init;
  node->body = cond;
  node->LNode = incre;
  node->RNode = exe;
  return node;
}

// 生成新的 While 结点，Body 存储条件结点，RNode 存储条件成立执行结点
static Node* mkWhileNode(Token* token, Node* cond, Node* exe) {
  Node* node = mkLeaf(token);
  node->body = cond;
  node->RNode = exe;
  return node;
}

// 生成指定数据类型大小的 num Node
static Node* mkNum(int val) {
  // 此处需要 calloc，因为不在 parser.c 时内存会无效
  // Token token = {TK_NUM, NULL, val, NULL, 0};
  Token* token = calloc(1,sizeof(Token));
  token->kind = TK_NUM;
  token->val = val;
  return mkLeaf(token); 
}

// 生成新的 Add 或 Sub 结点，用于区别数值，指针计算
static Node* mkAddNode(Token* token, Node* left, Node* right) {
  // 为左右结点生成类型
  addType(left);
  addType(right);
  
  // num +|- num
  if (isInteger(left->ty) && isInteger(right->ty)) 
    return mkNode(token, left, right);
  
  // num + ptr 转换为 ptr + num
  if (isInteger(left->ty) && isPtr(right->ty)) {
    Node* tmp = left;
    left = right;
    right = tmp;
  }

  // ptr +|- num
  if (isPtr(left->ty) && isInteger(right->ty)) {
    // 使用了自带的 token
    right = mkNode(&multiplyToken, right, mkNum(8));
    addType(right);
    return mkNode(token, left, right);
    // node->ty = left->ty;
  }

  // ptr - ptr 返回两指针间有多少元素
  if (isPtr(left->ty) && isPtr(right->ty) && token->kind == TK_SUB) {
    Node* node = mkNode(token, left, right);
    node->ty = TypeInt;
    return mkNode(&divisionToken, node, mkNum(8));
  }

  errorTok(token, "Beckham~,operands invalid~~");
}

// 新建一个状态
static Status* newStatus(StatusKind kind) {
  Status* status = calloc(1, sizeof(Status));
  status->kind = kind;
  return status;
}

// Compound = { (Decla | Stmt)* }
static Node* compound(Token** rest, Token* token) {
  *rest = skip(token, "{");
  Node head = {};
  Node *cur = &head;
  // 当 token 为 "}" 时停下
  while ((*rest)->kind != TK_RBB) {
    switch ((*rest)->kind) {
      // 当为关键字时， => Decla
      case TK_INT:
        cur->next = decla(rest, *rest);
        // 此处可能不止一个语句
        while (cur->next) {
          cur = cur->next;
          addType(cur);
        }
        break;
      default:
        // => Stmt 
        cur->next = stmt(rest, *rest);
        break;
    }
    // 由于可识别空语句 cur->next 可能为 NULL
    if (cur->next) {
      cur = cur->next;
      addType(cur);
    }
  }
  *rest = skip(*rest, "}");
  // 返回构建的 compound 结点
  return mkBlockNode(token, head.next);
}

// Declspec: 数据类型
static Type* declspec(Token** rest, Token* token) {
  // 消耗掉特定关键字
  switch (token->kind) {
    case TK_INT:
      // 吸收 int
      *rest = (*rest)->next;
      return TypeInt;
    default:
      errorTok(token, "Little Dragon~, data structure not defined~");
  }  
}

// Declarator: '*'* ident Declarator 可以为多重指针
static Node* declarator(Token** rest, Token* token, Type* ty) {
  // 此时便加入符号表，此后的变量出现都只用在符号表中查找
  while ((*rest)->kind == TK_MUL) {
    // Delcaration 中即使修改 * TK_MUL -> TK_DEREF 也无意义，因为该 token 此后不用
    ty = pointerTo(ty);
    *rest = (*rest)->next;
  }
  if ((*rest)->kind != TK_VAR) {
    errorTok(*rest, "Pineapple~, We need a variable here~");
  }
  Obj* obj = newVar(*rest);
  obj->ty = ty;
  Node* node = mkLeaf(*rest);
  node->Var = obj;
  // 将变量 token 消耗
  *rest = (*rest)->next;
  return node;
}

// Decla = 
//        Declspec (Declarator ('=' expr)? (',' Declarator ('=' expr)?)*)? ';'
// 若为空语句，或者仅仅声明了变量，没有语句进行赋初值，则返回 NULL
static Node* decla(Token** rest, Token* token) {
  Type* ty = declspec(rest, token);
  Node head = {};
  // cur 用于表示当前 变量声明链表 中最后一个
  Node* cur = &head;
  switch ((*rest)->kind) {
    case TK_SEM:
      // 空语句 吸收
      *rest = (*rest)->next;
      return NULL;
    case TK_VAR:
    case TK_MUL:
      // 为变量，将此前的数据类型传入
      Node* declaration = declarator(rest, *rest, ty);
      if ((*rest)->kind == TK_ASS) {
        // 若定义时同时有赋值
        token = *rest;
        // 吸收 =
        *rest = (*rest)->next;
        declaration = mkNode(token, declaration, expr(rest, *rest)->ptr);
        // 加入赋初值语句链表中
        cur->next = declaration;
        cur = cur->next;
      }
      // 若有 ',' 则还需要继续定义变量
      while ((*rest)->kind == TK_COM) {
        *rest = (*rest)->next;
        // 再次用 declaration 获取变量
        declaration = declarator(rest, *rest, ty);
        if ((*rest)->kind == TK_ASS) {
          // 若定义时同时有赋值
          token = *rest;
          // 吸收 =
          *rest = (*rest)->next;
          declaration = mkNode(token, declaration, expr(rest, *rest)->ptr);
          // 加入赋初值语句链表中
          cur->next = declaration;
          cur = cur->next;
        }
      }
      *rest = skip(*rest, ";");
      return head.next;
    default:
      errorTok(*rest, "Hazard~,Not a variable (>·_·<)");
  }
  Node* variable = declarator(rest, *rest, ty);
}

// Stmt -> return ExprStmt
//        | ExprStmt
//        | if '(' Expr ')' Stmt (else Stmt)?
//        | for '(' ExprStmt Expr? ; Expr? ')' Stmt
//        | "while" "(" expr ")" stmt
//        | Compound
static Node* stmt(Token** rest, Token* token) {
  // Stmt -> return ExprStmt
  if (token->kind == TK_RET) {
    *rest = token->next;
    Node *exprS = exprStmt(rest, *rest);
    return mkNode(token, exprS, NULL);
  }

  // Stmt -> Compound
  if (token->kind == TK_LBB) {
    return compound(rest, *rest);
  }

  // Stmt -> if '(' Expr ')' Stmt (else Stmt)?
  if (token->kind == TK_IF) {
    *rest = skip(token->next, "(");
    Node *cond = expr(rest, *rest)->ptr;
    *rest = skip(*rest, ")");
    Node* yes = stmt(rest, *rest);
    // 建立 if 结点
    Node* ifStmt = mkIfNode(token, cond, yes);
    if ((*rest)->kind == TK_ELS) {
      *rest = (*rest)->next;
      ifStmt->RNode = stmt(rest, *rest);
    }
    return ifStmt;
  }

  // Stmt -> for '(' ExprStmt Expr? ; Expr? ')' Stmt
  if (token->kind == TK_FOR) {
    *rest = skip(token->next, "(");
    Node* init = exprStmt(rest, *rest);
    Node* cond = NULL;
    if ((*rest)->kind != TK_SEM) {
      cond = expr(rest, *rest)->ptr;
    }
    // 吸收循环条件语句分号
    *rest = skip(*rest, ";");
    Node* incre = NULL;
    if ((*rest)->kind != TK_RBR) {
      incre = expr(rest, *rest)->ptr;
    }
    // 吸收右括号
    *rest = skip(*rest, ")");
    Node* exe = stmt(rest, *rest);
    Node* forStmt = mkForNode(token, init, cond, incre, exe);
    return forStmt;
  }

  // Stmt -> "while" "(" expr ")" stmt
  if (token->kind == TK_WHI) {
    // 复用 for 循环
    token->kind = TK_FOR;
    *rest = skip(token->next, "(");
    Node* cond = expr(rest, *rest)->ptr;
    *rest = skip(*rest, ")");
    Node* exe = stmt(rest, *rest);
    Node* whileStmt = mkWhileNode(token, cond, exe);
    return whileStmt;
  }

  // Stmt -> ExprStmt
  return exprStmt(rest, token);
}

// ExprStmt = Expr? ;
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

// 暂时 Expr = assign
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
    // 此时识别的 '*' 应该认为是解引用
    case TK_MUL:
    case TK_ADDR:
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
    case TK_COM:
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
    // 此时识别的 '*' 应该认为是解引用
    case TK_MUL:
    case TK_ADDR:
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
    case TK_COM:
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
    // 此时识别的 '*' 应该认为是解引用
    case TK_MUL:
    case TK_ADDR:
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
    case TK_COM:
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
    // 此时识别的 '*' 应该认为是解引用
    case TK_MUL:
    case TK_ADDR:
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
          add_Prime(rest, *rest, mkAddNode(token, inherit, mult->ptr));
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
    case TK_COM:
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
    // 此时识别的 '*' 应该认为是解引用
    case TK_MUL:
    case TK_ADDR:
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
    case TK_COM:
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

// Primary -> num | (Expr) | + Primary | - Primary | * Primary | & Primary | Var ('(' ')')?
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
      // 检查是否为函数调用
      if (equal(*rest, "(")) {
        // 零参函数期待得到 ')'
        *rest = skip((*rest)->next, ")");
        token->kind = TK_FUNC;
        prim->ptr->funcName = strndup(token->loc, token->len);
      }
      return prim;
    case TK_MUL:
      // '*' 一直作为 TK_MUL 传递到此，实际为解引用操作
      token->kind = TK_DEREF;
      *rest = token->next;
      prim->ptr = mkNode(token, primary(rest, *rest)->ptr, NULL);
      return prim;
    case TK_ADDR:
      // '&' 操作
      *rest = token->next;
      prim->ptr = mkNode(token, primary(rest, *rest)->ptr, NULL);
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