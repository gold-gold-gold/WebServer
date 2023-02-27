#include <map>
#include <string>
using std::string;

class HttpRequest {
 public:
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };

    // HTTP版本
    enum VERSION { UNKNOWN, HTTP10, HTTP11 };

 public:
    HttpRequest() = default;
    ~HttpRequest() = default;

    bool setMethod(const string &method) {
        if (method == "GET") {
            method_ = GET;
            return true;
        } else {
            return false;
        }
    }

    void setPath(const string &str, size_t start, size_t end) {
        path_.assign(str, start, end);
    }

    void erasePath(size_t start, size_t end) {
        path_.erase(start, end);
    }

    const string& path() const { return path_; }

    void setVersion(const string &str, size_t start, size_t end) {
        version_.assign(str, start, end);
    }

    const string& version() const { return version_; }

    void addHeader(const string &key, const string &value) {
        headers_[key] = value;
    }

 private:
    METHOD method_;
    string path_;
    // string query_;
    string version_;
    std::map<string, string> headers_;
};