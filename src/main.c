#include "cache.h"
#include "common.h"
#include "log.h"
#include "local_record.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>


/*************************
 *                       *
 *        常量和宏        *
 *                       *
 *************************/
#define _CRT_SECURE_NO_WARNINGS

//允许接收来自IPV6的请求（向下兼容）
#define ACCEPT_IPV6_REQUEST

#ifdef ACCEPT_IPV6_REQUEST
typedef struct sockaddr_in6 _sockaddr_in;
#else
typedef struct sockadd_in _sockaddr_in;
#endif

typedef struct sockaddr SA;

#ifdef __linux
typedef WORD unsigned short
#endif


/*************************
 *                       *
 *        函数            *
 *                       *
 *************************/

/**
 * @brief 初始化,设置要询问的服务器
 * @param query_server_addr 询问服务器地址
 */
void init_query_server(const char *query_server_addr);

/**
 * @brief 快速询问一个服务器
 *
 * @param questions question
 * @param type type
 * @param buffer 返回的数据（报文）
 * @return 写入字节数
 */
int query(char questions[], int type, char buffer[MAX_DNS_SIZE]);

/**
 * @brief 中继
 *
 * @param id 请求报文id
 * @param question question
 * @param buffer 缓冲区，不需要放数据，但是要大于DNS_MAX_SIZE；节省开销
 * @param sockfd sock端口
 * @param target 目标ip地址
 */
void relay(int id, struct question *question, char buffer[MAX_DNS_SIZE],
           int sockfd, _sockaddr_in *target);



void init();

/*************************
 *                       *
 *        全局变量        *
 *                       *
 *************************/
struct cacheset cacheset;//缓存
struct sockaddr_in query_server;
int server_recv_sockfd,server_send_sockfd;//提供服务的sockfd
int query_sockfd;//向服务器询问用的文件描述符,只支持IPV4

int main(int argc, char **argv) {
  // struct sockaddr_in servaddr,inaddr, cliaddr;

  //_sockaddr_in在不启用ipv6的情况下是sockaddr_in;
  //否则是struct sockaddr_in6
  _sockaddr_in servaddr,cliaddr;

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

  fd_set rset;//读集合
  fd_set wset;//写集合
  FD_ZERO(&rset);
  FD_ZERO(&wset);

  // fileno
  
  init();

  clilen = sizeof(cliaddr);
  // sockfd = socket(AF_INET, SOCK_DGRAM, 0);
     

  memset(&servaddr, 0, sizeof(servaddr));
  #ifdef ACCEPT_IPV6_REQUEST
  servaddr.sin6_family = AF_INET6;
  servaddr.sin6_addr = in6addr_any;
  servaddr.sin6_port = htons(DNS_SERVER_PORT);
  #else
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  servaddr.sin_port = htons(DNS_SERVER_PORT);
  #endif
  

  // int nMode = 1; // 1: NON-BLOCKING
  // if (ioctlsocket (sockfd, FIONBIO, &nMode) == SOCKET_ERROR)
  // {
  //     // closesocket(SendingSocket);
  //     log_fatal("Winsock:%s",WSAGetLastError());
  //     WSACleanup();
  //     exit(EXIT_FAILURE);
  // }
  if (bind(server_send_sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {
  //!可能无权限
    log_fatal_exit_shortcut("bind error");
  }

  log_info("Listen on localhost:53");
  bool query_sockfd_status = 0;//0是将要读，1是将要写
  int max_fd = query_sockfd + 1;
  while (1) {

    // FD_SET(server_recv_sockfd,&rset);
    // FD_SET(server_send_sockfd,&wset);
    // if(query_sockfd_status){
    //   FD_SET(query_sockfd,&rset);
    // }else{
    //   FD_SET(query_sockfd,&wset);
    // }
    // int word_fd = select(max_fd,&rset,&wset,NULL,NULL);
    len = clilen;
    int rec;
    if ((rec = recvfrom(server_send_sockfd, recv_buffer, MAX_DNS_SIZE, 0, (SA *)&cliaddr, &len))<0){
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
    #ifdef ACCEPT_IPV6_REQUEST
    ip = (uint16_t*)&cliaddr.sin6_addr;//跨平台
    log_info("Recvfrom %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x port:%d ",
      ip[0],ip[1],ip[2],ip[3],ip[4],ip[5],ip[6],ip[7],ntohs(cliaddr.sin6_port));
    #else
    ip = cliaddr.sin_addr.s_addr; //!不需要翻过来
    log_info("Recvfrom %d:%d:%d:%d:port:%d ", *(char *)&ip,
             *(((char *)&ip) + 1), *(((char *)&ip) + 2), *(((char *)&ip) + 3),
             ntohs(cliaddr.sin_port));
    #endif

    read_dns_header(&header, recv_buffer);
    // sprint_dns(recv_buffer);
    if (header.flags != htons(FLAG_QUERY)) {
      //不是询问，也丢弃
      struct dns_header* buffer_=(struct dns_header*) recv_buffer;
      buffer_->rcode=5;
      if (sendto(server_send_sockfd, recv_buffer, rec , 0,(SA *)&cliaddr,
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
      relay(header.id, &question, recv_buffer, server_send_sockfd, &cliaddr);
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
      log_info("%d",_size);
      if (sendto(server_send_sockfd, recv_buffer, _size, 0, (SA *)&cliaddr,
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
        if (sendto(server_send_sockfd, query_buffer, _size, 0, (SA *)&cliaddr,
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

  #ifdef ACCEPT_IPV6_REQUEST
  int no = 0;  
  if( (server_send_sockfd = socket(PF_INET6, SOCK_DGRAM, 0))<0){
    log_fatal_exit_shortcut("socket open error");
  }
  if (setsockopt(server_send_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no))<0){
    log_fatal_shortcut("failed set ipv6 only off:");
  }
  #else
  if( (server_send_sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0){
      log_fatal_exit_shortcut("socket open error");
  }
  #endif

  if( (query_sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0){
      log_fatal_exit_shortcut("socket open error");
  }
  #ifdef _linux
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  if (setsockopt(query_sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
      log_error("set timeout error",strerror(errno));
  }
  #elif defined(_WIN64)
  DWORD timeout = 1 * 1000;//设置超时时间
  if(setsockopt(query_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout)<0){
      log_error_shortcut("set timeout error");
  }
  #endif
  log_info("init all done");
}




void init_query_server(const char *query_server_addr) {
  memset(&query_server, 0, ADDR_LEN);

  query_server.sin_family = AF_INET;
  query_server.sin_port = htons(DNS_SERVER_PORT);
  inet_pton(AF_INET,query_server_addr,&query_server.sin_addr.s_addr);
  
  // inet_pton(AF_INET,query_server_addr,&query_server.sin_addr.s_addr);
}

int query(char questions[], int type, char buffer[MAX_DNS_SIZE]) {
  //向DNS服务器询问
  socklen_t len = sizeof(query_server);
  int total_size = 0;

  
  total_size = write_dns_query(buffer, questions, type);
  if (sendto(query_sockfd, (char *)buffer, total_size, 0, (SA *)&query_server,
             ADDR_LEN)<0) {
    log_error_shortcut("sendto error");
    return -1;
  }

  total_size = recvfrom(query_sockfd , buffer, MAX_DNS_SIZE, 0,
                             (SA *)&query_server, &len);
  if (total_size<0) {
    log_error_shortcut("recvfrom error:");
    return -1;
  }
  // sprint_dns(buffer);
  return total_size;
}

void relay(int id, struct question *question, char buffer[MAX_DNS_SIZE],
           int sockfd, _sockaddr_in *target) {
  //中继
  int size;
  struct dns_header *header;
  log_debug("中继:向DNS服务器请求");
  size = query(question->label, question->qtype, buffer);
  if(size==-1){
    log_error_shortcut("未从服务器获取到数据.");
    return;
  }
  header = (struct dns_header *)buffer;
  header->id = htons(id);
  if (sendto(sockfd, buffer, size, 0, (SA *)target, sizeof(*target)) < 0) {
    log_error_shortcut("sendto error:");
  } else {
    log_info("成功回复");
  }
}