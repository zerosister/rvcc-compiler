#include "rvcc.h"

/************************** 主体部分 ***************************/
int main(int argc, char** argv) {
  // argv[0] 为程序名称，argv[1] 为传入第一个参数...依此类推
  if (argc != 2) {
    // 参数异常处理
    error("%s:  invalid number of arguments", argv[0]);
  }
  // argv[1] = "int main() { return ({ 1; }) + ({ 2; }) + ({ 3; }); }";
  // 分词
  Token* token = tokenize(argv[1]);
  // 语法分析
  Token** rest = &token;
  Program* root = parse(rest, token);
  assert((*rest)->kind == TK_EOF);
  // 代码生成
  genCode(root);
  return 0;
}
