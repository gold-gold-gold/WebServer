#ifndef BUFFER_H
#define BUFFER_H

#include <sys/uio.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

class Buffer {
public:
    Buffer() : read_index_(0), iov_count_(1), total_len_(0) {
        buffer_.resize(DEFAULT_BUFFER_SIZE);
        iov_[0].iov_base = buffer_.data();
        iov_[0].iov_len = 0;
    }

    Buffer(size_t sz) : read_index_(0), iov_count_(1), total_len_(0) {
        buffer_.resize(sz);
        iov_[0].iov_base = buffer_.data();
        iov_[0].iov_len = 0;
    }

    ~Buffer() = default;

    // const char* findCRLF(const char *start) const;
    // const char* peek() const { return buffer_.data() + index_; }
    // char* readBegin() { return buffer_.data() + read_index_; }
    // const char* readBegin() const { return buffer_.data() + read_index_; }

    char* writeBegin() { return buffer_.data() + write_index_; }

    size_t writable() { return buffer_.size() - write_index_; }

    size_t readable() { return total_len_ - read_index_; }

    void hasRead(size_t len) {
        read_index_ += len;
        if (iov_count_ == 2 && read_index_ > write_index_) {
            iov_[0].iov_len = 0;
            iov_[1].iov_base = addr_ + read_index_ - write_index_;
            iov_[1].iov_len = total_len_ - read_index_;
        }
    }

    void append(const std::string &str) {
        int len = snprintf(writeBegin(), writable(), "%s", str.c_str());
        write_index_ += len;
        iov_[0].iov_len += len;
    }

    void appendFile(char *addr, size_t len) {
        addr_ = addr;
        iov_[1].iov_base = addr;
        iov_[1].iov_len = len;
        iov_count_ = 2;
        total_len_ = iov_[0].iov_len + iov_[1].iov_len;
        printf("total_len = %d\n", total_len_);
    }

    bool readFinished() { return read_index_ == total_len_; }

    const struct iovec *getIOV() { return iov_; }

    int count() { return iov_count_; }

    void reset() {
        read_index_ = 0;
        write_index_ = 0;
        iov_[0].iov_len = 0;
        iov_[1].iov_base = NULL;
        iov_[1].iov_len = 0;
        iov_count_ = 1;
        total_len_ = 0;
    }

private:
    std::vector<char> buffer_;
    size_t read_index_;
    size_t write_index_;
    static const int DEFAULT_BUFFER_SIZE = 2048;

    char *addr_;

    struct iovec iov_[2];
    int iov_count_;

    int total_len_;
};

#endif