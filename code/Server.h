#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <unistd.h>

#include <memory>

#include "ThreadPool.h"
#include "HttpConn.h"
#include "Epoller.h"
#include "timer/HeapTimer.h"
#include "log/Log.h"
// #include "EventLoop.h"

template <typename T>
class Server {
 public:
    Server(int port, int timeoutMS = 60000, int threadnum = 4,
           int logLevel = 1, int logQueSize = 1024);
    ~Server();
    void start();

 private:
    bool initSocket(); 
    void InitEventMode_(int trigMode);
    void AddClient(int fd, sockaddr_in addr);
  
    void handleListen();
    void handleWrite(HttpConn* client);
    void handleRead(HttpConn* client);

    void SendError(int fd, const char*info);
    void ExtentTime(HttpConn* client);
    void CloseConn(HttpConn* client);

    void onRead(HttpConn* client);
    void onWrite(HttpConn* client);
    void onProcess(HttpConn* client);

    bool setSockPortReusable(int sfd);
    bool setLinger(int fd, int sec);
    void setFdNonblocking(int fd);

    static const int MAX_FD = 65536;
    static const int LISTEN_BACKLOG = 5;
    // EventLoop *mainloop;

    int port_;
    struct sockaddr_in addr_;
    bool isClosed_;
    int listenFd_;
    int timeoutMS_;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool<T>> pool_;
    std::unique_ptr<Epoller> epoller_;
};

#endif