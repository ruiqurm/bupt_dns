#include "common.h"
#include"log.h"
#include <errno.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#include <sys/time.h>

typedef struct sockaddr SA;

struct sockaddr_in exservaddr;
const socklen_t ADDR_LEN  =sizeof(exservaddr);

int init_server(){
    bzero(&exservaddr,ADDR_LEN);
    exservaddr.sin_family = AF_INET;
    exservaddr.sin_port = htons(DNS_SERVER_PORT);
    inet_pton(AF_INET,"10.3.9.4",&exservaddr.sin_addr);
}

int query(char questions[],int type,struct rr answers[]){
    //向DNS服务器询问
    int sockfd; 
    socklen_t len;
    char buffer[MAX_DNS_SIZE];
    int total_size = 0;
    
    sockfd = socket(AF_INET,SOCK_DGRAM,0);

    // struct timeval tv;
    // tv.tv_sec = 5;
    // tv.tv_usec = 0;
    // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    //     log_error("set timeout error",strerror(errno));
    // }

    total_size = write_dns_query(buffer,questions,type);
    
    if (sendto(sockfd,(char*)buffer,total_size,0,(SA*)&exservaddr,ADDR_LEN)<0){
        log_error("sendto error: %s",strerror(errno));
        return -1;
    }
    
    if ((total_size = recvfrom(sockfd,buffer,MAX_DNS_SIZE,0,(SA*)&exservaddr,&len))<0){
        log_error("recvfrom error: %s",strerror(errno));;
        return -1;
    }
    sprint_dns(buffer);
    return read_dns_answers(answers,buffer);
}

int main(int argc, char **argv)
{
    int sockfd; 
    struct sockaddr_in servaddr,cliaddr;
    socklen_t len,clilen;

    init_server();

    clilen = sizeof(cliaddr);
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(DNS_SERVER_PORT);


    if (bind(sockfd,(SA*)&servaddr,sizeof(servaddr))<0){
        log_fatal("bind error:%s",strerror(errno));
        //!可能无权限
        exit(0);
    }

    log_info("Listen on localhost:53");
    // struct rr ans[4];
    // query(sockfd,&servaddr,"www.baidu.com",ans);
    
    char buffer[MAX_DNS_SIZE];

    while(1){
        len = clilen;
        if (recvfrom(sockfd,buffer,MAX_DNS_SIZE,0,(SA*)&cliaddr,&len)<0){
            log_error("recvfrom error:%s",strerror(errno));
        }else{
            unsigned ip = ntohl(cliaddr.sin_addr.s_addr);
            log_info("Recvfrom %d:%d:%d:%d:port:%d ",*(char*)&ip,*(((char*)&ip)+1),*(((char*)&ip)+2),*(((char*)&ip)+3),ntohs(cliaddr.sin_port));
            struct dns_header header;
            read_dns_header(&header,buffer);
            if(header.flags==htons(FLAG_QUERY)){
                sprint_dns(buffer);
                struct question question;
                read_dns_questions(&question,buffer);
                struct rr ans[4];
                log_debug("尝试向DNS服务器请求");
                int ans_num = query(question.label,question.qtype,ans);
                if (ans_num>=0){
                    log_debug("获取到%d条回复",ans_num);
                    int size = write_dns_response_by_query(buffer,ans,ans_num);
                    if (sendto(sockfd,buffer,size,0,(SA*)&cliaddr,len)<0){
                        log_error("sendto error:%s",strerror(errno));
                    }
                    log_debug("发送回复到客户端");
                }else{
                    log_warn("获取DNS地址失败");
                }
            }
        }
    }
    return 0;
}




