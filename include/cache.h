#pragma once
#include "common.h"
#define MAX_LRU_BUFFER_LENGTH 1000
//循环队列最多只能用N-1个位置
#define MAX_LRU_CACHE 102400
//100K;
// #define HASH_LOAD_FACTOR 0.7
#define HASH_TABLE_LENGTH 1453


struct record_data* get_cache(const char* label);
int set_cache(const char* label,const struct IP* ip);
void clear_cache();
void init_list();
unsigned _hash(const char* label,size_t size);
#define hash(a) _hash(a,HASH_TABLE_LENGTH)
struct record_data{
    char label[MAX_LABEL_LANGTH];
    struct IP ip;
};  