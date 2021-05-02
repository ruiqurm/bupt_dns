#include<stdio.h>
#include<string.h>
#include"cache.h"

typedef struct record_data myrecord;
myrecord records[2000];
myrecord tmp = {"www.bupt.edu.cn",{0x12345678,IPV4}};
int len = sizeof("www.bupt.edu.cn");
// extern unsigned _hash(const char* label,size_t size);
// extern void set_cache(const char* label,const struct IP* ip);



void init_records(){
    for(int i=0;i<2000;i++){
        // memcpy(&records[i],&tmp,sizeof(record));
        sprintf((char*)&records[i].label,"%s%d",(char*)&tmp.label,i);
        records[i].ip.addr.v4 = 0x12345678+i;
    }
}
#include<time.h>
#include<stdlib.h>

int main(){
    srand(time(NULL));
    init_records();
    // printf("%d",_hash("www.baidu.com1",1000))
    init_list();
    for (int i =0;i<10000000;i++){
        int r = rand() % 2000;
        if(r%4==0){
            set_cache((const char*)&records[r].label,&records[r].ip);
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