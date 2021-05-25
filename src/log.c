#include "log.h"



/*
锁
*/
static struct logging {
  void *udata;
  log_LockFn lock;
  int level;
  bool quiet;
} LOGGING, FILE_LOGGING;

static int has_init_file = 0;

static void lock(void) {
  if (LOGGING.lock) {
    LOGGING.lock(true, LOGGING.udata);
  }
}

static void unlock(void) {
  if (LOGGING.lock) {
    LOGGING.lock(false, LOGGING.udata);
  }
}
static void lock_file(void) {
  if (FILE_LOGGING.lock) {
    FILE_LOGGING.lock(true, FILE_LOGGING.udata);
  }
}
static void unlock_file(void) {
  if (FILE_LOGGING.lock) {
    FILE_LOGGING.lock(false, FILE_LOGGING.udata);
  }
}
static void init_event(log_Event *ev, void *udata) { //配置时间和流
  if (!ev->time) {
    time_t t=time(NULL);
    ev->time = localtime(&t);
  }
  ev->udata = udata;
}
static void stdout_callback(log_Event *ev) { //向控制台写
  static char buf[64];
  buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] =
      '\0'; //把日期写入buffer
  // #if defined(_WIN64) || defined( _WIN32)
  // fprintf(ev->udata, "%s %-5s %s:%d: ", buf,
  //         LOG_LEVEL_STRING[ev->level], ev->file,
  //         ev->line);
  // // SetConsoleTextAttribute(handle, 0x7);
  // #elif __linux
  fprintf(ev->udata, "%s \x1b[1m%s%-5s %s:%d:\x1b[0m ", buf,
          LOG_LEVEL_COLOR[ev->level], LOG_LEVEL_STRING[ev->level], ev->file,
          ev->line);
  // #endif
  vfprintf(ev->udata, ev->fmt, ev->ap); 
  fprintf(ev->udata, "\n");
  fflush(ev->udata);
}

static void file_callback(log_Event *ev) { //向文件写
  char buf[64];
  buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
  fprintf(ev->udata, "%s %-5s %s:%d: ", buf, LOG_LEVEL_STRING[ev->level],
          ev->file, ev->line);
  vfprintf(ev->udata, ev->fmt, ev->ap);
  fprintf(ev->udata, "\n");
  fflush(ev->udata);
}

/*
外部接口
*/
inline static void set_lock(struct logging *log, log_LockFn fn, void *udata) {
  log->lock = fn;
  log->udata = udata;
}
inline static void set_level(struct logging *log, int level) {
  log->level = level;
}
inline static void set_quiet(struct logging *log, bool enable) {
  log->quiet = enable;
}

void log_set_lock(log_LockFn fn, void *udata) { set_lock(&LOGGING, fn, udata); }

void log_set_level(int level) { set_level(&LOGGING, level); }

void log_set_quiet(bool enable) { set_quiet(&LOGGING, enable); }

void flog_set_lock(log_LockFn fn, void *udata) {
  set_lock(&FILE_LOGGING, fn, udata);
}
void flog_set_level(int level) { set_level(&FILE_LOGGING, level); }

void flog_set_quiet(bool enable) { set_quiet(&FILE_LOGGING, enable); }
bool filelog_init(const char *filename) {
  FILE *fp;
  char error_buffer[36];
  if (fopen_s(&fp,"log.log", "w+")!=0){
      log_error("log file open error:%s",strerror_s(error_buffer,36,errno));
      return false;
    }
  FILE_LOGGING.udata = fp;
  has_init_file = 1;
  return true;
}
void filelog_close() {
  if (FILE_LOGGING.udata) {
    fclose(FILE_LOGGING.udata);
  }
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {
  log_Event ev = {
      .fmt = fmt,
      .file = file,
      .line = line,
      .level = level,
  };
  lock();
  if (!LOGGING.quiet && level >= LOGGING.level) { //如果非静默 且等级高于阈值
    init_event(&ev, stderr);
    va_start(ev.ap, fmt); //按照格式保存参数到ev.ap
    stdout_callback(&ev); //执行log
    va_end(ev.ap);
  }
  unlock();
  if (has_init_file == 1) {
    lock_file();
    if (!FILE_LOGGING.quiet && level >= FILE_LOGGING.level) {
      ev.udata = FILE_LOGGING.udata;
      file_callback(&ev); //执行log
    }
    unlock_file();
  }
}