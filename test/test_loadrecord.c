#include"local_record.h"
#include<stdlib.h>
#include<stdio.h>
#include"log.h"
cache local;
bool _check(const char *q,const char* a){
    static char buffer[256];
    struct record_data *pdata = get_cache_A_record(&local,q);
    if(pdata){
        inet_ntop(AF_INET,&pdata->ip, buffer, sizeof(buffer));
        if(pdata!=NULL && !strcmp(buffer,a)){
            return false;
        }
    }else{
        return false;
    }
    return true;
}
void check(const char*q,const char*a){
    if(_check(q,a)){
        printf("error:%s %s\n",q,a);
        exit(EXIT_FAILURE);
    }
}

int main(int argc,char**argv){
    log_set_level(LOG_INFO);
    char file[128];
    if(argc==2){
        strcpy_s(file,128,argv[1]);
    }else{
        strcpy_s(file,128,"dnsrelay.txt");
    }
    
    if(!load_local_A_record(&local,file)){
        printf("can't find dnsrelay.txt\n");
        exit(EXIT_FAILURE);
    }
    check("khm.l.google.com","210.242.125.98");
    check("ci1.googleusercontent.com","74.125.207.132");
    check("lh0.googleusercontent.com","74.125.207.132");
    check("productideas.appspot.com","64.233.185.141");
    printf("test successfully\n");
    return 0;
}