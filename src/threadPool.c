#include "threadPool.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
 
 
static void* work_routine(void* args)
{
   tpool_t* pool = (tpool_t*)args;
   tpool_work_t* work = NULL;
 
   while(1){
        pthread_mutex_lock(&pool->queue_lock);
        while(!pool->tpool_head && !pool->shutdown){ // if there is no works and pool is not shutdown, it should be suspended for being awake
            pthread_cond_wait(&pool->queue_ready,&pool->queue_lock);
        }
 
        if(pool->shutdown){
           pthread_mutex_unlock(&pool->queue_lock);//pool shutdown,release the mutex and exit
           pthread_exit(NULL);
        }
 
        /* tweak a work*/
        work = pool->tpool_head;
        pool->tpool_head = (tpool_work_t*)pool->tpool_head->next;
        pthread_mutex_unlock(&pool->queue_lock);
 
        work->work_routine(work->args);
 
        free(work);
   }
return NULL;
}
 
int create_tpool(tpool_t** pool,size_t max_thread_num)
{
   (*pool) = (tpool_t*)malloc(sizeof(tpool_t));
   if(NULL == *pool){
        printf("in %s,malloc tpool_t failed!,errno = %d,explain:%s\n",__func__,errno,strerror(errno));
        exit(-1);
   }
   (*pool)->shutdown = 0;
   (*pool)->maxnum_thread = max_thread_num;
   (*pool)->thread_id = (pthread_t*)malloc(sizeof(pthread_t)*max_thread_num);
   if((*pool)->thread_id == NULL){
        printf("in %s,init thread id failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
        exit(-1);
   }
   (*pool)->tpool_head = NULL;
   if(pthread_mutex_init(&((*pool)->queue_lock),NULL) != 0){
        printf("in %s,initial mutex failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
        exit(-1);
   }
 
   if(pthread_cond_init(&((*pool)->queue_ready),NULL) != 0){
        printf("in %s,initial condition variable failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
        exit(-1);
   }
 
   for(int i = 0; i < max_thread_num; i++){
        if(pthread_create(&((*pool)->thread_id[i]),NULL,work_routine,(void*)(*pool)) != 0){
           printf("pthread_create failed!\n");
           exit(-1);
        }
   }
return 0;
}
 
void destroy_tpool(tpool_t* pool)
{
   tpool_work_t* tmp_work;
 
   if(pool->shutdown){
        return;
   }
   pool->shutdown = 1;
 
   pthread_mutex_lock(&pool->queue_lock);
   pthread_cond_broadcast(&pool->queue_ready);
   pthread_mutex_unlock(&pool->queue_lock);
 
   for(int i = 0; i < pool->maxnum_thread; i++){
        pthread_join(pool->thread_id[i],NULL);
   }
   free(pool->thread_id);
   while(pool->tpool_head){
        tmp_work = pool->tpool_head;
        pool->tpool_head = (tpool_work_t*)pool->tpool_head->next;
        free(tmp_work);
   }
 
   pthread_mutex_destroy(&pool->queue_lock);
   pthread_cond_destroy(&pool->queue_ready);
   free(pool);
}
 
int add_task_2_tpool(tpool_t* pool,void* (*routine)(void*),void* args)
{
   tpool_work_t* work,*member;
 
   if(!routine){
        printf("rontine is null!\n");
        return -1;
   }
 
   work = (tpool_work_t*)malloc(sizeof(tpool_work_t));
   if(!work){
        printf("in %s,malloc work error!,errno = %d,explain:%s\n",__func__,errno,strerror(errno));
        return -1;
   }
 
   work->work_routine = routine;
   work->args = args;
   work->next = NULL;
 
   pthread_mutex_lock(&pool->queue_lock);
   member = pool->tpool_head;
   if(!member){
        pool->tpool_head = work;
   }
   else{
        while(member->next){
           member = (tpool_work_t*)member->next;
        }
        member->next = work;
   }
 
   //notify the pool that new task arrived!
   pthread_cond_signal(&pool->queue_ready);
   pthread_mutex_unlock(&pool->queue_lock);
return 0;
}