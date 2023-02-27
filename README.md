# WebServer
C++ based tiny web server on Linux

## 功能
* 采用I/O多路复用与线程池结合的多线程服务端编程模式
* 实现可自动增长的缓冲区，以适应不同大小的消息
* 通过有限状态机解析HTTP请求报文，实现对静态资源的请求的处理
* 基于优先队列实现的定时器，能够关闭超时的非活动连接
* 使用阻塞队列和单例模式实现异步的日志系统

## 目录树
```
.
├── code
│   ├── buffer
│   │   ├── Buffer.cpp
│   │   └── Buffer.h
│   ├── Buffer.h
│   ├── Epoller.h
│   ├── HttpConn.cpp
│   ├── HttpConn.h
│   ├── HttpContext.h
│   ├── HttpRequest.h
│   ├── HttpResponse.h
│   ├── HttpResponse.txt
│   ├── Locker.h
│   ├── log
│   │   ├── BlockingQueue.h
│   │   ├── Log.cpp
│   │   └── Log.h
│   ├── main
│   ├── main.cpp
│   ├── Server.cpp
│   ├── Server.h
│   ├── ThreadPool.h
│   ├── timer
│   │   ├── HeapTimer.cpp
│   │   └── HeapTimer.h
│   ├── TimerQueue.cpp
│   └── TimerQueue.h
├── log
├── README.md
├── resources
└── webbench-1.5
```