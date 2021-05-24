#include "hash.h"
#include<stdio.h>

//找到比n大的最小质数
int find_smallest_prime(int n){
    while(true){
      int sqrt_n = ceil(sqrt(n));
      if(1==n){
          n++;
          continue;
      }
      int flag =0;
      for(int i=2; i<=sqrt_n; ++i){
          if(n%i == 0)
              flag = 1;
              break;
      }
      if(!flag)break;
      else n++;
    }
    return n;
}
// struct hash_node {
//   struct hash_node *next;
//   void* record;
// };
typedef unsigned (*HASHTABLE_hash_function)(void *label);
typedef bool (*HASHTABLE_compare_function)(void* record,void*label2);//!!!
typedef struct hash_node *(*HASHTABLE_get_function)(const void* label);
typedef void *(*HASHTABLE_set_function)(const void*label,const void* data);
typedef void *(*HASHTABLE_reset_function)(const void*label);



// struct hashtable{
//   struct hash_node* nodes;
//   int size;
//   HASHTABLE_hash_function hash;
//   HASHTABLE_compare_function compare;
// };

void init_hashtable(struct hashtable* table,int size,HASHTABLE_hash_function hash,
HASHTABLE_compare_function compare){
  size = size / 0.7;
  table->size = find_smallest_prime(size);
  table->nodes = (struct hash_node*)calloc(table->size,sizeof(struct hash_node));
  table->hash = hash;
  table->compare = compare;
  // pthread_mutex_unlock(&table->lock);
}
struct hash_node* get_hash_node(struct hashtable* table,void *label,bool*is_find){
  #if DEBUG == 1
  printf("get_node:%s\n",label);
  int count =0;
  #endif

  unsigned pos = table->hash(label) % table->size;
  struct hash_node * node= &table->nodes[pos],*last;
  while (node->next) { //头结点
    last = node;
    node = node->next;
    if (table->compare(node->record,label)) {
       if (is_find){
         *is_find = 1;
       }
       return node;
    }
  }
  if (is_find)
    *is_find = 0;
  return node;
}

int reset_hash_node(struct hashtable* table,void *label){
  #if DEBUG == 1
  printf("reset_node:%s",label);
  #endif
  unsigned pos = table->hash(label);
  struct hash_node *node, *last;
  node = &table->nodes[pos];
  while (node->next) {
    last = node;
    node = node->next;
    if (table->compare(node->record,label)) {
    // if (!strcmp(node->record->data.label, label)) {
      last->next = node->next;
      free(node);
      return 1;
    }
  }
  return 0;
}
struct hash_node* set_hash_node(struct hashtable* table,void *label,void*record,
                                bool* exist){
  #if DEBUG == 1
  printf("set_node:%s\n",label);
  #endif

  bool is_find =false;
  struct hash_node* node = get_hash_node(table,label,&is_find);
  if(exist)*exist=is_find;
  if (is_find){
    if(record!=NULL)node->record = record;
  }else{
    node = node->next = (struct hash_node *)malloc(sizeof(struct hash_node));
    node->next = NULL;//现在node是最后一个节点
    // node->id = table->hash(label) % table->size;
    //！！！即使record==NULL也会照样赋值。
    node->record = record;
  }
  return node;
}


void free_hashtable(struct hashtable* table){
  free(table->nodes);
}
