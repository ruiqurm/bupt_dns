#pragma once
#define HOST_MAX_LENGTH 64
#define COMPRESS_FLAG 0xc0
#define COMPRESS_MASK 0x3fff

#include <arpa/inet.h>

#define IN_16b_RANGE(x) ((int)x>=0&&(int)x<=0xffff)
#define IN_32b_RANGE(x) ((int)x>=0&&(int)x<=0xffffffff)
#define get_compress_pos(x) (x & COMPRESS_MASK)
#define is_compress(x) (x & COMPRESS_FLAG)
#define QUERY_FLAG 0x0100
#define write_query_header(x) write_dns_header(x,-1,QUERY_FLAG,1,0,0,0)
#define DNS_SERVER_PORT 53

enum RRtype{
    A=1,NS=2,CNAME=5,SOA=6,PTR=12,MX=15,AAAA=28
};
enum Class{
    HTTP_CLASS=1,NONE_CLASS=254,ALL_CLASS=255
};

union ip{
    union{
    //     #pragma pack(1)
    //     struct {
    //         char __padding[sizeof(__uint128_t) - sizeof(uint32_t)];
    //         uint32_t v4;
    //     };
    //    #pragma pack()
        uint32_t v4;
        uint8_t v4byte[4];
    };
    
    union{
        #ifdef __SIZEOF_INT128__
        __uint128_t v6;
        #else
        #warning ip.v6 only not avilable now for the system is not 64 bit. `ip.v6`字段不支持
        #endif
        uint16_t v6byte[8];
    };

};
struct IP{
    union ip addr;
    u_int8_t type;
};
int saveip(struct IP*ip,const char* src,int version);
struct dns_header{
    uint16_t id; // identification number
    union{
        uint16_t flags;
        #pragma pack(1)
        struct {
            unsigned char rd :1; // 是否期望递归
            unsigned char tc :1; // 是否是截断的
            unsigned char aa :1; // 应答时有效，指出是否为权威服务器
            unsigned char opcode :4; // 0标准查询，1反向查询，2服务器状态查询 其他未用
            unsigned char qr :1; // query/response 区分请求或者应答
            
            unsigned char rcode :4; // 应答码
            unsigned char cd :1; // 禁止校验
            unsigned char ad :1; // 是否是真实数据
            unsigned char z :1; // zero，填0
            unsigned char ra :1; // 递归可用
        };
        #pragma pack()
    };
    uint16_t qdcount; // 问题数
    uint16_t ancount; // 答案数
    uint16_t nscount; // 授权信息数
    uint16_t adcount; // 额外信息
};

struct question{
    uint16_t qtype;
    uint16_t qclass;
    char label[64];
};

struct rr{
    char name[64];
    uint16_t type;
    uint16_t class_;
    uint32_t ttl;
    union{
        struct IP address;
        char cname[512];
    };
};


//用于定位数据
#pragma pack(1)
struct rr_struct{
    uint16_t type;
    uint16_t class_;
    uint32_t ttl;
    uint16_t length;
};
struct question_struct{
    uint16_t qtype;
    uint16_t qclass;
};
struct header_struct{
    uint16_t id; // identification number
    uint16_t flags;
    uint16_t qdcount; // 问题数
    uint16_t ancount; // 答案数
    uint16_t nscount; // 授权信息数
    uint16_t adcount; // 额外信息
};
#pragma pack()



int fetch_umcompress_label(char*dest,const char*src);
int fetch_label(char* dest,char*src,char* base,char**end_pos);
int str_to_label(char* dest,const char* host,unsigned maxlen);

int multiquery(int sockfd,struct sockaddr_in*to,char*questions[],size_t questions_size);

int read_dns_header(struct dns_header* header,char*src);
int read_dns_answer(struct rr*dest,char* src,char*message_start);//读取一条dns回答
int read_dns_answers(struct rr *answers,char* message);//读取所有dns回答
int read_dns_question(struct question* question,char*src,char*message_start);
int read_dns_questions(struct question* question,char*message);
// int write_dns_question(char*buffer,struct);
int write_dns_header(char*buffer,int id,uint16_t flags,int qdcount,int ancount,int nscount,int adcount);
int write_dns_question(char*buffer,const char*question,int class,int type);
int write_dns_rr(char* buffer,const char*name,int type,int class,int ttl,const void*rdata);//写rr

// 打包区
// 快速发送询问
int write_dns_query(char*buffer,char questions[],int rrtype);


// 调试区
int sprint_dns_header(char*dest,char*header);
int sprint_dns_questions(char*dest,char*message_start);
int sprint_dns_answers(char*dest,char*rr);
int sprint_dns(char*dns);

// void print_dns_header(const char*header);

// int write_dns_query_with_header();
// int write_dns_response();
// int write_dns_with_header(char*dest,struct header*header,struct question* question);


