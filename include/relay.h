/*
* 中继服务处理
*
*/
#pragma once
#include "common.h"
/*
* 与其他DNS服务器通信
*/
typedef struct sockaddr SA;
struct sockaddr_in query_server;
#define query_server_addr "10.3.9.4"

//初始化
void init_query_server();
int query(char questions[],int type,char buffer[MAX_DNS_SIZE]);
void relay(int id,struct question* question,char buffer[MAX_DNS_SIZE],int sockfd,struct sockaddr_in *target);