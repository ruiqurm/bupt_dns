#include"local_record.h"
#include<stdio.h>
// #include<stdbool.h>
bool read_local_record(const char* filename){
    char name[513];
    char ip[32];
    FILE* file = fopen(filename,"r");
    if(!file)return false;
    
    while(fscanf(file, "%s %s",ip,name)==2){
        printf("%s %s\n",name,ip);
    }
}

// int main(){
//     read_local_record("dnsrelay.txt");
// }