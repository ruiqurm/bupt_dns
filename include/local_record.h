#pragma once
/**
 * @brief 解析本地记录
 * 
 */

#include<stdbool.h>
#include"cache.h"
bool load_local_A_record(cache *Cache,const char* filename);