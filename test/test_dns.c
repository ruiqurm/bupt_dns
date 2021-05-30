#include "common.h"
#include <stdio.h>
#include <windows.h>
#include <WinSock2.h>
#include "log.h"
typedef struct sockaddr SA;
int ss;
char buffer[1024];
  struct answer ans[50];
struct sockaddr_in query_server;
static inline void log_ip(const char* str,struct IP* addr){
    log_info("%s %d:%d:%d:%d",str,
              addr->addr.v4byte[0],addr->addr.v4byte[1],addr->addr.v4byte[2],addr->addr.v4byte[3]);
}
int init(){
    WSADATA wsaData;
    int x;
    if(x=WSAStartup(MAKEWORD(2,2),&wsaData)!=0){//协商版本winsock 2.2
      log_fatal_exit_shortcut("winsock init error");
    }

    system("chcp 65001");//修改控制台格式为65001
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
     FILE *in= fopen("dnsrelay.txt", "r");
    char ip[1024];
    char name[1024];
	char result[10240];
  int error=0;
    while(fscanf(in, "%s%s",ip,name)==2){
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
      int ip_num;
      int ans_num = read_dns_answers(ans, buffer);
      // printf("%d",ans_num);
      if(ans_num==0&&!strcmp(ip,"0.0.0.0")){
        continue;
      }
      else if(ans_num==0){
        printf("ERROR %d %s %s\n",++error,ip,name);
      }
      else{
        inet_pton(AF_INET,ip,&ip_num);
        int flag=0;
        for(int i=0;i<ans_num;i++){
          if(ip_num==ans[i].address.addr.v4){
          flag=1;
          break;
        }
        }
        if(!flag){
          printf("ERROR %d %s %s\n",++error,ip,name);
        }
      }
    }
    if(error){
    printf("find %d wrong",error);
    exit(EXIT_FAILURE);}
    else{
      printf("恭喜！\nおめでとうございます\nCongratulations!");
    }
    return 0;
}