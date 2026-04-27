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
        string post_body;  // 保持 post body 生命周期
    };

    HttpMulti() {
        multi = curl_multi_init();
    }

    ~HttpMulti() {
        reset();
        if (multi) curl_multi_cleanup(multi);
    }

    HttpMulti(const HttpMulti&) = delete;
    HttpMulti& operator=(const HttpMulti&) = delete;

    // 添加一个异步 POST 请求
    void add_post(int target_idx, const string& url, 
                  const string& json_body, const vector<string>& headers) {
        auto* req = new Request();
        req->target_idx = target_idx;
        req->post_body = json_body;  // 拷贝到Request中，确保生命周期
        req->easy = curl_easy_init();
        if (!req->easy) { delete req; return; }

        // 构造带 Content-Type 的请求头
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

    // 驱动异步传输
    void perform() {
        int running;
        curl_multi_perform(multi, &running);
    }

    // 获取本轮完成的请求
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

    // 是否全部完成
    bool all_done() const {
        for (auto* req : requests)
            if (!req->done) return false;
        return true;
    }

    // 等待事件发生或超时
    int wait(int timeout_ms) {
        int numfds;
        curl_multi_wait(multi, nullptr, 0, timeout_ms, &numfds);
        return numfds;
    }

    // 重置：清理所有请求，准备下一轮
    void reset() {
        for (auto* req : requests) {
            if (req->easy) {
                curl_multi_remove_handle(multi, req->easy);
                curl_easy_cleanup(req->easy);
            }
            delete req;
        }
        requests.clear();
    }

private:
    CURLM* multi = nullptr;
    vector<Request*> requests;

    static size_t write_cb(void* data, size_t size, size_t nmemb, void* userp) {
        auto* s = static_cast<string*>(userp);
        s->append(static_cast<char*>(data), size * nmemb);
        return size * nmemb;
    }
};
