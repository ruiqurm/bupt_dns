#pragma once
#include "common.h"
#include "time.h"
/*
* 常量
*/

//LRU大小
#define MAX_LRU_BUFFER_LENGTH 1000
//缓存大小
#define MAX_LRU_CACHE 102400 //100K;
//哈希表大小，这里取质数，装填因子约为0.7
#define HASH_TABLE_LENGTH 1453
//每次POP移出的项数量(一般对性能影响不大)
#define LRU_POP_ITEM 10 

/*
*  结构体
*/
//记录类型
struct record_data{
    char label[MAX_LABEL_LANGTH];
    struct IP ip;
    time_t ttl;//过期时间
};  

/*
* 操作cache
*/

//初始化
void init_cache();
//获取数据
struct record_data* get_cache(const char* label);
//设置数据
int set_cache(const char* label,const struct IP* ip,time_t ttl);
//清空缓存
void clear_cache();

/*
* 其他函数
*/

//哈希函数
#define hash_label(a) _hash_label(a,HASH_TABLE_LENGTH)



/*
* 测试用函数
*/
void test_normal();//检查cache是否正常
