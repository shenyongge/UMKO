#include "logger.h"
#include <stdio.h>
#include <stdarg.h>

const char *level_map[Logger::LEVEL_COUNT] = {
    "DEBUG",
    " INFO",
    " WARN",
    "ERROR",
    "FATAL"
};

Logger &Logger::GetInstance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
}

Logger::~Logger()
{
}

//return filename or last 'width' name.
const char *process_name(const char *filepath, int width)
{
    int last_slash = 0;
    int i = 0;
    for(;i < 1024; i++) {
        if (filepath[i] == 0) {
            break;
        }
        if (filepath[i] == '/') {
            last_slash = i;
        }
    }
    if (i - last_slash > width) {
        return filepath + (i - width);
    } else {
        return filepath + last_slash + 1;
    }
}

void Logger::log(Level level, const char *fileName, int line, const char *format,...)
{
    //过滤低级别日志
    if(levels > level)
    {
        return;
    }

    printf("%15s:%3d:%s:",process_name(fileName, 15), line, level_map[level]);
    //获取写入日志内容
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
