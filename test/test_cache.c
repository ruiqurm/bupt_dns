#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>
#define DEBUG 1
#include"cache.h"
#include"log.h"

struct myrecord {
  struct IP ip;
  time_t ttl; //过期时间
  struct record_data* next;//链表
  char label[MAX_NAME_LENGTH];
}records[2000];
struct  {
  char label[MAX_NAME_LENGTH];
  struct IP ip;
}tmp={"www.bupt.edu.cn",{0x12345678,IPV4}};
int len = sizeof("www.bupt.edu.cn");


void init_records(){
    time_t now = time(NULL);
    for(int i=0;i<2000;i++){
        // memcpy(&records[i],&tmp,sizeof(record));
        sprintf((char*)&records[i].label,"%s%d",(char*)&tmp.label,i);
        records[i].ip.addr.v4 = 0x12345678+i;
        records[i].ttl = rand()%65536+now;
    }
}

cache testcache;
int main(){
    log_set_level(LOG_INFO);
    srand(time(NULL));
    init_records();
    // printf("%d",_hash("www.baidu.com1",1000))
    init_A_record_cache_default(&testcache);
    size_t now;
    // printf("%d %d",testcache.length,testcache.max_size);

    struct record_data tmp_data;
    tmp_data.next=NULL;
    tmp_data.label = malloc(256); 
    strcpy(tmp_data.label,records[0].label);
    // strcpy_s(tmp_data.label,256,records[0].label);
    tmp_data.ip = records[0].ip;
    tmp_data.ttl = time(NULL)+1000000;
    set_cache_A_record(&testcache,tmp_data.label,(void*)&tmp_data);    
    struct record_data *tmp = get_cache_A_record(&testcache,(const char*)records[0].label);
    if(!tmp){log_error("can't get cache",tmp->label);exit(0);}

    clock_t start_time = clock(),end_time;
    for (int i =0;i<100000;i++){
        int r = rand() % 2000;
        // log_debug("%d",i);
        if(now%500==0)now =time(NULL);
        if(r%4==0){
            log_debug("set cache");
            strcpy(tmp_data.label,records[r].label);
            // strcpy_s(tmp_data.label,256,records[r].label);
            tmp_data.ip = records[r].ip;
            tmp_data.ttl = records[r].ttl;
            set_cache_A_record(&testcache,tmp_data.label,&tmp_data);
        }else{
            log_debug("get cache");
            struct record_data *tmp = get_cache_A_record(&testcache,(const char*)records[r].label);
            if(tmp)
                log_debug("%s\n",tmp->label);
            else   
                log_debug("No exist\n");
        }
        // printf("%s\n",records[r].label);
        test_normal(&testcache);
        // printf("\n");
    }
    end_time = clock();
    test_normal(&testcache);
    log_info("test 100000 randomly insert and get successfully in %f",(double)(end_time - start_time) / CLOCKS_PER_SEC);

    struct record_data* send_data[10];
    struct record_data _data[10];
    for (int i =0;i<10000;i++){
        log_debug("iter=%d\n",i);
        int r = rand() % 1996;
        if(now%500==0)now =time(NULL);

        if(r%4==0&&r!=0){
            for(int j=0;j<4;j++){
                memcpy(&_data[j].ip,&records[r+j].ip,sizeof(struct IP));
                // memcpy_s(&_data[j].ip,sizeof(struct IP),&records[r+j].ip,sizeof(struct IP));
                _data[j].ttl = records[r+j].ttl;
                _data[j].label = records[r+j].label;
            }
            send_data[0] = &_data[0];
            send_data[2] = &_data[1];
            send_data[1] = &_data[2];
            send_data[3] = &_data[3];
            // printf("%s",send_data[0]->label);
            // puts(send_data[0]);
            // set_cache_A_record(&testcache,tmp_data.label,&tmp_data);
            set_cache_A_multi_record(&testcache,records[r].label,(void**)send_data,4);
        }else{
            log_debug("get cache label=%s",records[r].label);
            get_cache_A_record(&testcache,(const char*)records[r].label);
        }
    }
    // printf("%d",set_cache_A_record((const char*)&records[0].label,&records[0].ip));
    // printf("%d",_cache((const char*)&records[0].label,&records[0].ip));
    free_cache(&testcache);
    printf("恭喜！\nおめでとうございます\nCongratulations!");
    return 0;
}