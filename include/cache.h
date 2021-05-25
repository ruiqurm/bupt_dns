#pragma once
#include "common.h"
#include"hash.h"
#include<time.h>
#include<stdbool.h>
/*************************
 *                       *
 *        常量和宏        *
 *                       *
 *************************/

// LRU链表表项数量
#define LRU_BUFFER_LENGTH 1000

//装填因子0.7
#define CACHE_FILLING_FACTOR 0.7

//缓存大小
#define CACHE_SIZE 102400 // 100K;


//每次POP移出的项数量(测试中对性能影响不大)
#define LRU_POP_ITEM 10

/**
 * @brief 哈希函数
 * @param a 域名
 */

// #define hash_label(a) _hash_label(a, HASH_TABLE_LENGTH)

/*************************
 *                       *
 *        结构体          *
 *                       *
 *************************/

//记录类型
struct record_data {
  char label[MAX_LABEL_LANGTH];
  struct IP ip;
  time_t ttl; //过期时间
};
struct record {
  struct record_data data;
  int next, last;
  // unsigned id;
  bool valid;
};
typedef struct link_list {
  // struct record records[LRU_BUFFER_LENGTH + 1];
  // unsigned int stack[LRU_BUFFER_LENGTH]; //剩余位置栈
  struct record* records;
  unsigned int *stack;
  int stack_top;                             //栈顶指针
  size_t length;                             //队列长度
  size_t max_length;
  size_t used_size;                          //所占空间
  size_t max_size;                           //最大空间
  int first, last; //头指针指向第一个元素，尾指针指向新元素
  // bool is_init;    //是否初始化
  // struct record_data *(*get_by_label)(const char *label);
  // int (*set)(const char *label, const struct IP *ip);
  hashtable label_hash;
} cache;
/*************************
 *                       *
 *        函数接口        *
 *                       *
 *************************/

/**
 * @brief 初始化cache
 *
 */
void init_cache(cache*Cache,int record_length,int max_size,double filling_factor);
void init_default_cache(cache*Cache);

/**
 * @brief 获取cache数据
 *
 * @param label 标签
 * @return record_data 结构体
 */
struct record_data *get_cache(cache* Cache,const char *label);

/**
 * @brief cache放入数据
 *
 * @param label label名称
 * @param ip ip
 * @param ttl time to live
 * @return 返回1则成功，否则失败
 */
int set_cache(cache*Cache,const char *label, const struct IP *ip, time_t ttl);

// /**
//  * @brief 清除cache
//  */
// void clear_cache();

/*
 * 测试用函数
 */
void test_normal(cache* Cache); //检查cache是否正常
