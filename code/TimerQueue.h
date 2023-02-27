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

#include "../log/Log.h"

using TimeoutCallback = std::function<void()>;
using Clock = std::chrono::high_resolution_clock ;
using MS = std::chrono::milliseconds ;
using TimeStamp = Clock::time_point;

struct Timer {
    int id;
    TimeStamp expires;
    TimeoutCallback cb;
    bool operator<(const Timer& t) {
        return expires < t.expires;
    }
};

class TimerQueue {
public:
    TimerQueue() { heap_.reserve(64); }

    ~TimerQueue() { clear(); }
    
    void adjust(int id, int newExpires);

    void add(int id, int timeOut, const TimeoutCallback& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int getNextTick();

private:
    void del(size_t i);
    
    void siftup(size_t i);

    bool siftdown(size_t index, size_t n);

    void SwapNode(size_t i, size_t j);

    std::vector<Timer> heap_;
    std::unordered_map<int, size_t> ref_;
};

#endif // TIMERQUEUE_H