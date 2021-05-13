#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>
#include"cache.h"

typedef struct record_data myrecord;
myrecord records[2000];
myrecord tmp = {"www.bupt.edu.cn",{0x12345678,IPV4}};
int len = sizeof("www.bupt.edu.cn");
// extern unsigned _hash(const char* label,size_t size);
// extern void set_cache(const char* label,const struct IP* ip);



void init_records(){
    time_t now = time(NULL);
    for(int i=0;i<2000;i++){
        // memcpy(&records[i],&tmp,sizeof(record));
        sprintf((char*)&records[i].label,"%s%d",(char*)&tmp.label,i);
        records[i].ip.addr.v4 = 0x12345678+i;
        records[i].ttl = rand()%1+now;
    }
}


int main(){
    srand(time(NULL));
    init_records();
    // printf("%d",_hash("www.baidu.com1",1000))
    init_cache();
    size_t now;
    for (int i =0;i<10000;i++){
        int r = rand() % 2000;
        if(now%500==0)now =time(NULL);
        if(r%4==0){
            set_cache((const char*)&records[r].label,&records[r].ip,records[r].ttl-now);
        }else{
            get_cache((const char*)&records[r].label);
        }
        // printf("%s\n",records[r].label);
        
        // printf("%d\n",i);
    }
    printf("\n");
    // printf("%d",set_cache((const char*)&records[0].label,&records[0].ip));
    // printf("%d",_cache((const char*)&records[0].label,&records[0].ip));
    return 0;
}