#include "rvcc.h"

// 输入的字符串
static char* currentInput;
// 当前文件
static char* currentFile;

/************************** 错误处理 **************************/
// 运行程序时参数出错
void error(char* Fmt, ...) {
  va_list VA;
  va_start(VA, Fmt);
  vfprintf(stderr, Fmt, VA);
  fprintf(stderr, "\n");
  va_end(VA);
  exit(1);
}

// 输出错误出现位置，并退出
void verrorAt(int lineNum, char* loc, char* Fmt, va_list VA) {
  // 获取某行第一个字符
  char* first = loc;
  // first 递减到当前行最开始位置
  while (currentInput < first && first[-1] != '\n') 
    first --;
  // 该行的末尾
  char* end = loc;
  while (*end != '\n') 
    end ++;
  // 输出源信息
  fprintf(stderr, "%s Line %d:", currentFile, lineNum);
  for (char* cur = first; cur < end; cur++) {
    fprintf(stderr, "%c", *cur);
  }
  // 输出出错信息
  // 计算出错位置，loc 为出错位置指针，first 为当前输入行首地址
  int pos = loc - first;
  // 计算行号占了几个字符
  int posNum = 0;
  while (lineNum > 0) {
    lineNum /= 10;
    posNum ++;
  }
  pos += strlen(currentFile) + posNum + 7;
  // 将字符串补齐 pos 位，%*s 允许传递一个整数参数输出相应长度空格:)
  fprintf(stderr, "\n%*s", pos, "");
  fprintf(stderr, "^ ");
  // 报错信息
  vfprintf(stderr, Fmt, VA);
  fprintf(stderr, "\n");
  va_end(VA);
  exit(1);
}

// 分词 (tokenize) 时出错
void errorAt(char* loc, char* Fmt, ...) {
  va_list VA;
  va_start(VA, Fmt);
  char* p = currentInput;
  int lineNum = 1;
  while (p < loc) {
    if (*p == '\n') {
      lineNum ++;
    }
  }
  verrorAt(lineNum, loc, Fmt, VA);
}

// 语法分析出错
void errorTok(Token* token, char* Fmt, ...) {
  va_list VA;
  va_start(VA, Fmt);
  verrorAt(token->lineNum, token->loc, Fmt, VA);
}

/********************* 分词 ***********************/
// 生成新的 Token
static Token* newToken(TokenKind kind, int val, char* loc, int len) {
  // calloc 有一个特性为会将内存初始化为 0，所以 token->next 自动置为 NULL
  Token* token = calloc(1, sizeof(Token));
  token->kind = kind;
  token->val = val;
  token->loc = loc;
  token->len = len;
  return token;
}

// 确定究竟为哪一种 punctuation
static int specify_puntc(char* p) {
  switch (*p) {
    case '+':
      return TK_ADD;
    case '-':
      return TK_SUB;
    case '*':
      return TK_MUL;
    case '/':
      return TK_DIV;
    case '(':
      return TK_LBR;
    case ')':
      return TK_RBR;
    case '!':
      if (*(p + 1) == '=')
        return TK_NEQ;
      else
        return TK_NOT;
    case '=':
      if (*(p + 1) == '=')
        return TK_DEQ;
      else
        return TK_ASS;
    case '>':
      if (*(p + 1) == '=')
        return TK_BGE;
      else
        return TK_BGT;
    case '<':
      if (*(p + 1) == '=')
        return TK_LSE;
      else
        return TK_LST;
    case ';':
      return TK_SEM;
    case '{':
      return TK_LBB;
    case '}':
      return TK_RBB;
    case '&':
      return TK_ADDR;
    case ',':
      return TK_COM;
    case '[':
      return TK_LMB;
    case ']':
      return TK_RMB;
    case '"':
      return TK_DQU;
    default:
      errorAt(p, "Darling T.T ~~ I can't deal with this punctuation");
  }
  return 1;
}

// 判断 token 值是否为指定值
bool equal(Token* token, char* str) {
  // LHS 为左字符串，RHS 为右字符串
  // memcmp 按照字典序比较 LHS<RHS 返回负值，=返回 0,>返回正值
  return memcmp(token->loc, str, token->len) == 0 && str[token->len] == '\0';
}

// 期待得到一个数字，否则报错
static int getNumber(Token* token) {
  if (token->kind != TK_NUM) {
    errorTok(token, "Honey~,here expecte a number");
  } else
    return token->val;
  return 1;
}

// 读取到字符串字面量的结尾
static char* strLiteralEnd(char* p) {
  char* start = p;
  while (*p != '"') {
    // 未到字符串终结
    if (*p == '\n' || *p == '\0') {
      errorAt(p, "Szro~,不正确的字符串字面量 <-_->\n");
    }
    if (*p == '\\') {
      // 若为转义符则跳过下一个字符
      p += 1;
    }
    p += 1;
  }
  return p;
}

static int spec_Hex(char* ch) {
  if (*ch >= '0' && *ch <= '9') 
    return *ch - '0';
  if (*ch >= 'A' && *ch <= 'F') 
    return *ch - 'A' + 10;
  if (*ch >= 'a' && *ch <= 'f') 
    return *ch - 'a' + 10;
  errorAt(ch, "Yanzu~, Maybe not a hex");
}

static int specify_char(char** pos, char* ch) {
  // 识别八进制 \abc = (((a*8)+b)*8)+c 最多 3 位
  if (*ch >= '0' && *ch <= '7') {
    int num = *ch - '0';
    ch++;
    if (*ch >= '0' && *ch <= '7') {
      num = num * 8 + (*ch - '0');
      if (*(ch + 1) >= '0' && *(ch + 1) <= '7') {
        ch++;
        num = num * 8 + (*ch - '0');
      }  
    }
    *pos = ch;
    return num;
  }

  if (*ch == 'x') {
    // 进入十六进制识别
    int cnt = 1;
    int num = spec_Hex(++ch);
    while (isxdigit(*(++ch))) {
      num = (num << 4) + spec_Hex(ch);
    }
    *pos = ch - 1;
    return num;
  }
  
  *pos = *pos + 1;
  switch (*ch) {
    case 'a': // 响铃（警报）
      return '\a';
    case 'b': // 退格
      return '\b';
    case 't': // 水平制表符，tab
      return '\t';
    case 'n': // 换行
      return '\n';
    case 'v': // 垂直制表符
      return '\v';
    case 'f': // 换页
      return '\f';
    case 'r': // 回车
      return '\r';
    // 属于 GNU C 拓展
    case 'e': // 转义符
      return 27;
    default: // 默认将原字符返回
      return *ch;
  }
}

// 识别字符串字面量
static Token* strLiteral(char* p) {
  // 吸收 "
  p = p + 1;
  char* start = p;
  char* end = strLiteralEnd(start);
  char* buf = calloc(1, end - start);
  int len = 0;
  for (; p < end; p++) {
    if (*p == '\\') {
      buf[len++] = specify_char(&p, p + 1);
    }
    else
      buf[len++] = *p;
  }
  Token* token = newToken(TK_CHL, 0, start, end - start);
  token->str = buf;
  return token;
}

// skip 期待得到指定符号
Token* skip(Token* token, char* str) {
  if (!equal(token, str)) errorTok(token, "Sweety~,expected %s", str);
  return token->next;
}

bool startsWith(char* Str, char* SubStr) {
  // 比较两字符串前 N 个字符是否相等
  return strncmp(Str, SubStr, strlen(SubStr)) == 0;
}

// 判断标记符首字母规则
// [a-zA-Z_]
static bool isIdent1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z' || c == '_');
}

// 判断标记符其余字符规则
// [0-9a-zA-Z_]
static bool isIdent2(char c) { return isIdent1(c) || ('0' <= c && c <= '9'); }

// 判断是否为关键字
static int specify_keyWord(Token* token) {
  static char* keyWords[] = {"return", "if", "else", "for", "while", "int", "sizeof", "char"};
  // 指针数组大小 / 指针大小 = 指针个数
  for (int i = 0; i < sizeof(keyWords) / sizeof(*keyWords); i++) {
    if (equal(token, keyWords[i])) {
      switch (i) {
        case 0:
          return TK_RET;
        case 1:
          return TK_IF;
        case 2:
          return TK_ELS;
        case 3:
          return TK_FOR;
        case 4:
          return TK_WHI;
        case 5:
          return TK_INT;
        case 6:
          return TK_SIZEOF;
        case 7:
          return TK_CHAR;
        default:
          break;
      }
    }
  }
  return -1;
}

// 添加行号
static void addLineNum(Token* token) {
  char* p = currentInput;
  int n = 1;
  do {
    if (p == token->loc) {
      token->lineNum = n;
      token = token->next;
    }
    if (*p == '\n') 
      n ++;
  } while (*p++);
}

// 终结符解析
static Token* tokenize(char* p) {
  // 使用一个链表进行存储各个 Token
  // head 表示链表头
  currentInput = p;
  // 也可以用 Token head = {}; 相当于赋初值全为 0
  // Token* head = newToken(2,0,0,0);
  Token head = {};
  Token* cur = &head;
  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }

    // 跳过行注释
    if (startsWith(p, "//")) {
      p += 2;
      while (!(startsWith(p, "\n"))) {
        p ++;
      }
      continue;
    }

    // 跳过块注释
    if (startsWith(p, "/*")) {
      p += 2;
      while (!(startsWith(p, "*/"))) {
        p ++;
      }
      p += 2;
      continue;
    } 

    if (isdigit(*p)) {
      char* startloc = p;
      // 获得数值的大小 (absulute)
      int val = strtol(p, &p, 10);
      // token 长度
      int len = p - startloc;
      cur->next = newToken(TK_NUM, val, startloc, len);
      cur = cur->next;
      continue;
    }
    if (ispunct(*p)) {
      // 识别运算符
      int tk_kind = specify_puntc(p);
      int len = 1;
      switch (tk_kind) {
        case TK_DEQ:
        case TK_NEQ:
        case TK_BGE:
        case TK_LSE:
          len = 2;
          break;
        case TK_DQU:
          cur->next = strLiteral(p);
          cur =  cur->next;
          // 吸收 "
          p = cur->loc + cur->len + 1;
          continue;
        default:
          break;
      }
      cur->next = newToken(tk_kind, 0, p, len);
      cur = cur->next;
      p += cur->len;
      continue;
    }
    if (isIdent1(*p)) {
      char* startloc = p;
      p++;
      while (isIdent2(*(p))) {
        p++;
      }
      cur->next = newToken(TK_VAR, 0, startloc, p - startloc);
      // 此时检查是否为关键字
      int keyWord = specify_keyWord(cur->next);
      if (keyWord > -1) {
        // 识别为关键字
        cur->next->kind = keyWord;
      }
      cur = cur->next;
      continue;
    }
    // 识别到非法字符
    errorAt(p, "Baby~,invalid input");
  }
  cur->next = newToken(TK_EOF, 0, p, 0);

  // 为 token 添加行号
  addLineNum(head.next);
  // 因为头节点不存储信息故返回 head->next
  return head.next;
}

// 读文件
static char* readFile(char* path) {
  currentFile = path;
  FILE* fp;

  if (strcmp(path, "-") == 0) {
    // 若文件名为 "-",则从 stdin 中读取 
    fp = stdin;
  }
  else {
    fp = fopen(path, "r");
    if (!fp) {
      // errno 为系统最后一次的错误代码
      // strerror 以字符串形式输出错误代码
      error("Precious~, cannot open %s: %s", path, strerror(errno));
    }
  }

  // 要返回的字符串
  char* buf;
  size_t bufLen;
  FILE* out = open_memstream(&buf, &bufLen);

  // 读取整个文件
  while (true) {
    char buf2[4096];
    // fread 从文件流中读取数据至数组
    // 缓冲数组 buf2 先按照元素大小 1 读取 fp，n 为返回实际读取的元素个数
    int n = fread(buf2, 1, sizeof(buf2), fp);
    if (n == 0) 
      // 表示已经读完整个文件
      break;
    // 数组指针 buf2，数组元素大小 1，实际元素个数 n, 文件流指针
    fwrite(buf2, 1, n, out);
  }

  // 完成读取，非标准输入的文件全部关闭
  if (fp != stdin) 
    fclose(fp);
  
  // 刷新流的输出缓冲区，确保内容输出至流中
  fflush(out);
  // 确保最后一行以 '\n' 结尾
  if (bufLen == 0 || buf[bufLen - 1] != '\n') {
    // 将字符输出至流中
    fputc('\n', out);
  }
  fputc('\0', out);
  fclose(out);
  return buf;
}

// 对文件进行分词
Token* tokenizeFile(char* path) { return tokenize(readFile(path)); }