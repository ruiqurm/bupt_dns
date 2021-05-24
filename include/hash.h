#pragma once
#include<stdlib.h>
#include<math.h>
#include<stdbool.h>
// # include <pthread.h>

//找到比n大的最小质数
int find_smallest_prime(int n);
struct hash_node {
  struct hash_node *next;
  void* record;
  // unsigned id;
};
typedef unsigned (*HASHTABLE_hash_function)(void *label);
typedef bool (*HASHTABLE_compare_function)(void* record,void*label2);//!!!
typedef struct hash_node *(*HASHTABLE_get_function)(const void* label);
typedef void *(*HASHTABLE_set_function)(const void*label,const void* data);
typedef void *(*HASHTABLE_reset_function)(const void*label);



struct hashtable{
  struct hash_node* nodes;
  int size;
  HASHTABLE_hash_function hash;
  HASHTABLE_compare_function compare;
};

void init_hashtable(struct hashtable* table,int size,HASHTABLE_hash_function hash,
HASHTABLE_compare_function compare);
struct hash_node* get_hash_node(struct hashtable* table,void *label,bool*is_find);


int reset_hash_node(struct hashtable* table,void *label);
struct hash_node* set_hash_node(struct hashtable* table,void *label,void*record,bool* exist);


void free_hashtable(struct hashtable* table);
