#include "rvcc.h"

// 格式化后返回字符串
char* format(char* Fmt, ...) {
  char* buf;
  size_t bufLen;
  // 将字符串对应内存作为 I/O 流
  FILE* out = open_memstream(&buf, &bufLen);

  va_list VA;
  va_start(VA, Fmt);
  // 向流中写入数据
  vfprintf(out, Fmt, VA);
  va_end(VA);

  fclose(out);
  return buf;
}