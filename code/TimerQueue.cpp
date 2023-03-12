#include "TimerQueue.h"

void TimerQueue::swap_nodes(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());

    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
} 

bool TimerQueue::down(size_t i) {
    // 向下调整堆
    assert(i >= 0 && i < heap_.size());

    size_t t = i, l = t * 2 + 1; 
    while (l < heap_.size()) {
        if (heap_[t] < heap_[l]) {  // 与左孩子比较
            l = t;
        }
        if (l + 1 < heap_.size() && heap_[l + 1] < heap_[l]) {  // 与右孩子比较
            l = l + 1;
        }
        if (l == t) {
            break;
        }
        swap_nodes(l, t);
        t = l;
        l = 2 * t + 1;
    }
    return t != i;
}

void TimerQueue::up(size_t i) {
    // 向上调整堆
    size_t t = i / 2;
    while (t >= 0 && heap_[t] < heap_[i]) {
        swap_nodes(t, i);
        i = t;
        t = i / 2;
    }
}

void TimerQueue::add(int id, int timeout, const TimeoutCallback& cb) {
    assert(id >= 0);

    size_t i;
    if (ref_.count(id)) {   // 修改原有结点
        i = ref_[id];
        Timer node = heap_[i];
        node.expires = Clock::now() + MS(timeout);
        node.cb = cb;
        if (!down(i)) { // 向上或向下调整
            up(i);
        }
    } 
    else {  // 增加结点
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        up(i);
    }
}

void TimerQueue::doWork(int id) {
    // 执行指定结点的回调函数，并删除该结点 
    if (heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    Timer node = heap_[i];
    node.cb();
    del(i);
}

void TimerQueue::del(size_t i) {
    // 删除堆中指定下标i的结点
    assert(!heap_.empty() && i >= 0 && i < heap_.size());

    size_t t = heap_.size() - 1;
    if (i < t) {
        swap_nodes(i, t);
        if (!down(i)) {
            up(i);
        }
    }
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void TimerQueue::adjust(int id, int timeout) {
    // 调整指定id的结点
    assert(!heap_.empty() && ref_.count(id) > 0);

    int i = ref_[id];
    heap_[i].expires = Clock::now() + MS(timeout);
    if(!down(i)) {
        up(i);
    }
}

void TimerQueue::tick() {
    // 处理所有到期的任务
    if (heap_.empty()) {
        return;
    }
    while (!heap_.empty()) {
        Timer node = heap_.front();
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();
        pop();
    }
}

void TimerQueue::pop() {
    assert(!heap_.empty());
    del(0);
}

void TimerQueue::clear() {
    ref_.clear();
    heap_.clear();
}

int TimerQueue::getNextTick() {
    tick();
    size_t res = -1;
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0)
            res = 0;
    }
    return res;
}