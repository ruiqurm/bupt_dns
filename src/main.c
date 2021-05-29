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
//取消宏定义可以仅处理Ipv4请求
#define ACCEPT_IPV6_REQUES

#ifdef ACCEPT_IPV6_REQUEST
typedef struct sockaddr_in6 _sockaddr_in;
#else
typedef struct sockaddr_in _sockaddr_in;
#endif

typedef struct sockaddr SA;

#ifdef __linux
typedef WORD unsigned short
#endif

//IP转换器的大小
#define IDAadpter_SIZE 1024
#if IDAadpter_SIZE>32767
#error IDAadpter_SIZE should smaller than 32767
#endif

/*************************
 *                       *
 *       ID转换器         *
 *                       *
 *************************/


/**
 * @brief 返回给用户的ID转换数据
 */
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



/**
 * @brief ID转换器压入一个ID
 * 
 * @details 如果使用的空间满了，会返回-1
 * @param id 旧的报文编号
 * @param source 源端地址
 * @param type 报文类型
 * 
 * @return short ID转换器编号
 */
inline static short IDAdapter_push(unsigned short id,_sockaddr_in* source,unsigned char type);

/**
 * @brief ID转换器弹出一个ID
 * 
 * @param pos ID转换器分配的ID
 */
inline static void IDAdapter_pop(short pos);

/**
 * @brief ID转换器获取ID对应的数据
 * 
 * @param pos ID转换器分配的ID
 * @return IDtoAddr* 
 */
inline static IDtoAddr* IDAdapter_get(short pos);


/*************************
 *                       *
 *        函数            *
 *                       *
 *************************/

///初始化,处理命令行参数，初始化cache，初始化WSA和socket套接字,设置超时，绑定端口
void init(int,char **);
void on_SIGINT(int sig);
void clean_up();

///打印ip
static inline void log_ip(const char* str,_sockaddr_in* addr){
    unsigned char* ipv4;
    #ifdef ACCEPT_IPV6_REQUEST
    WORD* ipv6;
    // if (*((unsigned long long*)&addr->sin6_addr) == 0){//如果前64位是0，说明是ipv4地址
    //   ipv4 = (unsigned char*)&addr->sin6_addr.u.Word[4];
    //   log_info("%s %d:%d:%d:%d:port:%d ",str,ipv4[0],ipv4[1],ipv4[2],ipv4[3],
    //       ntohs(addr->sin6_port));
    // }else{
      ipv6 = (uint16_t*)&addr->sin6_addr;//跨平台
      log_info("%s %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x port:%d ",str,
        ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7],ntohs(addr->sin6_port));
    // }
    #else
    ipv4 = (unsigned char*)&addr->sin_addr; //!不需要翻过来
    log_info("%s %d:%d:%d:%d:port:%d ",str,
              ipv4[0],ipv4[1],ipv4[2],ipv4[3],
              ntohs(addr->sin_port));
    #endif
}

/*************************
 *                       *
 *        全局变量        *
 *                       *
 *************************/
struct cacheset cacheset;//缓存集
struct sockaddr_in query_server;
int server_sockfd,server_sockfd;//提供服务的sockfd
int query_sockfd;//向服务器询问用的文件描述符,只支持IPV4
char recv_buffer[1024];

int main(int argc, char **argv) {

  //_sockaddr_in在不启用ipv6的情况下是sockaddr_in;
  //否则是struct sockaddr_in6
  _sockaddr_in cliaddr;

  socklen_t len, clilen;
  
  struct dns_header header;//暂存DNS header
  struct question question;//暂存DNS question
  struct answer ans[10];//暂存DNS answer
  struct record_data tmp_ans_data[10];
  struct record_data *data;//获取缓存的指针
  struct static_record_data *static_data;//获取本地缓存的指针
  // unsigned ip;
  
  int _size; //临时变量
  time_t now;

  fd_set rset;//读集合
  FD_ZERO(&rset);

  //初始化,处理命令行参数，初始化cache，初始化WSA和socket套接字,设置超时，绑定端口
  init(argc,argv);

  clilen = sizeof(cliaddr);
     

  int max_fd = query_sockfd + 1;
  // struct timeval timeout;
  while (1) {
    FD_SET(server_sockfd,&rset);
    FD_SET(query_sockfd,&rset);
    
    int work_fd;  
    if( (work_fd = select(0,&rset,NULL,NULL,NULL))<0){
      //!!!!加处理
      log_info("跳过");
      continue;
    }
    handle_relay://处理中继
    if (FD_ISSET(query_sockfd,&rset)){
      //优先处理未完成的请求
      log_info("处理DNS服务器发来请求");
      int total_size;
      int ans_num = 0;
      IDtoAddr * iptoaddr;
      if (( (total_size = recvfrom(query_sockfd , recv_buffer, MAX_DNS_SIZE, 0,(SA *)&query_server, &len))<0)){
          log_error_shortcut("recvfrom error:");
          goto handle_request;//跳过本次处理
      }
      read_dns_header(&header, recv_buffer);
      if( (iptoaddr = IDAdapter_get(header.id)) == NULL){
        log_error("invalid id");
        goto handle_request;//跳过本次处理
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
          log_debug("count:%d",count);
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
      log_ip("finish: ",&cliaddr);
      IDAdapter_pop(header.id);
      
    }

    handle_request:
    if (FD_ISSET(server_sockfd,&rset)){
      //如果有一个新的请求
      int rec;
      len = clilen;
      if ((rec = recvfrom(server_sockfd, recv_buffer, MAX_DNS_SIZE, 0, (SA *)&cliaddr, &len))<0){
      continue;
    }
      log_ip("recvfrom:",&cliaddr);

      read_dns_header(&header, recv_buffer);
      // sprint_dns(recv_buffer);
      if (header.flags != htons(FLAG_QUERY)) {
        //不是询问，也丢弃
        set_header_flag(recv_buffer,FLAG_RESPONSE_NORMAL);
        set_header_rcode_refused(recv_buffer);
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
      static_data = NULL;
      if ( (static_data=get_static_cache(&cacheset.blacklist,question.label)) ){//不能少括号
        log_info("拦截请求");
        set_header_flag(recv_buffer,FLAG_RESPONSE_NORMAL);
        set_header_rcode_name_error(recv_buffer);
        if (sendto(server_sockfd, recv_buffer, rec , 0,(SA *)&cliaddr,
                  sizeof(cliaddr)) < 0) {
            log_error_shortcut("sendto error:");
        } else {
          log_info("发送成功");
        }
        continue;
      }
      if(question.qtype==RRTYPE_A){
        static_data = get_static_cache(&cacheset.A.local,question.label);
        if(!static_data)data = get_cache_A_record(&cacheset.A.temp,question.label);
      }else if (question.qtype==RRTYPE_AAAA){
        static_data = get_static_cache(&cacheset.AAAA.local,question.label);
        if(!static_data)data = get_cache_A_record(&cacheset.AAAA.temp,question.label);
      }
      int count=0;
      if(static_data){
        //取到本地记录
        ans[count].ttl = 3600;//1小时
        memcpy(&ans[count].address, &data->ip, sizeof(struct IP));
        ans[count].type = question.qtype;
        ans[count].class_ = question.qclass;
        ans[count].has_cname = false;
        strcpy_s(ans[count].name, sizeof(ans[0].name),question.label);
        count = 1;
      }else if(data){
        //取到
        now = time(NULL);
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
      }
      if(count){
        //取到缓存   
        log_info("取到缓存");
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
            set_header_id(recv_buffer,new_id);
            if (sendto(query_sockfd, (char *)recv_buffer, rec, 0, (SA *)&query_server,
                      ADDR_LEN)<0) {
              log_error_shortcut("sendto error");
            }
        }
      }
    }
    // err_handle:

  }
  return 0;
}








void init_query_server(const char *query_server_addr) {
  memset(&query_server, 0, ADDR_LEN);

  query_server.sin_family = AF_INET;
  query_server.sin_port = htons(DNS_SERVER_PORT);
  inet_pton(AF_INET,query_server_addr,&query_server.sin_addr.s_addr);
  
  // inet_pton(AF_INET,query_server_addr,&query_server.sin_addr.s_addr);
}

inline static void bind_addr(){
    _sockaddr_in servaddr;
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
    if (bind(server_sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {
    //!可能无权限
      log_fatal_exit_shortcut("bind error");
    }
    log_info("Listen on localhost:53");
}

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
  load_local_record(&cacheset,file==0?"dnsrelay.txt":argv[file]);
  init_A_record_cache_default(&cacheset.A.temp);
  init_A_record_cache_default(&cacheset.AAAA.temp);
  log_info("init cache done");

  init_query_server(dns_server==0?"10.3.9.4":argv[dns_server]);
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
  log_info("server accept IPV6 and IPV4 connection");
  #else
  if( (server_sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0){
      log_fatal_exit_shortcut("socket open error");
  }
    if( (server_sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0){
      log_fatal_exit_shortcut("socket open error");
  }
  log_info("server accept IPV4 connection only");
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
  log_info("setting timeout = %d ms",tv.tv_sec * 1000 + tv.tv_usec);
  #elif defined(_WIN64)
  DWORD timeout = 1 * 1000;//设置超时时间
  if(setsockopt(query_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout)<0){
      log_error_shortcut("set timeout error");
  }
  log_info("setting timeout = %d ms",timeout);
  #endif
  if(signal(SIGINT, on_SIGINT)<0){
    log_fatal_exit_shortcut("can't set SIGINT");
  }
  log_info("init all done");
  bind_addr();
}
void on_SIGINT(int sig){
  log_info("catch SIGINT");
  clean_up();
  log_info("bye.");
  log_set_quiet(true);
   fflush(stdout);//有点问题
  exit(0);
}
void clean_up(){
  #ifdef _WIN64
    shutdown(query_sockfd,2);
    shutdown(server_sockfd,2);
    closesocket(query_sockfd);
    closesocket(server_sockfd);
    WSACleanup();
  #elif _linux
  #endif
  free_staticCache(&cacheset.A.local);
  free_staticCache(&cacheset.AAAA.local);
  free_staticCache(&cacheset.blacklist);  
  free_cache(&cacheset.A.temp);
  free_cache(&cacheset.AAAA.temp);
  
}