#ifndef BLOCKINGQUEUE_H
#define BLOCKINGQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

template<class T>
class BlockingQueue {
public:
    explicit BlockingQueue(size_t MaxCapacity = 1000);

    ~BlockingQueue();

    void clear();
    void close();

    bool empty();
    bool full();

    size_t size();
    size_t capacity();

    T front();
    T back();

    void push(const T &item);
    bool pop(T &item);
    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;

    size_t capacity_;

    bool is_closed_;

    std::mutex mutex_;
    std::condition_variable cond_consumer_;
    std::condition_variable cond_producer_;
};


template<class T>
BlockingQueue<T>::BlockingQueue(size_t MaxCapacity): capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    is_closed_ = false;
}

template<class T>
BlockingQueue<T>::~BlockingQueue() {
    close();
}

template<class T>
void BlockingQueue<T>::close() {
    {   
        std::lock_guard<std::mutex> locker(mutex_);
        deq_.clear();
        is_closed_ = true;
    }
    cond_producer_.notify_all();
    cond_consumer_.notify_all();
}

template<class T>
void BlockingQueue<T>::flush() {
    cond_consumer_.notify_one();
}

template<class T>
void BlockingQueue<T>::clear() {
    std::lock_guard<std::mutex> locker(mutex_);
    deq_.clear();
}

template<class T>
T BlockingQueue<T>::front() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.front();
}

template<class T>
T BlockingQueue<T>::back() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.back();
}

template<class T>
size_t BlockingQueue<T>::size() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.size();
}

template<class T>
size_t BlockingQueue<T>::capacity() {
    std::lock_guard<std::mutex> locker(mutex_);
    return capacity_;
}

template<class T>
bool BlockingQueue<T>::empty() {
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.empty();
}

template<class T>
bool BlockingQueue<T>::full(){
    std::lock_guard<std::mutex> locker(mutex_);
    return deq_.size() >= capacity_;
}

template<class T>
void BlockingQueue<T>::push(const T &item) {
    std::unique_lock<std::mutex> locker(mutex_);
    while (deq_.size() >= capacity_) {
        cond_producer_.wait(locker);
    }
    deq_.push_back(item);
    cond_consumer_.notify_one();
}

template<class T>
bool BlockingQueue<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mutex_);
    while (deq_.empty()) {
        cond_consumer_.wait(locker);
        if (is_closed_) {
            return false;
        }
    }
    assert(!deq_.empty());
    item = deq_.front();
    deq_.pop_front();
    cond_producer_.notify_one();
    return true;
}

template<class T>
bool BlockingQueue<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mutex_);
    while (deq_.empty()) {
        if (cond_consumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout) {
            return false;
        }
        if (is_closed_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    cond_producer_.notify_one();
    return true;
}

#endif // BLOCKINGQUEUE_H