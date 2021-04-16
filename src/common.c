#include "common.h"
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include<time.h>

const int DNS_HEADER_SIZE = sizeof(struct dns_header);
const int QUERY_SIZE = sizeof(struct question);
const int MAX_DNS_SIZE = 513;

int saveip(struct IP*ip,const char* src,int version){
    switch (version)
    {
    case 4:
        memcpy((void*)ip,src,4);        
        memset((void*)ip+4,0,8);
        ip->type = 4;
        return 4;
    case 6:
        memcpy((void*)ip,src,16);  
        ip->type = 6;
        return 16;
    default:
        return -1;
    }
}



void init_header(struct header_struct *header){
    header->id = (unsigned short) htons(2);
    header->flags = htons(0x0100);
    header->adcount =header->nscount = header->ancount = header->qdcount = htons(0);
}

int str_to_label(char* dest,const char* host,unsigned maxlen){
    /*
    用于把域名转化成DNS格式
    例如   host = www.bupt.edu.cn
    转化为 host = \x1www\x4bupt\x3edu\x2cn\x0
    */
   int n = strlen(host);
   unsigned last_pos = 0,cumulate=0;
   int i,j = 1;//空出第一格
    for(i =0;i<n&&j<maxlen;i++,j++){
        if (host[i]=='.'){
            dest[last_pos] = cumulate;
            cumulate = 0;
            last_pos = j;
        }else{
            dest[j] = host[i];
            cumulate++;
        }
    }
    if(i<n)return -1;
    dest[last_pos] = cumulate;
    dest[j] = 0;
    return j+1;
} 





//目前DNS不支持多查询，返回结果都是format error
//https://stackoverflow.com/questions/4082081/requesting-a-and-aaaa-records-in-single-dns-query/4083071#4083071
int multiquery(int sockfd,struct sockaddr_in*to,char*questions[],size_t questions_size){
    struct header_struct *header;
    struct question_struct *question;
    char *qname_pos,*question_pos;
    char buffer[MAX_DNS_SIZE];
    int i,qname_size=0;
    int total_size = 0;

    header = (struct header_struct*)&buffer; //直接在buffer上写
    init_header(header);
    header->qdcount = htons(questions_size);

    qname_pos = buffer+DNS_HEADER_SIZE;//第一个question的位置
    for(i = 0;i<questions_size;i++){
        qname_size = str_to_label(qname_pos,questions[i],MAX_DNS_SIZE -(qname_pos-buffer));
        question_pos = qname_pos + qname_size;
        question = (struct question_struct*)question_pos;
        question->qclass= htons(1);
        question->qtype = htons(1);
        qname_pos+=qname_size+QUERY_SIZE;//加上整个question的偏移量
    }
    total_size = qname_pos-buffer;
    for(int i=0;i<total_size;i++){
        printf("%d",buffer[i]);
    }
    if (sendto(sockfd,(char*)buffer,total_size,0,(struct sockaddr*)to,sizeof(struct sockaddr_in))<0){
        printf("Failed\n");
        return -1;
    }
    return 0;
}




// int fetch_umcompress_label(char*dest,const char*src){
//     short now = 0;
//     short i = 0;
//     while(src[now]!=0){
//         for(short j =1;j<=src[now];j++)
//             dest[i++]=src[j+now];
//         dest[i++] = '.';
//         now += src[now]+1;
//     }
//     dest[now-1] = 0;
//     return i;
// }
int fetch_label(char* dest,char*src,char* base,char**next){
    /*
    获取标签。

    dest: 要复制到的字符串
    src:  压缩字符串开始的位置
    base: DNS报文头
    next:串结束的位置，不需要的话可以传入一个NULL
    返回值：
    -1： 异常
    其他正数： 读取的字符串长度

    压缩方案使得标签有如下三种可能：
    * 以'\0'结束
    * 以指针结束
    * 指针（只有一个指针）
    并且，一个标签最多只有一个指针。

    下面的解决方案是：
    * 如果头是指针，直接整个复制下来。
    * 否则，按正常的读取，直到遇到指针或者0.
    * 遇到0，则终止；否则整个复制下来，结束。
    */
    short now = 0;
    short i = 0;

    while(src[now]!=0){
        if(is_compress(src[now])){
            unsigned short pos = get_compress_pos(ntohs(*(const unsigned short*)(src+now)));
            if(next)*next = src+now+2;
            return fetch_label(dest+i,base+pos,base,NULL);
        }else{
            for(short j =1;j<=src[now];j++)
                dest[i++]=src[j+now];
            dest[i++] = '.';
            now += src[now]+1;
        }
    }
    dest[i-1] = '\0';
    if(next)*next = src+now;
    return i-1; 
}
int read_dns_header(struct dns_header* header,char*src){
    struct dns_header* header_= (struct dns_header*)src;
    header->id = ntohs(header_->id);
    header->flags = header_->flags;
    header->qdcount = ntohs(header_->qdcount);
    header->ancount = ntohs(header_->ancount);
    header->nscount = ntohs(header_->nscount);
    header->adcount = ntohs(header_->adcount);
    return sizeof(struct dns_header);
}
int read_dns_question(struct question* question,char*src,char*message_start){
    char *src_ = src;
    if (fetch_label((char*)&question->label,src,message_start,&src)<0){
        //TODO:异常处理
        return -1;
    }
    struct question_struct* q;
    q = (struct question_struct*)src;
    question->qclass = ntohs(q->qclass);
    question->qtype = ntohs(q->qtype);
    return src-src_+1;
}
int read_dns_questions(struct question* question,char*message){
    //读取报文中的问题
    struct dns_header *header;
    header = (struct dns_header*)message;
    unsigned short question_num = ntohs(header->qdcount);
    char* data = message + DNS_HEADER_SIZE; 
    //一般只会有1个question
    data += read_dns_question(question,data,message);
    struct question_struct* q = (struct question_struct*)data;
    question->qclass = ntohs(q->qclass);
    question->qtype = ntohs(q->qtype);
    return question_num;
}
int read_dns_answer(struct rr*dest,char* src,char*message_start){
    char* src_ = src;
    int cnt = fetch_label((char*)&(dest->name),src,message_start,&src);
    if(cnt==-1)return -1;
    struct rr_struct* 
            rr_struct = (struct rr_struct*)src;
    dest->ttl = ntohl(rr_struct->ttl);
    dest->class_ = ntohs(rr_struct->class_);
    dest->type = ntohs(rr_struct->type);
    uint16_t length = ntohs(rr_struct->length);
    src += sizeof(struct rr_struct);
    switch (dest->type){
        case A:
        case AAAA:
            saveip(&(dest->address),src,length);
            break;
        case CNAME:
            // dest->cname = (char*) malloc(80);
            fetch_label(dest->cname,src,message_start,NULL);
            break;
        case MX:
            return -1;
            // next = 0;
            break;
        default:
            return -1;
            // next = 0;
            break;
    }
    src += length;
    return src - src_;
}

int read_dns_answers(struct rr *answers,char* message){
    //读取报文中所有回答
    struct dns_header *header;
    struct rr_struct* rr_struct;
    header = (struct dns_header*)message;
    unsigned short ans = ntohs(header->ancount);
    unsigned short question = ntohs(header->qdcount);
    char* data = message + DNS_HEADER_SIZE; 
    for(int i=0;i<question;i++){
        while((*data)!=0)data+=(*data)+1;
        data+=5;
    }
    for(int i =0;i<ans&&data!=0;i++){
        data += read_dns_answer((struct rr*)&answers[i],data,message);
    }
    return ans;
}    

int write_dns_header(char*buffer,int id,uint16_t flags,int qdcount,int ancount,int nscount,int adcount){
    struct dns_header *header;
    size_t size =sizeof(struct header_struct);
    header = (struct dns_header*)buffer;
    if IN_16b_RANGE(id){
        header->id = id;
    }else{
        srand(time(NULL));
        header->id = rand();
    }
    header->flags = htons(flags);
    header->qdcount = htons(IN_16b_RANGE(qdcount)?qdcount:0);
    header->ancount = htons(IN_16b_RANGE(ancount)?ancount:0);
    header->nscount = htons(IN_16b_RANGE(nscount)?nscount:0);
    header->adcount = htons(IN_16b_RANGE(adcount)?adcount:0);
    return size;
}

int write_dns_question(char*buffer,const char*question,int class,int type){
    struct question_struct* qu;
    // size_t size =sizeof(struct question_struct);
    int size = str_to_label(buffer,question,MAX_DNS_SIZE);//TODO：max_len
    if (size==-1)return -1;
    buffer+=size;
    qu = (struct question_struct*)buffer;
    qu->qclass = htons(IN_16b_RANGE(class)?class:0);//默认无效值
    qu->qtype = htons(IN_16b_RANGE(type)?type:0); //默认无效值
    size+=sizeof(struct question_struct);
    return size;
}

int write_dns_rr(char* buffer,const char*name,int type,int class,int ttl,const void*rdata){
    //TODO: 添加异常处理
    struct rr_struct* rr;
    int count,size,tmp;//临时变量

    size = str_to_label(buffer,name,MAX_DNS_SIZE);
    if(size==-1)return -1;
    else {
        buffer+=size;
        count=size;
    };
    rr = (struct rr_struct*)buffer;
    rr->type = htons(IN_16b_RANGE(type)?type:0);
    rr->class_ =htons(IN_16b_RANGE(class)?class:0);
    rr->ttl = htonl(IN_32b_RANGE(ttl)?ttl:0);
    tmp = sizeof(struct rr_struct);
    buffer+=tmp;
    switch (type){
    case A:
        rr->length = htons(4);
        break;
    case AAAA:
        rr->length = htons(16);
        break;
    case CNAME:
        rr->length = htons(strlen((char*)rdata));
        break;
    // case MX:
        // break;
    default:
        return -1;
    }
    size = rr->length;//size重新利用一下
    memcpy(buffer,rdata,size);
    buffer += size;
    count += tmp+size;
    return count;
}

int write_dns_query(char*buffer,char questions[],int rrtype){
    char* buffer_ = buffer;
    
    buffer_ += write_query_header(buffer_);
    buffer_ += write_dns_question(buffer_,questions,HTTP_CLASS,rrtype);

    return buffer_ - buffer;
}



static enum{
    DEBUG_PRINT_TYPE_qr,DEBUG_PRINT_TYPE_opcode,
    DEBUG_PRINT_TYPE_aa,DEBUG_PRINT_TYPE_tc,
    DEBUG_PRINT_TYPE_rd,DEBUG_PRINT_TYPE_ra,
    DEBUG_PRINT_TYPE_z,DEBUG_PRINT_TYPE_ad,
    DEBUG_PRINT_TYPE_cd,DEBUG_PRINT_TYPE_rcode,
    DEBUG_PRINT_TYPE_qdcount,DEBUG_PRINT_TYPE_ancount,
    DEBUG_PRINT_TYPE_nscount,DEBUG_PRINT_TYPE_arcount,
    DEBUG_PRINT_TYPE_type,DEBUG_PRINT_TYPE_class
}DEBUG_PRINT_TYPE;

inline 
static const char* debugstr(int type,int value){
    switch (type){
    case DEBUG_PRINT_TYPE_qr:
        /* code */
        if (value == 0)return "Message is a query";
        else return "Message is a response";
    case DEBUG_PRINT_TYPE_opcode:
        if (value == 0)return "Standard query (0)";
        else if(value==1) return "Reverse query (1)";
        else return "Unknow";
    case DEBUG_PRINT_TYPE_aa:
        if (value==0)return "Server is not an authority for domain";
        else return "Server is an authority for domain";
    case DEBUG_PRINT_TYPE_tc:
        if (value==0)return "Message is not truncated";
        else return "Message is truncated";
    case DEBUG_PRINT_TYPE_rd:
        if (value==0)return "Can't do recursively";
        else return "Do query recursively";
    case DEBUG_PRINT_TYPE_ra:
        if (value==0)return "Can't do recursively";
        else return "Do query recursively";
    case DEBUG_PRINT_TYPE_z:
        return "reserved (0)";
    case DEBUG_PRINT_TYPE_ad:
        if(value==0)return "Answer/authority portion was not authenticated by the server";
        else return "Answer/authority portion was authenticated by the server";
    case DEBUG_PRINT_TYPE_cd:
        if (value==0) return "Non-authenticated data";
        else return "Authenticated data";
    case DEBUG_PRINT_TYPE_rcode:
        switch (value)
        {
        case 0:
            return "No error (0)";
        case 1:
            return "FormErr (1)";//格式错误
        case 2:
            return "ServFail (2)";//服务器失效
        case 3:
            return "NXDomain (3)";//不存在域名
        case 4:
            return "NotImp (4)";//未实现
        case 5:
            return "Refused (5)";//查询拒绝
        default:
            return "Unknow Error";
        }
    case DEBUG_PRINT_TYPE_type:
        switch (value)
        {
        case AAAA:
            return "AAAA";
        case A:
            return "A";
        case CNAME:
            return "CNAME";
        case MX:
            return "MX";
        default:
            return "Unknow";
        }
    case DEBUG_PRINT_TYPE_class:
        if (value==HTTP_CLASS)
            return "IN";
        else return "Unknow";
    default:
        return "unknow";
    }
}
inline
static const char* bit_to_str(unsigned char val,int num){
    static char b[9];
    unsigned char x = val & (-1>>(8-num));
    b[0] = '\0';
    int z;
    for (z = 1<<(num-1); z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}
int sprint_dns_header(char*dest,char*src){
    struct dns_header header;
    read_dns_header(&header,src);
    int size = 0;
    if (!header.qr){
        //询问
            size+=sprintf(dest,
"------------------------------------\n\
Transaction ID: 0x%2x\n\
FLAG: 0x%04x\n\\
%d... .... .... .... = Response:%s\n\
.%s... .... .... = Opcode: %s)\n\
.... ..%d. .... .... = Truncated: %s\n\
.... ...%d .... .... = Recursion desired: %s\n\
.... .... .%d.. .... = Z: reserved (0)\n\
.... .... ...%d .... = %s\n",
                header.id,
                header.flags,
                0,debugstr(DEBUG_PRINT_TYPE_qr,header.qr),
                bit_to_str(header.opcode,4),debugstr(DEBUG_PRINT_TYPE_opcode,header.opcode),
                header.tc,debugstr(DEBUG_PRINT_TYPE_tc,header.tc),
                header.rd,debugstr(DEBUG_PRINT_TYPE_rd,header.rd),
                header.z,
                header.cd,debugstr(DEBUG_PRINT_TYPE_cd,header.cd));
    }else{
        //回答
            size+=sprintf(dest,
"------------------------------------\n\
Transaction ID: 0x%2x\n\
FLAG: 0x%04x\n\
%d... .... .... .... = Response:%s\n\
.%s... .... .... = Opcode: %s\n\
.... .%d.. .... .... = Authoritative: %s\n\
.... ..%d. .... .... = Truncated: %s\n\
.... ...%d .... .... = Recursion desired: %s\n\
.... .... %d... .... = Recursion available: %s\n\
.... .... .%d.. .... = Z: reserved (0)\n\
.... .... ..%d. .... = Answer authenticated: %s\n\
.... .... ..%d .... = %s\n\
.... .... .... %s = Reply code: %s\n",
                header.id,
                header.flags,
                1,debugstr(DEBUG_PRINT_TYPE_qr,1),
                bit_to_str(header.opcode,4),debugstr(DEBUG_PRINT_TYPE_opcode,header.opcode),
                header.aa,debugstr(DEBUG_PRINT_TYPE_aa,header.aa),
                header.tc,debugstr(DEBUG_PRINT_TYPE_tc,header.tc),
                header.rd,debugstr(DEBUG_PRINT_TYPE_rd,header.rd),
                header.ra,debugstr(DEBUG_PRINT_TYPE_ra,header.ra),
                header.z,
                header.ad,debugstr(DEBUG_PRINT_TYPE_ad,header.ad),
                header.cd,debugstr(DEBUG_PRINT_TYPE_cd,header.cd),
                bit_to_str(header.rcode,4),debugstr(DEBUG_PRINT_TYPE_rcode,header.rcode));
    }
    dest+=size;
    size+=sprintf(dest,
"Questions: %d\n\
Answer RRs: %d \n\
Authority RRs: %d\n\
Additional RRs: %d\n",
header.qdcount,header.ancount,header.nscount,header.adcount);
    return size;
}
int sprint_dns_questions(char*dest,char*message_start){
    struct question q;
    char*dest_=dest;
    read_dns_questions(&q,message_start);
    dest+=sprintf(dest,"-------QUESTIONS----------\n");
    dest+=sprintf(dest,"%s type:%s class:%s\n",
    q.label,debugstr(DEBUG_PRINT_TYPE_type,q.qtype),debugstr(DEBUG_PRINT_TYPE_class,q.qclass));
    return dest-dest_;
}
int sprint_dns_answers(char*dest,char*message){
    struct rr rrs[8];
    char* dest_ = dest;
    int n = read_dns_answers(rrs,message);
    dest+=sprintf(dest,"-------ANSWERS----------\n");
    for (int i =0;i<n;i++){
        dest+=sprintf(dest,"%s type:%s class:%s ",
            rrs[i].name,
            debugstr(DEBUG_PRINT_TYPE_type,rrs[i].type),
            debugstr(DEBUG_PRINT_TYPE_class,rrs[i].class_));
        switch (rrs[i].type)
        {
        case A:
            dest+=sprintf(dest,"ip");
            for(int j=0;j<4;j++)
                dest+=sprintf(dest,":%d",rrs[i].address.addr.v4byte[j]);
            break;
        case AAAA:
            for(int j=0;j<8;j++)
                dest+=sprintf(dest,":%04x",rrs[i].address.addr.v6byte[j]);;
        case CNAME:
            dest+= sprintf(dest,"CNAME:%s",rrs[i].cname);
            // free(&rrs[i].cname);
        default:
            break;
        }
        dest+=sprintf(dest,"\n");
    }
    return dest - dest_;
}
int sprint_dns(char*dns){
    char buffer[2048];
    char*dest = buffer;
    dest+=sprint_dns_header(dest,dns);

    // const char* query_start = dns + sizeof(struct header_struct);
    // dest+=sprint_dns_header(dest,dns);
    dest+=sprint_dns_questions(dest,dns);
    dest+=sprint_dns_answers(dest,dns);
    puts(buffer);
    // fflush(stdout);
    fflush(stdout);
}