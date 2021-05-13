#include "common.h"
#include"log.h"
#include"relay.h"
#include"cache.h"
#include <errno.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#include <sys/time.h>

typedef struct sockaddr SA;


void init(){
    init_query_server();
    init_cache();
}


int main(int argc, char **argv)
{
    int sockfd; 
    struct sockaddr_in servaddr,cliaddr;
    socklen_t len,clilen;
    char recv_buffer[MAX_DNS_SIZE];
    char query_buffer[MAX_DNS_SIZE];
    struct dns_header header;
    struct question question;
    struct rr ans[4];
    struct record_data* data;
    unsigned ip;
    int enable_cache;
    int _size;//临时变量
    time_t now;

    init();

    clilen = sizeof(cliaddr);
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(DNS_SERVER_PORT);
    

    if (bind(sockfd,(SA*)&servaddr,sizeof(servaddr))<0){
        //!可能无权限
        log_fatal("bind error:%s",strerror(errno));
        exit(0);
    }

    log_info("Listen on localhost:53");

    

    while(1){
        
        len = clilen;

        if (recvfrom(sockfd,recv_buffer,MAX_DNS_SIZE,0,(SA*)&cliaddr,&len)<DNS_HEADER_SIZE){
            //接收失败，直接丢弃
            log_error("recvfrom error:%s",strerror(errno));
            errno = 0;
            continue;
        }
        
        ip = ntohl(cliaddr.sin_addr.s_addr);
        log_info("Recvfrom %d:%d:%d:%d:port:%d ",*(char*)&ip,*(((char*)&ip)+1),*(((char*)&ip)+2),*(((char*)&ip)+3),ntohs(cliaddr.sin_port));
        
        read_dns_header(&header,recv_buffer);
        sprint_dns(recv_buffer);
        if(header.flags!=htons(FLAG_QUERY)){
            //不是询问，也丢弃
            continue;
        }
            
        read_dns_questions(&question,recv_buffer);
        
        switch (question.qtype){
        case A:
        // case AAAA:
            enable_cache = 1;
            //只处理这几个
            break;
        default:
            enable_cache = 0;
            break;
        }
        if(!enable_cache){
            relay(header.id,&question,recv_buffer,sockfd,&cliaddr);
            continue;
        }
        if ((data=get_cache(question.label))){
            log_debug("取到缓存");
            now = time(NULL);
            ans[0].ttl = data->ttl-now;
            memcpy(&ans[0].address,&data->ip,sizeof(struct IP));
            strcpy(ans[0].name,question.label);
            ans[0].type = question.qtype;
            ans[0].class_ = question.qclass;
            _size = write_dns_response_by_query(query_buffer,ans,1);
            if (sendto(sockfd,query_buffer,_size,0,(SA*)&cliaddr,sizeof(cliaddr))<0){
                    log_error("sendto error:%s",strerror(errno));
                    errno = 0;
            }
        }else{
            log_debug("未取到缓存");
            _size = query(question.label,question.qtype,query_buffer);
            int _ans_num = read_dns_answers(ans,query_buffer);
            struct dns_header* pheader;
            if(_ans_num>0){
                now = time(NULL);
                set_cache(ans[0].name,&ans[0].address,ans[0].ttl+now);
            }
            pheader = (struct dns_header*)query_buffer;
            pheader->id = header.id;
            if (sendto(sockfd,query_buffer,_size,0,(SA*)&cliaddr,sizeof(cliaddr))<0){
                log_error("sendto error:%s",strerror(errno));
                errno = 0;
            }
        }
    }
    return 0;
}




