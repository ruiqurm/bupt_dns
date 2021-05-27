#pragma once
#include<stdlib.h>
#include<math.h>
#include<stdbool.h>


typedef struct hashnode{
  struct hashnode *next;
  void* record;
}hashnode;

/**
 * @brief 哈希函数
 * @param key 哈希键值
 * @return 一个非负整数
 */
typedef unsigned (*HASHTABLE_hash_function)(void *key);

/**
 * @brief 比较一个存储结构体和label是否相同
 * @param record 结构体指针
 * @param key 键值指针
 * @return 真或假
 */
typedef bool (*HASHTABLE_compare_function)(void* record,void*key);



typedef struct{
  hashnode* nodes;
  double filling_factor;
  HASHTABLE_hash_function hash;
  HASHTABLE_compare_function compare;
  int size;
}hashtable;

/**
 * @brief 初始化哈希表
 * 
 * @param table 哈希表
 * @param size 大小
 * @param filling_factor 填充因子
 * @param hash 哈希函数
 * @param compare 比较结构体与label的函数。
 */
bool init_hashtable(hashtable* table,int size,double filling_factor,HASHTABLE_hash_function hash,
HASHTABLE_compare_function compare);

/**
 * @brief 获取哈希节点
 * 
 * @param table 哈希表
 * @param key 哈希键
 * @param is_find 是否找到，需要传入的是变量的指针;可以传入NULL
 * @return hashnode* hash节点
 */
hashnode* get_hashnode(hashtable* table,void *key,bool*is_find);

/**
 * @brief 重置哈希数据
 * 
 * @param table 哈希表
 * @param key 哈希键值
 * @return 是否重置成功
 */
bool reset_hash_node(hashtable* table,void *key);

/**
 * @brief 设置哈希表数据
 * 
 * @param table 哈希表
 * @param key 哈希键值
 * @param record 数据指针；如果传入NULL，并且数据存在哈希表中，那么NULL不会覆盖数据
 * @param exist 是否存在，需要传入变量指针；可以传入NULL
 * @return 返回设置的哈希节点
 */
hashnode* set_hashnode(hashtable* table,void *key,void*record,bool* exist);

/**
 * @brief 释放内存
 * 
 * @param table 哈希表
 */
void free_hashtable(hashtable* table);

/**
 * @brief 扩展哈希表
 * 
 * @param table 哈希表
 * @param new_size 新的哈希表大小
 */
// void expand_hashtable(hashtable* table,int new_size);