#include "threadPool.h"
#include<unistd.h>
#include<stdio.h>
#ifdef __linux
#include <sys/select.h>
#endif
void* fun_read(void* args)
{
   int t;
   if (scanf("%d",&t)==1)
      printf("thread %ld read %d\n",pthread_self(),t);
   sleep(1);
   return NULL;
}

int main(){
  fd_set rset,wset;
  FD_ZERO(&rset);
  int max_fd = 3,fd;//2+1
  tpool_t* pool = NULL;
   if(0 != create_tpool(&pool,5)){
        printf("create_tpool failed!\n");
        return -1;
   }
  while (1){
     FD_SET(STDIN_FILENO,&rset);
     fd = select(max_fd,&rset,&wset,NULL,NULL);
      if(FD_ISSET(STDIN_FILENO,&rset)){
         add_task_2_tpool(pool,fun_read,NULL);
      }
  }
   // sleep(2);
   destroy_tpool(pool);
}