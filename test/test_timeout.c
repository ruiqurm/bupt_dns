#include <stdio.h>
#include<stdlib.h>
#include "common.h"
#include <stdio.h>
#include "log.h"
#ifdef _WIN64
#include <windows.h>
#include <WinSock2.h>
#else
#include <unistd.h>
#endif

typedef struct sockaddr SA;
int ss;
char buffer[1024];
struct answer ans[50];
struct dns_header header;
struct sockaddr_in query_server;
static inline void log_ip(const char* str,struct IP* addr){
    log_info("%s %d:%d:%d:%d",str,
              addr->addr.v4byte[0],addr->addr.v4byte[1],addr->addr.v4byte[2],addr->addr.v4byte[3]);
}
int init(){
  #ifdef _WIN64
    WSADATA wsaData;
    int x;
    if(x=WSAStartup(MAKEWORD(2,2),&wsaData)!=0){//协商版本winsock 2.2
      log_fatal_exit_shortcut("winsock init error");
    }
    system("chcp 65001");//修改控制台格式为65001
  #endif
    
    if( (ss = socket(AF_INET, SOCK_DGRAM, 0))<0){
      log_fatal_exit_shortcut("socket open error");
  }
  memset(&query_server, 0, ADDR_LEN);
  query_server.sin_family = AF_INET;
  query_server.sin_port = htons(DNS_SERVER_PORT);
  inet_pton(AF_INET,"127.0.0.1",&query_server.sin_addr.s_addr);
}

int main(){
    init();
     FILE *in;
    if( (in = fopen("dnsrelay1.txt","r"))==NULL){//fopen_s和fopen的接口不一样
        log_error_shortcut("file open error:");
        exit(EXIT_FAILURE);
    }
     printf("请确定自己离线中\n");
     #ifdef _WIN64
     Sleep(1000);
     #else
     sleep(1000);
     #endif
    char ip[1024];
    char name[1024];
	char result[10240];
  int error=0;
  int x=0;


    while(fscanf(in, "%s",name)==1){
        printf("%d ===================== %s\n",++x,name);
        struct question dns_question;
        strcpy( dns_question.label,name);
        dns_question.qclass= HTTP_CLASS;
        dns_question.qtype = RRTYPE_A;
        int total_size=write_dns_query(buffer,name,RRTYPE_A);
         if ( (sendto(ss, buffer, total_size, 0, (SA *)&(query_server),sizeof(query_server) )  ) < 0){
            // printf("%d\n",total_size);
          log_error_shortcut("sendto error:");
      }
      int len=sizeof(query_server) ;
      if (( (total_size = recvfrom(ss, buffer, MAX_DNS_SIZE, 0,(SA *)&query_server, &len))<0)){
          log_error_shortcut("recvfrom error:");
      }
      read_dns_header(&header,buffer);
      if(header.rcode!=5){
          printf("ERROR %d %s\n",++error,name);
      }
      
    }
    if(error){
    printf("find %d wrong",error);
    exit(EXIT_FAILURE);}
    else{
      printf("恭喜！\nおめでとうございます\nCongratulations!");
    }
    system("pause");
    return 0;
}