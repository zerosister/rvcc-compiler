#include "rvcc.h"

// 栈深度
static int Depth = 0;

// 将 a0 中值压入 sp 栈中
static void Push(void) {
  // riscv64 压栈为 8 个字节
  printf("\taddi sp,sp,-8\t#push\n");
  printf("\tsd a0,0(sp)\n");
  Depth++;
}

// 弹出 sp 栈中值到 reg 中
static void Pop(char *reg) {
  printf("\tld %s,0(sp)\t#pop\n", reg);
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
    printf("\taddi a0,fp,%d\t#store var\n", node->Var->Offset);
    return;
  }
  error("Da Zhang Wei says: not a variable");
}

// 计数程序
static int count(){
  static int cnt = 0;
  cnt += 1;
  return cnt;
}

// 代码生成：遍历结点
static void genCode_re(Node *root) {
  // 深度优先遍历 DFS left -> right -> root

  // 如果为 NULL 则不生成任何代码
  if (!root)  return;

  Token *token_root = root->token;
  if (token_root->kind == TK_NUM) {
    printf("\tli a0,%d\t#load num\n", token_root->val);
    return;
  }
  if (token_root->kind == TK_VAR) {
    // 将变量地址存入 a0
    genAddr(root);
    // 将变量 load 至 a0
    printf("\tld a0,0(a0)\n");
    return;
  }
  if (token_root->kind == TK_RET) {
    // 首先将 exprStmt 代码生成
    // 注意 exprStmt 可能会产生 NULL 结点
    genCode_re(root->LNode);  
    // 进行汇编语句的跳转
    printf("\tj .L.return\n");
    return;
  }
  if (token_root->kind == TK_LBB) {
    // 表明进入 compound, 需要遍历其 body
    Node* body = root->body;
    while (body) {
      genCode_re(body);
      body = body->next;
    }
    return;
  }
  //  Node 使用信息详见 parser.c mkIfNode 函数
  if (token_root->kind == TK_IF) {
    int if_cnt = count();
    // 进入 if node 首先执行条件判断
    genCode_re(root->body);
    printf("\tbeqz a0, .L.else%d\n", if_cnt);
    // 进行 if 成功分支
    genCode_re(root->LNode);
    printf("\tj .L.endIf%d\n", if_cnt);
    printf("\t.L.else%d:\n", if_cnt);
    // 进行 if 失败分支
    genCode_re(root->RNode);
    printf("\t.L.endIf%d:\n", if_cnt);
    return;
  }
  
  // Node 使用信息详见 parser.c mkForNode 函数
  if (token_root->kind == TK_FOR) {
    int for_cnt = count();
    // 首先执行初始化语句
    genCode_re(root->init);
    // 循环开始标签
    printf("\t.L.begin%d:\n", for_cnt);
    // 进行条件判断
    genCode_re(root->body);
    printf("\tbeqz a0, .L.end%d\n", for_cnt);
    // 满足条件，执行相应语句
    genCode_re(root->RNode);
    // 执行递增语句
    genCode_re(root->LNode);
    printf("\tj .L.begin%d\n", for_cnt);
    printf("\t.L.end%d:\n", for_cnt);
    return;
  }

  if (token_root->kind == TK_WHI) {
    int whi_cnt = count();
    printf("\t.L.begin%d:\n", whi_cnt);
    // 执行条件判断
    genCode_re(root->body);
    printf("\t beqz a0, .L.end%d\n", whi_cnt);
    // 满足条件，执行相应语句
    genCode_re(root->RNode);
    printf("\t j .L.begin%d\n", whi_cnt);
    printf("\t.L.end%d:\n", whi_cnt);
    return;
  }
  // 当前为操作符，递归遍历
  if (!root->LNode)
    errorTok(token_root, "Juliet~,An operand is losed when generating code~");
  genCode_re(root->LNode);
  // 左操作数入栈
  Push();

  if (!root->RNode) {
    // 可能为单元运算符 - 或 !，二者操作不一样
    Pop("a0");
    if (token_root->kind == TK_SUB) {
      // neg a0, a0 是 sub a0, x0, a0 的别名，即 a0=0-a0
      printf("\tneg a0,a0\n");
      return;
    } else if (token_root->kind == TK_NOT) {
      // ! 操作为，若数值非零，则置为 0，若为 0，则置为 1
      // set equal zero,sltui a0, a0, 1，小于 1 当然只有 0 了
      printf("  seqz a0, a0\n");
      return;
    }
    errorTok(token_root, "Juliet~,An operand is losed when generating code~");
  } else
    // 右操作数本身会在 a0
    genCode_re(root->RNode);

  // 将左结点值 pop 至 a1 中
  Pop("a1");
  switch (token_root->kind) {
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
      // a1 >= a0，即为 slt 取反
      printf("\tslt a0,a1,a0\n");
      // 取反不能用 neg,-0 = 0
      printf("\txori a0,a0,1\n");
      return;
    case TK_BGT:
      // a1 > a0，将 a0,a1 换位即可
      printf("\tslt a0,a0,a1\n");
      return;
    case TK_LSE:
      // a1 <= a0，换位同 BGE
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
      // 右值当前在 a0，需要移动到 a1
      printf("\tmv a1,a0\n");
      // 将左结点地址放入 a0 寄存器
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
static void assignVarOffsets(Function *prog) {
  int offset = 0;
#if USE_HASH
  // 读取所有变量
  for (int i = 0; i < HASH_SIZE; i++) {
    Obj *var = prog->Locals->objs[i];
    while (var) {
      offset += 8;
      var->Offset = -offset;
      var = var->next;
    }
  }
#else
  // 读取所有变量
  for (Obj *var = prog->Locals->locals; var; var = var->Next) {
    // 每个变量分配 8 字节
    offset += 8;
    // 记录下每个变量的栈中地址
    var->Offset = -offset;
  }
#endif
  // 将栈对齐到 16 字节
  prog->StackSize = alignTo(offset, 16);
}

void genCode(Function *prog) {
  // 每条语句一个结点，需要遍历每条语句
  Node *body = prog->Body;
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
  // 将 fp 指针压栈，此时 fp 应当为上一级值
  printf("\taddi sp,sp,-8\n");
  printf("\tsd fp,0(sp)\n");
  // 将 fp 置为当前 sp 值
  printf("\tmv fp,sp\n");
  printf("\taddi sp,sp,-%d\n", prog->StackSize);
  while (body) {
    genCode_re(body);
    body = body->next;
  }

  // return 语句生成
  printf("\t.L.return:\n");
  // post process
  // 将栈复原
  printf("\taddi sp,sp,%d\n", prog->StackSize);
  // 恢复 fp
  printf("\tld fp,0(sp)\n");
  printf("\taddi sp,sp,8\n");
  // 栈未清零则报错
  assert(Depth == 0);
  printf("\tret\n");
}