#include "Log.h"

using namespace std;

Log::Log() {
    lineCount_ = 0;
    write_thread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log::~Log() {
    if (write_thread_ && write_thread_->joinable()) {
        while(!deque_->empty()) {
            deque_->flush();
        };
        deque_->close();
        write_thread_->join();
    }
    if (fp_) {
        lock_guard<mutex> locker(mutex_);
        flush();
        fclose(fp_);
    }
}

int Log::getLevel() {
    lock_guard<mutex> locker(mutex_);
    return level_;
}

void Log::setLevel(int level) {
    lock_guard<mutex> locker(mutex_);
    level_ = level;
}

void Log::init(int level = 1, const char* path,
               const char* suffix, int maxQueueSize) {
    is_opened_ = true;
    level_ = level;
    if (maxQueueSize > 0) {
        is_async_ = true;
        if (!deque_) {
            unique_ptr<BlockingQueue<std::string>> newDeque(new BlockingQueue<std::string>);
            deque_ = move(newDeque);
            
            std::unique_ptr<std::thread> NewThread(new thread(flushLogThread));
            write_thread_ = move(NewThread);
        }
    } else {
        is_async_ = false;
    }

    lineCount_ = 0;

    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;

    {
        lock_guard<mutex> locker(mutex_);
        buff_.retrieveAll();
        if (fp_) { 
            flush();
            fclose(fp_); 
        }

        fp_ = fopen(fileName, "a");
        if (fp_ == nullptr) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        } 
        assert(fp_ != nullptr);
    }
}

void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    // 日志日期，行数
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mutex_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday) {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        unique_lock<mutex> locker(mutex_);
        lineCount_++;
        int n = snprintf(buff_.writeBegin(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff_.hasWritten(n);
        appendLogPrefix_(level);

        va_start(vaList, format);
        int m = vsnprintf(buff_.writeBegin(), buff_.writable(), format, vaList);
        va_end(vaList);

        buff_.hasWritten(m);
        buff_.append("\n\0");

        if (is_async_ && deque_ && !deque_->full()) {
            deque_->push(buff_.retrieveAllToStr());
        } else {
            fputs(buff_.readBegin(), fp_);
        }
        buff_.retrieveAll();
    }
}

void Log::appendLogPrefix_(int level) {
    switch (level) {
    case 0:
        buff_.append("[debug]: ");
        break;
    case 1:
        buff_.append("[info]: ");
        break;
    case 2:
        buff_.append("[warn] : ");
        break;
    case 3:
        buff_.append("[error]: ");
        break;
    default:
        buff_.append("[info] : ");
        break;
    }
}

void Log::flush() {
    if(is_async_) { 
        deque_->flush(); 
    }
    fflush(fp_);
}

void Log::AsyncWrite_() {
    string str = "";
    while (deque_->pop(str)) {
        lock_guard<mutex> locker(mutex_);
        fputs(str.c_str(), fp_);
    }
}

Log* Log::getInstance() {
    static Log inst;
    return &inst;
}

void Log::flushLogThread() {
    Log::getInstance()->AsyncWrite_();
}