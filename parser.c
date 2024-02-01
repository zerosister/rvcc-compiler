#include "rvcc.h"

// 全局变量 与 函数名
HashTable* globals = NULL;

// 局部变量与形参
HashTable* locals = NULL;

// 全局作用域链表
Scope* scopeList;

// 所有符号表链表
HashTable* hashHead;

// 结构体类型链表
Type* locTypes = NULL;

// 新建一个作用域
static void enterScope() {
  HashTable* locals = calloc(1, sizeof(HashTable));
  locTypes = calloc(1, sizeof(Type));
  Scope* scope = calloc(1, sizeof(Scope));
  // 进栈
  scope->next = scopeList->next;

  scope->hashTable = locals;
  scope->tyList = locTypes;
  // 插入符号表链表
  locals->next = hashHead->next;
  hashHead->next = locals;
  scopeList->next = scope;
}

// 退出作用域
static void exitScope() {
  if (!(scopeList->next)) 
    error("Jackson~,T.T 缺少作用域");
  scopeList->next = scopeList->next->next;
}

// 自行构建 token 变量
static Token multiplyToken = {TK_MUL};
static Token divisionToken = {TK_DIV};
static Token addToken = {TK_ADD};
static Token dereferenceToken = {TK_DEREF};
static Token varToken = {TK_VAR};

// 非递归的语法分析，发现 LL(1) 文法用栈其实无法解决本质问题
// 因为仅仅为生成产生式时有用，附加语义动作则需要在 parse 中同时进行
// so，回归课程中方法
// 性质：递归下降

// program = (Function | GlobalVar)*
// Function = Declspec '*'* Var Typesuffix Compound
// Typesuffix = '(' (Declspec Declarator (, Declspec Declarator)*)? ')'
// Compound = { (Decla | Stmt)* }
// Decla = Declspec (Declarator Decla_Prim)? ';'
// Decla_Prim =  (('=' Assign)+(',' Declarator ('=' Assign)?)*))?
// Declspec: 数据类型
// Declarator: '*'* Var ArrarySuffix 可以为多重指针
// ArrarySuffix = ('['num']')*
// Stmt -> return ExprStmt
//        | ExprStmt
//        | if '(' Expr ')' Stmt (else Stmt)?
//        | for '(' ExprStmt Expr? ; Expr? ')' Stmt
//        | "while" "(" expr ")" stmt
//        | Compound
// ExprStmt = Expr? ;
// 暂时 Expr = Assign ("," Expr)?
// Assign -> Equa Assign'
// Assign' -> = Equa Assign | null
// Equa -> ! Equa | Rela Equa'
// Equa' -> == Rela Equa'  | != Rela Equa' | null
// Rela -> Add Rela'
// Rela' -> < Add Rela' | > Add Rela' | <= Add Rela' | >= Add Rela' | null
// Add -> Mul Add'
// Add' -> + Mul Add1' | - Mul Add1' | null
// Mul -> Unary Mul'
// Mul' -> * Unary Mul1' | / Unary Mul1' | null
// Unary ->   + Unary 
//          | - Unary 
//          | * Unary 
//          | & Unary 
//          | Primary ('[' Expr ']' | '.' Var )*
// Primary -> num 
//            | Var 
//            | Var FuncArgu 
//            | '('Expr')'
//            | sizeof Unary
//            | '(' StmtEx ')'
// StmtEx -> Compound 
// FuncArgu -> '(' Assign? (, Assign)* ')'

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
static Status* unary(Token** rest, Token* token);
static Node* primary(Token** rest, Token* token);
static Node* funcArgu(Token** rest, Token* token);

static Type* arraySuffix(Token** rest, Token* token, Type* ty);

// 得到全局变量
static HashTable* getGlobals(void) {
  if (globals == NULL) globals = calloc(1, sizeof(HashTable));
  return globals;
}

// 新增唯一名称
static char* newUniqueName(void) {
  static int Id = 0;
  return format(".L..%d", Id++);
}

static Node *newStrLiteral(Token* token) {
  // 直接新建变量
  Obj* var = insert(getGlobals(), newUniqueName(), 20);
  var->isLocal = false;
  Type* ty = arrayOf(TypeChar, token->len + 1);
  var->ty = ty;
  var->initData = strndup(token->str, token->len);
  Node* node = calloc(1, sizeof(Node));
  node->token = &varToken;
  node->Var = var;
  node->ty = ty;
  return node;
}

// hash 方法查找本地变量（符号）
static Obj* findLocalVar(Token* token) {
  Scope* sc = scopeList->next;
  for ( ; sc; sc = sc->next ) {
    HashTable* hashTable = sc->hashTable;
    Obj* obj = search(hashTable, token->loc, token->len);
    if (obj) 
      return obj;
  }
  return NULL; 
}

// hash 方法新增变量至符号表，而变量只会从 token 中产生
static Obj* newLocalVar(Token* token) {
  // 只在当前域中寻找是否已经定义过
  Obj* var = search(scopeList->next->hashTable, token->loc, token->len);
  if (var == NULL) {
    // 未找到，新建 Obj
    var = insert(scopeList->next->hashTable, token->loc, token->len);
    var->isLocal = true;
    return var;
  }
  errorTok(token, "Cafe~, duplicated definition");
}

static Obj* findGlobalVar(Token* token) {
  globals = getGlobals();
  return search(globals, token->loc, token->len);
}

static Obj* newGlobalVar(Token* token) {
  Obj* var = findGlobalVar(token);
  if (var == NULL) {
    // 未找到，新建 Obj
    var = insert(getGlobals(), token->loc, token->len);
    var->isLocal = false;
  }
  return var;
}
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
  node->Var = findLocalVar(token);
  if (!node->Var) {
    // 体现局部变量覆盖全局变量特点
    node->Var = findGlobalVar(token);
    // if(!node->Var)
    //   // 表示没有此变量
    //   errorTok(token, "Rose~,Unexpected variable");
  }
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
    // 指针加法为每次加减指针指向基类的大小
    right = mkNode(&multiplyToken, right, mkNum(left->ty->base->size));
    addType(right);
    return mkNode(token, left, right);
  }

  // ptr - ptr 返回两指针间有多少元素
  if (isPtr(left->ty) && isPtr(right->ty) && token->kind == TK_SUB) {
    Node* node = mkNode(token, left, right);
    node->ty = TypeInt;
    return mkNode(&divisionToken, node, mkNum(left->ty->base->size));
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
  enterScope();
  Node head = {};
  Node *cur = &head;
  // 当 token 为 "}" 时停下
  while ((*rest)->kind != TK_RBB) {
    switch ((*rest)->kind) {
      // 当为关键字时， => Decla
      case TK_INT:
      case TK_CHAR:
      case TK_STRUCT:
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
  Node* node = mkBlockNode(token, head.next);
  exitScope();
  return node;
}

// addMembers 为结构体添加成员
static void addMembers(Member* head, Member* cur, Type* ty, Token** rest, int* offset) {
  while ((*rest)->kind == TK_MUL) {
    // Delcaration 中即使修改 * TK_MUL -> TK_DEREF 也无意义，因为该 token 此后不用
    ty = pointerTo(ty);
    *rest = (*rest)->next;
  }
  if ((*rest)->kind != TK_VAR) 
    errorTok(*rest, "Cinderella~, We need a variable name here~");
  // 遍历结构体中已有变量避免重名
  for (head = head->next; head; head = head->next) {
    if (!strncmp(head->name, (*rest)->loc, (*rest)->len)) {
      errorTok(*rest, "Aurora~, duplicated definition~");
    }
  }
  Member* member = calloc(1, sizeof(Member));
  member->name = strndup((*rest)->loc, (*rest)->len);
  // 将变量 token 消耗
  *rest = (*rest)->next;
  ty = arraySuffix(rest, *rest, ty);
  member->ty = ty;
  member->offset = *offset;
  *offset += ty->size;
  cur->next = member;
}

static Type* insertStruct(Token** rest, Token* token) {
  Type* ty = locTypes->structNext;
  while (ty) {
    if (!strncmp(ty->structName, token->loc, token->len)) {
      // 类型已经被定义
      errorTok(token, "Oppa~, redefinition of struct");
    }
    ty = ty->structNext;
  }
  Type* new = calloc(1, sizeof(Type));
  new->tyKind = TY_STRUCT;
  new->structName = strndup(token->loc, token->len);
  new->structNext = locTypes->structNext;
  // 插入类型链表
  locTypes->structNext = new;
  // 吸收结构体标签
  *rest = (*rest)->next;
  return new;
}

// 从域中寻找结构体类型
static Type* searchStruct(Token* token) {
  Scope* sc = scopeList->next;
  Type* ty = NULL;
  while (sc) {
    ty = sc->tyList->structNext;
    while (ty) {
      if (!(strncmp(ty->structName, token->loc, token->len))) {
        return ty;    
      }
      ty = ty->structNext;
    }
    sc = sc->next;
  }
  errorTok(token, "First Lady~,No such struct");
}

// 进行结构体变量对齐
static void alignMembers(Type* type) {
  Member* ms = type->members;
  int alignNum = 1;
  int rest, memOffset = 0;
  // 确定对齐大小
  while (ms) {
    if (ms->ty->size > alignNum) {
      alignNum = ms->ty->size;
    }
    ms = ms->next;
  }
  ms = type->members;
  if (ms) {
    // rest 用于表示对齐字节所剩余字节数
    // align 表示下一个结构体成员可能开始的相对字节数（相对于对齐大小）
    ms->align = ms->ty->size;
    rest = alignNum - ms->align;
    memOffset = ms->ty->size;
    ms = ms->next;
  }
  while (ms) {
    if (rest >= ms->ty->size) {
      // 表示对齐大小还可以放下另一个成员
      ms->offset = memOffset;
      ms->align = alignNum - rest + ms->ty->size;
      rest = rest - ms->ty->size;
    }
    else {
      rest = alignNum - ms->ty->size;
      memOffset = alignTo(memOffset, alignNum);
      ms->offset = memOffset;
      // 更新下一个变量的起始位置
      memOffset = memOffset + ms->ty->size;
      ms->align = ms->ty->size;
    }
    ms = ms->next;
  }
  type->size = alignTo(memOffset, alignNum);
}

// Declspec: 数据类型
static Type* declspec(Token** rest, Token* token) {
  // 消耗掉特定关键字
  switch (token->kind) {
    case TK_INT:
      // 吸收 int
      *rest = (*rest)->next;
      return TypeInt;
    case TK_CHAR:
      *rest = (*rest)->next;
      return TypeChar;
    // struct Tag? {...}
    case TK_STRUCT: {
      int offset = 0;
      Member memsHead = {};
      Member* cur = &memsHead;
      // 吸收 struct 
      *rest = (*rest)->next;
      Type* type = calloc(1, sizeof(Type));
      if ((*rest)->kind == TK_VAR) {
        if ((*rest)->next->kind == TK_LBB) {
          // 结构体标签 且为定义结构体
          type = insertStruct(rest, *rest);
        }
        else {
          type = searchStruct(*rest);
          // 吸收标签
          *rest = (*rest)->next;
          return type; 
        }
      }
      *rest = skip(*rest, "{");
      while ((*rest)->kind != TK_RBB) {
        // 未遇到右大括号继续循环
        Type* base = declspec(rest, *rest);
        switch ((*rest)->kind) {
          case TK_SEM:
            // 空语句 吸收
            *rest = (*rest)->next;
            break;
          case TK_VAR:
          case TK_MUL: {
            // 将此前的数据类型传入
            Type* ty = base; 
            addMembers(&memsHead, cur, ty, rest, &offset);
            if (cur->next) {
              cur = cur->next;
            }
            while ((*rest)->kind == TK_COM) {
              *rest = (*rest)->next;
              addMembers(&memsHead, cur, ty, rest, &offset);
              if (cur->next) {
                cur = cur->next;
              }
            }
            *rest = skip(*rest, ";");
            break;
          }
          default:
            errorTok(*rest, "Eva~,Not a variable Name~");
          }
      }
      *rest = skip(*rest, "}");
      type->members = memsHead.next;
      alignMembers(type);
      return type;
    }
    case TK_EOF:
      return NULL;  
    default:
      errorTok(token, "Little Dragon~, data structure not defined~");
  }  
}

// ([num])*
static Type* arraySuffix(Token** rest, Token* token, Type* ty) {
  if (token->kind == TK_LMB) {
    // 检测到为数组，吸收 [
    *rest = (*rest)->next;
    // 记录下数组元素个数
    int cnt = (*rest)->val;
    *rest = (*rest)->next;
    *rest = skip(*rest, "]");
    // 考虑为多维数组或结构体成员
    ty = arraySuffix(rest, *rest, ty);
    ty = arrayOf(ty, cnt);
  }
  return ty;
}

// Declarator: '*'* ArrarySuffix Var  变量可以为多重指针
static Node* declarator(Token** rest, Token* token, Type* ty, bool isLocal) {
  // 此时便加入符号表，此后的变量出现都只用在符号表中查找
  while ((*rest)->kind == TK_MUL) {
    // Delcaration 中即使修改 * TK_MUL -> TK_DEREF 也无意义，因为该 token 此后不用
    ty = pointerTo(ty);
    *rest = (*rest)->next;
  }
  if ((*rest)->kind != TK_VAR) 
    errorTok(*rest, "Pineapple~, We need a variable here~");
  Obj* obj = NULL;
  // 处理局部变量 或 全局变量
  if (isLocal) obj = newLocalVar(*rest);
  else obj = newGlobalVar(*rest);
  // 自动认为不是函数名，虽然 calloc 时就已经令为 false 了
  obj->isFuncName = false;
  Node* node = mkLeaf(*rest);
  // 将变量 token 消耗
  *rest = (*rest)->next;
  ty = arraySuffix(rest, *rest, ty);
  obj->ty = ty;
  node->Var = obj;
  return node;  
}

// Decla_Prim =  (('=' Assign)+(',' Declarator ('=' Assign)?)*))?
// 若为空语句，或者仅仅声明了变量，没有语句进行赋初值，则返回 NULL
static void decla_Prim(Token** rest, Token* token, Type* ty, Node* declaration, Node* head, bool isLocal) {
  // cur 用于表示当前 变量声明链表 中最后一个
  Node* cur = head;
  switch ((*rest)->kind) {
    case TK_ASS:
      // 若定义时同时有赋值
      token = *rest;
      // 吸收 =
      *rest = (*rest)->next;
      declaration = mkNode(token, declaration, assign(rest, *rest)->ptr);
      // 加入赋初值语句链表中
      cur->next = declaration;
      cur = cur->next;
      // 同时还需要判断接下来是否为 , 
    case TK_COM:
      // 若有 ',' 则还需要继续定义变量
      while ((*rest)->kind == TK_COM) {
        *rest = (*rest)->next;
        // 再次用 declaration 获取变量
        declaration = declarator(rest, *rest, ty, isLocal);
        if ((*rest)->kind == TK_ASS) {
          // 若定义时同时有赋值
          token = *rest;
          // 吸收 =
          *rest = (*rest)->next;
          declaration = mkNode(token, declaration, assign(rest, *rest)->ptr);
          // 加入赋初值语句链表中
          cur->next = declaration;
          cur = cur->next;
        }
      }
      *rest = skip(*rest, ";");
      return;
    case TK_SEM:
      *rest = skip(*rest, ";");
      return;
    default:
      // 错误了
      errorTok(token, "HITer~, Sorry something wrong in initializing or defying arguments");
  }
}

// Decla = Declspec (Declarator Decla_Prim)? ';'
static Node* decla(Token** rest, Token* token) {
  Node head = {};
  Type* ty = declspec(rest, token);
  switch ((*rest)->kind) {
    case TK_SEM:
      // 空语句 吸收
      *rest = (*rest)->next;
      return NULL;
    case TK_VAR:
    case TK_MUL:
      // 为变量，将此前的数据类型传入
      Node* declaration = declarator(rest, *rest, ty, true);
      decla_Prim(rest, *rest, ty, declaration, &head, true);
      return head.next;
    default:
      errorTok(*rest, "Hazard~,Not a variable (>·_·<)");
  }
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
  Node* root = expr(rest, token)->ptr;
  *rest = skip(*rest, ";");
  return root;
}

// 暂时 Expr = Assign ("," Expr)?
static Status* expr(Token** rest, Token* token) { 
  Status* stat = assign(rest, token);
  if ((*rest)->kind == TK_COM) {
    token = *rest;
    *rest = (*rest)->next;
    Node* right = expr(rest, *rest)->ptr;
    stat->ptr = mkNode(token, stat->ptr, right);
    addType(stat->ptr);
  } 
  return stat;
}

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
    case TK_SIZEOF:
    case TK_CHL:
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
    case TK_RMB:
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
    case TK_NOT:
      // Equa -> ! Equa，消耗 !
      *rest = token->next;
      equa->ptr = mkNode(token, equation(rest, *rest)->ptr, NULL);
      return equa;
    case TK_LBR:
    case TK_NUM:
    case TK_ADD:
    case TK_SUB:
    // 新增变量等价于 NUM
    case TK_VAR:
    // 此时识别的 '*' 应该认为是解引用
    case TK_MUL:
    case TK_ADDR:
    case TK_SIZEOF:
    case TK_CHL:
      // Equa -> Rela Equa'
      Status* relation = rela(rest, token);
      Status* equa_P = equation_Prime(rest, *rest, relation->ptr);
      equa->ptr = equa_P->system;
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
    case TK_RMB:
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
    case TK_SIZEOF:
    case TK_CHL:
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
    case TK_RMB:
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
    case TK_SIZEOF:
    case TK_CHL:
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
    case TK_RMB:
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

// Mul -> Unary Mul'
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
    case TK_SIZEOF:
    case TK_CHL:
      Status* prim = unary(rest, token);
      Status* mult_P = mul_Prime(rest, *rest, prim->ptr);
      mult->ptr = mult_P->system;
      return mult;
    default:
      errorTok(token, "Soulmate~,May be not a multipler");
  }
}

// Mul' -> * Unary Mul1' | / Unary Mul1' | null
static Status* mul_Prime(Token** rest, Token* token, Node* inherit) {
  Status* mult_P = newStatus(ST_MPrim);
  switch (token->kind) {
    // 识别到* or / ,Mul' -> * Unary Mul1' | / Unary Mul1'
    case TK_MUL:
    case TK_DIV:
      // printf("Mul' -> * Unary Mul1' | / Unary Mul1'\n");
      *rest = token->next;
      Status* prim = unary(rest, *rest);
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
    case TK_RMB:
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

// 返回对应结构体的成员
static Member* checkMember(Type* ty, Token* token) {
  for (Member* m = ty->members; m; m = m->next) {
    if (!strncmp(m->name, token->loc, token->len)) {
      // 找到成员
      return m;
    }
  }
  // 未找到返回 空
  return NULL;
}

// 后处理
static Node* postProcess(Token** rest, Node* node) {
  switch ((*rest)->kind) {
    case TK_LMB: {
      // 吸收 [
      *rest = (*rest)->next;
      Node* add = expr(rest, *rest)->ptr;
      // 吸收 ]
      *rest = skip(*rest, "]");
      // [] 中的 expr 加上 变量地址
      node = mkAddNode(&addToken, node, add);
      // 再进行解引用
      node = mkNode(&dereferenceToken, node, NULL);
      // 递归调用吸收后面 [expr]
      node = postProcess(rest, node);
      return node;
    }
    case TK_POI: {
      Token* token = *rest;
      // 吸收 '.'
      *rest = (*rest)->next;
      addType(node);
      node->member = checkMember(node->ty, *rest);
      // 吸收结构体变量 如 x.a, 吸收变量 a
      *rest = (*rest)->next;
      // 新建 · 单叉树
      node = mkNode(token, node, NULL);
      node = postProcess(rest, node);
      return node;
    }
    default:
      return node;
  }
}

// Unary ->   + Unary 
//          | - Unary 
//          | * Unary 
//          | & Unary 
//          | Primary ('[' Expr ']')*
static Status* unary(Token** rest, Token* token) {
  Status* prim = newStatus(ST_Primary);
  switch (token->kind) {
    case TK_SUB:
      // 消耗 -
      *rest = token->next;
      // 因为为从左到右遍历二叉树，所以需要先从左结点得到数值，再进行单元运算
      // 所以课程中的从右到左遍历二叉树好处就是，单运算符时，运算符在左结点
      prim->ptr = mkNode(token, unary(rest, *rest)->ptr, NULL);
      return prim;
    case TK_ADD:
      // 不需要 genCode，只需要消耗 + 即可
      while (token->kind == TK_ADD) {
        // 最终会在种别为 TK_NUM 时停止
        *rest = token->next;
        token = *rest;
      }
      return unary(rest, *rest);
    case TK_MUL:
      // '*' 一直作为 TK_MUL 传递到此，实际为解引用操作
      token->kind = TK_DEREF;
      *rest = token->next;
      prim->ptr = mkNode(token, unary(rest, *rest)->ptr, NULL);
      return prim;
    case TK_ADDR:
      // '&' 操作
      *rest = token->next;
      prim->ptr = mkNode(token, unary(rest, *rest)->ptr, NULL);
      return prim;
    default:
      // 进入 Primary
      prim->ptr = primary(rest, token);
      prim->ptr = postProcess(rest, prim->ptr);
      return prim;
  }
}

// Primary -> num 
//            | Var 
//            | Var FuncArgu 
//            | '('Expr')'
//            | sizeof Unary
//            | '(' StmtEx ')'
static Node* primary(Token** rest, Token* token) {
  switch (token->kind) {
    case TK_LBR: {
      // 识别到 Primary -> (Expr) 或语句表达式
      Node* node = NULL;
      *rest = token->next;
      switch ((*rest)->kind) {
        case TK_LBB:
        // 语句表达式
        token = *rest;
        node = compound(rest, *rest);
        stmtExType(node);
        // 将语句进行标记，codeGen 中进行 类似 Compound 的处理
        token->kind = TK_StmtEx;
          break;
        default:
          token = token->next;
          Status* equa = expr(rest, token);
          node = equa->ptr;
          break;
      }
      // 同时此时需要消耗 )
      token = *rest;
      *rest = skip(token, ")");
      return node;
    }
    case TK_NUM:
      // 识别到 Primary -> num
      *rest = token->next;
      return mkLeaf(token);
    case TK_VAR: {
      // 识别到变量
      Node* var = mkVarNode(token);
      *rest = token->next;
      // 检查是否为函数调用
      if (equal(*rest, "(")) {
        var->argus = funcArgu(rest, *rest);
        token->kind = TK_FUNC;
        var->funcName = strndup(token->loc, token->len);
      }
      return var;
    }
    case TK_CHL: {
      // 识别到字符型字面量，新建变量并得到变量结点
      *rest = token->next;
      return newStrLiteral(token);
    }
    case TK_SIZEOF: {
      // 吸收 sizeof
      *rest = token->next;
      Node* size = unary(rest, *rest)->ptr;
      addType(size);
      size = mkNum(size->ty->size);
      return size;
    }
    default:
        // 非法的
      errorTok(token, "boo boo,a num is expected~");
      break;
  }
}

// FuncArgu -> '(' Assign? (, Assign)* ')'
static Node* funcArgu(Token** rest, Token* token) {
  // 吸收 '('
  *rest = skip(token, "(");
  if ((*rest)->kind == TK_RBR) {
    // 若为右括号，则为零参函数，吸收 ')' 返回
    *rest = (*rest)->next;
    return NULL;
  }
  Node head = {};
  Node* cur = &head;
  // 吸收第一个参数
  cur->next = assign(rest, *rest)->ptr;
  cur = cur->next;
  addType(cur);
  while ((*rest)->kind == TK_COM) {
    // 吸收 ','
    *rest = (*rest)->next;
    cur->next = assign(rest, *rest)->ptr;
    cur = cur->next;
    addType(cur);
  }
  // 吸收 ')'
  *rest = skip(*rest, ")");
  return head.next;
}

// 为函数识别形参
// Typesuffix = '(' (Declspec Declarator (, Declspec Declarator)*)? ')' 
static Type* typeSuffix(Token** rest, Token* token) {
    // 吸收左括号
    *rest = skip(token, "(");
    Type head = {};
    Type* cur = &head;
    if ((*rest)->kind != TK_RBR) {
      cur->next = copyType(declspec(rest, *rest)); 
      // 此时增加的为临时变量
      cur->next->var = declarator(rest, *rest, cur->next, true)->Var;
      cur = cur->next;
      while ((*rest)->kind == TK_COM) {
        *rest = skip(*rest, ",");
        cur->next = copyType(declspec(rest, *rest));
        cur->next->var = declarator(rest, *rest, cur->next, true)->Var;
        cur = cur->next;
        // 需要将结点的 next 置为 NULL
        cur->next = NULL;
      }
    }
    *rest = skip(*rest, ")");
    return head.next;
}

// Function = Declspec '*'* Var Typesuffix Compound
static Function* function(Token** rest, Token* token, Type* baseType) {
  // 记录下变量或方程结点
  Node* tmp = declarator(rest, token, baseType, false);
  if ((*rest)->kind == TK_LBR) {
    // 因为可能有形参，故需要再进入一个域
    hashHead = calloc(1, sizeof(HashTable)); 
    enterScope();
    // 表示为函数
    tmp->Var->isFuncName = true;
    Type* params = typeSuffix(rest, *rest); 
    Node* body = compound(rest, *rest);
    // 生成当前 Function 结点
    Function* func = calloc(1, sizeof(Function));
    func->FType = funcType(tmp->Var->ty);
    func->locals = hashHead->next;
    func->funcName = strndup(tmp->Var->Name, strlen(tmp->Var->Name));
    func->params = params;
    func->Body = body;
    func->isFunction = true;
    exitScope();
    return func;
  }
  // 将全局变量的初始化作为一个 Function 处理
  // 添加全局变量，传入 baseType 即不含指针的基本类型
  Function* initGlobals = calloc(1, sizeof(Function));
  initGlobals->isFunction = false;
  decla_Prim(rest, *rest, baseType, tmp, initGlobals->Body,false);
  return initGlobals;
}

// program = (Function | GlobalVar)*
Program* parse(Token** rest, Token* token) {
  Function head = {};
  Function* cur = &head;
  scopeList = calloc(1, sizeof(Scope));
  hashHead = calloc(1, sizeof(HashTable)); 
  enterScope();
  Type* baseType = declspec(rest, token);
  while (baseType != NULL) { 
    cur->next = function(rest, *rest, baseType);
    cur = cur->next;
    baseType = declspec(rest, *rest);
  }
  exitScope();
  Program* prog = calloc(1, sizeof(Program));
  prog->funcs = head.next;
  prog->globals = getGlobals();
  return prog;
}