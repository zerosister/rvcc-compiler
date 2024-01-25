#include "rvcc.h"

// 目标文件路径
static char* optO;

// 输入文件路径
static char* inputPath;

// 输出程序使用说明
static void usage(int status) {
  fprintf(stderr, "rvcc [ -o <path>] <file>\n");
  exit(status);
}

// 解析传入参数
static void parseArgs(int argc, char** argv) {
  // 遍历所有参数
  for (int i = 1; i < argc; i++) {
    // 如果存在 help 则显示用法说明
    if (!(strcmp(argv[i], "--help"))) 
      usage(0);     // 正常退出
    
    // 解析 -o XXX 参数
    if (!(strcmp(argv[i], "-o"))) {
      // 不存在目标文件即报错
      if (!(argv[++i])) 
        usage(1);
      // 目标文件路径
      optO = argv[i];
      continue;
    }

    // 解析 -oXXX 参数
    if (!strncmp(argv[i], "-o", 2)) {
      // 目标文件路径
      optO = argv[i] + 2;
      continue;
    }

    // 解析 - 参数
    if (argv[i][0] == '-' && argv[i][1] != '\0') 
      error("Asuka~,unkown argument: %s", argv[i]);

    // 其他情况为匹配输入文件
    inputPath = argv[i];
  }

  // 不存在输入文件则报错
  if (!inputPath) 
    error("yekina~, no input files");
} 

// 打开需要写入的文件
static FILE* openFile(char* path) {
  if (!path || strcmp(path, "-") == 0)
    return stdout;
  
  // 以写入模式打开文件
  FILE* out = fopen(path, "w");
  if (!out) 
    error("Zigzag~,cannot open output file: %s: %s", path, strerror(errno));
  return out;
}

/************************** 主体部分 ***************************/
int main(int argc, char** argv) {
  // 解析参数
  parseArgs(argc, argv);

  // 分词
  Token* token = tokenizeFile(inputPath);
  
  // 语法分析
  Token** rest = &token;
  Program* root = parse(rest, token);
  assert((*rest)->kind == TK_EOF);

  // 代码生成
  FILE* out = openFile(optO);
  // .file 文件编号 文件名
  fprintf(out, ".file 1\"%s\"\n", inputPath);
  genCode(root, out);
  return 0;
}
