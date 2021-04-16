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

#include<errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

static const int MAX_CALLBACK_NUM = 4;
typedef struct {
  va_list ap;//参数列表
  const char *fmt;//格式串
  const char *file;//写文件
  struct tm *time;//时间
  void *udata; //流，比如stderr
  int line;//行号
  int level;//等级
} log_Event;

typedef void (*log_LockFn)(bool lock, void *udata);//对udata的锁
typedef void (*log_Callback)(log_Event *ev);//回调函数

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
static const char *LOG_LEVEL_STRING[] ={
    "TRACE","DEBUG","INFO","WARN","ERROR","FATAL"
};
static const char *LOG_LEVEL_COLOR[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
/*
TRACE: Light blue 蓝色
DEBUG: Cyan 青色
INFO：Green
WARN：Yellow 黄色
ERROR: Red 红色
FATAL: Magenta紫色
具体用法参考：https://www.codeproject.com/Tips/5255355/How-to-Put-Color-on-Windows-Console
*/

#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

void log_log(int level, const char *file, int line, const char *fmt, ...);//log 上面是快捷设置等级的宏

void log_set_lock(log_LockFn fn, void *udata);
void log_set_level(int level);

void log_set_quiet(bool enable);
int log_add_callback(log_Callback fn, void *udata, int level);
int log_add_fp(FILE *fp, int level);


