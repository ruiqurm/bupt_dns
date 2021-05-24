#pragma once
#include "common.h"
#include "time.h"

/*************************
 *                       *
 *        常量和宏        *
 *                       *
 *************************/

// LRU大小
#define MAX_LRU_BUFFER_LENGTH 1000

//缓存大小
#define MAX_LRU_CACHE 102400 // 100K;

//哈希表大小，这里取质数，装填因子约为0.7
#define HASH_TABLE_LENGTH 1453

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

/*************************
 *                       *
 *        函数接口        *
 *                       *
 *************************/

/**
 * @brief 初始化cache
 *
 */
void init_cache();
/**
 * @brief 获取cache数据
 *
 * @param label 标签
 * @return record_data 结构体
 */
struct record_data *get_cache(const char *label);

/**
 * @brief cache放入数据
 *
 * @param label label名称
 * @param ip ip
 * @param ttl time to live
 * @return 返回1则成功，否则失败
 */
int set_cache(const char *label, const struct IP *ip, time_t ttl);

/**
 * @brief 清除cache
 */
void clear_cache();

/*
 * 测试用函数
 */
void test_normal(); //检查cache是否正常
