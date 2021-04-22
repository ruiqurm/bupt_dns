#include "cache.h"
#include "common.h"
#include<stdlib.h>
struct record{
    char label[MAX_LABEL_LANGTH];
    struct IP ip;
    cha
};

//记录
static struct node{
    struct node* next,*last;
    void* data;//载荷
    unsigned id;
};
static struct link_list{
    struct node head;//头结点
    struct node* next_to_last;//最后一个节点之前的指针
    int length;
    void (*save_data)(void*dest,void*src);    
    void (*free_data)(void*data);
};

static
inline init_link_list(struct link_list list){
    list.head.last = list.head.next = list.head.data= NULL;
    list.next_to_last = &list.head;
    list.length = 0;
}

static
inline free_link_list(struct link_list list){

}
// static inline
// struct node* get_
static inline
struct node* insert_after_by_node(struct link_list* list,struct node *node,unsigned id,void* data){
    //向当前节点之后插入值
    struct node *new_node = (struct node*)malloc(sizeof(struct node));

    //重新连接
    if (node->next==NULL){//如果是最后一个
        list->next_to_last = node;
        new_node->next = NULL;
    }else{
        new_node->next = node->next;
        new_node->next->last = new_node;
    }
    new_node->last = node;
    node->next = new_node;

    //添加数据
    new_node->id = id;
    list->save_data(node->data,data);
    list->length++;
    return new_node;
}
static inline
int remove_front(struct link_list* list){
    //移除当前节点，返回当前长度
    if (list->head.next){
        struct node* node=list->head.next;
        list->head.next = list->head.next->next;
        list->free_data(node->data);
        free(node);
        list->length -=1;
        return list->length;
    }else return 0;
}
static inline
int remove_last(struct link_list* list){
    //移除最后一个节点，返回当前长度
    if (list->head.next){
        struct node* node=list->head.next;
        list->head.next = list->head.next->next;
        list->free_data(node->data);
        free(node);
        list->length -=1;
        return list->length;
    }else return 0;
}

static inline
int _move_end_to_front(struct link_list*list){
    //把最后一个节点移到最前面
    if(list->length==1){
        return 0;
    }
    struct node* now_first_node = list->next_to_last,
               * now_second_node = list->head.next;
    list->head.next = now_first_node;
    list->
    


    
}
void init_records(){
    //重建索引到内存

}

