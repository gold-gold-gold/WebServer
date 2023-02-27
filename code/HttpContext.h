#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
// #include "Buffer.h"
#include "HttpRequest.h"

using std::string;

class HttpContext {
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
    HTTP_CODE parseRequest();
    HTTP_CODE parseRequestLine(const char *text); // 解析请求行
    HTTP_CODE parseHeader(const char *begin);
    HTTP_CODE parseContent(const char *text);     // 解析请求体

    LINE_STATUS parseLine();
    char* getLine() { return read_buf_.data() + start_line_; }
    
 private:
    // Buffer context_;
    std::vector<char> read_buf_;
    int read_idx_;                      // 下一次读开始的索引
    // METHOD method_;
    
    int start_line_;
    int checked_idx_;
    CHECK_STATE state_;
    LINE_STATUS line_status_;

    HttpRequest request_;
//     string path_;
//     string ver_;
//     std::map<string, string> headers_;
};

HttpContext::HTTP_CODE HttpContext::parseRequest() {
    LINE_STATUS stat = LINE_BAD;
    HTTP_CODE ret = NO_REQUEST;

    char *text = 0;

    // while ( (state_ == CHECK_STATE_CONTENT) && (line_status_ == LINE_OK)
    //         || (line_status_ = parseLine()) == LINE_OK )
    // {
    while (true) {
        line_status_ = parseLine();
        printf("line_status_ = %d\n", line_status_);
        if (line_status_ != LINE_OK) break;

        // 获取一行数据
        text = getLine();
        start_line_ = checked_idx_;
        printf("got 1 http line : %s\n", text);

        switch (state_) {
        case CHECK_STATE_REQUESTLINE:
            ret = parseRequestLine(text);
            if (ret == BAD_REQUEST) {
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            ret = parseHeader(text);
            if (ret == BAD_REQUEST) {
                return BAD_REQUEST;
            } else if (ret == GET_REQUEST) {
                // return handleRequest();
            }
            break;
        case CHECK_STATE_CONTENT:
            ret = parseContent(text);
            if (ret == GET_REQUEST) {
                // return handleRequest();
            }
            line_status_ = LINE_OPEN;
            break;    
        default:
            return INTERNAL_ERROR;
            break;
        }
    }

    return NO_REQUEST;
} 

// 解析一行：将\r\n更改为\0\0
HttpContext::LINE_STATUS HttpContext::parseLine() {
    char temp;
    for ( ; checked_idx_ < read_idx_; checked_idx_++) {
        temp = read_buf_[checked_idx_];
        if (temp == '\r') {
            if (checked_idx_ + 1 == read_idx_) {    // \r为当前读到的数据末尾
                return LINE_OPEN;
            } else if (read_buf_[checked_idx_+1] == '\n') {
                // 将\r\n置为\0\0，便于读取文本
                read_buf_[checked_idx_++] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if (checked_idx_ > 1 && read_buf_[checked_idx_-1] == '\r') {
                read_buf_[checked_idx_-1] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 解析请求行，获得请求方法、URL、版本
HttpContext::HTTP_CODE HttpContext::parseRequestLine(const char *text) {
    // GET /index.html HTTP/1.1
    string line(text, text + strlen(text));

    size_t space1 = line.find(' ');
    if (space1 == string::npos)
        return BAD_REQUEST;

    string method(line.begin(), line.begin() + space1);
    if (!request_.setMethod(method))
        return BAD_REQUEST;
    
    size_t start = space1 + 1;
    size_t space2 = line.find(' ', start);
    if (space2 == string::npos)
        return BAD_REQUEST;
    
    request_.setPath(line, start, space2 - start);

    // 如果url格式为http://192.168.1.1:1000/index.html，忽略ip地址
    if (request_.path().compare(0, 7, "http://") == 0) {
        size_t found = request_.path().find('/', 7);
        if (found == string::npos)
            return BAD_REQUEST;
        request_.erasePath(0, found);
    }

    start = space2 + 1;
    request_.setVersion(line, start, line.size() - start);

    state_ = CHECK_STATE_HEADER;
    printf("method: %s\nurl: %s\nversion: %s\n",
        method.c_str(), request_.path().c_str(), request_.version().c_str());
    printf("request line has been parsed\n\n");

    return NO_REQUEST;
}

HttpContext::HTTP_CODE HttpContext::parseHeader(const char *text) {
    string line(text, text + strlen(text));

    size_t colon = line.find(':');
    if (colon == string::npos)
        return BAD_REQUEST;
    
    string key(text, colon);

    size_t start = colon + 1;
    size_t end = strlen(text);
    while (start < end && isspace(line[start])) {
        ++start;
    }
    if (start >= end)
        return BAD_REQUEST;
    
    string value(start, end);
    request_.addHeader(key, value);

    return NO_REQUEST;
}

HttpContext::HTTP_CODE HttpContext::parseContent(const char *text) {
    // todo
}