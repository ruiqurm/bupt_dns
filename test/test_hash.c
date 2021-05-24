#include"hash.h"
#include<stdlib.h>
#include<stdio.h>
#include<time.h>
#include<string.h>
#define TEST_SIZE 1000000
#define TABLE_SIZE 10000
typedef struct {
    int a;
}tdata;
unsigned hash(void *label){
    return ((tdata *)label)->a;
}
bool compare(void * record,void* label){
    return (((tdata *)record)->a ==*(int*)label);
}

int main(int argc,char**argv){
    srand(time(NULL));
    // bool check[TEST_SIZE];
    bool* check = malloc(sizeof(bool)*TEST_SIZE);
    tdata* data = malloc(sizeof(tdata)*TEST_SIZE);
    memset(check,0,sizeof(check));
    hashtable table;
    if (init_hashtable(&table,TABLE_SIZE,0.7,hash,compare)==false){
        exit(EXIT_FAILURE);
    }
    for(int i=0;i<TEST_SIZE;i++){
        data[i].a = i;
        set_hashnode(&table,&i,&data[i],NULL);
    }
    bool is_find;
    for(int i=0;i<TEST_SIZE;i++){
        is_find = false;
        get_hashnode(&table,&i,&is_find);
        if(is_find==false){
            printf("%d get failure\n",i);
            exit(EXIT_FAILURE);
        }
    }
    for(int i=0;i<table.size;i++){
        hashnode* node = &table.nodes[i];
        int max_count=100;
        while(node->next){
            
            node = node->next;
            int d = ((tdata*)node->record)->a;
            if(false==check[d]){
                check[d]=true;
            }else{
                printf("%d:redundant element\n",i);
                exit(EXIT_FAILURE);
            }
            
            max_count--;
            if(max_count<=0){
                printf("%d:last member not null\n",i);
                exit(EXIT_FAILURE);
            }
        }
    }
    for(int i=0;i<TEST_SIZE;i++){
        if(check[i]==false){
            printf("not insert %d\n",i);
            exit(EXIT_FAILURE);
        }
    }
    printf("successful\n");
    clock_t now = clock(),next;
    for(int i=0;i<=TEST_SIZE;i++){
        reset_hash_node(&table,&i);
    }
    next=clock();
    printf("reset %d element with table sized %d in %f s\n",TEST_SIZE,table.size,(double)(next - now) / CLOCKS_PER_SEC);
    now =next;
    for(int i=0;i<TEST_SIZE;i++){
        int j =rand()%TEST_SIZE;
        set_hashnode(&table,&j,&data[j],NULL);
    }
    next=clock();
    printf("insert %d element randomly with table sized %d in %f s\n",TEST_SIZE,table.size,(double)(next - now) / CLOCKS_PER_SEC);
    now =next;
    for(int i=0;i<TEST_SIZE;i++){
        int j =rand()%TEST_SIZE;
        get_hashnode(&table,&j,NULL);
    }
    next=clock();
    printf("get %d element randomly with table sized %d in %f s\n",TEST_SIZE,table.size,(double)(next - now) / CLOCKS_PER_SEC);

    free_hashtable(&table);
    return 0;
}