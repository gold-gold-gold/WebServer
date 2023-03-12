#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <iostream>
#include <map>

// #include "Locker.h"
#include "ThreadPool.h"
#include "HttpConn.h"
#include "log/Log.h"
#include "TimerQueue.h"

#define handle_error(msg) \
        do {perror(msg); exit(EXIT_FAILURE);} while (0)

const int MAX_FDS = 65535;
const int LISTEN_BACKLOG = 5;
const int MAX_EVENTS = 10;
const int TIME_OUT = 60000; // ms

extern void epoll_add(int epfd, int sfd, bool oneshot);
extern void epoll_del(int epfd, int sfd);
extern void epoll_mod(int epfd, int sfd, uint32_t events);
extern bool setSockPortReusable(int);

int main(int argc, char *argv[]) {
    int lfd, cfd, epfd, curfd, nready;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in my_addr, client_addr;
    socklen_t client_addr_len;

    if (argc <= 1) {
        fprintf(stderr, "Usage: %s port_number\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);

    signal(SIGPIPE, SIG_IGN);

    // 实例化Log类
    Log::getInstance()->init(1);

    // 创建定时器队列
    TimerQueue timer_queue;

    // 创建线程池
    ThreadPool<HttpConn> *pool = nullptr;
    try {
        pool = new ThreadPool<HttpConn>;
    } catch (...) {
        exit(EXIT_FAILURE);
    }

    // 保存客户信息
    // HttpConn *users = new HttpConn[MAX_FDS];
    std::map<int, HttpConn> conns;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    // if (lfd < 0)
    //     handle_error("socket");
    setSockPortReusable(lfd);

    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(port);

    bind(lfd, (struct sockaddr *)&my_addr, sizeof(my_addr));

    listen(lfd, LISTEN_BACKLOG);

    epfd = epoll_create1(0);
    
    epoll_add(epfd, lfd, false);
    HttpConn::epfd_ = epfd;

    LOG_INFO("========== Server start ==========");

    for ( ; ; ) {
        int wait_time = timer_queue.getNextTick();
        nready = epoll_wait(epfd, events, MAX_EVENTS, wait_time);

        for (int i = 0; i < nready; ++i) {
            curfd = events[i].data.fd;
            // printf("curfd = %d\n", curfd);
            if (curfd == lfd) {
                client_addr_len = sizeof(client_addr);
                cfd = accept(lfd, (sockaddr *)&client_addr, &client_addr_len);
                // if (cfd == -1) perror("accept");
                if (conns.size() >= MAX_FDS) {
                    close(cfd);
                    continue;
                }
                conns[cfd].init(cfd, client_addr);
                // printf("cfd = %d\n", cfd);
                // 添加定时器
                timer_queue.add(cfd, TIME_OUT, std::bind(&HttpConn::close_conn, &conns[cfd]));
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                conns[curfd].close_conn();
            }
            else if (events[i].events & EPOLLIN) {
                if (conns[curfd].read()) {
                    timer_queue.adjust(cfd, TIME_OUT);
                    pool->enqueue(&conns[curfd]);
                }
                else {
                    conns[curfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT) { 

                if (!conns[curfd].write())
                    conns[curfd].close_conn();

            }
        }  

    }

    return 0;
}