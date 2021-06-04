#include "cache.h"
#include "common.h"
#include "log.h"
#include "local_record.h"
#include "threadPool.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
// #define _XOPEN_SOURCE 500
#include <pthread.h>

#ifdef __linux
#include <sys/select.h>
#endif

/*************************
 *                       *
 *        常量和宏        *
 *                       *
 *************************/
#define _CRT_SECURE_NO_WARNINGS

#define DNS_TTL 1000  //DNS超时时限
#define SELECT_TTL 800000  //select函数超时时限，单位为微秒

//允许接收来自IPV6的请求（向下兼容）SW
//取消宏定义可以仅处理Ipv4请求
#define ACCEPT_IPV6_REQUES

#ifdef ACCEPT_IPV6_REQUEST
typedef struct sockaddr_in6 _sockaddr_in;
#else
typedef struct sockaddr_in _sockaddr_in;
#endif

typedef struct sockaddr SA;

#ifdef __linux
typedef unsigned short WORD ;
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
    clock_t start; //存开始时间
    char message[1024]; //存报文
  }data[IDAadpter_SIZE];
  bool is_init;
  unsigned short next;
  pthread_rwlock_t rwlock;   //读写锁
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
inline static short IDAdapter_push(unsigned short id,_sockaddr_in* source,unsigned char type,clock_t time,char *buffer);

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
void* handle_request(void*_____);
void* handle_relay(void *______);

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
typedef struct{
  struct dns_header header;
  struct question question;//暂存DNS question
  struct answer ans[20];//暂存DNS answer
  struct record_data tmp_ans_data[20];
  char recv_buffer[1024];
  _sockaddr_in cliaddr;
  // struct record_data *data;//获取缓存的指针
  // struct static_record_data *static_data;//获取本地缓存的指针
}threadBuffer;//线程上下文
struct threadContext{
  // hashtable table;
  int* table;
  threadBuffer* buffer;
  bool is_init;
  int size;
  pthread_mutex_t rlock;//读socket锁
  pthread_mutex_t wlock;//写socket锁
  pthread_mutex_t srlock;//临时使用
  pthread_mutex_t swlock;//临时使用
}threadContext;
struct tid_hash_node{
  unsigned short id;
  pthread_t tid;
};
static unsigned tid_hash(void* tid){
  return *(int*)tid;
}
static bool compare_tid_node_with_hash(void* record,void* key){
  return ( ((struct tid_hash_node*)record)->tid == *(pthread_t*)key);
}
// void init_threadContext(struct threadContext*context,int n){
void init_threadContext(tpool_t *pool,int n){
  if(!threadContext.is_init){
    // init_hashtable(&threadContext.table,n,0.5,tid_hash,compare_tid_node_with_hash);
    threadContext.buffer = (threadBuffer*)calloc(n+1,sizeof(threadBuffer));
    threadContext.is_init = true;
    // struct tid_hash_node tmp;
    // for(int i=0;i<n;i++){
    //   tmp.id = i;
    //   tmp.tid = pool->thread_id[i];
    //   set_hashnode(&threadContext.table,(void*)&tmp.tid,&tmp,NULL);
    // }
    threadContext.size = n;
    threadContext.table = (int*)malloc(sizeof(int)*(n+1));
    for(int i=0;i<n;i++){
      threadContext.table[i] = pool->thread_id[i];
      log_info("tid = %ld", pool->thread_id[i]);
    }
    pthread_mutex_init(&threadContext.rlock,NULL);//初始化锁
    pthread_mutex_init(&threadContext.wlock,NULL);//初始化锁

    pthread_mutex_init(&threadContext.srlock,NULL);//初始化锁    
    pthread_mutex_init(&threadContext.swlock,NULL);//初始化锁
  }
}
// threadBuffer* get_threadBuffer(struct threadContext* context,pthread_t tid){
threadBuffer* get_threadBuffer(pthread_t tid){
  //不会写入，不需要加锁
  // struct hashnode* node = get_hashnode(&threadContext.table,&tid,NULL);
  int i;
  for(i =0;i<threadContext.size;i++){
    if(threadContext.table[i]==tid){
      break;
    }
  }
  if(i==threadContext.size)abort();
  log_info("tid=%d,id=%d",tid,i);
  return &threadContext.buffer[i];
  // if(node==NULL)abort();
  // return &threadContext.buffer[((struct tid_hash_node*)node->record)->id];
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
int clilen;

tpool_t* pool = NULL;

int main(int argc, char **argv) {

  //_sockaddr_in在不启用ipv6的情况下是sockaddr_in;
  //否则是struct sockaddr_in6
  

  socklen_t len, clilen;
  
  //暂存DNS header
  
  
  
  
  // unsigned ip;
  
  int _size; //临时变量
  time_t now;

  fd_set rset;//读集合
  FD_ZERO(&rset);

  struct timeval timeout; //select函数中timeout参数
  timeout.tv_usec= SELECT_TTL; //设置timeout的值
  timeout.tv_sec=0;
  //初始化,处理命令行参数，初始化cache，初始化WSA和socket套接字,设置超时，绑定端口
  init(argc,argv);

  
     

  int max_fd = query_sockfd + 1;
  // struct timeval timeout;
  while (1) {
    FD_SET(server_sockfd,&rset);
    FD_SET(query_sockfd,&rset);
    
    int work_fd;  
    if( (work_fd = select(max_fd,&rset,NULL,NULL,NULL))<0){
      //!!!!加处理
      log_info("跳过");
      continue;
    }

    // //查找超时的项并处理
    // if(work_fd==0){
    //   for(int i=0;i<IDAadpter_SIZE;i++){
    //     if(IDAdapter.data[i].valid&&(clock()-IDAdapter.data[i].start>=DNS_TTL)){
    //       IDtoAddr * idtoaddr = &IDAdapter.data[i].info;
    //       log_info("超时回复");
    //       set_header_flag(IDAdapter.data[i].message,FLAG_RESPONSE_NORMAL);
    //       set_header_rcode_refused(IDAdapter.data[i].message);
    //       if (sendto(server_sockfd, IDAdapter.data[i].message, strlen(IDAdapter.data[i].message), 0,(SA *)&idtoaddr->addr,
    //                 sizeof(idtoaddr->addr)) < 0) {
    //           log_error_shortcut("sendto error:");
    //       } else {
    //         log_info("发送成功");
    //       }
    //       IDAdapter.data[i].valid=false;
    //     }
    //   }
    // }
    
    handle_relay://处理中继
    if (FD_ISSET(query_sockfd,&rset)){
      //优先处理未完成的请求
      // printf("aaaa\n");
      add_task_2_tpool(pool,handle_relay,NULL);
    }

    handle_request:
    if (FD_ISSET(server_sockfd,&rset)){
      add_task_2_tpool(pool,handle_request,NULL);
    // err_handle:
    }
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
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(DNS_SERVER_PORT);
    #endif
    if (bind(server_sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0) {
    //!可能无权限
      log_fatal_exit_shortcut("bind error");
    }
    int len = sizeof(servaddr);
  if (getsockname(server_sockfd, (SA*)&servaddr, &len) == -1)
      perror("getsockname");
  else{
      log_ip("listen on ",&servaddr);
  }
}

inline static short IDAdapter_push(unsigned short id,_sockaddr_in* source,unsigned char type,clock_t time,char *buffer){
  pthread_rwlock_wrlock(&IDAdapter.rwlock);
  short next = IDAdapter.next;
  if(!IDAdapter.data[next].valid){
      IDAdapter.data[next].valid=true;
      memcpy(&IDAdapter.data[next].info,source,sizeof(_sockaddr_in));
      IDAdapter.data[next].info.old_id = id;
      IDAdapter.data[next].info.type = type;
      IDAdapter.data[next].start = time;
      strcpy(IDAdapter.data[next].message,buffer);
      // strcpy_s(IDAdapter.data[next].message,1024,buffer);
      IDAdapter.next = (IDAdapter.next + 1) % IDAadpter_SIZE;
      pthread_rwlock_unlock(&IDAdapter.rwlock);
      return next;
  }else{
    pthread_rwlock_unlock(&IDAdapter.rwlock);
    return -1;
  }

}
inline static void IDAdapter_pop(short pos){
  pthread_rwlock_rdlock(&IDAdapter.rwlock);
  IDAdapter.data[pos % IDAadpter_SIZE].valid=false;
  pthread_rwlock_unlock(&IDAdapter.rwlock);
}
inline static IDtoAddr* IDAdapter_get(short pos){
  //可能需要加锁
  return IDAdapter.data[pos % IDAadpter_SIZE].valid?&IDAdapter.data[pos % IDAadpter_SIZE].info:NULL;
}

void dnsExit(){
  printf("Usage:\n");
  printf("    .\\dns [-opt ...]             # interactive mode using default server,file and cache size \n "\
  "    .\\dns [-opt ...] -d number   # interactive mode using debug number lever \n"\
  "    .\\dns [-opt ...] -f file     # interactive mode using this file as dnsrelay \n"\
  "    .\\dns [-opt ...] -s server   # interactive mode using 'server' \n"\
  "    .\\dns [-opt ...] -c number   # interactive mode using the number size of cache");
  exit(0);
}
void init(int argc,char **argv) {
  int debug_lev=0,cache_num=0,dns_server=0,file=0;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"-d")){
      if(i==argc-1){
        dnsExit();
      }
      else{
        debug_lev=++i;
      }
    }
    else if(!strcmp(argv[i],"-f")){
      if(i==argc-1){
        dnsExit();
      }
      else{
        file=++i;
      }
    }
    else if(!strcmp(argv[i],"-s")){
      if(i==argc-1){
        dnsExit();
      }
      else{
        dns_server=++i;
      }
    }
    else if(!strcmp(argv[i],"-c")){
      if(i==argc-1){
        dnsExit();
      }
      else{
        cache_num=atoi(argv[++i]);
      }
    }
    else{
        dnsExit();
    }
  }
  log_set_level(LOG_DEBUG);
  // log_set_level(debug_lev!=0?atoi(argv[debug_lev]):2);
  load_local_record(&cacheset,file==0?"dnsrelay.txt":argv[file]);
  init_A_record_cache(&cacheset.A.temp,cache_num?cache_num:LRU_BUFFER_LENGTH,CACHE_INFINTE_SIZE,CACHE_FILLING_FACTOR);
  init_A_record_cache(&cacheset.AAAA.temp,cache_num?cache_num:LRU_BUFFER_LENGTH,CACHE_INFINTE_SIZE,CACHE_FILLING_FACTOR);
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
  log_info("server accept IPV4 connection only");
  #endif

  if( (query_sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0){
      log_fatal_exit_shortcut("socket open error");
  }
  #ifdef __linux
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
  clilen= sizeof(_sockaddr_in);
  log_info("init all done");
  bind_addr();
  pthread_rwlock_init(&IDAdapter.rwlock, NULL);
  if(0 != create_tpool(&pool,5)){
      log_fatal_exit_shortcut("create_tpool failed!\n");
  }
  init_threadContext(pool,5);
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
  #elif __linux
  #endif
  free_staticCache(&cacheset.A.local);
  free_staticCache(&cacheset.AAAA.local);
  free_staticCache(&cacheset.blacklist);  
  free_cache(&cacheset.A.temp);
  free_cache(&cacheset.AAAA.temp);  
}

void* handle_relay(void *______){
    // log_info("处理DNS服务器发来请求");
    int ret = pthread_mutex_trylock(&threadContext.srlock);
    if (ret==0 || ret==EBUSY){
      return NULL;
    }
    threadBuffer * buffer = get_threadBuffer(pthread_self());
    int total_size;
    int ans_num = 0;
    IDtoAddr * iptoaddr;
    time_t now;
    int len = sizeof(query_server);
    if (( (total_size = recvfrom(query_sockfd , buffer->recv_buffer, MAX_DNS_SIZE, 0,(SA *)&query_server, &len))<0)){
        log_error_shortcut("recvfrom error:");
        return NULL;//跳过本次处理
    }
    pthread_mutex_unlock(&threadContext.srlock);
    read_dns_header(&buffer->header,  buffer->recv_buffer);
    if( (iptoaddr = IDAdapter_get(buffer->header.id)) == NULL){
      log_error("invalid id");
      return NULL;
    }
    log_info("报文id=%d,旧id=%d type=%d", buffer->header.id,iptoaddr->old_id,iptoaddr->type);
    if(iptoaddr->type == RRTYPE_A||iptoaddr->type==RRTYPE_AAAA){
      //可以缓存的类型
      ans_num = read_dns_answers( buffer->ans,  buffer->recv_buffer);
      // log_debug("ans_num: %d\n",ans_num);
      if (ans_num > 0) {
        int count =0;
        struct record_data* send_data[20];
        now = time(NULL);
        for(int i=0;i<ans_num;i++){
            // memcpy_s(&tmp_ans_data[count].ip,sizeof(struct IP),&ans[i].address,sizeof(struct IP)); 
            // puts(ans[0].name);
            memcpy(& buffer->tmp_ans_data[count].ip,& buffer->ans[i].address,sizeof(struct IP));
             buffer->tmp_ans_data[count].label =  buffer->ans[i].name;
             buffer->tmp_ans_data[count].ttl = now +  buffer->ans[i].ttl;
            send_data[count] = & buffer->tmp_ans_data[count];
            count++;
        }
        log_debug("count:%d",count);
        if(count>0){
          if(iptoaddr->type == RRTYPE_A){
            set_cache_A_multi_record(&cacheset.A.temp, buffer->tmp_ans_data[0].label,(void**)send_data,count);
          }else if(iptoaddr->type == RRTYPE_AAAA){
            set_cache_A_multi_record(&cacheset.AAAA.temp, buffer->tmp_ans_data[0].label,(void**)send_data,count);
          }
        }
      }
    }
    set_header_id( buffer->recv_buffer,iptoaddr->old_id);
    pthread_mutex_lock(&threadContext.wlock);
    if ( (sendto(server_sockfd,  buffer->recv_buffer, total_size, 0, (SA *)&(iptoaddr->addr),sizeof(iptoaddr->addr) )  ) < 0){
        log_error_shortcut("sendto error:");
    }
    pthread_mutex_unlock(&threadContext.wlock);
    log_ip("处理结束: ",&buffer->cliaddr);
    IDAdapter_pop( buffer->header.id);
    return NULL;
}


void* handle_request(void*_____){
        //如果有一个新的请求
      int ret =pthread_mutex_trylock(&threadContext.rlock);
      if (ret==EBUSY || ret==0){
        return NULL;
      }
      threadBuffer * tbuffer = get_threadBuffer(pthread_self());
      _sockaddr_in cliaddr;
      int rec;
      int len = clilen;
      struct record_data*data;
      struct  static_record_data*static_data;
      
      if ((rec = recvfrom(server_sockfd, tbuffer->recv_buffer, MAX_DNS_SIZE, 0, (SA *)&cliaddr, &len))<0){
        return NULL;
      }
      pthread_mutex_unlock(&threadContext.rlock);
      log_ip("recvfrom:",&cliaddr);
      
      read_dns_header(&tbuffer->header, tbuffer->recv_buffer);
      // sprint_dns(recv_buffer);
      if (tbuffer->header.flags != htons(FLAG_QUERY)) {
        //不是询问，也丢弃
        set_header_flag(tbuffer->recv_buffer,FLAG_RESPONSE_NORMAL);
        set_header_rcode_refused(tbuffer->recv_buffer);
        pthread_mutex_lock(&threadContext.wlock);
        if (sendto(server_sockfd, tbuffer->recv_buffer, rec , 0,(SA *)&cliaddr,
                    sizeof(cliaddr)) < 0) {
          log_error_shortcut("sendto error:");
        } else {
          log_info("成功回复");
        }
        pthread_mutex_unlock(&threadContext.wlock);
        return NULL;
      }
      read_dns_questions(&tbuffer->question, tbuffer->recv_buffer);
      log_info("%s %s",RRtype_to_str(tbuffer->question.qtype),tbuffer->question.label);
      data = NULL;
      static_data = NULL;
      if ( (static_data=get_static_cache(&cacheset.blacklist,tbuffer->question.label)) ){//不能少括号
        log_info("拦截请求");
        set_header_flag(tbuffer->recv_buffer,FLAG_RESPONSE_NORMAL);
        set_header_rcode_name_error(tbuffer->recv_buffer);
        pthread_mutex_lock(&threadContext.wlock);
        if (sendto(server_sockfd, tbuffer->recv_buffer, rec , 0,(SA *)&cliaddr,
                  sizeof(cliaddr)) < 0) {
            log_error_shortcut("sendto error:");
        } else {
          log_info("发送成功");
        }
        pthread_mutex_unlock(&threadContext.wlock);
        return NULL;
      }
      if(tbuffer->question.qtype==RRTYPE_A){
        // puts(question.label);
        static_data = get_static_cache(&cacheset.A.local,tbuffer->question.label);
        // if(!static_data)puts("aaaaa");
        if(!static_data)data = get_cache_A_record(&cacheset.A.temp,tbuffer->question.label);
      }else if (tbuffer->question.qtype==RRTYPE_AAAA){
        static_data = get_static_cache(&cacheset.AAAA.local,tbuffer->question.label);
        if(!static_data)data = get_cache_A_record(&cacheset.AAAA.temp,tbuffer->question.label);
      }
      int count=0;
      if(static_data){
        //取到本地记录
        while(static_data){
          tbuffer->ans[count].ttl = 3600;//1小时
          memcpy(&tbuffer->ans[count].address, &static_data->ip, sizeof(struct IP));
          tbuffer->ans[count].type = tbuffer->question.qtype;
          tbuffer->ans[count].class_ = tbuffer->question.qclass;
          tbuffer->ans[count].has_cname = false;
          strcpy(tbuffer->ans[count].name,tbuffer->question.label);
          // strcpy_s(ans[count].name, sizeof(ans[0].name),question.label);
          count += 1;
          static_data = static_data->next;
        }
      }else if(data){
        //取到
        time_t now = time(NULL);
        while(data){
          tbuffer->ans[count].ttl = data->ttl - now;
          memcpy(&tbuffer->ans[count].address, &data->ip, sizeof(struct IP));
          strcpy(tbuffer->ans[count].name,tbuffer->question.label);
          // strcpy_s(ans[count].name, sizeof(ans[0].name),question.label);
          tbuffer->ans[count].type = tbuffer->question.qtype;
          tbuffer->ans[count].class_ = tbuffer->question.qclass;
          tbuffer->ans[count].has_cname = false;
          // strcpy_s(ans[count].name, sizeof(ans[0].name),question.label);
          strcpy(tbuffer->ans[count].name, tbuffer->question.label);
          count++;
          data=data->next;
        }
      }
      if(count){
        //取到缓存   
        log_info("取到缓存");
        // printf("aaa\n%04x:%04x:%04x:%04x",
        // ans[0].address.addr.v6byte[0], ans[0].address.addr.v6byte[1], ans[0].address.addr.v6byte[2],ans[0].address.addr.v6byte[3]);
        int _size = write_dns_response_by_query(tbuffer->recv_buffer, tbuffer->ans, count);
        // sprint_dns(query_buffer);
        pthread_mutex_lock(&threadContext.wlock);
        if (sendto(server_sockfd, tbuffer->recv_buffer, _size, 0, (SA *)&cliaddr,
                  sizeof(cliaddr)) < 0) {
          log_error_shortcut("send error");
        } else {
          log_info("send data. size=%d ans数量=%d .成功回复",_size,count);
        }
        pthread_mutex_unlock(&threadContext.wlock);
      }else{
        //未取到缓存，需要查询
        log_info("未取到缓存");
        short new_id=0;
        *(tbuffer->recv_buffer+rec)='\0';
        if ( (new_id = IDAdapter_push(tbuffer->header.id,&cliaddr,tbuffer->question.qtype,clock(),tbuffer->recv_buffer)) !=-1){
            log_debug("old_id=%d,new_id=%d",tbuffer->header.id ,new_id);
            len = sizeof(struct sockaddr_in);
            set_header_id(tbuffer->recv_buffer,new_id);
            pthread_mutex_lock(&threadContext.swlock);
            if (sendto(query_sockfd, (char *)tbuffer->recv_buffer, rec, 0, (SA *)&query_server,
                      ADDR_LEN)<0) {
              log_error_shortcut("sendto error");
            }
            pthread_mutex_unlock(&threadContext.swlock);
        }
      }
}