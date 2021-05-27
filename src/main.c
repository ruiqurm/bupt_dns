#include "cache.h"
#include "common.h"
#include "log.h"
#include "relay.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct sockaddr SA;

void init() {

  init_query_server("10.3.9.4");
  // init_cache();
  #ifdef _WIN64
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2),&wsaData)!=0){
      log_fatal_exit_shortcut("winsock init error");
    }
    system("chcp 65001");//修改控制台格式为65001
  #endif
}
// void cleanup(){

// }
int main(int argc, char **argv) {
  int sockfd;
  struct sockaddr_in servaddr,inaddr, cliaddr;
  socklen_t len, clilen;
  char recv_buffer[MAX_DNS_SIZE];
  char query_buffer[MAX_DNS_SIZE];
  struct dns_header header;
  struct question question;
  struct answer ans[4];
  struct record_data *data;
  unsigned ip;
  int enable_cache;
  int _size; //临时变量
  time_t now;
  cache global_cache;
  init_default_cache(&global_cache);
  init();

  clilen = sizeof(cliaddr);
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == INVALID_SOCKET){
    log_fatal_exit_shortcut("socket error");
		exit(0);
	}
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  servaddr.sin_port = htons(DNS_SERVER_PORT);

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
    int rec;
    // log_info()
    rec = recvfrom(sockfd, recv_buffer, MAX_DNS_SIZE, 0, (SA *)&cliaddr, &len);
    if (rec==SOCKET_ERROR){
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
    ip = cliaddr.sin_addr.s_addr; //!不需要翻过来
    log_info("Recvfrom %d:%d:%d:%d:port:%d ", *(char *)&ip,
             *(((char *)&ip) + 1), *(((char *)&ip) + 2), *(((char *)&ip) + 3),
             ntohs(cliaddr.sin_port));

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
    log_info("请求 %s",question.label);
    switch (question.qtype) {
    case A:
      // case AAAA:
      enable_cache = 1;
      //只处理这几个
      break;
    default:
      enable_cache = 0;
      break;
    }
    if (!enable_cache) {
      relay(header.id, &question, recv_buffer, sockfd, &cliaddr);
      continue;
    }
    if ((data = get_cache(&global_cache,question.label))) {
      log_debug("取到缓存");
      // sprint_dns(recv_buffer);
      now = time(NULL);
      ans[0].ttl = data->ttl - now;
      memcpy(&ans[0].address, &data->ip, sizeof(struct IP));
      strcpy_s(ans[0].name, sizeof(ans[0].name),question.label);
      ans[0].type = question.qtype;
      ans[0].class_ = question.qclass;
      _size = write_dns_response_by_query(recv_buffer, ans, 1);
      // sprint_dns(query_buffer);
      if (sendto(sockfd, recv_buffer, _size, 0, (SA *)&cliaddr,
                 sizeof(cliaddr)) < 0) {
        log_error_shortcut("send error");
      } else {
        log_info("成功回复");
      }
    } else {
      log_debug("未取到缓存");
      _size = query(question.label, question.qtype, query_buffer);
      if (_size>0){
        int _ans_num = read_dns_answers(ans, query_buffer);
        struct dns_header *pheader;
        if (_ans_num > 0) {
          now = time(NULL);
          set_cache(&global_cache,ans[0].name, &ans[0].address, ans[0].ttl + now);
        }
        pheader = (struct dns_header *)query_buffer;
        pheader->id = htons(header.id);
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
