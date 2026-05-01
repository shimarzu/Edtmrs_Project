#pragma once
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H
#include <string>

struct HttpResponse {
    int         status_code = 0;
    std::string body        = "";
    bool        success     = false;
};

class HttpClient {
public:
    HttpClient(const std::string& host, int port);
    ~HttpClient();
    HttpResponse post(const std::string& path, const std::string& jsonBody);
    HttpResponse get (const std::string& path);
    HttpResponse heartbeat(const std::string& hostname);
private:
    std::string m_host;
    int         m_port;
};
#endif
