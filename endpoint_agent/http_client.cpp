// EDTMRS v2.0 - HTTP Client Implementation
// Uses WinHTTP - built into Windows, no extra installs needed
// KEY FIX: Correct wide-string length (wlen-1 strips null terminator)
// KEY FIX: Added connection timeout so agent doesn't hang if server is down

#include "http_client.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>

#pragma comment(lib, "winhttp.lib")

HttpClient::HttpClient(const std::string& host, int port)
    : m_host(host), m_port(port) {}

HttpClient::~HttpClient() {}

// Convert std::string to std::wstring (strips null terminator correctly)
static std::wstring toWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');   // len-1 to exclude null terminator
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &result[0], len);
    return result;
}

HttpResponse HttpClient::post(const std::string& path, const std::string& jsonBody) {
    HttpResponse result = { 0, "", false };

    std::wstring wHost = toWide(m_host);
    std::wstring wPath = toWide(path);

    // ── Open session ──────────────────────────────────────────────────────────
    HINTERNET hSession = WinHttpOpen(
        L"EDTMRS-Agent/2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) {
        std::cerr << "[EDTMRS] WinHttpOpen failed: " << GetLastError() << std::endl;
        return result;
    }

    // Set timeouts: resolve=5s, connect=5s, send=10s, receive=10s
    WinHttpSetTimeouts(hSession, 5000, 5000, 10000, 10000);

    // ── Connect ───────────────────────────────────────────────────────────────
    HINTERNET hConnect = WinHttpConnect(
        hSession, wHost.c_str(), (INTERNET_PORT)m_port, 0);
    if (!hConnect) {
        std::cerr << "[EDTMRS] WinHttpConnect failed: " << GetLastError()
                  << " (Is the server running at " << m_host << ":" << m_port << "?)" << std::endl;
        WinHttpCloseHandle(hSession);
        return result;
    }

    // ── Open request (HTTP, not HTTPS) ────────────────────────────────────────
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST", wPath.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0   // 0 = HTTP,  WINHTTP_FLAG_SECURE = HTTPS
    );
    if (!hRequest) {
        std::cerr << "[EDTMRS] WinHttpOpenRequest failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // ── Set Content-Type header ───────────────────────────────────────────────
    WinHttpAddRequestHeaders(
        hRequest,
        L"Content-Type: application/json\r\n",
        (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD
    );

    // ── Send request with JSON body ───────────────────────────────────────────
    BOOL ok = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)jsonBody.c_str(),
        (DWORD)jsonBody.size(),
        (DWORD)jsonBody.size(),
        0
    );
    if (!ok) {
        std::cerr << "[EDTMRS] WinHttpSendRequest failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // ── Receive response ──────────────────────────────────────────────────────
    ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (!ok) {
        std::cerr << "[EDTMRS] WinHttpReceiveResponse failed: " << GetLastError() << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // ── Read HTTP status code ─────────────────────────────────────────────────
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );
    result.status_code = (int)statusCode;

    // ── Read response body ────────────────────────────────────────────────────
    DWORD dwSize = 0, dwRead = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
        char* buf = new char[dwSize + 1];
        memset(buf, 0, dwSize + 1);
        if (WinHttpReadData(hRequest, buf, dwSize, &dwRead))
            result.body += std::string(buf, dwRead);
        delete[] buf;
    } while (dwSize > 0);

    result.success = (statusCode >= 200 && statusCode < 300);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

HttpResponse HttpClient::heartbeat(const std::string& hostname) {
    std::string json = "{\"hostname\":\"" + hostname + "\"}";
    return post("/api/heartbeat", json);
}
