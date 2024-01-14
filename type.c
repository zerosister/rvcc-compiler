#include "rvcc.h"

//  (Type){...} 构造一个复合字面量，相当于 Type 的匿名变量
Type *TypeInt = &(Type){TY_INT};

// 判断 Type 是否为 int 类型
bool isInteger(Type *ty) { return ty->tyKind == TY_INT; }

// 判断 Type 是否为 ptr 类型 
bool isPtr(Type *ty) { return ty->tyKind == TY_PTR; }

// 确定 type 为指针，并指向基类
Type *pointerTo(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->tyKind = TY_PTR;
  ty->base = base;
  return ty;
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
    // 结点类型设为 指针指向的类型，若解引用所指向的非指针，则报错
    case TK_DEREF:
      if (node->LNode->ty->tyKind != TY_PTR) 
        errorTok(node->token, "Warrior~,invalid pointer dereference");
      node->ty = node->LNode->ty->base;
    default:
      break;
  }
}