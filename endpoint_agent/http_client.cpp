// EDTMRS v6.1 - HTTP Client using WinHTTP
#include "http_client.h"
#include "device_monitor.h"   // for EDTMRS_VERSION
#include <windows.h>
#include <winhttp.h>

HttpClient::HttpClient(const std::string& host, int port)
    : m_host(host), m_port(port) {}
HttpClient::~HttpClient() {}

static std::wstring toWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring r(len-1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &r[0], len);
    return r;
}

static HttpResponse doRequest(const std::string& host, int port,
                               const std::string& method,
                               const std::string& path,
                               const std::string& body = "") {
    HttpResponse result;
    HINTERNET hSess = WinHttpOpen(L"EDTMRS-Agent/6.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return result;
    WinHttpSetTimeouts(hSess, 5000, 5000, 10000, 10000);

    HINTERNET hConn = WinHttpConnect(hSess, toWide(host).c_str(), (INTERNET_PORT)port, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return result; }

    HINTERNET hReq = WinHttpOpenRequest(hConn, toWide(method).c_str(),
        toWide(path).c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return result; }

    if (!body.empty())
        WinHttpAddRequestHeaders(hReq, L"Content-Type: application/json\r\n",
            (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL ok = WinHttpSendRequest(hReq,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
        (DWORD)body.size(), (DWORD)body.size(), 0);

    if (ok) ok = WinHttpReceiveResponse(hReq, nullptr);

    if (ok) {
        DWORD sc = 0, sz = sizeof(sc);
        WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &sc, &sz, WINHTTP_NO_HEADER_INDEX);
        result.status_code = (int)sc;
        result.success     = (sc >= 200 && sc < 300);
        DWORD avail = 0, read = 0;
        do {
            if (!WinHttpQueryDataAvailable(hReq, &avail) || !avail) break;
            char* buf = new char[avail+1];
            memset(buf, 0, avail+1);
            if (WinHttpReadData(hReq, buf, avail, &read))
                result.body += std::string(buf, read);
            delete[] buf;
        } while (avail > 0);
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return result;
}

HttpResponse HttpClient::post(const std::string& path, const std::string& body) {
    return doRequest(m_host, m_port, "POST", path, body);
}
HttpResponse HttpClient::get(const std::string& path) {
    return doRequest(m_host, m_port, "GET", path);
}
HttpResponse HttpClient::heartbeat(const std::string& hostname) {
    std::string json =
        std::string("{\"hostname\":\"") + hostname +
        "\",\"ip_address\":\"\",\"username\":\"\",\"agent_version\":\"" +
        EDTMRS_VERSION + "\"}";
    return post("/api/heartbeat", json);
}
