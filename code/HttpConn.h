#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/uio.h>

#include <vector>
#include <map>

#include "Buffer.h"
// #include "HttpRequest.h"
#include "HttpResponse.h"
using std::string;

class HttpConn {
 public:
	 // HTTP请求方法，这里只支持GET
    enum METHOD { GET, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };

	// 解析客户端请求时，主状态机的状态
	// CHECK_STATE_REQUESTLINE:当前正在分析请求行
	// CHECK_STATE_HEADER:当前正在分析头部字段
	// CHECK_STATE_CONTENT:当前正在解析请求体
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
    // 从状态机的三种可能状态，即行的读取状态，分别表示
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum LINE_STATUS { LINE_OK, LINE_BAD, LINE_OPEN };

	// 服务器处理HTTP请求的可能结果，报文解析的结果
	// NO_REQUEST          :   请求不完整，需要继续读取客户数据
	// GET_REQUEST         :   表示获得了一个完成的客户请求
	// BAD_REQUEST         :   表示客户请求语法错误
	// NO_RESOURCE         :   表示服务器没有资源
	// FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
	// FILE_REQUEST        :   文件请求,获取文件成功
	// INTERNAL_ERROR      :   表示服务器内部错误
	// CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST,
                     FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };

 public:
	HttpConn()
        : cfd_(-1),
          read_idx_(0),
        //   write_idx_(0),
          start_line_(0),
          checked_idx_(0),
          method_(GET),
          linger_(false),
          state_(CHECK_STATE_REQUESTLINE),
          write_buf_(WRITE_BUFFER_SIZE)
    {
        read_buf_.resize(READ_BUFFER_SIZE);
        // write_buf_.resize(WRITE_BUFFER_SIZE);
    }
	~HttpConn() = default;

    void init(int cfd, const sockaddr_in &addr);
    void process();
    void close_conn();
    bool read();
    bool write();

    static int epfd_;
    // static int user_counter_;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 2048;

 private:
    HTTP_CODE parseRequest();
    HTTP_CODE parseRequestLine(const char *text); // 解析请求行
    HTTP_CODE parseHeader(const char *text);
    // HTTP_CODE parseHeader(char *text);      // 解析请求头
    HTTP_CODE parseContent(char *text);     // 解析请求体

    LINE_STATUS parseLine();
    // char* getLine() { return read_buf_ + start_line_; }
    char* getLine() { return read_buf_.data() + start_line_; }

    HTTP_CODE handleRequest();
    // void unmap();

    bool generateResponse(HTTP_CODE read_ret);
    
    void reset();

 private:
    int cfd_;
    sockaddr_in addr_;
    
    // char read_buf_[READ_BUFFER_SIZE];
    std::vector<char> read_buf_;
    int read_idx_;                      // 下一次读开始的索引
    // std::vector<char> write_buf_;
    // int write_idx_;

    int start_line_;
    int checked_idx_;
    CHECK_STATE state_;
    LINE_STATUS line_status_;
    
    METHOD method_;
    // char *url_;
    // char *version_;
    std::map<string, string> headers_;

    bool linger_;

    string real_file_;
    string path_;
    string ver_;
    // struct stat file_stat_;
    // char *file_addr_;

    Buffer write_buf_;
    HttpResponse response_;

    // struct iovec iov_[2];
    // int iov_count_;
};

#endif