#ifndef _LOGGER_H_
#define _LOGGER_H_

#define log_debug(format,...) \
    Logger::GetInstance().log(Logger::DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__);

#define log_info(format,...) \
    Logger::GetInstance().log(Logger::INFO, __FILE__, __LINE__, format, ##__VA_ARGS__);

#define log_warn(format,...) \
    Logger::GetInstance().log(Logger::WARN, __FILE__, __LINE__, format, ##__VA_ARGS__);

#define log_error(format,...) \
    Logger::GetInstance().log(Logger::ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__);

#define log_fatal(format,...) \
    Logger::GetInstance().log(Logger::FATAL, __FILE__, __LINE__,format, ##__VA_ARGS__);

class Logger{
public:
    //日志级别
    enum Level
    {
        DEBUG = 0,
        INFO,
        WARN,
        ERROR,
        FATAL,
        LEVEL_COUNT  //记录日志级别个数  
    };

    //创建Logger对象
    static Logger &GetInstance();
    //打印日志
    void log(Level level,const char *fileName,int line,const char *format,...);
    //设置日志级别
    void setLevel(Level level)
    {
        levels=level;
    }

private:
    Logger();
    ~Logger();

private:
    //当前日志级别(用于过滤低级别日志内容)
    Level levels = DEBUG;
};
#endif //LOGGER_H
