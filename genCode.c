#include "rvcc.h"

// 设置一个全局的函数，表示当前生成函数
static Function* current_func;
static void genStmt(Node* root);
static void genExpr(Node* root);

// 栈深度
static int Depth = 0;

// 将 a0 中值压入 sp 栈中
static void Push(void) {
  // riscv64 压栈为 8 个字节
  printf("# 压栈，将 a0 值存入栈顶\n");
  printf("\taddi sp,sp,-8\n");
  printf("\tsd a0,0(sp)\n");
  Depth++;
}

// 弹出 sp 栈中值到 reg 中
static void Pop(char *reg) {
  printf("# 弹栈，将栈顶值存入 %s\n", reg);
  printf("\tld %s,0(sp)\n", reg);
  printf("\taddi sp,sp,8\n");
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
    printf("# 获取变量 %s 的栈内地址为 %d(fp)\n", node->Var->Name,
           node->Var->Offset);
    printf("\taddi a0,fp,%d\n", node->Var->Offset);
    return;
  }
  if (node->token->kind == TK_DEREF) {
    // genCode_re(node->LNode);
    genExpr(node->LNode);
    // 正常遇到变量便会解引用
    return;
  }
  errorTok(node->token, "Da Zhang Wei says: not an lvalue");
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
    printf("# 将第 %d 个参数压栈\n", argusCnt);
    printf("\taddi sp,sp,-8\n");
    printf("\tsd a0,0(sp)\n");
    argus = argus->next;
  }
  while (argusCnt > 0) {
    printf("# 将第 %d 个参数弹栈到 a%d\n", argusCnt, argusCnt - 1);
    printf("\tld a%d,0(sp)\n", argusCnt - 1);
    printf("\taddi sp,sp,8\n");
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
      printf("# 将 %d 加载至 a0\n", token_root->val);
      printf("\tli a0,%d\n", token_root->val);
      return;
    case TK_VAR:
      // 将变量地址存入 a0
      genAddr(root);
      // 将变量 load 至 a0
      printf("# 读取 a0 中存放的地址，得到值存入 a0\n");
      printf("\tld a0,0(a0)\n");
      return;
    case TK_ADDR:
      // 获取变量地址，注意到 & 为右结合操作符，需要递归
      genAddr(root->LNode);
      return;
    case TK_DEREF:
      genExpr(root->LNode);
      // 现在需要访问的地址在 a0
      printf("# 解引用时 将结点对应变量内容加载至 a0\n");
      printf("\tld a0,0(a0)\n");
      return;
    case TK_ASS:
      // 产生地址
      genAddr(root->LNode);
      Push();
      // 产生右值
      genExpr(root->RNode);
      // 将左部地址存入 a1
      Pop("a1");
      // 将右值放入左结点变量中
      printf("# 将 a0 值，写入 a1 存放的地址中\n");
      printf("\tsd a0,0(a1)\n");
      return;
    case TK_FUNC: 
      printf("\n# 调用函数%s\n",root->funcName);
      genFuncall(root->argus);
      printf("\tcall %s\n", root->funcName);
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
        printf("# 对 a0 值取相反数\n");
        printf("\tneg a0,a0\n");
        return;
      case TK_NOT:
        // ! 操作为，若数值非零，则置为 0，若为 0，则置为 1
        // set equal zero,sltui a0, a0, 1，小于 1 当然只有 0 了
        printf("# 将 a0 置为 !a0\n");
        printf("  seqz a0, a0\n");
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
      printf("# 将 a0 + a1 写入 a0\n");
      printf("\tadd a0,a1,a0\n");
      return;
    case TK_SUB:
      printf("# 将 a1 - a0 写入 a0\n");
      printf("\tsub a0,a1,a0\n");
      return;
    case TK_MUL:
      printf("# 将 a0 * a1 写入 a0\n");
      printf("\tmul a0,a1,a0\n");
      return;
    case TK_DIV:
      printf("# 将 a1 / a0 写入 a0\n");
      printf("\tdiv a0,a1,a0\n");
      return;
    case TK_LST:
      // a1 < a0,slt:set less than:R[rd] = (R[rs1]<R[rs2])?1:0
      printf("# 判断 a1 < a0\n");
      printf("\tslt a0,a1,a0\n");
      return;
    case TK_BGE:
      // a1 >= a0，即为 slt 取反
      printf("# 判断 a1 <= a0\n");
      printf("\tslt a0,a1,a0\n");
      // 取反不能用 neg,-0 = 0
      printf("\txori a0,a0,1\n");
      return;
    case TK_BGT:
      // a1 > a0，将 a0,a1 换位即可
      printf("# 判断 a0 < a1\n");
      printf("\tslt a0,a0,a1\n");
      return;
    case TK_LSE:
      // a1 <= a0，换位同 BGE
      printf("# 判断 a0 <= a1\n");
      printf("\tslt a0,a0,a1\n");
      printf("\txori a0,a0,1\n");
      return;
    case TK_DEQ:
      // a1 == a0
      printf("# 判断 a1 == a0\n");
      printf("\txor a0,a0,a1\n");
      // equal zero?sltiu a0,a0,1
      printf("\tseqz a0,a0\n");
      return;
    case TK_NEQ:
      printf("# 判断 a1 != a0\n");
      printf("\txor a0,a1,a0\n");
      // not equal zero? sltu a0,x0,a0
      printf("\tsnez a0,a0\n");
      return;
    default:
      break;
  }
  error("apple~,we met invalid expression q_q");
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
      printf("\n# ========分支语句 %d ========\n", if_cnt);
      // 进入 if node 首先执行条件判断
      printf("\n # Condition 表达式 %d\n", if_cnt);
      genExpr(root->body);
      printf("# 若 a0 为 0，即条件失败，跳转至分支 %d 的.L.else%d 段\n", if_cnt,
             if_cnt);
      printf("\tbeqz a0, .L.else%d\n", if_cnt);
      // 进行 if 成功分支
      printf("\n# Then 语句\n");
      genStmt(root->LNode);
      // 执行完跳转 if 后语句
      printf("# 跳转到分支 %d 的.L.endIF%d 段标签\n", if_cnt, if_cnt);
      printf("\tj .L.endIf%d\n", if_cnt);
      // Else 代码块
      printf("\n # Else 语句 %d\n", if_cnt);
      printf("# 分支 %d 的.L.else%d 段标签\n", if_cnt, if_cnt);
      printf(".L.else%d:\n", if_cnt);
      // 进行 if 失败分支
      genStmt(root->RNode);
      printf("\n # 分支 %d 的.L.end%d 段标签\n", if_cnt);
      printf(".L.endIf%d:\n", if_cnt);
      return;
    }
    case TK_FOR: {
      // Node 使用信息详见 parser.c mkForNode 函数
      int for_cnt = count();
      printf("\n# ==== 循环语句 %d =============\n", for_cnt);
      // 首先执行初始化语句
      printf("\n# Init 语句\n");
      genExpr(root->init);
      // 循环开始标签
      printf("# 循环 %d 的.L.begin%d 的段标签\n", for_cnt);
      printf(".L.begin%d:\n", for_cnt);
      // 进行条件判断
      printf("\n # Condition 表达式\n");
      genExpr(root->body);
      printf("# 若 a0 为 0，跳转至循环 %d 的.L.end%d 段\n", for_cnt, for_cnt);
      printf("\tbeqz a0, .L.end%d\n", for_cnt);
      // 满足条件，执行相应语句
      printf("\n# Exe 循环执行部分\n");
      genStmt(root->RNode);
      // 执行递增语句
      printf("\n# Increment 部分\n");
      genExpr(root->LNode);
      printf("# 跳转至循环 %d 的.L.begin%d 段\n", for_cnt);
      printf("\tj .L.begin%d\n", for_cnt);
      printf("# 循环结束标签\n");
      printf(".L.end%d:\n", for_cnt);
      return;
    }
    case TK_RET: {
      // 首先将 exprStmt 代码生成
      // 注意 exprStmt 可能会产生 NULL 结点
      genExpr(root->LNode);
      // 进行汇编语句的跳转
      printf("# 跳转到 .L.%s.return\n", current_func->funcName);
      printf("\tj .L.%s.return\n", current_func->funcName);
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
#if USE_HASH
  // 读取所有变量
  for (int i = 0; i < HASH_SIZE; i++) {
    Obj *var = func->Locals->objs[i];
    while (var) {
      offset += 8;
      var->Offset = -offset;
      var = var->next;
    }
  }
#else
  // 读取所有变量
  for (Obj *var = func->Locals->locals; var; var = var->Next) {
    // 每个变量分配 8 字节
    offset += 8;
    // 记录下每个变量的栈中地址
    var->Offset = -offset;
  }
#endif
  // 将栈对齐到 16 字节
  func->StackSize = alignTo(offset, 16);
}

static void codeGen(Function* func) {
  // 每条语句一个结点，需要遍历每条语句
  current_func = func;
  Node *body = func->Body;
  assignVarOffsets(func);
  printf("\n# 定义 %s 段\n", func->funcName);
  printf("\t.global %s\n", func->funcName);
  printf("\n #====%s程序开始============\n", func->funcName);
  printf("# %s 段标签，程序入口段\n", func->funcName);
  printf("%s:\n", func->funcName);

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
  printf("# 将 ra 压栈，保存返回地址\n");
  printf("\t addi sp, sp, -8\n");
  printf("\tsd ra,0(sp)\n");
  // 将 fp 指针压栈，此时 fp 应当为上一级值
  printf("# 将当前 fp 压栈，fp 属于“被调用者保存”的寄存器，需要恢复原值\n");
  printf("\taddi sp,sp,-8\n");
  printf("\tsd fp,0(sp)\n");
  // 将 fp 置为当前 sp 值
  printf("# 将 sp 写入 fp\n ");
  printf("\tmv fp,sp\n");
  printf("# sp 腾出 StackSize 大小的栈空间\n");
  printf("\taddi sp,sp,-%d\n", func->StackSize);
  // 将传入的参数映射到函数的变量中
  Type* param = func->params;
  int paraCnt = 0;
  while (param) {
    printf("# 将传入参数赋值给 变量%s\n", param->var->Name);
    printf("\tsd a%d,%d(fp)\n", paraCnt,param->var->Offset);
    paraCnt++;
    param = param->next;
  }
  printf("\n# ====正文部分===============\n");
  while (body) {
    genStmt(body);
    body = body->next;
  }

  // return 语句生成
  printf("\n# ====%s程序结束===============\n", func->funcName);
  printf("# return 标签\n");
  printf(".L.%s.return:\n", func->funcName);
  // post process
  // 将栈复原
  printf("\taddi sp,sp,%d\n", func->StackSize);
  // 恢复 fp
  printf("# 恢复 fp\n");
  printf("\tld fp,0(sp)\n");
  printf("\taddi sp,sp,8\n");
  // 恢复 ra
  printf("# 恢复 ra, sp\n");
  printf("\tld ra,0(sp)\n");
  printf("\taddi sp,sp,8\n");
  // 栈未清零则报错
  assert(Depth == 0);
  printf("# 返回 a0 值给系统调用\n");
  printf("\tret\n");
}

void genCode(Function* prog) {
  while (prog) {
    codeGen(prog);
    prog = prog->next;
  }
}