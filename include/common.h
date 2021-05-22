#pragma once
/**
 * @file common.h
 * @brief 提供封装后的DNS接口
 */

#include <arpa/inet.h>

/**********
 *        *
 *   宏   *
 *        *
 **********/

// #define HOST_MAX_LENGTH 64
// DNS query标记
#define FLAG_QUERY 0x0100

// DNS 回复头标记 （后面改）
#define FLAG_RESPONSE_NORMAL 0x8150

//判断是否是两字节
#define IN_16b_RANGE(x) ((int)x >= 0 && (int)x <= 0xffff)

//判断是否是四字节
#define IN_32b_RANGE(x) ((int)x >= 0 && (int)x <= 0xffffffff)

// DNS 域名压缩标记
#define COMPRESS_FLAG 0xc0

// DNS 域名压缩mask，用于直接提取偏移量
#define COMPRESS_MASK 0x3fff

/**
 * @brief 获取压缩偏移量
 * @param x 偏移量
 */
#define get_compress_pos(x) ((unsigned short)x & COMPRESS_MASK)

/**
 * @brief 判断是否是压缩标记
 * @param x 字符
 */
#define is_compress(x) ((unsigned char)x & COMPRESS_FLAG)

// DNS协议端口号
#define DNS_SERVER_PORT 53

// UDP最大报文长度
#define MAX_UDP_SIZE 1500

// DNS最大报文长度
#define MAX_DNS_SIZE 520

// DNS最大label长度(应该是63 留一个位置作结束符)
#define MAX_LABEL_LANGTH 64

// NAME最大长度（应该是512 留一个位置作结束符)
#define MAX_NAME_LENGTH 513

// sockaddr_in的长度
const unsigned int ADDR_LEN;

// DNS头部长度
const unsigned int DNS_HEADER_SIZE;

// question_struct结构体的大小
const int QUERY_SIZE;

/*************************
 *                       *
 *   枚举、结构体和union  *
 *                       *
 *************************/

// RR type
enum RRtype { A = 1, NS = 2, CNAME = 5, SOA = 6, PTR = 12, MX = 15, AAAA = 28 };
// RR class
enum Class { HTTP_CLASS = 1, NONE_CLASS = 254, ALL_CLASS = 255 };

enum { IPV4 = 4, IPV6 = 6 };

/** @union ip
*  @brief 同时保存ipv4和ipv6的union
*  @details -----------------   \n
            |4/6|4/6|4/6|4/6|   \n
            | 6 | 6 | 6 | 6 |   \n
            | 6 | 6 | 6 | 6 |   \n
            | 6 | 6 | 6 | 6 |   \n
            -----------------   \n
*/
union ip {
  union {
    //     #pragma pack(1)
    //     struct {
    //         char __padding[sizeof(__uint128_t) - sizeof(uint32_t)];
    //         uint32_t v4;
    //     };
    //    #pragma pack()
    uint32_t v4;
    uint8_t v4byte[4];
  };

  union {
#ifdef __SIZEOF_INT128__
    __uint128_t v6;
#else
#warning ip.v6 only not avilable now for the system is not 64 bit. `ip.v6`字段不支持
#endif
    uint16_t v6byte[8];
  };
};

// IP地址结构体
struct IP {
  union ip addr;      //上面的IP
  unsigned char type; //是数字4或者6
};
//从字符串保存IP
int saveip(struct IP *ip, const char *src, int version);

/** @struct dns_header
 *  @brief DNS header
 */
struct dns_header {
  uint16_t id; // identification number
  union {
    uint16_t flags;
#pragma pack(1)
    struct {
      unsigned char rd : 1; // 是否期望递归
      unsigned char tc : 1; // 是否是截断的
      unsigned char aa : 1; // 应答时有效，指出是否为权威服务器
      unsigned char
          opcode : 4; // 0标准查询，1反向查询，2服务器状态查询 其他未用
      unsigned char qr : 1; // query/response 区分请求或者应答

      unsigned char rcode : 4; // 应答码
      unsigned char cd : 1;    // 禁止校验
      unsigned char ad : 1;    // 是否是真实数据
      unsigned char z : 1;     // zero，填0
      unsigned char ra : 1;    // 递归可用
    };
#pragma pack()
  };
  uint16_t qdcount; // 问题数
  uint16_t ancount; // 答案数
  uint16_t nscount; // 授权信息数
  uint16_t adcount; // 额外信息
};

/** @struct question
 *  @brief 用于保存DNS question数据
 */
struct question {
  uint16_t qtype;
  uint16_t qclass;
  char label[64];
};

/** @struct answer
 *  @brief 用于保存DNS answer数据
 */
struct answer {
  char name[64];
  uint16_t type;
  uint16_t class_;
  uint32_t ttl;
  union {
    struct IP address;
    char cname[512];
  };
};

// DNS定位结构体，不用于保存数据
//之所以要分离出来，是因为有一些像TTL，length的数据没有什么用，不需要保存。但是定位解析又需要它，因此把它们分离开来。

#pragma pack(1)
/** @struct answer_struct
 *  @brief 定位answer的结构体
 */
struct answer_struct {
  uint16_t type;
  uint16_t class_;
  uint32_t ttl;
  uint16_t length;
};

/** @struct question_struct
 *  @brief 定位question的结构体
 */
struct question_struct {
  uint16_t qtype;
  uint16_t qclass;
};

/** @struct header_struct
 *  @brief 定位header的结构体
 * （也可以用上面的header替代，此处为了格式规整，故加上。
 */
struct header_struct {
  uint16_t id; // identification number
  uint16_t flags;
  uint16_t qdcount; // 问题数
  uint16_t ancount; // 答案数
  uint16_t nscount; // 授权信息数
  uint16_t adcount; // 额外信息
};
#pragma pack()

/*************************
 *                       *
 *        函数接口        *
 *                       *
 *************************/

/******************
 * 解析和创建label *
 ******************/

// int fetch_umcompress_label(char*dest,const char*src); //已移除

/**
 *   @brief 获取标签。
 *   @param dest: 要复制到的字符串
 *   @param src:  压缩字符串开始的位置
 *   @param base: DNS报文头
 *   @param next:串结束的位置，不需要的话可以传入一个NULL
 *
 *   返回值：
 *    @return -1： 异常\n其他正数： 读取的字符串长度
 *
 */
int fetch_label(char *dest, char *src, char *base, char **end_pos);

/**
 *  @brief 用于把域名转化成DNS格式label
 *  @details  例如   host = www.bupt.edu.cn转化为 host =
 @verbatim\x1www\x4bupt\x3edu\x2cn\x0 @endverbatim
 *  @param dest: char数组，DNS报文地址
 *  @param host: 字符串,域名
 *  @param maxlen: 最大长度

 *  返回值
 *    @return 写入字节数
 */
int str_to_label(char *dest, const char *host, unsigned maxlen);

/***************************
 *   解析和写入DNS报文      *
 ***************************/

/**
 *  @brief 读取DNS头
 *  @param dns_header: 将要写入的DNS头地址
 *  @param src       : DNS报文的第一个字节地址
 *  返回值： @return DNS头的长度（12字节）
 */
int read_dns_header(struct dns_header *header, char *src);

/**
 *  @brief 读取一条 DNS answer
 *  @param dest         : 将要写入的DNS answer地址
 *  @param src          : DNS answer的第一个字节地址
 *  @param message_start: DNS报文的第一个字节
 *
 *  返回值：
 *  @return 这一条DNS
 * answer的长度（指字符串中，可以用来计算下一个answer开始的位置）
 */
int read_dns_answer(struct answer *dest, char *src, char *message_start);

/**
 *  @brief 读取全部dns answer。用户要自行保证answer*指针不会越界
 *  @param dest         : 将要写入的DNS answer数组地址
 *  @param src          : DNS answer的第一个字节地址
 *  @param message_start: DNS报文的第一个字节
 *  返回值：
 *  @return 读取到的DNS answer数量
 */
int read_dns_answers(struct answer *answers, char *message);

/**
 *  @brief 读取一条 DNS question
 *  @param dest         : 将要写入的DNS question结构体地址
 *  @param src          : DNS question的第一个字节地址
 *  @param message_start: DNS报文的第一个字节
 *
 *  返回值：
 * @return 这一条DNS question的长度（可以用来计算下一个question开始的位置）
 */
int read_dns_question(struct question *question, char *src,
                      char *message_start);

/**
 *  @brief 读取全部dns
 * questions。目前不可能有超过1的question。如果有的话就是格式错误。
 *  @param dest         : 将要写入的DNS question数组地址
 *  @param src          : DNS question的第一个字节地址
 *  @param message_start: DNS报文的第一个字节
 *  返回值：
 * @return 读取到的DNS question数量(1)
 */
int read_dns_questions(struct question *question, char *message);
// int write_dns_question(char*buffer,struct);

/**
 *  @brief 写一条DNS header
 *  @param buffer    : DNS报文开始写入的地址
 *  @param id        : id
 *  @param flags     : 标记位。flag不需要翻转
 *  @param qdcount   : 问题数
 *  @param ancount   : 回答数
 *  @param nscount   : ns数
 *  @param adcount   : ad数
 *
 *  返回值：
 * @return 写入字节数
 */
int write_dns_header(char *buffer, int id, uint16_t flags, int qdcount,
                     int ancount, int nscount, int adcount);

/**
 * @brief 写入请求头部。自行决定id
 * @param x DNS buffer
 * @return 写入字节数
 */
#define write_query_header_auto(x)                                             \
  write_dns_header(x, -1, FLAG_QUERY, 1, 0, 0, 0)

/**
 * @brief 写入请求头部
 * @param x DNS buffer
 * @param id DNS id
 * @return 写入字节数
 */
#define write_query_header(x, id)                                              \
  write_dns_header(x, id, FLAG_QUERY, 1, 0, 0, 0)

/**
 *  @brief 写一条DNS question
 *  @param buffer    : DNS报文开始写入的地址
 *  @param question  : DNS question的第一个字节地址
 *  @param class     : class.详见rfc
 *  @param type      : type。详见rfc
 *
 *  返回值：
 * @return 写入字节数
 */
int write_dns_question(char *buffer, const char *question, int class, int type);

/**
 *  @brief 写一条DNS answer
 *  @param buffer    : DNS报文开始写入的地址
 *  @param name      : 解析的域名
 *  @param class     : class.详见rfc
 *  @param type      : type。详见rfc
 *  @param ttl       : time to live. 生存时间期
 *  @param rdata     : 数据
 *
 *  返回值：
 * @return 写入字节数
 */
int write_dns_answer(char *buffer, const char *name, int type, int class,
                     int ttl, const void *rdata); //写answer

/********************
 *   快速发送接口    *
 ********************/

/**
 * @brief 写一条DNS查询
 *
 * @param buffer DNS报文开始（注意是DNS报文的第一个字节的地址）
 * @param questions question
 * @param rrtype rr类型
 * @return 写入字节数
 */
int write_dns_query(char *buffer, char questions[], int rrtype);

/**
 * @brief 在DNS query的基础上写answer
 *
 * @param buffer DNS报文开始（注意是DNS报文的第一个字节的地址
 * @param answer answer
 * @param answer_num answer的数目
 * @return 写入字节数
 */
int write_dns_response_by_query(char *buffer, struct answer answer[],
                                uint16_t answer_num);

/**
 *
 * @deprecated
 * @brief 多查询
 *        目前DNS不支持多查询，返回结果都是format eansweror,该接口没有用
 * @param sockfd
 * @param to
 * @param questions
 * @param questions_size
 * @return int
 */
int multiquery(int sockfd, struct sockaddr_in *to, char *questions[],
               size_t questions_size);

/*********************
 *                   *
 *       调试        *
 *                   *
 *********************/

/**
 * @brief 打印DNS头到buffer
 *
 * @param dest buffer
 * @param header header
 * @return 写入字节数
 */
int sprint_dns_header(char *dest, char *header);

/**
 * @brief 打印DNS question到buffer
 *
 * @param dest buffer
 * @param message_start questions开始的地方
 * @return 写入字节数
 */
int sprint_dns_questions(char *dest, char *message_start);

/**
 * @brief 打印DNS answer到buffer
 *
 * @param dest buffer
 * @param answer  answer
 * @return 写入字节数
 */
int sprint_dns_answers(char *dest, char *answer);

/**
 * @brief 打印DNS报文
 *
 * @param dns DNS报文
 */
void sprint_dns(char *dns);

/******************
 *      其他      *
 ******************/
