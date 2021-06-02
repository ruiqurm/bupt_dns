#include "hash.h"
#include<math.h>
#include<stdio.h>

//找到比n大的最小质数
static int find_smallest_prime(int n){
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



typedef hashnode *(*HASHTABLE_get_function)(const void* label);
typedef void *(*HASHTABLE_set_function)(const void*label,const void* data);
typedef void *(*HASHTABLE_reset_function)(const void*label);



bool init_hashtable(hashtable* table,int size,double filling_factor,HASHTABLE_hash_function hash,
HASHTABLE_compare_function compare){
  size = size / filling_factor;
  if(filling_factor<=0||filling_factor>1)return false;
  table->filling_factor = filling_factor;
  table->size = find_smallest_prime(size);
  table->nodes = (hashnode*)calloc(table->size,sizeof(hashnode));
  table->hash = hash;
  table->compare = compare;
  return true;
}
hashnode* get_hashnode(hashtable* table,void *label,bool*is_find){
  // #if DEBUG == 1

  // int count =0;
  // #endif

  unsigned pos = table->hash(label) % table->size;
  hashnode * node= &table->nodes[pos],*last;
  // printf("pos=%d\n",pos);
  if((struct record*)node->record){
    printf("error!");
    exit(0);
  }
  // printf(" has next %d\n",node->next);
  while (node->next) { //头结点
    last = node;
    node = node->next;
    // printf("check %lld\n",node->record);
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

bool reset_hash_node(hashtable* table,void *label){
  // #if DEBUG == 1
  
  // #endif
  unsigned pos = table->hash(label) % table->size;
  hashnode *node, *last;
  node = &table->nodes[pos];
  while (node->next) {
    last = node;
    node = node->next;
    if (table->compare(node->record,label)) {
      last->next = node->next;
      free(node);
      return 1;
    }
  }
  return 0;
}
hashnode* set_hashnode(hashtable* table,void *label,void*record,
                                bool* exist){
  // #if DEBUG == 1
  // printf("set_node:%d\n",*(int*)label);
  // #endif

  bool is_find =false;
  hashnode* node = get_hashnode(table,label,&is_find);
  if(exist)*exist=is_find;
  if (is_find){
    if(record!=NULL)node->record = record;
  }else{
    node->next = (hashnode *)malloc(sizeof(hashnode));
    node = node->next;
    node->next = NULL;//现在node是最后一个节点
    // node->id = table->hash(label) % table->size;
    //！！！即使record==NULL也会照样赋值。
    node->record = record;
  }
  return node;
}


void free_hashtable(hashtable* table){
  free(table->nodes);
}

// bool expand_hashtable(hashtable* table,int new_size){
//   if (new_size<table->size){
//       return false;
//   }
//   new_size = new_size / table->filling_factor;
//   table->size = find_smallest_prime(new_size);
//   table->nodes = (hashnode*)calloc(table->size,sizeof(hashnode));
// }