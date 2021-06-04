#pragma once
/*
Copyright (c) 2020 rxi

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
修改自https://github.com/rxi/log.c
*/
// #define LOG_INCLUDED

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include<stdlib.h>

#include<pthread.h>
// #include <windows.h> 
#if defined(_WIN64) || defined( _WIN32)
#include <windows.h>
#elif __linux
#include<errno.h>
#include<pthread.h>
#endif


static const int MAX_CALLBACK_NUM = 4;
typedef struct {
  va_list ap;       //参数列表
  const char *fmt;  //格式串
  const char *file; //写文件
  struct tm *time;  //时间
  void *udata;      //流，比如stderr
  int line;         //行号
  int level;        //等级
} log_Event;

typedef void (*log_LockFn)(bool lock, void *udata); //对udata的锁

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

/**
 * @brief 6个LOG等级
 */
static const char *LOG_LEVEL_STRING[] = {"TRACE", "DEBUG", "INFO",
                                         "WARN",  "ERROR", "FATAL"};

/**
 * @brief log等级对应的颜色
 * @details TRACE: Light blue 蓝色
            DEBUG: Cyan 青色
            INFO：Green
            WARN：Yellow 黄色
            ERROR: Red 红色
            FATAL: Magenta紫色
            具体用法参考：https://www.codeproject.com/Tips/5255355/How-to-Put-Color-on-Windows-Console
 */

static const char *LOG_LEVEL_COLOR[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m",
                                        "\x1b[33m", "\x1b[31m", "\x1b[35m"};

/**
 * @brief trace等级log
 */
#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief debug等级log
 */
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief info等级log
 */
#define log_info(...) log_log(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief warn等级log
 */
#define log_warn(...) log_log(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief error等级log
 */
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)


#ifdef __linux
/**
 * @brief 快速打印异常
 * @param error 异常名称（用户提供）
 */
#define log_error_shortcut(error) log_error("%s:%s",error,strerror(errno));errno=0
#elif defined(_WIN64) || defined( _WIN32)
/**
 * @brief 快速打印异常
 * @param error 异常名称（用户提供）
 */
#define log_error_shortcut(error) log_error("%s:code[%d]",error,GetLastError())
#endif
              
/**
 * @brief fatal等级log
 */
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#ifdef __linux
/**
 * @brief 打印终止原因
 * @param error 异常名称（用户提供）
 */
#define log_fatal_shortcut(error) log_fatal("%s:%s",error,strerror(errno))
/**
 * @brief 打印终止原因并推出
 * @param error 异常名称（用户提供）
 */
#define log_fatal_exit_shortcut(error) log_fatal_shortcut(error);exit(EXIT_FAILURE)
#elif defined(_WIN64) || defined( _WIN32)
/**
 * @brief 打印终止原因
 * @param error 异常名称（用户提供）
 */
#define log_fatal_shortcut(error) log_fatal("%s:code[%d]",error,GetLastError())
/**
 * @brief 打印终止原因并退出
 * @param error 异常名称（用户提供）
 */
#define log_fatal_exit_shortcut(error) log_fatal_shortcut(error);exit(EXIT_FAILURE)
#endif

// 日志函数
void log_log(int level, const char *file, int line, const char *fmt, ...);

/**
 * @brief log锁
 */
void log_init_lock();

/**
 * @brief log调整过滤等级
 * @param level log等级
 */
void log_set_level(int level);

/**
 * @brief log关闭/开启
 *
 * @param enable true开启 false关闭
 */
void log_set_quiet(bool enable);

/**
 * @brief 文件log初始化
 *
 * @param filename 文件名称
 */
bool filelog_init(const char *filename);

/**
 * @brief 文件log关闭
 */
void filelog_close();

/**
 * @brief 文件log锁
 */
void filelog_init_lock();

/**
 * @brief 文件log调整过滤等级
 * @param level log等级
 */
void filelog_set_level(int level);

/**
 * @brief 文件log关闭/开启
 *
 * @param enable true开启 false关闭
 */
void filelog_set_quiet(bool enable);




void init_file_lock();