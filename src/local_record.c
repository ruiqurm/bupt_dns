#include"local_record.h"
#include"log.h"
#include<stdio.h>
#include<errno.h>

// #include<stdbool.h>
bool read_local_record(const char* filename){
    char name[513];
    char ip[32];
    FILE* file;
    if(fopen_s(&file,filename,"r")!=0){//fopen_s和fopen的接口不一样
        log_error("file open error:%s",strerror_s(name,513,errno));
        return false;
    }
    while(fscanf_s(file, "%s %s",ip,name)==2){
        printf("%s %s\n",name,ip);
    }
    return true;
}

// int main(){
//     read_local_record("dnsrelay.txt");
// }