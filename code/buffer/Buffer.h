#ifndef BUFFER_H
#define BUFFER_H

#include <sys/uio.h>
#include <string>
#include <vector>

class Buffer {
public:
    Buffer();
    Buffer(size_t sz);
    ~Buffer();

    // const char* findCRLF(const char *start) const;
    // const char* peek() const { return buffer_.data() + index_; }
    // char* readBegin() { return buffer_.data() + read_index_; }

    const char* readBegin() const;
    char* writeBegin();

    size_t writable();
    size_t readable();

    void hasRead(size_t len);
    void hasWritten(size_t len);

    void retrieveAll();
    std::string retrieveAllToStr();

    void append(const std::string &str);
    void appendFile(char *addr, size_t len);

    bool readFinished();

    const struct iovec *getIOV();

    int count();

    void reset();

private:
    std::vector<char> buffer_;
    size_t read_index_;
    size_t write_index_;
    static const int DEFAULT_BUFFER_SIZE = 2048;

    char *addr_;

    struct iovec iov_[2];
    int iov_count_;

    int total_len_;

    void extend(size_t len);
};

#endif