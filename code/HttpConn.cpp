#include "HttpConn.h"

#include <sys/epoll.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <algorithm>
#include <iostream>
using std::cout; using std::endl;

// 定义HTTP响应的一些状态信息
string ok_200_title = "OK";
string error_400_title("Bad Request");
string error_400_form("Your request has bad syntax or is inherently impossible to satisfy.\n");
string error_403_title("Forbidden");
string error_403_form("You do not have permission to get file from this server.\n");
string error_404_title("Not Found");
string error_404_form("The requested file was not found on this server.\n");
string error_500_title("Internal Error");
string error_500_form("There was an unusual problem serving the requested file.\n");


int HttpConn::epfd_ = -1;
// int HttpConn::user_counter_ = 0;
// const char HttpConn::CRLF[] = "\r\n";

string doc_root = "/home/lxy/WebServer/resources";

void epoll_add(int epfd, int sfd, bool oneshot) {
    epoll_event ev;
    ev.data.fd = sfd;
    ev.events = EPOLLIN | EPOLLRDHUP /*| EPOLLET*/;
    if(oneshot)
        ev.events |= EPOLLONESHOT;

    epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);
}

void epoll_del(int epfd, int sfd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, sfd, 0);
    close(sfd);
}

void epoll_mod(int epfd, int sfd, uint32_t events) {
    epoll_event ev;
    ev.data.fd = sfd;
    ev.events = events | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(epfd, EPOLL_CTL_MOD, sfd, &ev);
}

bool setSockPortReusable(int sfd) {
    int optval = 1;
    return setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == 0;
}

void setFdNonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);
}

void HttpConn::init(int cfd, const sockaddr_in &addr) {
    cfd_ = cfd;
    addr_ = addr;

    setSockPortReusable(cfd);
    epoll_add(epfd_, cfd, false);
    setFdNonblocking(cfd);
}

void HttpConn::close_conn() {
    if (cfd_ != -1) {
        epoll_del(epfd_, cfd_);
        close(cfd_);
        cfd_ = -1;
    }
}

bool HttpConn::read() {
    if (read_idx_ >= READ_BUFFER_SIZE) {
        return false;
    }

    int num_read = 0;
    while (true) {
        // num_read = recv(cfd_, read_buf_ + read_idx_, READ_BUFFER_SIZE - read_idx_, 0);
        num_read = recv(cfd_, read_buf_.data() + read_idx_, read_buf_.size() - read_idx_, 0);
        printf("num_read = %d\n", num_read);
        if (num_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // no data
                printf("errno: %d\n", errno);
                break;
            }
            perror("recv");
            return false;
        } else if (num_read == 0) { // connection was closed
            return false;
        }
        read_idx_ += num_read;
    }
    // printf("读取到了数据：%s\n", read_buf_);
    printf("读取到了数据：%s\nover\n", read_buf_.data());

    return true;
}

void HttpConn::process() {
    // 解析HTTP请求
    HTTP_CODE read_ret = parseRequest();
    if (read_ret == NO_REQUEST) { // 请求不完整
        epoll_mod(epfd_, cfd_, EPOLLIN);
        return;
    }

    // auto it = headers_.cbegin();
    // while (it != headers_.cend()) {
    //     cout << it->first << ": "
    //         << it->second << endl;
    //     ++it;
    // }
    // cout << "end" << endl;

    // 生成响应
    cout << "read_Ret = " << read_ret << endl;
    bool write_ret = generateResponse(read_ret);
    cout << "generateResponse over" << endl;
    
    if (!write_ret) {
        close_conn();
    }
    epoll_mod(epfd_, cfd_, EPOLLOUT);
}

// 主状态机，解析请求
HttpConn::HTTP_CODE HttpConn::parseRequest() {
    LINE_STATUS stat = LINE_BAD;
    HTTP_CODE ret = NO_REQUEST;

    char *text = 0;

    // while ( (state_ == CHECK_STATE_CONTENT) && (line_status_ == LINE_OK)
    //         || (line_status_ = parseLine()) == LINE_OK )
    // {
    while (true) {
        line_status_ = parseLine();
        // printf("line_status_ = %d\n", line_status_);
        if (line_status_ != LINE_OK) break;

        // 获取一行数据
        text = getLine();
        start_line_ = checked_idx_;
        // printf("got 1 http line : %s\n", text);

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
                return handleRequest();
            }
            break;
        case CHECK_STATE_CONTENT:
            ret = parseContent(text);
            if (ret == GET_REQUEST) {
                return handleRequest();
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
HttpConn::LINE_STATUS HttpConn::parseLine() {
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
HttpConn::HTTP_CODE HttpConn::parseRequestLine(const char *text) {
    // GET /index.html HTTP/1.1
    string line(text, text + strlen(text));

    size_t space1 = line.find(' ');
    if (space1 == string::npos) {
         return BAD_REQUEST;
    }

    string method(line.begin(), line.begin() + space1);
    if (method == "GET") {
        method_ = GET;
    } else {
        return BAD_REQUEST;
    }

    size_t start = space1 + 1;
    size_t space2 = line.find(' ', start);
    if (space2 == string::npos) {
        return BAD_REQUEST;
    }
    
    path_.assign(line, start, space2 - start);

    // 如果url格式为http://192.168.1.1:1000/index.html，忽略ip地址
    if (path_.compare(0, 7, "http://") == 0) {
        size_t found = path_.find('/', 7);
        if (found == string::npos) {
            return BAD_REQUEST;
        }
        path_.erase(0, found);
    }

    start = space2 + 1;
    ver_.assign(line, start, line.size() - start);

    state_ = CHECK_STATE_HEADER;
    printf("method: %s\nurl: %s\nversion: %s\n",
        method.c_str(), path_.c_str(), ver_.c_str());
    printf("request line has been parsed\n\n");

    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::parseHeader(const char *text) {
    if (*text == '\0') {
        if (headers_.find("Content-Length") != headers_.end()) {
            state_ = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }

    size_t end = strlen(text);
    string line(text, text + end);

    size_t colon = line.find(':');
    if (colon == string::npos) {
        return BAD_REQUEST;
    }
       
    string key(text, colon);

    size_t start = colon + 1;
    while (start < end && isspace(line[start])) {
        ++start;
    }
    if (start >= end) {
        return BAD_REQUEST;
    }

    string value(text, start, end);
    headers_[key] = value;
    if (key == "Connection" && value == "keep-alive") 
        linger_ = true;
    
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::parseContent(char *text) {
    // todo
}

HttpConn::HTTP_CODE HttpConn::handleRequest() {
    real_file_ = doc_root + path_;

    // 获取文件状态信息，-1失败，0成功
    struct stat file_stat;
    if (stat(real_file_.c_str(), &file_stat) == -1) {
        return NO_RESOURCE;
    }

    // 判断访问权限
    if (!(file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }

    // 是否为目录
    if (S_ISDIR(file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    int fd = open(real_file_.c_str(), O_RDONLY);
    char *file_addr = (char *)mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_addr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    response_.setBody(file_addr, file_stat.st_size);

    close(fd);

    return FILE_REQUEST;
}

// 对内存映射区执行munmap操作
// void HttpConn::unmap() {
//     if (response_.file_addr_) {
//         munmap(response_.file_addr_, response_.file_stat_.st_size);
//         response_.file_addr_ = NULL;
//     }
// }

bool HttpConn::generateResponse(HTTP_CODE read_ret) {
    switch (read_ret) {
    case FILE_REQUEST:
        response_.setStatusCode(HttpResponse::OK);
        response_.setStatusMessage("OK");
        if (path_ == "/index.html") {
            response_.setContentType("text/html");
        } else if (path_ == "/images/image1.jpg") {
            response_.setContentType("image/jpg");
        }
        break;
    case BAD_REQUEST:
        response_.setStatusCode(HttpResponse::BadRequest);
        response_.setStatusMessage(error_400_title);
        response_.setBody(error_400_form);
        break;
    case NO_RESOURCE:
        response_.setStatusCode(HttpResponse::NotFound);
        response_.setStatusMessage(error_404_title);
        response_.setBody(error_404_form);
        break;
    default:
        break;
    }

    response_.addToBuffer(write_buf_);

    return true;
}

bool HttpConn::write() {
    int num_sent = 0;
    while (true) {
        // num_sent = send(cfd_, write_buf_.readBegin(), write_buf_.readable(), 0);
        num_sent = writev(cfd_, write_buf_.getIOV(), write_buf_.count());
        printf("num_sent = %d\n", num_sent);
        if (num_sent == -1) {
            if (errno == EAGAIN) { // no data
                epoll_mod(epfd_, cfd_, EPOLLOUT);
                return true;
            }
            response_.unmap();
            perror("writev");
            return false;
        }
        // cout << "发送了数据："
        //     << string(write_buf_.readBegin(), 0, write_buf_.readable()) << endl;
        
        write_buf_.hasRead(num_sent);
        printf("readable = %zd\n", write_buf_.readable());

        if (write_buf_.readFinished()) {
            response_.unmap();
            epoll_mod(epfd_, cfd_, EPOLLIN);
            if(linger_) {
                reset();
                return true;
            } else {
                return false;
            }
        }
    }
    return true;
}

void HttpConn::reset() {
    read_idx_ = 0;
    start_line_ = 0;
    checked_idx_ = 0;
    linger_ = false;
    state_ = CHECK_STATE_REQUESTLINE;

    bzero(read_buf_.data(), read_buf_.size());
    write_buf_.reset();
    response_.reset();
}

// 解析请求行，获得请求方法、URL、版本
// HttpConn::HTTP_CODE HttpConn::parseRequestLine(char *text) {
//     // GET /index.html HTTP/1.1
//     url_ = strchr(text, ' ');
//     *url_ = '\0';
//     ++url_;

//     char *method = text;
//     if (strcasecmp(method, "GET") == 0) {
//         method_ = GET;
//     } else {
//         return BAD_REQUEST;
//     }

//     version_ = strchr(url_, ' ');
//     if (!version_) 
//         return BAD_REQUEST;
//     *version_ = '\0';
//     ++version_;
//     if (strcasecmp(version_, "HTTP/1.1") != 0)
//         return BAD_REQUEST;
    
//     // 如果url格式为http://192.168.1.1:1000/index.html，忽略ip地址
//     if (strncasecmp(url_, "http://", 7) == 0) {
//         url_ += 7;
//         url_ = strchr(url_, '/');
//     }

//     if (!url_ || url_[0] != '/') {
//         return BAD_REQUEST;
//     }

//     state_ = CHECK_STATE_HEADER;
//     printf("method: %s\nurl: %s\nversion: %s\n", method, url_, version_);
//     printf("request line has been parsed\n");

//     return NO_REQUEST;
// }



// HttpConn::HTTP_CODE HttpConn::parseHeader(char *text) {
//     char* colon = strchr(text, ':');
//     if (!colon)
//         return BAD_REQUEST;
    
//     string key(text, colon);
    
//     char* end = text + strlen(text);

//     ++colon;
//     while (colon < end && isspace(*colon)) {
//         ++colon;
//     }
    
//     string value(colon, end);
//     headers_[key] = value;

//     return NO_REQUEST;
// }

// const char* HttpConn::findCRLF(const char *start) const {
//     const char* crlf = std::search(start, end(), CRLF, CRLF+2);
//     return crlf == end() ? nullptr : crlf;
// }

// void HttpConn::addHeader(const char *colon, const char *end) {
//     string key(peek(), colon);
//     colon++;
//     while (colon < end && isspace(*colon)) {
//         colon++;
//     }
//     string value(colon, end);
//     headers_[key] = value;
// }

// HttpConn::HTTP_CODE HttpConn::parseHeader(const char *begin) {
//     const char* crlf = findCRLF(begin);
//     if (!crlf)
//         return BAD_REQUEST;
//     const char* colon = std::find(peek(), crlf, ':');
//     if (colon != crlf) {
//         addHeader(colon, crlf);
//     } else {
//         return BAD_REQUEST;
//     }
// }