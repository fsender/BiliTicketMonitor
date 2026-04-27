#pragma once

#include <string>
#include <vector>
#include <curl/curl.h>
#include "Config.hpp"

using namespace std;

// RAII: 线程退出时自动清理 curl 句柄
struct ThreadCurl {
    CURL* handle = nullptr;
    ThreadCurl() { handle = curl_easy_init(); }
    ~ThreadCurl() { if (handle) curl_easy_cleanup(handle); }
    ThreadCurl(const ThreadCurl&) = delete;
    ThreadCurl& operator=(const ThreadCurl&) = delete;
};

struct HttpResponse {
    string data;
    long status_code;
    string error;
};

inline string json_build_stock_check(const string& projectId, const string& skuId, const string& screenId) {
    return "{\"projectId\":\"" + projectId + "\",\"skuId\":" + skuId + ",\"screenId\":" + screenId + "}";
}

inline string json_build_bark_payload(const string& title, const string& body, const string& group, bool is_stock) {
    string json = "{\"title\":\"" + title + "\",\"body\":\"" + body + "\",\"group\":\"" + group + "\",\"level\":\"" + (is_stock ? "critical" : "active") + "\"";
    if (is_stock) json += ",\"sound\":\"alarm\"";
    json += "}";
    return json;
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// 设置 curl 的通用选项 (每次请求前调用)
inline void setup_curl_opts(CURL* curl, const string& url, const vector<string>& headers, HttpResponse& resp) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.data);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, Config::TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);  // TCP keepalive 复用连接

    struct curl_slist* header_list = nullptr;
    for (const auto& h : headers) {
        header_list = curl_slist_append(header_list, h.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
    }
    curl_slist_free_all(header_list);
}

// 执行HTTP GET请求 (复用线程级 curl 句柄)
inline HttpResponse http_get(const string& url, const vector<string>& headers) {
    HttpResponse response;
    thread_local ThreadCurl tc;
    CURL* curl = tc.handle;
    if (curl) {
        response.data.clear();
        response.status_code = 0;
        response.error.clear();
        setup_curl_opts(curl, url, headers, response);
    }
    return response;
}

// 执行HTTP POST请求 (复用线程级 curl 句柄)
inline HttpResponse http_post(const string& url, const string& json_body, const vector<string>& headers) {
    HttpResponse response;
    thread_local ThreadCurl tc;
    CURL* curl = tc.handle;
    if (curl) {
        response.data.clear();
        response.status_code = 0;
        response.error.clear();
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_body.size());
        // 构造带 Content-Type 的请求头
        vector<string> all_headers = headers;
        all_headers.insert(all_headers.begin(), "Content-Type: application/json");
        setup_curl_opts(curl, url, all_headers, response);
    }
    return response;
}
