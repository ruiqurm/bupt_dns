#include "log.h"

/*
锁
*/
typedef struct{
    log_Callback fn;
    void* udata;
    int level;
}Callback;

static struct {
  void *udata;
  Callback callbacks[4];
  int max_call_back;
  log_LockFn lock;
  int level;
  bool quiet;
} LOGGING;

static void lock(void)   {
    if (LOGGING.lock) { LOGGING.lock(true, LOGGING.udata); }
}

static void unlock(void) {
    if (LOGGING.lock) { LOGGING.lock(false, LOGGING.udata); }
}

static void init_event(log_Event *ev, void *udata) {//配置时间和流
    if (!ev->time) {
        time_t t = time(NULL);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}
static void stdout_callback(log_Event *ev) {
    char buf[64];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';//把日期写入buffer
    fprintf(
        ev->udata, "%s \x1b[1m%s%-5s %s:%d:\x1b[0m ",
        buf, LOG_LEVEL_COLOR[ev->level], LOG_LEVEL_STRING[ev->level],
        ev->file, ev->line);
    vfprintf(ev->udata, ev->fmt, ev->ap);//向文件输出
    fprintf(ev->udata, "\n");
    fflush(ev->udata);
}

static void file_callback(log_Event *ev) {//file 回调
  char buf[64];
  buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
  fprintf(
    ev->udata, "%s %-5s %s:%d: ",
    buf, LOG_LEVEL_STRING[ev->level], ev->file, ev->line);
  vfprintf(ev->udata, ev->fmt, ev->ap);
  fprintf(ev->udata, "\n");
  fflush(ev->udata);
}

/*
对外部接口
*/
void log_set_lock(log_LockFn fn, void *udata) {
    LOGGING.lock = fn;
    LOGGING.udata = udata;
}

void log_set_level(int level) {
    LOGGING.level = level;
}

void log_set_quiet(bool enable) {
    LOGGING.quiet = enable;
}

int log_add_fp(FILE *fp, int level) {
  return log_add_callback(file_callback, fp, level);
}

int log_add_callback(log_Callback fn, void *udata, int level) {
  for (int i = 0; i < MAX_CALLBACK_NUM; i++) {
    if (!LOGGING.callbacks[i].fn) {
      LOGGING.callbacks[i] = (Callback) { fn, udata, level };
      return 0;
    }
  }
  return -1;
}
void log_log(int level, const char *file, int line, const char *fmt, ...) {
    log_Event ev = {
        .fmt   = fmt,
        .file  = file,
        .line  = line,
        .level = level,
    };

    lock();

    if (!LOGGING.quiet && level >= LOGGING.level) {//如果非静默 且等级高于阈值
        init_event(&ev, stderr);
        va_start(ev.ap, fmt);//按照格式保存参数到ev.ap
        stdout_callback(&ev);//执行log
        va_end(ev.ap);
    }
    for(int i =0;i<LOGGING.max_call_back;i++){
        Callback *cb = &LOGGING.callbacks[i];
        if (level >= cb->level) {
            init_event(&ev, cb->udata);
            va_start(ev.ap, fmt);
            cb->fn(&ev);
            va_end(ev.ap);
        }
    }
    unlock();
}

// int main(){
//     log_error("ERROR");
//     log_fatal("VERY IMPORTANT");
// }