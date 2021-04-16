

#include "common.h"
#include <errno.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#define MAX_BUFFER 20480

int MAX_DNS_SIZE;

int query(int sockfd,struct sockaddr_in*to,char questions[],struct rr answers[]){
    char buffer[MAX_DNS_SIZE];
    int total_size = 0;
    total_size = write_dns_query(buffer,questions,A);
    socklen_t len =sizeof(struct sockaddr_in);
    // sprint_dns(buffer);
    if (sendto(sockfd,(char*)buffer,total_size,0,(struct sockaddr*)to,len)<0){
        fprintf(stderr,"sendto,%s\n", strerror(errno));
        exit(0);
    }
    
    if ((total_size = recvfrom(sockfd,buffer,MAX_DNS_SIZE,0,(struct sockaddr*)to,&len))==-1){
        fprintf(stderr,"recvfrom,%s\n", strerror(errno));
        exit(0);
    }
    sprint_dns(buffer);
    return read_dns_answers(answers,buffer);
}


int main(int argc, char **argv)
{
    int sockfd; 
    struct sockaddr_in servaddr;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DNS_SERVER_PORT);
    inet_pton(AF_INET,"202.96.0.133",&servaddr.sin_addr);
    sockfd = socket(AF_INET,SOCK_DGRAM,0);

    char *ques[2]={"www.baidu.com","www.bilibili.com"};
    struct rr ans[4];
    size_t ans_size;
    query(sockfd,&servaddr,"www.baidu.com",ans);
    return 0;
}




