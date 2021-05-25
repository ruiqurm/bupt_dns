#include"local_record.h"
#include"log.h"

#include<stdio.h>
#include<errno.h>
#include<string.h>
// #include<stdbool.h>
bool load_local_A_record(cache *Cache,const char* filename){
    char buffer[630];
    char name[513];
    char ip[32];
    FILE* file;
    int line=0;
    if(Cache->is_init){
        free_cache(Cache);
    }
    if(fopen_s(&file,filename,"r")!=0){//fopen_s和fopen的接口不一样
        log_error("file open error:%s",strerror_s(name,513,errno));
        return false;
    }
    while (fgets(buffer, 630, file)){
        if(strcmp(buffer,"\n")){
            line++; // 非空行
        }
    }
    rewind(file);
    // fseek(file, 0, SEEK_SET);//移动读头回开头
    init_cache(Cache,line,line*sizeof(struct record),0.9);
    int _ = line*2;//防止无限循环
    struct IP ips;
    ips.type=IPV4;
    while(_--){
        if (fscanf(file, "%s%s",ip,name)==2){
            inet_pton(AF_INET,ip,&ips.addr);
            log_debug("read url:%s  ip:%s\n",name,ip);
            fflush(stdin);
            set_cache(Cache,name,&ips,DNS_CACHE_PERMANENT);
            line--;
            if(line==0)break;
        }
    }
    return true;
}

// int main(){
//     read_local_record("dnsrelay.txt");
// }