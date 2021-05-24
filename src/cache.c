#include "cache.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 常量
 */
#define LRU_BUFFER_HEAD_POINT 0 // LRU头结点 位置

/*
 * 全局数据和结构体
 */
struct hash_node {
  struct hash_node *next;
  struct record *record;
};
struct record {
  struct record_data data;
  int next, last;
  unsigned id;
  bool valid;
};
struct link_list {
  struct record records[MAX_LRU_BUFFER_LENGTH + 1];
  unsigned int stack[MAX_LRU_BUFFER_LENGTH]; //剩余位置栈
  int stack_top;                             //栈顶指针
  size_t length;                             //队列长度
  size_t used_size;                          //所占空间
  int first, last; //头指针指向第一个元素，尾指针指向新元素
  bool is_init;    //是否初始化
  struct record_data *(*get_by_label)(const char *label);
  int (*set)(const char *label, const struct IP *ip);
} cache;

struct hash_node hash_table[HASH_TABLE_LENGTH];

const int record_size = sizeof(struct record);

/*
 *  通用函数
 */
static inline unsigned _hash_label(const char *label, size_t size);

/*
 * cache操作内部函数
 */
//初始化链表
void init_cache();

//判断链表是否已满
static inline int is_full();
//插入一条数据
static inline int push_front(const char *label, const struct IP *ip,
                             unsigned long ttl);
//移除若干条数据
static inline void pop_back();
//通过hash获取节点
inline static struct record *get(const char *label);
struct hash_node *get_node(const char *label, bool *is_find);

//在一个hash_node之后插入record
static inline void insert_record_after_node(struct hash_node *node,
                                            struct record *record);
// static inline int set(struct record* record); //弃用

//清空label对应的缓存
static inline int reset(const char *label);

//从剩余位置栈中移除、添加
static inline int cache_stack_pop();
static inline void cache_stack_push(int x);

//把一个节点从链表中移除
static inline void linklist_remove_node(struct link_list *cache,
                                        struct record *record);

//获得record相对基址的偏移
static inline int get_record_addr(struct record *record);

void init_cache() {
  //初始化函数
  cache.first = cache.last = -1;

  cache.records[LRU_BUFFER_HEAD_POINT].last = -1;
  cache.records[LRU_BUFFER_HEAD_POINT].next = -1;
  cache.records[LRU_BUFFER_HEAD_POINT].valid = 1; //头结点

  for (int i = 0, j = MAX_LRU_BUFFER_LENGTH; i < MAX_LRU_BUFFER_LENGTH;
       i++, j--) {
    cache.stack[i] = j; // 1~MAX 都是可用的 0是头结点
  }
  cache.stack_top = MAX_LRU_BUFFER_LENGTH - 1;
  cache.used_size = 0;
  cache.length = 0;
}

static inline int cache_stack_pop() {
  if (cache.stack_top > 0) {
    return cache.stack[cache.stack_top--];
  } else {
    return -1;
  }
}
static inline void cache_stack_push(int x) {
  if (x >= 0 && x < MAX_LRU_BUFFER_LENGTH)
    cache.stack[++cache.stack_top] = x;
}
static inline int get_record_addr(struct record *record) {
  //获得record相对基址的偏移
  long long tmp = record - (struct record *)cache.records;

  return (tmp > 0 && tmp <= MAX_LRU_BUFFER_LENGTH) ? tmp : -1;
}
static inline void linklist_remove_node(struct link_list *cache,
                                        struct record *record) {
  if (record->next > 0) {
    //不能保证后面一定有
    cache->records[record->last].next = record->next;
    cache->records[record->next].last = record->last;
  } else {
    cache->last = record->last;
    cache->records[record->last].next = -1;
  }
}

static inline int is_full() {
  return (cache.length >= MAX_LRU_BUFFER_LENGTH - 1);
}

static inline int push_front(const char *label, const struct IP *ip,
                             unsigned long ttl) {
  //向头插入
  bool is_find;
  struct hash_node *node;
  struct record *operate_record;
  int operate_record_addr;

  time_t now = time(NULL);
  if (now >= ttl)
    return -1;

  if (is_full()) {
    pop_back(cache);
  }
  while (cache.used_size + record_size > MAX_LRU_CACHE) {
    pop_back(cache);
  }
  // node = get_hash
  node = get_node(label, &is_find);//TODO:哈希表使用

  if (is_find) { //找到了,但是也需要把它拉到最前面去

    operate_record = node->record;
    operate_record_addr = get_record_addr(operate_record);
    if (operate_record_addr == cache.first) {
      //本来就是第一个
      memcpy(&operate_record->data.ip, ip, sizeof(struct IP));
      return 1;
    }
    linklist_remove_node(&cache, operate_record);

  } else {
    //需要新分配一个空间
    if ((operate_record_addr = cache_stack_pop()) < 0) {
      printf("no remain space");
      abort();
      //不应该发生这种情况
    }
    insert_record_after_node(node, &cache.records[operate_record_addr]);//TODO:哈希表使用

    operate_record = &cache.records[operate_record_addr];
    operate_record_addr = operate_record_addr;

    strcpy(operate_record->data.label, label);
    operate_record->id = hash_label(label);//TODO:哈希表使用
    operate_record->valid = 1;
    cache.length++;
    cache.used_size += record_size;
  }

  memcpy(&operate_record->data.ip, ip, sizeof(struct IP));
  operate_record->data.ttl = ttl;
  if (cache.length > 1) {
    operate_record->next = cache.first;
    operate_record->last = LRU_BUFFER_HEAD_POINT;
    cache.first = cache.records[LRU_BUFFER_HEAD_POINT].next =
        cache.records[cache.first].last = operate_record_addr;
  } else {
    //只有新插入节点一个节点
    operate_record->next = -1;
    operate_record->last = LRU_BUFFER_HEAD_POINT;
    cache.first = cache.last = cache.records[LRU_BUFFER_HEAD_POINT].next =
        operate_record_addr;
  }
  return 1;
}

void pop_back() {
  //移除若干条数据
  int t = LRU_POP_ITEM;
  while ((t--)) {
    if (cache.length > 0) {
      int next_to_last = cache.records[cache.last].last; //最后一个的上一个
      cache.records[cache.last].valid = 0;

      cache_stack_push(cache.last);

      reset(cache.records[cache.last].data.label); //移出哈希表中的字段
      //TODO:哈希表使用
      cache.used_size -= record_size;
      cache.records[next_to_last].next = -1;

      cache.last = next_to_last;
      cache.length--;

      if (cache.length == 1) {
        cache.first = cache.last;
      } else if (cache.length == 0) {
        cache.first = cache.last = LRU_BUFFER_HEAD_POINT; //头结点
      }
    }
  }
}
static inline void pop(struct record *record) {
  int addr = get_record_addr(record);
  int substitute_addr;

  if (addr == cache.last) {
    substitute_addr = cache.records[cache.last].last;
    cache.records[substitute_addr].next = -1;
    cache.last = substitute_addr;
  } else if (addr == cache.first) {
    substitute_addr = cache.records[cache.first].next;
    cache.records[substitute_addr].last = LRU_BUFFER_HEAD_POINT;
    cache.records[LRU_BUFFER_HEAD_POINT].next = substitute_addr;
    cache.first = substitute_addr;
  } else {
    linklist_remove_node(&cache, record);
  }
  reset(record->data.label); //移出哈希表中的字段
  //TODO:哈希表使用
  record->valid = 0;
  cache.used_size -= record_size;
  cache.length--;
  cache_stack_push(addr);
}

struct record_data *get_cache(const char *label) {
#if DEBUG
  test_normal();
#endif
  struct record *record = get(label);
  //TODO:哈希表使用
  if (!record)
    return NULL;
  int now = get_record_addr(record);
  if (now != cache.first) {
    linklist_remove_node(&cache, &cache.records[now]);
    cache.records[LRU_BUFFER_HEAD_POINT].next = now;
    record->last = LRU_BUFFER_HEAD_POINT;
    record->next = cache.first;
    cache.first = cache.records[cache.first].last = now;
  }
  return &record->data;
  // return NULL;
}
int set_cache(const char *label, const struct IP *ip, time_t ttl) {
#if DEBUG==1
  test_normal();
#endif
  return push_front(label, ip, ttl);
}
void clear_cache() {
  //清空缓存
  abort();
  memset(&cache, 0, sizeof(struct link_list));
  // memset(&)
  //在做了。
}

struct hash_node *get_node(const char *label, bool *is_find) {
  //
  unsigned pos = hash_label(label);
  struct hash_node *node = &hash_table[pos], *last;
  //TODO:哈希表使用
  while (node->next) { //头结点
    last = node;
    node = node->next;
    if (!strcmp(node->record->data.label, label)) {
      time_t now = time(NULL);
      if (now < node->record->data.ttl) {
        if (is_find)
          *is_find = 1;
        return node;
      } else {
        pop(node->record);
        // node成为悬浮指针
        node = last;
      }
    }
  }
  if (is_find)
    *is_find = 0;
  return node;
}

inline static struct record *get(const char *label) {
  bool is_find;
  struct hash_node *tmp;
  tmp = get_node(label, &is_find);//TODO:哈希表使用
  return (tmp != NULL) ? tmp->record : NULL;
}

static inline int reset(const char *label) {
  unsigned pos = hash_label(label);
  struct hash_node *node, *last;
  node = &hash_table[pos];
  while (node->next) {
    last = node;
    node = node->next;
    if (!strcmp(node->record->data.label, label)) {
      last->next = node->next;
      free(node);
      return 1;
    }
  }
  return 0;
}

static inline void insert_record_after_node(struct hash_node *node,
                                            struct record *record) {
  //直接设置,将在该节点之后插入
  node = node->next = (struct hash_node *)malloc(sizeof(struct hash_node));
  node->next = NULL;
  node->record = record;
  // return hash_label(record->data.label);//相同接口
}

static inline unsigned _hash_label(const char *label, size_t size) {
  // hash函数 by 阎宏飞和谢正茂
  //《两种对URL的散列效果很好的函数》
  unsigned int n = 0;
  unsigned int len = strlen(label);
  char *b = (char *)&n;
  for (int i = 0; i < len; i++) {
    b[i % 4] ^= label[i];
  }
  return n % size;
}
static inline unsigned _hash_ip(const struct IP*ip,size_t size){
  return ip->addr.v6 % size;
}

/*
 * 测试函数
 */

void test_normal() {
  struct record *record = &cache.records[LRU_BUFFER_HEAD_POINT];
  int count = 0, last;
  for (int i = 0; i < cache.length && record->next != -1; i++, count++) {
    last = record->next;
    record = &cache.records[record->next];
  }
  if (count != cache.length || record->next > 0) {
    printf("error1: length:%d,count=%d, record->next:%d,last.next:%d\n",
           (int)cache.length, count, record->next, cache.records[last].next);
    struct record *record = &cache.records[LRU_BUFFER_HEAD_POINT];
    int count = 0, last;
    for (int i = 0; i < cache.length && record->next != -1; i++) {
      last = record->next;
      record = &cache.records[record->next];
      printf("%d-", record->next);
    }
    exit(0);
  }
  count = 0;
  for (int i = 0; i < cache.length && record->last >= 0; i++, count++) {
    record = &cache.records[record->last];
  }

  if (count != cache.length) {
    printf("error2:%d\n", count);
    exit(0);
  }
  int first = cache.records[LRU_BUFFER_HEAD_POINT].next;
  if (cache.length > 1 && (cache.first != first || cache.last != last)) {
    printf("error3");
    exit(0);
  }
  printf("check good\n");
}

// 弃用函数
// static inline
// int set(struct record* record){
//     unsigned pos = hash_label(record->data.label);
//     struct hash_node*last = &hash_table[pos],*node;
//     node = last;
//     while(node&&node->next){//头结点
//         last = node;
//         node = node->next;
//         if
//         (!strcmp(node->record->data.label,record->data.label)&&node->record->valid){
//             memcpy(&node->record->data.ip,&record->data.ip,sizeof(struct
//             IP)); return (~pos);//此处用取反是为了避免pos==0时无法区分的情况
//         }
//     }
//     node = last->next = (struct hash_node*)malloc(sizeof(struct hash_node));
//     node->next = 0;
//     node->record = record;
//     return pos;
// }