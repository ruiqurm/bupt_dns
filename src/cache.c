#include "cache.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

/*
 * 常量
 */
#define LRU_BUFFER_HEAD_POINT 0 // LRU头结点 位置

/*
 * 全局数据和结构体
 */



const int record_size = sizeof(struct record);

/*
 *  通用函数
 */
static inline unsigned _hash_label(void *label);
static inline bool compare_record_with_label(void* record,void*label2);

/*
 * cache操作内部函数
 */
//初始化链表
void init_cache(cache*Cache,int record_length,int max_size,double filling_factor);
void init_default_cache(cache*Cache);


//判断链表是否已满
static inline int is_full(cache*Cache);
//插入一条数据
static inline int push_front(cache*Cache,const char *label, const struct IP *ip,
                             time_t ttl);
//移除若干条数据
static inline void pop_back(cache*Cache);
//通过hash获取节点
inline static struct record *get(cache*Cache,const char *label);
// hashnode *get_node(const char *label, bool *is_find);

// //在一个hash_node之后插入record
// static inline void insert_record_after_node(hashnode *node,
//                                             struct record *record);
// static inline int set(struct record* record); //弃用

//清空label对应的缓存
// static inline int reset(const char *label);

//从剩余位置栈中移除、添加
static inline int cache_stack_pop(cache*Cache);
static inline void cache_stack_push(cache*Cache,int x);

//把一个节点从链表中移除
static inline void linklist_remove_node(cache*Cache,
                                        struct record *record);

//获得record相对基址的偏移
static inline int get_record_addr(cache*Cache,struct record *record);


void init_cache(cache*Cache,int record_length,int max_size,double filling_factor) {
  //初始化函数
  Cache->first = Cache->last = -1;
  
  if(record_length<=0)record_length = LRU_BUFFER_LENGTH; 
  Cache->max_length = record_length;

  Cache->records = (struct record*)calloc(record_length+1,sizeof(struct record));
  Cache->stack = (unsigned*)calloc(record_length,sizeof(unsigned));

  if(max_size<=0)max_size = CACHE_SIZE;
  Cache->max_size = max_size;

  Cache->records[LRU_BUFFER_HEAD_POINT].last = -1;
  Cache->records[LRU_BUFFER_HEAD_POINT].next = -1;
  Cache->records[LRU_BUFFER_HEAD_POINT].valid = 1; //头结点

  for (int i = 0, j = record_length; i < record_length;
       i++, j--) {
    Cache->stack[i] = j; // 1~MAX 都是可用的 0是头结点
  }
  Cache->stack_top = record_length - 1;
  Cache->used_size = 0;
  Cache->length = 0;
  if(filling_factor<=0||filling_factor>1)filling_factor = CACHE_FILLING_FACTOR;
  init_hashtable(&Cache->label_hash,record_length,filling_factor,_hash_label,compare_record_with_label);
  Cache->is_init = true;
}
/**
 * @brief 默认初始化
 * 
 */
void init_default_cache(cache*Cache){
  init_cache(Cache,LRU_BUFFER_LENGTH,CACHE_SIZE,CACHE_FILLING_FACTOR);
}
void free_cache(cache*Cache){
  Cache->is_init = false;
  free(Cache->records);
  free(Cache->stack);
  free_hashtable(&Cache->label_hash);
}
static inline int cache_stack_pop(cache*Cache) {
  if (Cache->stack_top > 0) {
    return Cache->stack[Cache->stack_top--];
  } else {
    return -1;
  }
}
static inline void cache_stack_push(cache*Cache,int x) {
  if (x >= 0 && x < Cache->max_length)
    Cache->stack[++Cache->stack_top] = x;
}
static inline int get_record_addr(cache*Cache,struct record *record) {
  //获得record相对基址的偏移
  long long tmp = record - (struct record *)Cache->records;

  return (tmp > 0 && tmp <= Cache->max_length) ? tmp : -1;
}
static inline void linklist_remove_node(cache* Cache,
                                        struct record *record) {
  if (record->next > 0) {
    //不能保证后面一定有
    Cache->records[record->last].next = record->next;
    Cache->records[record->next].last = record->last;
  } else {
    Cache->last = record->last;
    Cache->records[record->last].next = -1;
  }
}

static inline int is_full(cache*Cache) {
  return (Cache->length >= Cache->max_length - 1);
}
static inline int push_front(cache*Cache,const char *label, const struct IP *ip,
                             time_t ttl) {
  //向头插入
  bool is_find=false;
  hashnode *node;
  struct record *operate_record;
  struct record_data* operate_record_data;
  int operate_record_addr;
  time_t now = time(NULL);
  
  if (now >= ttl){
    return -1;
  }
  if (is_full(Cache)) {
    pop_back(Cache);
  }
  while (Cache->used_size + record_size > Cache->max_size) {
    pop_back(Cache);
  }
  node = set_hashnode(&Cache->label_hash,(void*)label,NULL,&is_find);//暂时用NULL占位
  if (is_find) { //找到了,但是也需要把它拉到最前面去
    operate_record = node->record;
    operate_record_addr = get_record_addr(Cache,operate_record);
    if (operate_record_addr == Cache->first) {
      //本来就是第一个
      memcpy(&operate_record->data.ip, ip, sizeof(struct IP));
      return 1;
    }
    linklist_remove_node(Cache, operate_record);
    // operate_record_data = &operate_record->data;
    // while(->next){
      
    // }
  } else {
    //需要新分配一个空间
    if ((operate_record_addr = cache_stack_pop(Cache)) < 0) {
      printf("no remain space");
      exit(EXIT_FAILURE);
    }
    node->record = &Cache->records[operate_record_addr];
    operate_record = &Cache->records[operate_record_addr];
    operate_record_addr = operate_record_addr;
    strcpy_s(operate_record->data.label,sizeof(operate_record->data.label),label);
    operate_record->valid = 1;
    Cache->length++;
    Cache->used_size += record_size;
  }
  memcpy(&operate_record->data.ip, ip, sizeof(struct IP));
  // operate_record->data.next=NULL;
  operate_record->data.ttl = ttl;
  if (Cache->length > 1) {
    operate_record->next = Cache->first;
    operate_record->last = LRU_BUFFER_HEAD_POINT;
    Cache->first = Cache->records[LRU_BUFFER_HEAD_POINT].next =
        Cache->records[Cache->first].last = operate_record_addr;
  } else {
    //只有新插入节点一个节点
    operate_record->next = -1;
    operate_record->last = LRU_BUFFER_HEAD_POINT;
    Cache->first = Cache->last = Cache->records[LRU_BUFFER_HEAD_POINT].next =
        operate_record_addr;
  }
  return 1;
}

void pop_back(cache* Cache) {
  //移除若干条数据
  int t = LRU_POP_ITEM;
  while ((t--)) {
    if (Cache->length > 0) {
      int next_to_last = Cache->records[Cache->last].last; //最后一个的上一个
      Cache->records[Cache->last].valid = 0;

      cache_stack_push(Cache,Cache->last);

      reset_hash_node(&Cache->label_hash,Cache->records[Cache->last].data.label);
      Cache->used_size -= record_size;
      Cache->records[next_to_last].next = -1;

      Cache->last = next_to_last;
      Cache->length--;

      if (Cache->length == 1) {
        Cache->first = Cache->last;
      } else if (Cache->length == 0) {
        Cache->first = Cache->last = LRU_BUFFER_HEAD_POINT; //头结点
      }
    }
  }
}
static inline void remove_record(cache* Cache,struct record *record) {
  int addr = get_record_addr(Cache,record);
  int substitute_addr;

  if (addr == Cache->last) {
    substitute_addr = Cache->records[Cache->last].last;
    Cache->records[substitute_addr].next = -1;
    Cache->last = substitute_addr;
  } else if (addr == Cache->first) {
    substitute_addr = Cache->records[Cache->first].next;
    Cache->records[substitute_addr].last = LRU_BUFFER_HEAD_POINT;
    Cache->records[LRU_BUFFER_HEAD_POINT].next = substitute_addr;
    Cache->first = substitute_addr;
  } else {
    linklist_remove_node(Cache, record);
  }
  // reset(record->data.label); //移出哈希表中的字段
  reset_hash_node(&Cache->label_hash,record->data.label);
  record->valid = 0;
  Cache->used_size -= record_size;
  Cache->length--;
  cache_stack_push(Cache,addr);
}

struct record_data *get_cache(cache* Cache,const char *label) {
#if DEBUG==1
  test_normal(Cache);
#endif
  struct record *record = get(Cache,label);
  time_t timestamp = time(NULL);
  if (!record)
    return NULL;
  if (record->data.ttl < timestamp && record->data.ttl != DNS_CACHE_PERMANENT){
    remove_record(Cache,record);
    return NULL;
  }
  int now = get_record_addr(Cache,record);
  if (now != Cache->first) {
    linklist_remove_node(Cache, &Cache->records[now]);
    Cache->records[LRU_BUFFER_HEAD_POINT].next = now;
    record->last = LRU_BUFFER_HEAD_POINT;
    record->next = Cache->first;
    Cache->first = Cache->records[Cache->first].last = now;
  }
  return &record->data;
  // return NULL;
}
int set_cache(cache*Cache,const char *label, const struct IP *ip, time_t ttl) {
#if DEBUG==1
  test_normal(Cache);
#endif
  return push_front(Cache,label, ip, ttl);
}
// void clear_cache() {
//   //清空缓存
//   abort();
//   memset(&cache, 0, sizeof(struct link_list));
//   // memset(&)
//   //在做了。
// }



inline static struct record *get(cache *Cache,const char *label) {
  bool is_find;
  hashnode *tmp;
  tmp = get_hashnode(&Cache->label_hash,(void*)label,&is_find);
  return (tmp != NULL) ? tmp->record : NULL;
}



// static inline void insert_record_after_node(hashnode *node,
//                                             struct record *record) {
//   //直接设置,将在该节点之后插入
//   node = node->next = (hashnode *)malloc(sizeof(hashnode));
//   node->next = NULL;
//   node->record = record;
//   // return hash_label(record->data.label);//相同接口
// }

static inline unsigned _hash_label(void*label) {
  // hash函数 by 阎宏飞和谢正茂
  //《两种对URL的散列效果很好的函数》
  unsigned int n = 0;
  unsigned int len = strlen((const char*)label);
  char *b = (char *)&n;
  for (int i = 0; i < len; i++) {
    b[i % 4] ^= ((const char*)label)[i];
  }
  return n ;
}
static inline unsigned _hash_ip(const struct IP*ip,size_t size){
  return ip->addr.v6 % size;
}

static inline bool compare_record_with_label(void* record,void*label){
  // struct record_data* record = ()_recor
  return !strcmp(((struct record*)record)->data.label, label);
}

/*
 * 测试函数
 */

void test_normal(cache* Cache) {
  struct record *record = &Cache->records[LRU_BUFFER_HEAD_POINT];
  int count = 0, last;
  for (int i = 0; i < Cache->length && record->next != -1; i++, count++) {
    last = record->next;
    record = &Cache->records[record->next];
  }
  if (count != Cache->length || record->next > 0) {
    printf("error1: length:%d,count=%d, record->next:%d,last.next:%d\n",
           (int)Cache->length, count, record->next, Cache->records[last].next);
    struct record *record = &Cache->records[LRU_BUFFER_HEAD_POINT];
    int count = 0, last;
    for (int i = 0; i < Cache->length && record->next != -1; i++) {
      last = record->next;
      record = &Cache->records[record->next];
      printf("%d-", record->next);
    }
    exit(EXIT_FAILURE);
  }
  count = 0;
  for (int i = 0; i < Cache->length && record->last >= 0; i++, count++) {
    record = &Cache->records[record->last];
  }

  if (count != Cache->length) {
    printf("error2:%d\n", count);
    exit(EXIT_FAILURE);
  }
  int first = Cache->records[LRU_BUFFER_HEAD_POINT].next;
  if (Cache->length > 1 && (Cache->first != first || Cache->last != last)) {
    printf("error3");
    exit(EXIT_FAILURE);
  }
  printf("check good\n");
}