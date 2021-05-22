#pragma once
/**
 * @brief 中继接口。用于快速向服务器请求
 *
 */
#include "common.h"
/*
 * 与其他DNS服务器通信
 */
typedef struct sockaddr SA;
struct sockaddr_in query_server;

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
           int sockfd, struct sockaddr_in *target);