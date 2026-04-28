#pragma once

#include <string>
#include <vector>
#include <curl/curl.h>
#include "Config.hpp"
#include "HttpClient.hpp"

using namespace std;

class HttpMulti {
public:
    struct Request {
        int target_idx = -1;
        HttpResponse response;
        bool done = false;
        CURL* easy = nullptr;
        string post_body;
        chrono::steady_clock::time_point submit_time;
    };

    HttpMulti() { multi = curl_multi_init(); }

    ~HttpMulti() {
        cleanup_all();
        if (multi) curl_multi_cleanup(multi);
    }

    HttpMulti(const HttpMulti&) = delete;
    HttpMulti& operator=(const HttpMulti&) = delete;

    // 添加请求 (优先复用已有 handle)
    void add_post(int target_idx, const string& url, 
                  const string& json_body, const vector<string>& headers) {
        auto* req = new Request();
        req->target_idx = target_idx;
        req->post_body = json_body;
        req->submit_time = chrono::steady_clock::now();

        // 复用空闲 handle 或新建
        if (!free_handles.empty()) {
            req->easy = free_handles.back();
            free_handles.pop_back();
            curl_easy_reset(req->easy);
        } else {
            req->easy = curl_easy_init();
        }

        curl_slist* slist = nullptr;
        slist = curl_slist_append(slist, "Content-Type: application/json");
        for (const auto& h : headers)
            slist = curl_slist_append(slist, h.c_str());

        curl_easy_setopt(req->easy, CURLOPT_URL, url.c_str());
        curl_easy_setopt(req->easy, CURLOPT_POST, 1L);
        curl_easy_setopt(req->easy, CURLOPT_POSTFIELDS, req->post_body.c_str());
        curl_easy_setopt(req->easy, CURLOPT_POSTFIELDSIZE, req->post_body.size());
        curl_easy_setopt(req->easy, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(req->easy, CURLOPT_WRITEDATA, &req->response.data);
        curl_easy_setopt(req->easy, CURLOPT_HTTPHEADER, slist);
        curl_easy_setopt(req->easy, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(req->easy, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(req->easy, CURLOPT_TIMEOUT, Config::TIMEOUT);
        curl_easy_setopt(req->easy, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(req->easy, CURLOPT_PRIVATE, req);

        requests.push_back(req);
        curl_multi_add_handle(multi, req->easy);
    }

    void perform() { int r; curl_multi_perform(multi, &r); }

    vector<Request*> get_completed() {
        vector<Request*> result;
        int msgs;
        while (CURLMsg* msg = curl_multi_info_read(multi, &msgs)) {
            if (msg->msg == CURLMSG_DONE) {
                Request* req = nullptr;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &req);
                if (req && !req->done) {
                    req->done = true;
                    if (msg->data.result == CURLE_OK)
                        curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &req->response.status_code);
                    else
                        req->response.error = curl_easy_strerror(msg->data.result);
                    result.push_back(req);
                }
            }
        }
        return result;
    }

    bool all_done() const {
        for (auto* req : requests)
            if (!req->done) return false;
        return true;
    }

    int wait(int timeout_ms) {
        int numfds;
        curl_multi_wait(multi, nullptr, 0, timeout_ms, &numfds);
        return numfds;
    }

    // 复用：重置请求状态，保留底层 curl handle (连接缓存不丢)
    void reuse() {
        for (auto* req : requests) {
            if (req->easy) {
                curl_multi_remove_handle(multi, req->easy);
                // 保留 easy handle 不清理 → TCP 连接持续复用
                free_handles.push_back(req->easy);
            }
            delete req;
        }
        requests.clear();
    }

private:
    CURLM* multi = nullptr;
    vector<Request*> requests;
    vector<CURL*> free_handles;

    void cleanup_all() {
        reuse();
        for (auto* h : free_handles) curl_easy_cleanup(h);
        free_handles.clear();
    }

    static size_t write_cb(void* data, size_t size, size_t nmemb, void* userp) {
        auto* s = static_cast<string*>(userp);
        s->append(static_cast<char*>(data), size * nmemb);
        return size * nmemb;
    }
};
