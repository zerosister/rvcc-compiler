#include "rvcc.h"
unsigned int hash(char *Name, int size, int len) {
  unsigned int hashValue = 0;
  for (int i = 0; i < len; i++) {
    // 一种简便的计算 str hash 值方法
    hashValue = (hashValue << 5) + *Name++;
  }
  return hashValue % size;
}

Obj *insert(HashTable *hashTable, char *Name, int len) {
  int index = hash(Name, HASH_SIZE, len);
  Obj *newObj = (Obj *)calloc(1, sizeof(Obj));
  newObj->Name = strndup(Name, len);  // 复制字符串键
  newObj->value = index;
  // 头插法插入哈希桶
  newObj->next = hashTable->objs[index];
  hashTable->objs[index] = newObj;
  hashTable->size++;
  return newObj;
}

Obj *search(HashTable *hashTable, char *Name, int len) {
  int index = hash(Name, HASH_SIZE, len);
  Obj *current = hashTable->objs[index];
  while (current != NULL) {
    if (strncmp(current->Name, Name, len) == 0) {  // 比较字符串键
      return current;
    }
    current = current->next;
  }
  return NULL;  // Key not found
}

void remove_hash(HashTable *hashTable, char *Name) {
  int index = hash(Name, HASH_SIZE, strlen(Name));
  Obj *current = hashTable->objs[index];
  Obj *prev = NULL;
  while (current != NULL) {
    if (strcmp(current->Name, Name) == 0) {  // 比较字符串键
      if (prev == NULL) {
        hashTable->objs[index] = current->next;
      } else {
        prev->next = current->next;
      }
      free(current->Name);  // 释放字符串键的内存
      free(current);        // 释放结构体的内存
      hashTable->size--;
      return;
    }
    prev = current;
    current = current->next;
  }
}