#ifndef TIMERQUEUE_H
#define TIMERQUEUE_H

#include <time.h>
#include <arpa/inet.h>
#include <assert.h> 

#include <queue>
#include <unordered_map>
#include <algorithm>
#include <functional> 
#include <chrono>

using TimeoutCallback = std::function<void()>;
using Clock = std::chrono::high_resolution_clock ;
using MS = std::chrono::milliseconds ;
using TimeStamp = Clock::time_point;

struct Timer {
    int id;
    TimeStamp expires;
    TimeoutCallback cb;
    bool operator<(const Timer& rhs) {
        return expires < rhs.expires;
    }
};

class TimerQueue {
public:
    TimerQueue() { heap_.reserve(64); }
    ~TimerQueue() { clear(); }
    
    void add(int id, int timeOut, const TimeoutCallback& cb);
    void adjust(int id, int newExpires);

    void doWork(int id);

    void clear();
    void pop();

    void tick();
    int getNextTick();

private:
    void del(size_t i);

    void swap_nodes(size_t i, size_t j);
    bool down(size_t i);
    void up(size_t i);

    std::vector<Timer> heap_;
    std::unordered_map<int, size_t> ref_;   // id->数组下标
};

#endif // TIMERQUEUE_H