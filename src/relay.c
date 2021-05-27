#include "relay.h"
#include "log.h"
#include <errno.h>
#include <string.h>
struct sockaddr_in query_server;

void init_query_server(const char *query_server_addr) {
  memset(&query_server, 0, ADDR_LEN);
  query_server.sin_family = AF_INET;
  query_server.sin_port = htons(DNS_SERVER_PORT);
  inet_pton(AF_INET,query_server_addr,&query_server.sin_addr.s_addr);
}

int query(char questions[], int type, char buffer[MAX_DNS_SIZE]) {
  //向DNS服务器询问
  int sockfd;
  socklen_t len = sizeof(query_server);
  int total_size = 0;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0){
    log_fatal_exit_shortcut("socket error");
		exit(0);
	}
  //  10.21.153.74
  // struct timeval tv;
  // tv.tv_sec = 5;
  // tv.tv_usec = 0;
  // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
  //     log_error("set timeout error",strerror(errno));
  // }

  total_size = write_dns_query(buffer, questions, type);
  if (sendto(sockfd, (char *)buffer, total_size, 0, (SA *)&query_server,
             ADDR_LEN)<0) {
    log_error_shortcut("sendto error");
    return -1;
  }
  
  total_size = recvfrom(sockfd, buffer, MAX_DNS_SIZE, 0,
                             (SA *)&query_server, &len);
  
  if (total_size==SOCKET_ERROR) {
    log_error_shortcut("recvfrom error:");
    return -1;
  }
  // sprint_dns(buffer);
  return total_size;
}

void relay(int id, struct question *question, char buffer[MAX_DNS_SIZE],
           int sockfd, struct sockaddr_in6 *target) {
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