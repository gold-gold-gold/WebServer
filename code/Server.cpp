#include "Server.h"
#include <fcntl.h>

template <typename T>
Server<T>::Server(int port, int timeout, int threadnum, int logLevel, int logQueSize)
    : port_(port), timeoutMS_(timeout), isClosed_(false),
      timer_(new HeapTimer()), pool_(new ThreadPool<T>()), epoller_(new Epoller())
{
    listenEvent_ = EPOLLRDHUP | EPOLLET;
    connEvent_ =  EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
    initSocket();

    Log::GetInstance()->init(logLevel, "./log", ".log", logQueSize);
    if(isClosed_) { LOG_ERROR("========== Server init error!=========="); }
    else {
        LOG_INFO("========== Server init ==========");
        LOG_INFO("Port:%d", port_);
        // LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger ? "true":"false");
        // LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
        //                 (listenEvent_ & EPOLLET ? "ET": "LT"),
        //                 (connEvent_ & EPOLLET ? "ET": "LT"));
        LOG_INFO("LogSys level: %d", logLevel);
        LOG_INFO("srcDir: %s", HttpConn::srcDir);
        // LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        LOG_INFO("ThreadPool num: %d", threadNum);
    }
}

template <typename T>
Server<T>::~Server() {
    close(listenFd_);
}

template <typename T>
void Server<T>::start() {
    int timeMS = -1;  // epoll_wait timeout == -1 无事件将阻塞
    if (!isClosed_) { LOG_INFO("========== Server start =========="); }
    while (!isClosed_) {
        if (timeoutMS_ > 0) {
            timeMS = timer_->getNextTick();
        }
        int nready = epoller_->wait(timeMS);
        for (int i = 0; i < nready; ++i) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if (fd == listenFd_) {
                handleListen();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn(&users_[fd]);
            } else if (events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                handleRead(&users_[fd]);
            } else if (events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                handleWrite(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

template <typename T>
void Server<T>::SendError(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);

}

template <typename T>
void Server<T>::CloseConn(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

template <typename T>
void Server<T>::AddClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&Server<T>::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

template <typename T>
void Server<T>::handleListen() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;}
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

template <typename T>
void Server<T>::handleRead(HttpConn* client) {
    assert(client);
    ExtentTime(client);
    threadpool_->AddTask(std::bind(&Server<T>::onRead, this, client));
}

template <typename T>
void Server<T>::handleWrite(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&Server<T>::onWrite, this, client));
}

template <typename T>
void Server<T>::ExtentTime(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

template <typename T>
void Server<T>::onRead(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}
template <typename T>
void Server<T>::onProcess(HttpConn* client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

template <typename T>
void Server<T>::onWrite(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->getFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

template <typename T>
bool Server<T>::initSocket() {
    int ret = 0;
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setLinger(listenFd_, 1);
    if (ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    ret = setSockPortReusable(listenFd_);
    if (ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret == -1) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if(ret == -1) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }

    setFdNonblocking(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

template <typename T>
bool Server<T>::setSockPortReusable(int fd) {
    int optval = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

template <typename T>
void Server<T>::setFdNonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);
}

// 优雅关闭: 直到所剩数据发送完毕或超时
template <typename T>
bool Server<T>::setLinger(int fd, int sec) {
    struct linger optLinger;
    optLinger.l_onoff = 1;
    optLinger.l_linger = sec;
    return setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
}