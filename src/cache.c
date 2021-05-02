#include "cache.h"
 #include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>

//1453是质数
#define LRU_BUFFER_HEAD_POINT 0 
#define LRU_POP_ITEM 10
struct record* get(const char* label);
struct hash_node{
    struct hash_node* next;
    struct record* record;
};
struct hash_node hash_table[HASH_TABLE_LENGTH];
static inline void pop_back();
struct hash_node* get_node(const char* label,bool* is_find);
static inline void set_node(struct hash_node*node,struct record* record);
static inline int set(struct record* record);
int reset(const char* label);
// static inline int find_first_valid_record();
static inline int cache_stack_pop();
static inline void cache_stack_push(int x);



unsigned _hash(const char* label,size_t size){
    // hash函数 by 阎宏飞和谢正茂
    //《两种对URL的散列效果很好的函数》
    unsigned int n = 0;
    unsigned int len = strlen(label);
    char* b = (char*)&n;
    for(int i=0;i<len;i++){
        b[i % 4]^=label[i];
    }
    return n%size;
}



struct record{
    struct record_data data;
    int next,last;
    unsigned id;
    bool valid;
};
const  int record_size = sizeof(struct record);

struct link_list{
    struct record records[MAX_LRU_BUFFER_LENGTH+1];
    unsigned int stack[MAX_LRU_BUFFER_LENGTH];//剩余位置栈
    int stack_top;//栈顶指针
    size_t length;//队列长度
    size_t used_size;//所占空间
    int first,last;//头指针指向第一个元素，尾指针指向新元素
    bool is_init;//是否初始化
}cache;

void test_normal(){
    struct record* record = &cache.records[LRU_BUFFER_HEAD_POINT];
    int count = 0,last;
    for(int i=0;i<cache.length&&record->next!=-1;i++,count++){
        last = record->next;
        record = &cache.records[record->next];
    }
    if(count!=cache.length||record->next>0){
        printf("error1: length:%d,count=%d, record->next:%d,last.next:%d\n",cache.length,count,record->next,cache.records[last].next);
        struct record* record = &cache.records[LRU_BUFFER_HEAD_POINT];
        int count = 0,last;
        for(int i=0;i<cache.length&&record->next!=-1;i++){
            last = record->next;
            record = &cache.records[record->next];
            printf("%d-",record->next);
        }
        exit(0);
    }
    count = 0;
    for(int i=0;i<cache.length&&record->last>=0;i++,count++){
        record = &cache.records[record->last];
    }
    
    if(count!=cache.length){
        printf("error2:%d\n",count);
        exit(0);
    }
    int first =cache.records[LRU_BUFFER_HEAD_POINT].next;
    if (cache.length>1&&(cache.first!=first || cache.last!=last)){
        printf("error3");
        exit(0);
    }
}

// init_list(LRU);

// static inline
// int find_first_valid_record(){ 
//     return (cache.stack_top>0)?cache.stack[cache.stack_top]:-1;
// }
static inline
int cache_stack_pop(){
    if (cache.stack_top>0){
        return cache.stack[cache.stack_top--];
    }else{
        return -1;
    }
}
static inline
void cache_stack_push(int x){
    if(x>=0&&x<MAX_LRU_BUFFER_LENGTH)
        cache.stack[++cache.stack_top] = x;
}
static inline
int get_record_addr(struct record* record){
    //获得record相对基址的偏移
    long long tmp =  record - (struct record*)cache.records;
    
    return (tmp>0&&tmp<=MAX_LRU_BUFFER_LENGTH)?tmp:-1;
}
static inline 
void linklist_remove_node(struct link_list* cache,struct record* record){
    if (record->next>0){
        //不能保证后面一定有
        cache->records[record->last].next = record->next;
        cache->records[record->next].last = record->last;
    }else{
        cache->last = record->last;
        cache->records[record->last].next = -1;
    }
}
void init_list(){
    cache.first = cache.last = -1;

    cache.records[LRU_BUFFER_HEAD_POINT].last=-1;
    cache.records[LRU_BUFFER_HEAD_POINT].next=-1;
    cache.records[LRU_BUFFER_HEAD_POINT].valid = 1;//头结点


    for(int i=0,j=MAX_LRU_BUFFER_LENGTH;i<MAX_LRU_BUFFER_LENGTH;i++,j--){
        cache.stack[i] = j;//1~MAX 都是可用的 0是头结点
    }
    cache.stack_top = MAX_LRU_BUFFER_LENGTH -1;
    cache.used_size = 0;
    cache.length=0;
}

 
int is_full(){
    return (cache.length>=MAX_LRU_BUFFER_LENGTH - 1);
}


static inline 
int push_front(const char* label,const struct IP* ip){
    //向头插入
    bool is_find;
    struct hash_node* node;
    struct record* operate_record;
    int operate_record_addr;
    if(is_full()){
        pop_back(cache);
    }
    while(cache.used_size+record_size>MAX_LRU_CACHE){
        pop_back(cache);
    }
    


    node = get_node(label,&is_find);
    if (is_find){//找到了,但是也需要把它拉到最前面去
        
        operate_record = node->record;
        operate_record_addr = get_record_addr(operate_record);
        if(operate_record_addr==cache.first){
            //本来就是第一个
            memcpy(&operate_record->data.ip,ip,sizeof(struct IP));
            return 1;
        }
        linklist_remove_node(&cache,operate_record);
        
    }else{
        //需要新分配一个空间
        if((operate_record_addr=cache_stack_pop())<0){
            printf("no remain space");
            abort();
            //不应该发生这种情况
        }
        set_node(node,&cache.records[operate_record_addr]);

        operate_record = &cache.records[operate_record_addr];
        operate_record_addr = operate_record_addr;
        
        strcpy(operate_record->data.label,label);
        operate_record->id = hash(label);
        operate_record->valid =1 ;
        cache.length++;
        cache.used_size += record_size;
    }

    memcpy(&operate_record->data.ip,ip,sizeof(struct IP));
    
    if (cache.length > 1){
        operate_record->next = cache.first;
        operate_record->last = LRU_BUFFER_HEAD_POINT;
        cache.first = cache.records[LRU_BUFFER_HEAD_POINT].next = cache.records[cache.first].last = operate_record_addr;
    }else{
        //只有新插入节点一个节点
        operate_record->next = -1;
        operate_record->last = LRU_BUFFER_HEAD_POINT;
        cache.first = cache.last = cache.records[LRU_BUFFER_HEAD_POINT].next = operate_record_addr;
    }
    return 1;
}


void pop_back(){
    //移除最后一个节点，返回当前长度
    int t=LRU_POP_ITEM;
    while ((t--)){
    if (cache.length>0){
        int next_to_last = cache.records[cache.last].last;//最后一个的上一个
        cache.records[cache.last].valid = 0;

        cache_stack_push(cache.last);
        
        reset(cache.records[cache.last].data.label);//移出哈希表中的字段
        
        cache.used_size -= record_size;
        cache.records[next_to_last].next = -1;

        cache.last = next_to_last;
        cache.length--;
        
        if(cache.length==1){
            cache.first = cache.last;
        }else if(cache.length==0){
            cache.first = cache.last = LRU_BUFFER_HEAD_POINT;//头结点
        }
        
    }}
}

struct record_data* get_cache(const char* label){
    // test_normal();
    struct record* record = get(label);
    if (!record)return NULL;
    int now = get_record_addr(record);
    if (now!=cache.first){
        linklist_remove_node(&cache,&cache.records[now]);  
        cache.records[LRU_BUFFER_HEAD_POINT].next = now;
        record->last = LRU_BUFFER_HEAD_POINT;
        record->next = cache.first;
        cache.first = cache.records[cache.first].last = now;
    }
    return &record->data;
    // return NULL;
}
int set_cache(const char* label,const struct IP* ip){
    // test_normal();
    return push_front(label,ip);
}
void clear_cache(){
    //清空缓存
    abort();    
    memset(&cache,0,sizeof(struct link_list));
    // memset(&)
    //在做了。
}
  

struct hash_node* get_node(const char* label,bool* is_find){
    unsigned pos = hash(label);
    struct hash_node* node = &hash_table[pos];
    while(node->next){//头结点
        node = node->next;
        if (!strcmp(node->record->data.label,label)){
            if(is_find)*is_find=1;
            return node;
        }
    }
    if(is_find)*is_find=0;
    return node;
}
struct record* get(const char* label){
    bool is_find;
    struct hash_node* tmp;
    tmp = get_node(label,&is_find);
    return (tmp!=NULL)?tmp->record:NULL;
}
int reset(const char* label){
    unsigned pos = hash(label);
    struct hash_node* node,*last;
    node = &hash_table[pos];
    while(node->next){
        last = node;
        node = node->next;
        if (!strcmp(node->record->data.label,label)){
            last->next = node->next;
            free(node);
            return 1;
        }
    }
    return 0;
}

static inline 
int set(struct record* record){
    unsigned pos = hash(record->data.label);
    struct hash_node*last = &hash_table[pos],*node;
    node = last;
    while(node&&node->next){//头结点
        last = node;
        node = node->next; 
        if (!strcmp(node->record->data.label,record->data.label)&&node->record->valid){
            memcpy(&node->record->data.ip,&record->data.ip,sizeof(struct IP));
            return (~pos);//此处用取反是为了避免pos==0时无法区分的情况
        }
    }
    node = last->next = (struct hash_node*)malloc(sizeof(struct hash_node));
    node->next = 0;
    node->record = record;
    return pos;
}
static inline
void set_node(struct hash_node*node,struct record* record){
    //直接设置,将在该节点之后插入
    node = node->next = (struct hash_node*)malloc(sizeof(struct hash_node));
    node->next = NULL;
    node->record = record;
    // return hash(record->data.label);//相同接口
}
