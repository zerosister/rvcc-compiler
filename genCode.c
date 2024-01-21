#include "rvcc.h"

// 文件输出流
FILE* outFile;
// 设置一个全局的函数，表示当前生成函数
static Function* current_func;
static void genStmt(Node* root);
static void genExpr(Node* root);

// 栈深度
static int Depth = 0;

static void printLn(char* Fmt, ...) {
  va_list VA;
  va_start(VA, Fmt);
  vfprintf(outFile, Fmt, VA);
  va_end(VA);

  fprintf(outFile, "\n");
}

// 将 a0 中值压入 sp 栈中
static void Push(void) {
  // riscv64 压栈为 8 个字节
  printLn("# 压栈，将 a0 值存入栈顶");
  printLn("\taddi sp,sp,-8");
  printLn("\tsd a0,0(sp)");
  Depth++;
}

// 弹出 sp 栈中值到 reg 中
static void Pop(char *reg) {
  printLn("# 弹栈，将栈顶值存入 %s", reg);
  printLn("\tld %s,0(sp)", reg);
  printLn("\taddi sp,sp,8");
  Depth--;
}

// 对齐到 align 的整数倍
static int alignTo(int n, int align) {
  // (0,align] 返回 align
  return (n + align - 1) / align * align;
}

// 计算给定结点的绝对地址
// 若报错，说明结点不在内存
static void genAddr(Node *node) {
  if (node->token->kind == TK_VAR) {
    // 偏移量相对于 fp
    if (node->Var->isLocal) {     
      printLn("# 获取变量 %s 的栈内地址为 %d(fp)", node->Var->Name,
            node->Var->Offset);
      printLn("\taddi a0,fp,%d", node->Var->Offset);
    }
    else {
      printLn("# 获取全局变量 %s 的地址", node->Var->Name);
      printLn("\tla a0, %s", node->Var->Name);
    }    
    return;
  }
  if (node->token->kind == TK_DEREF) {
    genExpr(node->LNode);
    // 正常遇到变量便会解引用
    return;
  }
  errorTok(node->token, "Da Zhang Wei says: not an lvalue");
}

// 加载变量到 a0
static void load(Type* ty) {
  // 若为数组则变量作为地址用
  if (ty->tyKind == TY_ARRAY) 
    return;
  // 将变量 load 至 a0
  printLn("# 读取 a0 中存放的地址，得到值存入 a0");
  if (ty->size == 1) 
    printLn("\tlb a0,0(a0)"); 
  else
    printLn("\tld a0,0(a0)");
}

// 将 a0 存储至指定内存中
static void store(Type* ty) {
  // 将左部地址存入 a1
  Pop("a1");
  // 将右值放入左结点变量中
  printLn("# 将 a0 值，写入 a1 存放的地址中");
  if (ty->size == 1) 
    printLn("\tsb a0,0(a1)");
  else 
    printLn("\tsd a0,0(a1)");
}

// 计数程序
static int count() {
  static int cnt = 0;
  cnt += 1;
  return cnt;
}

static void genFuncall(Node* argus) {
  if (!argus) 
    // 零参函数
    return;
  // 记录参数个数
  int argusCnt = 0;
  while (argus) {
    argusCnt += 1;
    genExpr(argus);
    printLn("# 将第 %d 个参数压栈", argusCnt);
    printLn("\taddi sp,sp,-8");
    printLn("\tsd a0,0(sp)");
    argus = argus->next;
  }
  while (argusCnt > 0) {
    printLn("# 将第 %d 个参数弹栈到 a%d", argusCnt, argusCnt - 1);
    printLn("\tld a%d,0(sp)", argusCnt - 1);
    printLn("\taddi sp,sp,8");
    argusCnt -= 1;
  }
}

static void genExpr(Node* root) {
  // 深度优先遍历 DFS left -> right -> root

  // 如果为 NULL 则不生成任何代码
  if (!root) return;

  Token* token_root = root->token;
  switch (token_root->kind) {
    case TK_NUM:
      printLn("# 将 %d 加载至 a0", token_root->val);
      printLn("\tli a0,%d", token_root->val);
      return;
    case TK_VAR:
      // 将变量地址存入 a0
      genAddr(root);
      load(root->ty);
      return;
    case TK_ADDR:
      // 获取变量地址，注意到 & 为右结合操作符，需要递归
      genAddr(root->LNode);
      return;
    case TK_DEREF:
      genExpr(root->LNode);
      load(root->ty);
      return;
    case TK_ASS:
      // 产生地址
      genAddr(root->LNode);
      Push();
      // 产生右值
      genExpr(root->RNode);
      store(root->LNode->ty);
      return;
    case TK_FUNC: 
      printLn("\n# 调用函数%s",root->funcName);
      genFuncall(root->argus);
      printLn("\tcall %s", root->funcName);
      return;
    case TK_StmtEx:
      // 将 token kind 改回 TK_LBB, 进行 块语句生成
      token_root->kind = TK_LBB;
      genStmt(root);
      return;
    default:
      break;
  }
  // 当前为操作符，递归遍历
  if (!root->LNode)
    errorTok(token_root, "Juliet~,An operand is losed when generating code~");
  genExpr(root->LNode);
  // 左操作数入栈
  Push();

  if (!root->RNode) {
    // 可能为单元运算符 - | !
    Pop("a0");
    switch (token_root->kind) {
      case TK_SUB:
        // neg a0, a0 是 sub a0, x0, a0 的别名，即 a0=0-a0
        printLn("# 对 a0 值取相反数");
        printLn("\tneg a0,a0");
        return;
      case TK_NOT:
        // ! 操作为，若数值非零，则置为 0，若为 0，则置为 1
        // set equal zero,sltui a0, a0, 1，小于 1 当然只有 0 了
        printLn("# 将 a0 置为 !a0");
        printLn("  seqz a0, a0");
        return;
      default:
        break;
    }
    errorTok(token_root, "Princess~,An operand is losed when generating code~");
  } else
    // 右操作数本身会在 a0
    genExpr(root->RNode);

  // 将左结点值 pop 至 a1 中
  Pop("a1");
  switch (token_root->kind) {
    case TK_ADD:
      printLn("# 将 a0 + a1 写入 a0");
      printLn("\tadd a0,a1,a0");
      return;
    case TK_SUB:
      printLn("# 将 a1 - a0 写入 a0");
      printLn("\tsub a0,a1,a0");
      return;
    case TK_MUL:
      printLn("# 将 a0 * a1 写入 a0");
      printLn("\tmul a0,a1,a0");
      return;
    case TK_DIV:
      printLn("# 将 a1 / a0 写入 a0");
      printLn("\tdiv a0,a1,a0");
      return;
    case TK_LST:
      // a1 < a0,slt:set less than:R[rd] = (R[rs1]<R[rs2])?1:0
      printLn("# 判断 a1 < a0");
      printLn("\tslt a0,a1,a0");
      return;
    case TK_BGE:
      // a1 >= a0，即为 slt 取反
      printLn("# 判断 a1 <= a0");
      printLn("\tslt a0,a1,a0");
      // 取反不能用 neg,-0 = 0
      printLn("\txori a0,a0,1");
      return;
    case TK_BGT:
      // a1 > a0，将 a0,a1 换位即可
      printLn("# 判断 a0 < a1");
      printLn("\tslt a0,a0,a1");
      return;
    case TK_LSE:
      // a1 <= a0，换位同 BGE
      printLn("# 判断 a0 <= a1");
      printLn("\tslt a0,a0,a1");
      printLn("\txori a0,a0,1");
      return;
    case TK_DEQ:
      // a1 == a0
      printLn("# 判断 a1 == a0");
      printLn("\txor a0,a0,a1");
      // equal zero?sltiu a0,a0,1
      printLn("\tseqz a0,a0");
      return;
    case TK_NEQ:
      printLn("# 判断 a1 != a0");
      printLn("\txor a0,a1,a0");
      // not equal zero? sltu a0,x0,a0
      printLn("\tsnez a0,a0");
      return;
    default:
      break;
  }
  errorTok(token_root, "apple~,we met invalid expression q_q");
}

static void genStmt(Node* root) {
  // 深度优先遍历 DFS left -> right -> root

  // 如果为 NULL 则不生成任何代码
  if (!root) return;

  Token *token_root = root->token;

  switch (root->token->kind) {
    // 确定是那种语句
    case TK_LBB: {
      // 表明进入 compound, 需要遍历其 body
      Node *body = root->body;
      while (body) {
        genStmt(body);
        body = body->next;
      }
      return;
    }
    case TK_IF: {
      // Node 使用信息详见 parser.c mkIfNode 函数
      int if_cnt = count();
      printLn("\n# ========分支语句 %d ========", if_cnt);
      // 进入 if node 首先执行条件判断
      printLn("\n # Condition 表达式 %d", if_cnt);
      genExpr(root->body);
      printLn("# 若 a0 为 0，即条件失败，跳转至分支 %d 的.L.else%d 段", if_cnt,
             if_cnt);
      printLn("\tbeqz a0, .L.else%d", if_cnt);
      // 进行 if 成功分支
      printLn("\n# Then 语句");
      genStmt(root->LNode);
      // 执行完跳转 if 后语句
      printLn("# 跳转到分支 %d 的.L.endIF%d 段标签", if_cnt, if_cnt);
      printLn("\tj .L.endIf%d", if_cnt);
      // Else 代码块
      printLn("\n # Else 语句 %d", if_cnt);
      printLn("# 分支 %d 的.L.else%d 段标签", if_cnt, if_cnt);
      printLn(".L.else%d:", if_cnt);
      // 进行 if 失败分支
      genStmt(root->RNode);
      printLn("\n # 分支 %d 的.L.end%d 段标签", if_cnt);
      printLn(".L.endIf%d:", if_cnt);
      return;
    }
    case TK_FOR: {
      // Node 使用信息详见 parser.c mkForNode 函数
      int for_cnt = count();
      printLn("\n# ==== 循环语句 %d =============", for_cnt);
      // 首先执行初始化语句
      printLn("\n# Init 语句");
      genExpr(root->init);
      // 循环开始标签
      printLn("# 循环 %d 的.L.begin%d 的段标签", for_cnt);
      printLn(".L.begin%d:", for_cnt);
      // 进行条件判断
      printLn("\n # Condition 表达式");
      genExpr(root->body);
      printLn("# 若 a0 为 0，跳转至循环 %d 的.L.end%d 段", for_cnt, for_cnt);
      printLn("\tbeqz a0, .L.end%d", for_cnt);
      // 满足条件，执行相应语句
      printLn("\n# Exe 循环执行部分");
      genStmt(root->RNode);
      // 执行递增语句
      printLn("\n# Increment 部分");
      genExpr(root->LNode);
      printLn("# 跳转至循环 %d 的.L.begin%d 段", for_cnt);
      printLn("\tj .L.begin%d", for_cnt);
      printLn("# 循环结束标签");
      printLn(".L.end%d:", for_cnt);
      return;
    }
    case TK_RET: {
      // 首先将 exprStmt 代码生成
      // 注意 exprStmt 可能会产生 NULL 结点
      genExpr(root->LNode);
      // 进行汇编语句的跳转
      printLn("# 跳转到 .L.%s.return", current_func->funcName);
      printLn("\tj .L.%s.return", current_func->funcName);
      return;
    }
    default:
    // 否则默认进入表达式生成
      genExpr(root);
  }
}


// 根据变量链表计算出偏移量
static void assignVarOffsets(Function *func) {
  int offset = 0;
  // 读取所有变量
  for (int i = 0; i < HASH_SIZE; i++) {
    Obj *var = func->Locals->objs[i];
    while (var) {
      // 偏移量置为类型偏移量
      offset += var->ty->size;
      var->Offset = -offset;
      var = var->next;
    }
  }
  // 将栈对齐到 16 字节
  func->StackSize = alignTo(offset, 16);
}

static void codeGen(Function* func) {
  // 每条语句一个结点，需要遍历每条语句
  current_func = func;
  Node *body = func->Body;
  if (func->isFunction) {
    assignVarOffsets(func);
    printLn("\n# 定义全局 %s 段", func->funcName);
    printLn("\t.global %s", func->funcName);
    printLn("# 代码段标签");
    printLn("\t.text");
    printLn("\n #====%s 段开始============", func->funcName);
    printLn("# %s 段标签", func->funcName);
    printLn("%s:", func->funcName);

  // 栈布局
    //-------------------------------// sp
    //              ra
    //-------------------------------// ra = sp-8
    //              fp
    //-------------------------------// fp = sp-16
    //             变量
    //-------------------------------// sp = sp-16-StackSize
    //           表达式计算
    //-------------------------------//

    // pre process
    // 将 ra 指针压栈，保存 ra 值
    printLn("# 将 ra 压栈，保存返回地址");
    printLn("\taddi sp, sp, -8");
    printLn("\tsd ra,0(sp)");
    // 将 fp 指针压栈，此时 fp 应当为上一级值
    printLn("# 将当前 fp 压栈，fp 属于“被调用者保存”的寄存器，需要恢复原值");
    printLn("\taddi sp,sp,-8");
    printLn("\tsd fp,0(sp)");
    // 将 fp 置为当前 sp 值
    printLn("# 将 sp 写入 fp ");
    printLn("\tmv fp,sp");
    printLn("# sp 腾出 StackSize 大小的栈空间");
    printLn("\taddi sp,sp,-%d", func->StackSize);
    // 将传入的参数映射到函数的变量中
    Type* param = func->params;
    int paraCnt = 0;
    while (param) {
      printLn("# 将传入参数赋值给 变量%s", param->var->Name);
      if (param->var->ty->size == 1) {
        printLn("\tsb a%d,%d(fp)", paraCnt,param->var->Offset); 
      }
      else 
        printLn("\tsd a%d,%d(fp)", paraCnt,param->var->Offset);
      paraCnt++;
      param = param->next;
    }
    printLn("\n# ====%s 正文部分===============", func->funcName);
    while (body) {
      genStmt(body);
      body = body->next;
    }

    // return 语句生成
    printLn("\n# ====%s程序结束===============", func->funcName);
    printLn("# return 标签");
    printLn(".L.%s.return:", func->funcName);
    // post process
    // 将栈复原
    printLn("\taddi sp,sp,%d", func->StackSize);
    // 恢复 fp
    printLn("# 恢复 fp");
    printLn("\tld fp,0(sp)");
    printLn("\taddi sp,sp,8");
    // 恢复 ra
    printLn("# 恢复 ra, sp");
    printLn("\tld ra,0(sp)");
    printLn("\taddi sp,sp,8");
    // 栈未清零则报错
    assert(Depth == 0);
    printLn("# 返回 a0 值给系统调用");
    printLn("\tret");
  }
}

static void emitData(HashTable* globals) {
  if (!globals) {
    // 若无全局变量，则直接返回，但不会因为函数名也存入了 globals 中
    return;
  }
  for (int i = 0; i < HASH_SIZE; i++) {
    Obj* var = globals->objs[i];
    while (var) {
      if (var->isFuncName) {
        var = var->next;
        continue;
      }
      printLn("# 数据段标签");
      printLn("\t.data");
      if (var->initData) {
        printLn("%s:", var->Name);
        for (int i = 0; i < var->ty->size; i++) {
          char ch = var->initData[i];
          if (isprint(ch)) 
            printLn("\t.byte %d\t# 字符：%c", ch, ch);
          else
            printLn("\t.byte %d", ch);
        }
      }
      else {
        printLn("\t.global %s", var->Name);
        printLn("# 全局变量 %s", var->Name);
        printLn("%s:", var->Name);
        printLn("# 零填充 %d 位", var->ty->size);
        printLn("\t.zero %d", var->ty->size);
      }
      var = var->next;
    }
  }
}
void genCode(Program* prog, FILE* out) {
  outFile = out;
  Function* funcs = prog->funcs;
  // 生成数据
  emitData(prog->globals);
  while (funcs) {
    codeGen(funcs);
    funcs = funcs->next;
  }
}