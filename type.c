#include "rvcc.h"

//  (Type){...} 构造一个复合字面量，相当于 Type 的匿名变量
Type *TypeInt = &(Type){TY_INT, .size = 8};
Type *TypeChar = &(Type){TY_CHAR, .size = 1};

// 判断 Type 是否为 整型 类型
bool isInteger(Type *ty) { return ty->tyKind == TY_INT || ty->tyKind == TY_CHAR; }

// 判断 Type 是否为 ptr 类型，或为 array 类型
// 只要 base 非 NULL 表示为指针或数组 
bool isPtr(Type *ty) { return !(ty->base == NULL); }

// 确定 type 为指针，并指向基类
Type* pointerTo(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->tyKind = TY_PTR;
  ty->base = base;
  ty->size = 8;       // 指针占 8 字节
  return ty;
}

// 另起内存空间复制数据类型
Type* copyType(Type* ty) {
  Type* myType = calloc(1, sizeof(Type));
  *myType = *ty;
  return myType;
}

// 构建返回值类型为 ty 的函数类型
Type* funcType(Type* ty) {
  Type* func = calloc(1, sizeof(Type));
  func->tyKind = TY_FUNC;
  func->retType = ty;
  return func;
}

// 构建含有 cnt 个 ty 类型元素的数组类型
Type* arrayOf(Type* ty, int cnt) {
  Type* arrayType = calloc(1, sizeof(Type));
  arrayType->tyKind = TY_ARRAY;
  arrayType->base = ty;
  arrayType->size = ty->size * cnt;
  return arrayType;
}

// 为结点内所有需要类型检查结点添加类型
void addType(Node *node) {
  // 若结点为空或已经有值，直接返回
  if (!node || node->ty) {
    return;
  }
  
  // 递归访问结点增加类型
  addType(node->LNode);
  addType(node->RNode);
  addType(node->init);
  addType(node->body);

  switch (node->token->kind) {
    case TK_LBB:
      // 需要遍历 body 的 Compound 结点，因为 body 上方已经访问
      // 直接访问 body -> next
      Node* body = node->body->next;
      while (body) {
        addType(node->body); 
        body = body->next;
      }
      return;
    // 结点类型设为 结点左部类型
    case TK_RET:
    case TK_NOT:
    case TK_ASS:
    case TK_SUB:
    case TK_ADD:
    case TK_MUL:
    case TK_DIV:
      node->ty = node->LNode->ty;
      return;
    // 结点类型设为 int
    case TK_DEQ:
    case TK_NEQ:
    case TK_LST:
    case TK_LSE:
    case TK_BGE:
    case TK_BGT:
    case TK_NUM:
    case TK_FUNC:
      node->ty = TypeInt;
      return;
    // 结点类型设为 变量类型
    case TK_VAR:
      node->ty = node->Var->ty;
      return;
    // 结点类型设为 指针，并指向左部类型
    case TK_ADDR:
      node->ty = pointerTo(node->LNode->ty);
      return;
    // 结点类型设为 指针指向的类型，若解引用所指向的非指针或数组，则报错
    case TK_DEREF:
      if (!(isPtr(node->LNode->ty))) 
        errorTok(node->token, "Warrior~,invalid pointer dereference");
      node->ty = node->LNode->ty->base;
    default:
      break;
  }
}