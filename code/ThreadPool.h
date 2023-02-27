#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <exception>
#include "Locker.h"

template <typename T>
class ThreadPool {
 public:
    ThreadPool(int num_worker = 4, int max_tasks = 10000);
    ~ThreadPool();
    bool enqueue(T *);

 private:
    static void *work(void *);
    void run();

    int num_workers_;
    pthread_t *workers_;

    int max_tasks_;
    std::queue<T*> tasks_;

    Locker queuelocker_;
    Sem full_;

    bool stop_;
};

template <typename T>
ThreadPool<T>::ThreadPool(int num_worker, int max_tasks)
    : num_workers_(num_worker), max_tasks_(max_tasks),
      stop_(false), workers_(nullptr)
{
    if (num_worker < 0 || max_tasks < 0)
        throw std::exception();

    workers_ = new pthread_t[max_tasks_];
    
    for (int i = 0; i < num_workers_; ++i) {
        printf("creating %dth thread...\n", i);
        if (pthread_create(workers_ + i, NULL, work, this)) {
            delete [] workers_;
            throw std::exception();
        }
        if (pthread_detach(workers_[i])) {
            delete [] workers_;
            throw std::exception();
        }
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool() {
    delete [] workers_;
    stop_ = true;
}

template <typename T>
bool ThreadPool<T>::enqueue(T *task) {
    queuelocker_.lock();
    if (tasks_.size() > max_tasks_) {
        queuelocker_.unlock();
        return false;
    }
    tasks_.push(task);
    queuelocker_.unlock();
    full_.post();
    return true;
}

template <typename T>
void *ThreadPool<T>::work(void *arg) {
    ThreadPool<T> *pool = (ThreadPool<T> *)arg;
    pool->run();
    return pool;
}

template <typename T>
void ThreadPool<T>::run() {
    while (!stop_) {
        full_.wait();
        queuelocker_.lock();
        if (tasks_.empty()) {
            queuelocker_.unlock();
            continue;
        }
        T *task = tasks_.front();
        tasks_.pop();
        queuelocker_.unlock();
        if (!task)
            continue;
        task->process();
    }
}

#endif