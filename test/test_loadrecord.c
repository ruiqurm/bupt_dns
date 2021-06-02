#include"local_record.h"
#include<stdlib.h>
#include<stdio.h>
#include"log.h"
// struct staticCache local;
struct cacheset cacheset;

bool check(struct staticCache* cache ,const char *q,const char* a){
    static char buffer[256];
    struct static_record_data *pdata = get_static_cache(cache,q);
    while(pdata){
        inet_ntop(AF_INET,&pdata->ip, buffer, sizeof(buffer));
        if(pdata!=NULL && !strcmp(buffer,a)){
            return true;
        }
        pdata=pdata->next;
    }
    log_error("error:%s %s %s",q,a,buffer);
    exit(EXIT_FAILURE);
}
#define check_A(q,a)  check(&cacheset.A.local,q,a)
#define check_black(q,a)  check(&cacheset.blacklist,q,a)
int main(int argc,char**argv){
    log_set_level(LOG_DEBUG);
    char file[128];
    if(argc==2){
        // strcpy_s(file,128,argv[1]);
        strcpy(file,argv[1]);
    }else{
        strcpy(file,"dnsrelay.txt");
        // strcpy_s(file,128,"dnsrelay.txt");
    }
    
    if(!load_local_record(&cacheset,file)){
        printf("can't find dnsrelay.txt\n");
        exit(EXIT_FAILURE);
    }
    check_A("khm.l.google.com","210.242.125.98");
    check_A("ci1.googleusercontent.com","74.125.207.132");
    check_A("uploads.clients.google.com","74.125.207.117");
    check_A("uploads.clients.google.com","74.125.207.113");
    check_A("lh0.googleusercontent.com","74.125.207.132");
    check_A("productideas.appspot.com","64.233.185.141");
    check_black("zelnet.ru","0.0.0.0");
    check_black("www.yule21.com","0.0.0.0");
    check_black("www.yysky.net","0.0.0.0");
    check_black("www.yyqy.com","0.0.0.0");
    printf("恭喜！\nおめでとうございます\nCongratulations!");
    return 0;
}