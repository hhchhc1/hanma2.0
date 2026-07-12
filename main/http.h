#pragma once

#include <string>
#include <cstddef>

class Http {
public:
    virtual ~Http() = default;
    virtual void SetHeader(const std::string& key, const std::string& value) = 0;
    virtual void SetContent(std::string&& data) = 0;
    virtual bool Open(const std::string& method, const std::string& url) = 0;
    virtual int GetStatusCode() const = 0;
    virtual size_t GetBodyLength() const = 0;
    virtual int Read(char* buffer, size_t size) = 0;
    virtual int Write(const char* data, size_t size) = 0;
    virtual std::string ReadAll() = 0;
    virtual void Close() = 0;
};
