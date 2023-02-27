#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <sys/mman.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "Buffer.h"
using std::string;

// 定义HTTP响应的一些状态信息
// extern string ok_200_title;
// extern string error_400_title;
// extern string error_400_form;
// extern string error_403_title;
// extern string error_403_form;
// extern string error_404_title;
// extern string error_404_form;
// extern string error_500_title;
// extern string error_500_form;

class HttpConn;

class HttpResponse {
public:
    enum HttpStatusCode { Unknown, OK = 200, MovedPermanently = 301, BadRequest = 400, NotFound = 404 };

public:
    HttpResponse()
        : status_code_(Unknown), body_(NULL), body_length_(0), hasMapping_(false) {}
    ~HttpResponse() = default;

    void setStatusCode(HttpStatusCode code)
    { status_code_ = code; }

    void setStatusMessage(const string& message)
    { status_message_ = message; }

    void addHeader(const string& key, const string& value)
    { headers_[key] = value; }

    void setContentType(const string& contentType)
    { addHeader("Content-Type", contentType); }

    void setContentLength(size_t len) 
    { addHeader("Content-Length", std::to_string(len)); }

    void setBody(char *addr, size_t len)
    { body_ = addr; body_length_ = len; setContentLength(len); hasMapping_ = true; }

    void setBody(const string &str)
    { body_ = (char *)str.data(); setContentLength(str.length()); }

    // char *MappingAddress()
    // { return hasMapping_ ? body_ : NULL; }

    bool addToBuffer(Buffer &buf) const;

    void reset() {
        status_code_ = Unknown;
        status_message_.clear();
        headers_.clear();
        body_ = NULL;
        hasMapping_ = false;
    }

    void unmap() {
        if (hasMapping_) {
            munmap(body_, body_length_);
            body_ = NULL;
            body_length_ = 0;
        }
    }

private:
    HttpStatusCode status_code_;
    string status_message_;
    std::map<string, string> headers_;

    char* body_;
    size_t body_length_;
    bool hasMapping_;
};

inline bool HttpResponse::addToBuffer(Buffer &buf) const {
    if (buf.writable() <= 0) {
        printf("buf.writable() <= 0\n");
        return false;
    }
    buf.append("HTTP/1.1 " + std::to_string(status_code_) + ' ' + status_message_);
    buf.append("\r\n");
    // buf.append("Content-Length: " + std::to_string(body_.size()));
    // buf.append("\r\n");
    for (const auto &header : headers_) {
        buf.append(header.first + ": " + header.second);
        buf.append("\r\n");
    }
    buf.append("\r\n");
    
    if (hasMapping_) {
        buf.appendFile(body_, body_length_);
        printf("body_length_ = %zd\n",body_length_);
    } else {
        buf.append(body_);
    }
}

#endif