#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <semaphore.h>

class Locker {
 public:
    Locker() {
        pthread_mutex_init(&mutex_, NULL);
    }

    ~Locker() {
        pthread_mutex_destroy(&mutex_);
    }

    bool lock() {
        return pthread_mutex_lock(&mutex_) == 0;
    }

    bool unlock() {
        return pthread_mutex_unlock(&mutex_) == 0;
    }

    pthread_mutex_t *get() {
        return &mutex_;
    }

 private:
    pthread_mutex_t mutex_;
};

class Cond {
 public:
    Cond() {
        pthread_cond_init(&cond_, NULL);
    }

    ~Cond() {
        pthread_cond_destroy(&cond_);
    }

    bool signal() {
        return pthread_cond_signal(&cond_) == 0;
    }

    bool broadcast() {
        return pthread_cond_broadcast(&cond_) == 0;
    }

    bool wait(pthread_mutex_t *mutex) {
        return pthread_cond_wait(&cond_, mutex) == 0;
    }

    bool timewait(pthread_mutex_t *mutex, const struct timespec *time) {
        return pthread_cond_timedwait(&cond_, mutex, time) == 0;
    }

 private:
    pthread_cond_t cond_;
};

class Sem {
 public:
    Sem() {
        sem_init(&sem_, 0, 0);
    }

    ~Sem() {
        sem_destroy(&sem_);
    }

    bool wait() {
        return sem_wait(&sem_) == 0;
    }

    bool post() {
        return sem_post(&sem_) == 0;
    }

 private:
    sem_t sem_;
};

#endif