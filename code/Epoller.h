#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

class Epoller {
 public:
    Epoller(int maxEvents = 1024) : epfd_(epoll_create1(0)), events_(maxEvents) {}

    ~Epoller() { close(epfd_); }

    void AddFd(int fd, uint32_t events) {
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = events; // EPOLLIN | EPOLLRDHUP /*| EPOLLET*/;

        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    void DelFd(int fd, uint32_t events) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, 0);
        close(fd);
    }

    void ModFd(int fd, uint32_t events) {
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = events; // | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
        epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
    }

    int wait(int timeoutMs) {
        return epoll_wait(epfd_, events_.data(), events_.size(), timeoutMs);
    }

 private:
    using EventList = std::vector<epoll_event>;

    int epfd_;
    EventList events_;
};

#endif // EPOLLER_H