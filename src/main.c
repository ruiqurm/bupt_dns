#include "cache.h"
#include "common.h"
#include "log.h"
#include "relay.h"
#include "local_record.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#define _CRT_SECURE_NO_WARNINGS
typedef struct sockaddr SA;

#ifdef __linux
typedef WORD unsigned short
#endif

struct cacheset cacheset;//缓存

void init() {
  log_set_level(LOG_INFO);

  load_local_A_record(&cacheset.A.local,"dnsrelay.txt");
  init_A_record_cache_default(&cacheset.A.temp);
  init_A_record_cache_default(&cacheset.AAAA.temp);

  init_query_server("10.3.9.4");
  // init_cache();
  #ifdef _WIN64
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2),&wsaData)!=0){//协商版本winsock 2.2
      log_fatal_exit_shortcut("winsock init error");
    }
    system("chcp 65001");//修改控制台格式为65001
  #endif
  #ifdef __linux
    signal(SIGPIPE, SIG_IGN);
    //忽略管道破裂
  #endif
  // signal(SIGINT,handle_)
}

int main(int argc, char **argv) {
  int sockfd;
  // struct sockaddr_in servaddr,inaddr, cliaddr;
  struct  sockaddr_in6 servaddr,cliaddr;

  socklen_t len, clilen;
  char recv_buffer[MAX_DNS_SIZE];
  char query_buffer[MAX_DNS_SIZE];
  struct dns_header header;
  struct question question;
  struct answer ans[10];
  struct record_data tmp_ans_data[10];
  struct record_data *data;
  // unsigned ip;
  WORD* ip;
  int enable_cache;
  int _size; //临时变量
  time_t now;
  
  
  init();

  clilen = sizeof(cliaddr);
  // sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  sockfd = socket(PF_INET6, SOCK_DGRAM, 0);//ipv6
  int no = 0;     
  if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no))<0){
    log_error_shortcut("set ipv6 only off:");
  }
  if (sockfd < 0){
    log_fatal_exit_shortcut("socket error");
		exit(0);
	}
  memset(&servaddr, 0, sizeof(servaddr));
  // servaddr.sin_family = AF_INET;
  servaddr.sin6_family = AF_INET6;
  // servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  servaddr.sin6_addr = in6addr_any;
  // servaddr.sin_port = htons(DNS_SERVER_PORT);
  servaddr.sin6_port = htons(DNS_SERVER_PORT);

  // int nMode = 1; // 1: NON-BLOCKING
  // if (ioctlsocket (sockfd, FIONBIO, &nMode) == SOCKET_ERROR)
  // {
  //     // closesocket(SendingSocket);
  //     log_fatal("Winsock:%s",WSAGetLastError());
  //     WSACleanup();
  //     exit(EXIT_FAILURE);
  // }
  if (bind(sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {
  //!可能无权限
    log_fatal_exit_shortcut("bind error");
  }

  log_info("Listen on localhost:53");

  while (1) {

    len = clilen;
    if (recvfrom(sockfd, recv_buffer, MAX_DNS_SIZE, 0, (SA *)&cliaddr, &len)<0){
      //接收失败，直接丢弃
      // #ifndef _WIN64
      //   log_error("recvfrom error:%s", strerror(errno));
      //   errno = 0;
      // #else
      //   log_error("bind error:code[%d]", GetLastError());
      // #endif
      log_info("%d",len);
      continue;
    }
    ip = (uint16_t*)&cliaddr.sin6_addr;//跨平台
    // ip = cliaddr.sin_addr.s_addr; //!不需要翻过来
    // log_info("Recvfrom %d:%d:%d:%d:port:%d ", *(char *)&ip,
    //          *(((char *)&ip) + 1), *(((char *)&ip) + 2), *(((char *)&ip) + 3),
    //          ntohs(cliaddr.sin_port));
    log_info("Recvfrom %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x port:%d ",
          ip[0],ip[1],ip[2],ip[3],ip[4],ip[5],ip[6],ip[7],ntohs(cliaddr.sin6_port));
    read_dns_header(&header, recv_buffer);
    // sprint_dns(recv_buffer);
    if (header.flags != htons(FLAG_QUERY)) {
      //不是询问，也丢弃
      struct dns_header* buffer_=(struct dns_header*) recv_buffer;
      buffer_->rcode=5;
      if (sendto(sockfd, recv_buffer, rec , 0,(SA *)&cliaddr,
                 sizeof(cliaddr)) < 0) {
        log_error_shortcut("sendto error:");
      } else {
        log_info("成功回复");
      }
      continue;
    }

    read_dns_questions(&question, recv_buffer);
    log_info("%s %s",RRtype_to_str(question.qtype),question.label);
    switch (question.qtype) {
    case A:
    // case AAAA:
      enable_cache = 1;
      //只缓存A记录
      break;
    default:
      enable_cache = 0;
      break;
    }
    if (!enable_cache) {
      relay(header.id, &question, recv_buffer, sockfd, &cliaddr);
      continue;
    }
    if ((data = get_cache_A_record(&cacheset.A.local,question.label))||
         (data = get_cache_A_record(&cacheset.A.temp,question.label))) {
      log_info("取到缓存");
      // sprint_dns(recv_buffer);
      now = time(NULL);
      int count=0;
      while(data){
        ans[count].ttl = data->ttl - now;
        memcpy(&ans[count].address, &data->ip, sizeof(struct IP));
        strcpy_s(ans[count].name, sizeof(ans[0].name),question.label);
        ans[count].type = question.qtype;
        ans[count].class_ = question.qclass;
        ans[count].has_cname = false;
        strcpy_s(ans[count].name, sizeof(ans[0].name),question.label);
        count++;
        data=data->next;
      }
      _size = write_dns_response_by_query(recv_buffer, ans, count);
      // sprint_dns(query_buffer);
      if (sendto(sockfd, recv_buffer, _size, 0, (SA *)&cliaddr,
                 sizeof(cliaddr)) < 0) {
        log_error_shortcut("send error");
      } else {
        log_info("成功回复");
      }
    } else {
      log_info("未取到缓存");
      _size = query(question.label, question.qtype, query_buffer);
      if (_size>0){
        int _ans_num = read_dns_answers(ans, query_buffer);
        log_info("ans_num: %d\n",_ans_num);
        struct dns_header *pheader;
        if (_ans_num > 0) {
          int count =0;
          struct record_data* send_data[10];
          now = time(NULL);
          for(int i=0;i<_ans_num;i++){
            if(ans[i].type==A){
              memcpy_s(&tmp_ans_data[count].ip,sizeof(struct IP),&ans[i].address,sizeof(struct IP));
              tmp_ans_data[count].label = ans[i].name;
              tmp_ans_data[count].ttl = now + ans[i].ttl;
              send_data[count] = &tmp_ans_data[count];
              count++;
            }
          }
          log_info("count:%d",count);
          if(count>0){
            set_cache_A_multi_record(&cacheset.A.temp,tmp_ans_data[0].label,(void**)send_data,count);
          }
        }
  
        pheader = (struct dns_header *)query_buffer;
        pheader->id = htons(header.id);
        // printf("%d\n",sizeof(query_buffer));
        if (sendto(sockfd, query_buffer, _size, 0, (SA *)&cliaddr,
                  sizeof(cliaddr)) < 0) {
          log_error_shortcut("send error");
          errno = 0;
        } else {
          log_info("成功回复");
        }
      }else{
        log_error("回复失败");
      }
    }
  }
  return 0;
}
