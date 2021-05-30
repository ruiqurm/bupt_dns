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

//永久
//1<<40超过1000年
#define DNS_CACHE_PERMANENT 1099511627776
#define CACHE_INFINTE_SIZE 429496729

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
  char *label;
  struct IP ip;
  time_t ttl; //过期时间
  struct record_data* next;//链表
};
struct static_record_data{
  char* label;
  struct IP ip;
  struct static_record_data* next;
};
struct record {
  // void* data;
  void* data;
  int next, last;
  int size;
  // char label[MAX_NAME_LENGTH];
  bool valid;
};

typedef void (*realse_data)(struct record*data);
typedef int (*add_data)(struct record*record,void*data);
typedef int (*add_multi_data)(struct record*record,void*data[],int size);
typedef bool (*check_data)(struct record*data);
typedef void* (*get_data_label)(struct record*data);

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
  bool is_init;    //是否初始化
  // struct record_data *(*get_by_label)(const char *label);
  // int (*set)(const char *label, const struct IP *ip);
  hashtable label_hash;
  realse_data realse;
  add_data add;
  add_multi_data add_multi;
  check_data check;
  get_data_label get_label;
} cache;

struct staticCache{
    struct static_record_data* data;
    bool is_init;    //是否初始化
    int size;
    hashtable table;
    int top;
};

struct cacheCompound{
  cache temp;//临时
  struct staticCache local;//本地
};

struct cacheset{
  struct staticCache blacklist;
  struct cacheCompound A;
  struct cacheCompound AAAA;
};//cache集


/*************************
 *                       *
 *        函数接口        *
 *                       *
 *************************/


/**
 * @brief 初始化Cache
 * 
 * @param Cache Cache的地址
 * @param record_length Cache长度
 * @param max_size Cache最大存储空间(目前没什么用)
 * @param filling_factor 填充因子
 */
void init_cache(cache*Cache,int record_length,int max_size,double filling_factor,
               realse_data realse,add_data add,add_multi_data add_multi,
               check_data check,get_data_label get_label);


void init_A_record_cache_default(cache*Cache);
void init_A_record_cache(cache*Cache,int record_length,int max_size,double filling_factor);
//A记录with cname

/**
 * @brief 释放内存
 * 
 * @param Cache Cache对象的地址
 */
void free_cache(cache*Cache);

/**
 * @brief 获取cache数据
 *
 * @param label 标签
 * @return record_data 结构体
 */
struct record_data *get_cache_A_record(cache* Cache,const char *label);

/**
 * @brief cache放入数据
 *
 * @param label label名称
 * @param ip ip
 * @param ttl time to live
 * @return 返回1则成功，否则失败
 */
int set_cache_A_record(cache*Cache,const char *label, void*data);
int set_cache_A_multi_record(cache*Cache,const char *label, void*data[],int size);

// int set_cache_A_record(cache*Cache,const char *label, const struct IP *ip, time_t ttl);

// /**
//  * @brief 清除cache
//  */
// void clear_cache();

/*
 * 测试用函数
 */
void test_normal(cache* Cache); //检查cache是否正常



bool set_static_cache(struct staticCache* Cache,const char* label,const struct IP*ip);
void init_staticCache(struct staticCache* Cache,int size);
void free_staticCache(struct staticCache* Cache);
struct static_record_data* get_static_cache(struct staticCache* Cache,const char* label);