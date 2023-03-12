#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/stat.h>
#include "BlockingQueue.h"
#include "../buffer/Buffer.h"
// #include "../Buffer.h"

class Log {
public:
    void init(int level, const char* path = "../log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* getInstance();
    static void flushLogThread();

    void write(int level, const char *format, ...);
    void flush();

    int getLevel();
    void setLevel(int level);
    bool isOpen() { return is_opened_; }
    
private:
    Log();
    virtual ~Log();
    void appendLogPrefix_(int level);
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char* path_;
    const char* suffix_;

    int MAX_LINES_;

    int lineCount_;
    int toDay_;
    int level_;

    bool is_opened_;
    bool is_async_;

    Buffer buff_;

    FILE* fp_;
    std::unique_ptr<BlockingQueue<std::string>> deque_; 
    std::unique_ptr<std::thread> write_thread_;
    std::mutex mutex_;
};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::getInstance();\
        if (log->isOpen() && log->getLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while (0)

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__);} while (0)
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__);} while (0)
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__);} while (0)
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__);} while (0)

#endif // LOG_H