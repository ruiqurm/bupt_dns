#include"local_record.h"
#include"log.h"

#include<stdio.h>
#include<errno.h>
#include<string.h>
// #include<stdbool.h>
#define _CRT_SECURE_NO_DEPRECATE

bool load_local_record(struct cacheset *cacheset,const char* filename){
    char buffer[630];
    char name[513];
    char ip[32];
    struct IP IP;
    FILE* file;
    int line=0,_line;
    int v4 = 0,v6 = 0,black=0;
    int sizeofip = sizeof(struct IP);

    if(fopen_s(&file,filename,"r")!=0){//fopen_s和fopen的接口不一样
        log_error("file open error:%s",strerror_s(name,513,errno));
        return false;
    }
    while (fgets(buffer, 630, file)){
        if(strcmp(buffer,"\n")){
            line++; // 非空行
        }
    }
    log_info("read %d lines",line);

    // 首先扫一遍，确定个数
    rewind(file);
    _line = line;
    int _ = _line*2;//防止无限循环
    while(_--){
        if (fscanf(file, "%s%s",ip,name)==2){
            if (inet_pton(AF_INET,ip,&IP.addr)==1){
                if(IP.addr.v4 == 0){
                    black++;
                }else{
                    v4++;
                }
            }else if (inet_pton(PF_INET6,ip,&IP.addr)==1){
                 if(IP.addr.v6==0){
                    black++;
                }else{
                    log_info("-------------%s",ip);
                    v6++;
                }
            }
            _line--;
            if(_line==0)break;
        }
    }


    log_info("v4 = %d,v6 = %d,black=%d",v4,v6,black);
    // 初始化集合
    if(v4>0){
        init_staticCache(&cacheset->A.local,v4);
    }
    if(v6>0){
        init_staticCache(&cacheset->A.local,v6);
    }
    if(black>0){
        init_staticCache(&cacheset->blacklist,black);
    }
    rewind(file);
     _ = line*2;//防止无限循环
    while(_--){
        if (fscanf(file, "%s%s",ip,name)==2){
            if (inet_pton(AF_INET,ip,&IP.addr)==1){
                IP.type = IPV4;
                if(IP.addr.v4==0){
                    set_static_cache(&cacheset->blacklist,name,&IP);
                }else{
                    set_static_cache(&cacheset->A.local,name,&IP);
                }
            }else if (inet_pton(PF_INET6,ip,&IP.addr)==1){
                IP.type = IPV6;
                 if(IP.addr.v6==0){
                    set_static_cache(&cacheset->blacklist,name,&IP);
                }else{
                    set_static_cache(&cacheset->AAAA.local,name,&IP);
                }
            }
            line--;
            if(line==0)break;
        }
    }
    return true;
}

// int main(){
//     read_local_record("dnsrelay.txt");
// }