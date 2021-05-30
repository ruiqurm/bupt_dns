#include "cache.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "log.h"
// #define LOG_INCLUDED
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

//判断链表是否已满
static inline int is_full(cache*Cache);

//预留一个空间
static inline struct record* push_front(cache*Cache,const char *label);

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


/**
 * @brief 自定义函数
 * 
 */
void realse_A_record(struct record*data);
int add_A_record(struct record*record,void*data);
void* get_cache_A_record_label(struct record*data);
bool check_A_record(struct record*record);
int add_A_multi_record(struct record*record,void*_data[],int size);



void init_cache(cache*Cache,int record_length,int max_size,double filling_factor,
               realse_data realse,add_data add,add_multi_data add_multi,
               check_data check,get_data_label get_label){
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
  log_info("record_length:%d",record_length);
  for (int i = 0, j = record_length; i < record_length;
       i++, j--) {
    Cache->stack[i] = j; // 1~MAX 都是可用的 0是头结点
  }
  Cache->stack_top = record_length - 1;
  Cache->used_size = 0;
  Cache->length = 0;
  if(filling_factor<=0||filling_factor>1)filling_factor = CACHE_FILLING_FACTOR;
  init_hashtable(&Cache->label_hash,record_length,filling_factor,_hash_label,compare_record_with_label);
  Cache->add = add;
  Cache->realse =realse;
  Cache->add_multi = add_multi;
  Cache->check = check;
  Cache->get_label =get_label;
  Cache->is_init = true;
}
/**
 * @brief 默认初始化
 * 
 */
void init_A_record_cache_default(cache*Cache){
  init_cache(Cache,LRU_BUFFER_LENGTH,CACHE_SIZE,CACHE_FILLING_FACTOR,
  realse_A_record,add_A_record,add_A_multi_record,check_A_record,get_cache_A_record_label);
}
void init_A_record_cache(cache*Cache,int record_length,int max_size,double filling_factor){
  
  init_cache(Cache,record_length,max_size,filling_factor,
  realse_A_record,add_A_record,add_A_multi_record,check_A_record,get_cache_A_record_label);
}

void free_cache(cache*Cache){
  if(Cache->is_init==true){
    Cache->is_init = false;
    for(int i=0;i<Cache->max_length;i++){
      if(Cache->records[i].valid){
        // printf("1\n");
        Cache->realse(&Cache->records[i]);
      }
    }
    free(Cache->records);
    free(Cache->stack);
    free_hashtable(&Cache->label_hash);
  }
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

/**
 * @brief 插入一个空间，返回record结构体
 * 
 * @param Cache Cache结构体
 * @param label label
 */
static inline struct record* push_front(cache*Cache,const char *label) {
  //向头插入
  bool is_find=false;
  hashnode *node;
  struct record *operate_record;
  // struct record_data* operate_record_data;
  int operate_record_addr;
  // time_t now = time(NULL);
  
  // if (now >= ttl){
  //   return -1;
  // }
  if (is_full(Cache)) {
    pop_back(Cache);
    #ifdef LOG_INCLUDED
    log_debug("pop back done\n");
    #endif
  }

  while (Cache->used_size + 1024 > Cache->max_size) {//预分配1kB内存，可能会超出范围
    pop_back(Cache);
    #ifdef LOG_INCLUDED
    log_debug("pop back done\n");
    #endif
  }
  node = set_hashnode(&Cache->label_hash,(void*)label,NULL,&is_find);//暂时用NULL占位

  if (is_find) { //找到了,但是也需要把它拉到最前面去
    operate_record = node->record;
    operate_record_addr = get_record_addr(Cache,operate_record);
    // operate_record_data = &operate_record->data;
    // //找到最后一个元素，并分配一个空间
    // while(operate_record_data->next){
    //   operate_record_data = operate_record_data->next;
    // }
    // operate_record_data =  operate_record_data->next = (struct record_data*)malloc(sizeof(struct record_data));
    
    //如果本来就是第一个，后面就不需要做了
    if (operate_record_addr == Cache->first) {
      // memcpy(&operate_record_data->ip, ip, sizeof(struct IP));
      return operate_record;
    }
    //否则就要把节点先切出来
    linklist_remove_node(Cache, operate_record);
  } else {
    //需要新分配一个空间
    if ((operate_record_addr = cache_stack_pop(Cache)) < 0) {
      printf("no remain space");
      exit(EXIT_FAILURE);
    }
    node->record = &Cache->records[operate_record_addr];
    operate_record = &Cache->records[operate_record_addr];
    // operate_record_data = &operate_record->data;
    operate_record_addr = operate_record_addr;
    // strcpy_s(operate_record->label,sizeof(operate_record->label),label);
    operate_record->valid = 1;
    operate_record->size = 0;
    Cache->length++;
    // Cache->used_size += record_size;
  }
  // memcpy(&operate_record->data.ip, ip, sizeof(struct IP));
  // operate_record->data.next=NULL;
  // operate_record->data.ttl = ttl;
  // operate_record->record_data_length++;

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
  return operate_record;
}

void pop_back(cache* Cache) {
  //移除若干条数据
  int t = LRU_POP_ITEM;
  struct record_data*pd,*tmp;
  int count =0;
  while ((t--)) {
    if (Cache->length > 0) {

      
      //重新链接大链表
      int next_to_last = Cache->records[Cache->last].last; //最后一个的上一个
      Cache->records[Cache->last].valid = 0;
      //压入可选栈
      cache_stack_push(Cache,Cache->last);

      //重置hash表
      // bool is_find;
      // get_hashnode(&Cache->label_hash,Cache->get_label(&Cache->records[Cache->last]),&is_find);
      // if(is_find){
      //   printf("ok");
      // }else{
      //   printf("error");
      // }
      reset_hash_node(&Cache->label_hash,Cache->get_label(&Cache->records[Cache->last]));
      // puts(Cache->get_label(&Cache->records[Cache->last]));
      Cache->used_size -= Cache->records[Cache->last].size;//减去动态分配内存
      Cache->realse(&Cache->records[Cache->last]);

      //重新链接大链表
      // Cache->used_size -= record_size;
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
    #ifdef LOG_INCLUDED
    log_debug("remove record;label= %s",((struct record_data*)record->data)->label);
    #endif 
  

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
  

  reset_hash_node(&Cache->label_hash,Cache->get_label(record));
  Cache->used_size -= Cache->records[Cache->last].size;//减去动态分配内存,这句一定要在前面,否则后面就被置为0了
  Cache->realse(&Cache->records[Cache->last]);

  record->valid = 0;
  // Cache->used_size -= record_size;
  Cache->length--;
  cache_stack_push(Cache,addr);
}

struct record_data *get_cache_A_record(cache* Cache,const char *label) {
  if(Cache->is_init==false){
    log_error("cache not init");
    return NULL;
  }
  struct record *record = get(Cache,label);
  if (!record)
    return NULL;

  #ifdef LOG_INCLUDED
  log_debug("get cache ;label:%s  label_finded= %s",label,((struct record_data*)record->data)->label);
  #endif 

  if (Cache->check(record)){
    #ifdef LOG_INCLUDED
    log_debug("%s timeout",label);
    #endif
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
  return (struct record_data *)record->data;
}
int set_cache_A_record(cache*Cache,const char *label, void*data) {
  if(Cache->is_init==false){
    log_error("cache not init");
    return -1;
  }
  struct record* record = push_front(Cache,label);//预留空间
  if(record){
    #ifdef LOG_INCLUDED
    log_debug("set cache ;label= %s",((struct record_data*)data)->label);
    #endif 
    if(record->size>0){
      #ifdef LOG_INCLUDED
      log_debug("length>0");
      #endif
      Cache->realse(record);
    }
    return Cache->add(record,data);
  }else{
    return 0;
  }
}
int set_cache_A_multi_record(cache*Cache,const char *label, void*data[],int size) {
  if(Cache->is_init==false){
    log_error("cache not init");
    return -1;
  }
  struct record* record = push_front(Cache,label);//预留空间
  // log_info("%lld\n",record);
  if(record){
    #ifdef LOG_INCLUDED
    log_debug("set cache ;label= %s",label);
    #endif 
    if(record->size>0){
      #ifdef LOG_INCLUDED
      log_debug("length>0");
      #endif
      Cache->realse(record);
    }
    Cache->add_multi(record,data,size);
    //       {
    //   bool is_find;
    //   printf("label = %s %s",label,Cache->get_label(record));
    //   get_hashnode(&Cache->label_hash,Cache->get_label(record),&is_find);
    //   if(is_find){
    //     printf("ok");
    //   }else{
    //     printf("error");
    //     exit(0);
    //   }
    // }
    return 1;
  }else{
    return 0;
  }
}
void realse_A_record(struct record*data){
    // int count = data->size;
    // printf("%d\n",count);
    struct record_data *pd = data->data,*tmp;
    #ifdef LOG_INCLUDED
    log_debug("realse a record;label= %s,size = %d",
    ((struct record_data*)data->data)->label,((struct record*)data)->size);
    #endif 
    int has_release_label = false;
    while(pd){
      tmp=pd;
      pd=pd->next;
      if(!has_release_label){
        free(tmp->label);
        has_release_label=true;
      }
      free(tmp);
      // count--;
    }
    if(pd!=NULL){
      fprintf(stderr,"record_data link error!\n");
      exit(EXIT_FAILURE);
    }
    data->size=0;
    // data->data=NULL;
    #ifdef LOG_INCLUDED
    log_debug("realse done");
    #endif
}
/**
 * @brief 添加a记录
 * 
 * @param record 
 * @param data 
 */
int add_A_record(struct record*record,void*data){
  time_t now = time(NULL);
  struct record_data*tmp;
  int len = 0;
  if(((struct record_data*)data)->ttl<now)return 0;
  int sizeofstruct = sizeof(struct record_data);
  if(record->size>0){
    //目前不可能执行这一段
    #ifdef LOG_INCLUDED
    log_debug("replace new A record;label= %s",((struct record_data*)record->data)->label);
    #endif 
    tmp = ((struct record_data*)record->data)->next;
    memcpy_s(record->data,sizeofstruct,data,sizeofstruct);//覆盖原数据
    ((struct record_data*)record->data)->next=tmp;
  }else{
    #ifdef LOG_INCLUDED
    log_debug("record has no data before");
    #endif 
    record->data = malloc(sizeofstruct);
    memcpy_s(record->data,sizeofstruct,data,sizeofstruct);
    len = strlen(((struct record_data*)data)->label) + 1;//注意加1
    ((struct record_data*)record->data)->label = malloc(len);
    strcpy_s(((struct record_data*)record->data)->label,len,((struct record_data*)data)->label);//复制name
    record->size = sizeofstruct + len;
    #ifdef LOG_INCLUDED
    log_debug("add new A record;label= %s,ip=%d",((struct record_data*)record->data)->label,((struct record_data*)record->data)->ip.addr.v4);
    #endif 
    ((struct record_data*)record->data)->next=NULL;
  }
  return record->size;
}

/**
 * @brief 添加A记录和cname。第一个是a记录。剩下的是cname
 * 
 * @param record 
 * @param data 
 * @param size 
 */
int add_A_multi_record(struct record*record,void*_data[],int size){
  //将会覆盖原来数据
  int sizeofstruct = sizeof(struct record_data),i,len;
  time_t now = time(NULL);
  struct record_data** data = (struct record_data**)_data;
  struct record_data*  dest = NULL;
  for(i=0;i<size;i++){
    if(now>(data[i]->ttl)){
      continue;
    }
    if(dest){
      //dest->next先被赋值，然后dest=dest->next
      //相当于转到下一个节点
      (dest)->next = malloc(sizeofstruct);
      memcpy_s(dest->next,sizeofstruct,data[i],sizeofstruct);
      dest = dest->next;
      dest->label= ((struct record_data*)(record->data))->label;
      // (**dest).label = NULL;//复制label
    }else{
      dest = record->data = malloc(sizeofstruct);
      memcpy_s(dest,sizeofstruct,data[i],sizeofstruct);
      len = strlen(data[i]->label) + 1;//注意加1
     dest->label = malloc(len);
    //  printf("%lld\n",(*dest)->label);
      strcpy_s(dest->label,len,data[i]->label);//复制name
    }
    #ifdef LOG_INCLUDED
    log_debug("add new A record;pre=%s  label= %s",data[0]->label,dest->label);
    #endif 

    record->size += sizeofstruct;
  }
  if(record->size&&dest)dest->next=NULL;
   #ifdef LOG_INCLUDED
    log_debug("first label:%s",((struct record_data*)(record->data))->label);
    #endif 
  return record->size;
}
bool check_A_record(struct record*record){
  return ((struct record_data*)record->data)->ttl < time(NULL) && 
  ((struct record_data*)record->data)->ttl != DNS_CACHE_PERMANENT;
  //过期且不等于无穷大
}

void* get_cache_A_record_label(struct record*data){
  return ((struct record_data*)data->data)->label;
}




inline static struct record *get(cache *Cache,const char *label) {
  bool is_find;
  hashnode *tmp;
  tmp = get_hashnode(&Cache->label_hash,(void*)label,&is_find);
  return (is_find) ? tmp->record : NULL;
}





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
  // log_debug("%s %s",(((struct record_data*)((struct record*)record)->data)->label),label);
  if (!(((struct record*)record)->valid)){
    printf("error\n");
    exit(0);
  }
  return !strcmp(((struct record_data*)((struct record*)record)->data)->label, label);
}
static inline bool compare_static_record_data_with_label(void* record_data,void*label){
  // struct record_data* record = ()_recor
  return !strcmp( ((struct static_record_data*)record_data)->label, label);
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
    #ifdef LOG_INCLUDED
    log_error("error1: length:%d,count=%d, record->next:%d,last.next:%d\n",
           (int)Cache->length, count, record->next, Cache->records[last].next);
    #endif
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
    #ifdef LOG_INCLUDED
    log_error("error2:%d\n", count);
    #endif
    exit(EXIT_FAILURE);
  }
  int first = Cache->records[LRU_BUFFER_HEAD_POINT].next;
  if (Cache->length > 1 && (Cache->first != first || Cache->last != last)) {
    #ifdef LOG_INCLUDED
    log_error("error3");
    #endif
    exit(EXIT_FAILURE);
  }
  #ifdef LOG_INCLUDED
  log_debug("check good\n");
  #endif
}


void init_staticCache(struct staticCache* Cache,int size){
  if(Cache->is_init==false){
    init_hashtable(&Cache->table,size,0.75,_hash_label,compare_static_record_data_with_label);
    Cache->data = (struct static_record_data*)malloc((size+2)*sizeof(struct static_record_data));
    Cache->top = 0;
    Cache->size = size;
    Cache->is_init = true;
  }
}
void free_staticCache(struct staticCache* Cache){
  if(Cache->is_init==true){
    struct static_record_data* data=NULL,*tmp=NULL;
    free_hashtable(&Cache->table);//释放哈希表
    for(int i=0;i<Cache->top;i++){//释放字符串
      free(Cache->data[i].label);
      tmp = data=&Cache->data[i];
      while(data){
        data = data->next;
        free(tmp);
        tmp = data;
      }
    }
    free(Cache->data);//释放数组
    Cache->is_init = false;
  }
}
struct static_record_data* get_static_cache(struct staticCache* Cache,const char* label){
  if(Cache->is_init==true){
    bool is_find=false;
    hashnode * node = get_hashnode(&Cache->table,(void*)label,&is_find);
    if(!is_find || node==NULL)return NULL;
    return node->record;
  }
  return NULL;
}
bool set_static_cache(struct staticCache* Cache,const char* label,const struct IP*ip){
  if(Cache->is_init==true){
    struct static_record_data* data=NULL;
    char* first_label = NULL;
    if ( (data=(get_static_cache(Cache,label))) !=NULL){
        //重复
      first_label = data->label;
      while(data->next){
        data= data->next;
      }
      data->next = (struct static_record_data*)malloc(sizeof(struct static_record_data));
      data =data->next;
      memcpy_s(&data->ip,sizeof(struct IP),ip,sizeof(struct IP));
      data->label = first_label;
      data->next=NULL;
    }else{
      int len = strlen(label) + 1;//记得加1
      int top = Cache->top;
      if(top==Cache->size){
        log_error("static cache overflow. top == %d",Cache->top);
        return false;
      }
      memcpy_s(&Cache->data[top].ip,sizeof(struct IP),ip,sizeof(struct IP));
      Cache->data[top].label = (char*)malloc(len);
      memcpy_s(Cache->data[top].label,len,label,len);
      Cache->data[top].next=NULL;
      set_hashnode(&Cache->table,(void*)label,&Cache->data[top],NULL);
      Cache->top+=1;
      return true;
    }
  }
  return false;
}