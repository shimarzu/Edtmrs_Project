#pragma once
// EDTMRS v2.0 - HTTP Client Header
// Uses WinHTTP (built-in Windows, no external libs needed)

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>

struct HttpResponse {
    int         status_code;
    std::string body;
    bool        success;
};

class HttpClient {
public:
    HttpClient(const std::string& host, int port);
    ~HttpClient();

    HttpResponse post(const std::string& path, const std::string& jsonBody);
    HttpResponse heartbeat(const std::string& hostname);

private:
    std::string m_host;
    int         m_port;
};

#endif
