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
typedef struct sockaddr_in _sockaddr_in;
#endif

typedef struct sockaddr SA;

#ifdef __linux
typedef WORD unsigned short
#endif

#define IDAadpter_SIZE 1024

#if IDAadpter_SIZE>32767
#error IDAadpter_SIZE should smaller than 32767
#endif

/*************************
 *                       *
 *        结构体         *
 *                       *
 *************************/
typedef struct{
  _sockaddr_in addr;
 unsigned short old_id;
 unsigned char type;
}IDtoAddr;

struct{
  struct {
    IDtoAddr info;
    bool valid;

  }data[IDAadpter_SIZE];
  bool is_init;
  unsigned short next;
}IDAdapter;

inline static short IDAdapter_push(unsigned short id,_sockaddr_in* source,unsigned char type){
  short next = IDAdapter.next;
  if(!IDAdapter.data[next].valid){
      IDAdapter.data[next].valid=true;
      memcpy(&IDAdapter.data[next].info,source,sizeof(_sockaddr_in));
      IDAdapter.data[next].info.old_id = id;
      IDAdapter.data[next].info.type = type;
      IDAdapter.next = (IDAdapter.next + 1) % IDAadpter_SIZE;
      return next;
  }else{
    return -1;
  }
}
inline static void IDAdapter_pop(short pos){
  IDAdapter.data[pos % IDAadpter_SIZE].valid=false;
}
inline static IDtoAddr* IDAdapter_get(short pos){
  return IDAdapter.data[pos % IDAadpter_SIZE].valid?&IDAdapter.data[pos % IDAadpter_SIZE].info:NULL;
}



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



void init(int,char **);

/*************************
 *                       *
 *        全局变量        *
 *                       *
 *************************/
struct cacheset cacheset;//缓存
struct sockaddr_in query_server;
int server_sockfd,server_sockfd;//提供服务的sockfd
int query_sockfd;//向服务器询问用的文件描述符,只支持IPV4
int sockfd;
char recv_buffer[1024];

int main(int argc, char **argv) {
  // struct sockaddr_in servaddr,inaddr, cliaddr;

  //_sockaddr_in在不启用ipv6的情况下是sockaddr_in;
  //否则是struct sockaddr_in6
  _sockaddr_in servaddr,cliaddr;

  socklen_t len, clilen;
  
  // char query_buffer[MAX_DNS_SIZE];
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
  // fd_set wset;//写集合
  FD_ZERO(&rset);
  // FD_ZERO(&wset);

  // fileno
  
  init(argc,argv);

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
  if (bind(server_sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {
  //!可能无权限
    log_fatal_exit_shortcut("bind error");
  }
  log_info("Listen on localhost:53");
  int max_fd = query_sockfd + 1;
  struct timeval timeout;
  while (1) {
    FD_SET(server_sockfd,&rset);
    FD_SET(query_sockfd,&rset);
    
    int work_fd;  
    if( (work_fd = select(0,&rset,NULL,NULL,NULL))<0){
      //!!!!加处理
      log_info("跳过");
      continue;
    }
    if (FD_ISSET(query_sockfd,&rset)){
      //优先处理未完成的请求
      log_info("处理DNS服务器发来请求");
      int total_size;
      int ans_num = 0;
      IDtoAddr * iptoaddr;
      if (( (total_size = recvfrom(query_sockfd , recv_buffer, MAX_DNS_SIZE, 0,(SA *)&query_server, &len))<0)){
            log_error_shortcut("recvfrom error:");
      }else{
        read_dns_header(&header, recv_buffer);
        if( (iptoaddr = IDAdapter_get(header.id)) == NULL){
          log_error("invalid id");
        }
        if(iptoaddr->type == RRTYPE_A || iptoaddr->type == RRTYPE_AAAA){
          //可以缓存的类型
          ans_num = read_dns_answers(ans, recv_buffer);
          // log_info("ans_num: %d\n",ans_num);
          if (ans_num > 0) {
            int count =0;
            struct record_data* send_data[10];
            now = time(NULL);
            for(int i=0;i<ans_num;i++){
              if(ans[i].type==RRTYPE_A){
                memcpy_s(&tmp_ans_data[count].ip,sizeof(struct IP),&ans[i].address,sizeof(struct IP));
                tmp_ans_data[count].label = ans[i].name;
                tmp_ans_data[count].ttl = now + ans[i].ttl;
                send_data[count] = &tmp_ans_data[count];
                count++;
              }
            }
            log_info("count:%d",count);
            if(count>0){
              if(iptoaddr->type == RRTYPE_A){
                set_cache_A_multi_record(&cacheset.A.temp,tmp_ans_data[0].label,(void**)send_data,count);
              }else if(iptoaddr->type == RRTYPE_AAAA){
                set_cache_A_multi_record(&cacheset.AAAA.temp,tmp_ans_data[0].label,(void**)send_data,count);
              }
            }
          }
        }
        set_header_id(recv_buffer,iptoaddr->old_id);
        if ( (sendto(server_sockfd, recv_buffer, total_size, 0, (SA *)&(iptoaddr->addr),sizeof(iptoaddr->addr) )  ) < 0){
            log_error_shortcut("sendto error:");
        }
        IDAdapter_pop(header.id);
      }
    }


    if (FD_ISSET(server_sockfd,&rset)){
      //如果有一个新的请求
      int rec;
      len = clilen;
      if ((rec = recvfrom(server_sockfd, recv_buffer, MAX_DNS_SIZE, 0, (SA *)&cliaddr, &len))<0){
      //接收失败，直接丢弃
      // #ifndef _WIN64
      //   log_error("recvfrom error:%s", strerror(errno));
      //   errno = 0;
      // #else
      //   log_error("bind error:code[%d]", GetLastError());
      // #endif
      // log_info("%d",len);
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
        if (sendto(server_sockfd, recv_buffer, rec , 0,(SA *)&cliaddr,
                    sizeof(cliaddr)) < 0) {
          log_error_shortcut("sendto error:");
        } else {
          log_info("成功回复");
        }
      }
      read_dns_questions(&question, recv_buffer);
      log_info("%s %s",RRtype_to_str(question.qtype),question.label);
      data = NULL;
      if (question.qtype==RRTYPE_A){
        //A记录缓存
         (data = get_cache_A_record(&cacheset.A.local,question.label))||
          (data = get_cache_A_record(&cacheset.A.temp,question.label)) ;
          //短路求值
      }
      if (question.qtype==RRTYPE_AAAA){
        data = get_cache_A_record(&cacheset.AAAA.temp,question.label);
        //此处AAAA和A的函数是相同的
      }
      if(data){
        //取到缓存   
        log_info("取到缓存");
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
        if (sendto(server_sockfd, recv_buffer, _size, 0, (SA *)&cliaddr,
                  sizeof(cliaddr)) < 0) {
          log_error_shortcut("send error");
        } else {
          log_info("send data. size=%d ans数量=%d .成功回复",_size,count);
        }
      }else{
        //未取到缓存，需要查询
        log_info("未取到缓存");
        short new_id=0;
        if ( (new_id = IDAdapter_push(header.id,&cliaddr,question.qtype)) !=-1){
            len = sizeof(struct sockaddr_in);
            printf("%d",new_id);
            set_header_id(recv_buffer,new_id);
            // sprint_dns(recv_buffer);
            if (sendto(query_sockfd, (char *)recv_buffer, rec, 0, (SA *)&query_server,
                      ADDR_LEN)<0) {
              log_error_shortcut("sendto error");
            }
        }
      }
    }

    
  }
  return 0;
}


void init(int argc,char **argv) {
  char x[5][4]={"-d","-s","-f","-n","-dd"};
  int debug_lev=0,catch_num=0,dns_server=0,file=0;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"-d")){
      if(i==argc-1){
        exit(0);
      }
      else{
        debug_lev=++i;
      }
    }
    else if(!strcmp(argv[i],"-f")){
      if(i==argc-1){
        exit(0);
      }
      else{
        file=++i;
      }
    }
    else if(!strcmp(argv[i],"-s")){
      if(i==argc-1){
        exit(0);
      }
      else{
        
        dns_server=++i;
      }
    }
    else if(!strcmp(argv[i],"-n")){
      if(i==argc-1){
          exit(0);
      }
      else{
        catch_num=atoi(argv[++i]);
      }
    }
    else{
        exit(0);
    }
  }
  
  log_set_level(debug_lev!=0?atoi(argv[debug_lev]):2);
  load_local_A_record(&cacheset.A.local,file==0?"dnsrelay.txt":argv[file]);
  init_A_record_cache_default(&cacheset.A.temp);
  init_A_record_cache_default(&cacheset.AAAA.temp);

  init_query_server(dns_server==0?"10.3.9.4":argv[dns_server]);
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
  #ifdef ACCEPT_IPV6_REQUEST
  int no = 0;  
  if( (server_sockfd = socket(PF_INET6, SOCK_DGRAM, 0))<0){
    log_fatal_exit_shortcut("socket open error");
  }
  if (setsockopt(server_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no))<0){
    log_fatal_shortcut("failed set ipv6 only off:");
  }
  #else
  if( (server_sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0){
      log_fatal_exit_shortcut("socket open error");
  }
    if( (server_sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0){
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



// void relay(int id, struct question *question, char buffer[MAX_DNS_SIZE],
//            int sockfd, _sockaddr_in *target) {
//   //中继
//   int size;
//   struct dns_header *header;
//   log_debug("中继:向DNS服务器请求");
//   size = query(question->label, question->qtype, buffer);
//   if(size==-1){
//     log_error_shortcut("未从服务器获取到数据.");
//     return;
//   }
//   header = (struct dns_header *)buffer;
//   header->id = htons(id);
//   if (sendto(sockfd, buffer, size, 0, (SA *)target, sizeof(*target)) < 0) {
//     log_error_shortcut("sendto error:");
//   } else {
//     log_info("成功回复");
//   }
// }