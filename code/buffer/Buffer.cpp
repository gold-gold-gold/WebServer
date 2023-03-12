#include "Buffer.h"

#include <stdio.h>
#include <string.h>

Buffer::Buffer(): read_index_(0), iov_count_(1), total_len_(0) {
    buffer_.resize(DEFAULT_BUFFER_SIZE);
    iov_[0].iov_base = buffer_.data();
    iov_[0].iov_len = 0;
}

Buffer::Buffer(size_t sz) : read_index_(0), iov_count_(1), total_len_(0) {
    buffer_.resize(sz);
    iov_[0].iov_base = buffer_.data();
    iov_[0].iov_len = 0;
}

Buffer::~Buffer() = default;

const char* Buffer::readBegin() const {
    return buffer_.data() + read_index_;
}

char* Buffer::writeBegin() {
    return buffer_.data() + write_index_;
}

size_t Buffer::writable() {
    return buffer_.size() - write_index_;
}

size_t Buffer::readable() {
    return total_len_ - read_index_;
}

void Buffer::hasRead(size_t len) {
    read_index_ += len;
    if (iov_count_ == 1 && read_index_ == write_index_) {
        read_index_ = write_index_ = 0;
        iov_[0].iov_len = 0;
        total_len_ = 0;
    }
    else if (iov_count_ == 2 && read_index_ > write_index_) {
        int overflow = read_index_ - write_index_;
        total_len_ -= len;
        iov_[0].iov_len = 0;
        iov_[1].iov_base = addr_ + overflow;
        iov_[1].iov_len = total_len_;
        read_index_ = write_index_ = 0;
    }
}

void Buffer::hasWritten(size_t len) {
    write_index_ += len;
}

void Buffer::retrieveAll() {
    bzero(buffer_.data(), buffer_.size());
    read_index_ = write_index_ = 0;
    iov_[0].iov_len = 0;
}

std::string Buffer::retrieveAllToStr() {
    std::string str(readBegin(), readable());
    retrieveAll();
    return str;
}

void Buffer::append(const std::string &str) {
    if (str.size() > writable()) {
        extend(str.size() - writable());
    }
    int len = snprintf(writeBegin(), writable(), "%s", str.c_str());
    write_index_ += len;
    iov_[0].iov_len += len;
    total_len_ = write_index_;
}

void Buffer::appendFile(char *addr, size_t len) {
    addr_ = addr;
    iov_[1].iov_base = addr;
    iov_[1].iov_len = len;
    iov_count_ = 2;
    total_len_ = iov_[0].iov_len + iov_[1].iov_len;
    printf("total_len = %d\n", total_len_);
}

bool Buffer::readFinished() { 
    return read_index_ == total_len_; 
}

const struct iovec *Buffer::getIOV() {
    return iov_;
}

int Buffer::count() {
    return iov_count_;
}

void Buffer::reset() {
    read_index_ = write_index_ = 0;
    iov_[0].iov_len = 0;
    iov_[1].iov_base = NULL;
    iov_[1].iov_len = 0;
    iov_count_ = 1;
    total_len_ = 0;
}

void Buffer::extend(size_t len) {
    size_t extend = len > buffer_.capacity() ? len : buffer_.capacity();
    buffer_.resize(buffer_.capacity() + len + 1);
}