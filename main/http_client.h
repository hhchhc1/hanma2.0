#pragma once

#include "http.h"
#include <string>
#include <map>
#include "esp_http_client.h"

class HttpClient : public Http {
public:
    HttpClient();
    ~HttpClient();

    void SetHeader(const std::string& key, const std::string& value) override;
    void SetContent(std::string&& data) override;
    bool Open(const std::string& method, const std::string& url) override;
    int GetStatusCode() const override;
    size_t GetBodyLength() const override;
    int Read(char* buffer, size_t size) override;
    int Write(const char* data, size_t size) override;
    std::string ReadAll() override;
    void Close() override;

private:
    esp_http_client_handle_t client_;
    std::map<std::string, std::string> headers_;
    std::string content_;
    int status_code_;
    size_t content_length_;
    bool opened_;
};
