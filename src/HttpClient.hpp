#pragma once

#include <string>
#include <vector>
#include <curl/curl.h>
#include "Config.hpp"
#include "cJSON.h"

using namespace std;

struct HttpResponse {
    string data;
    long status_code;
    string error;
};

inline string json_build_stock_check(const string& projectId, const string& skuId, const string& screenId) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return "";
    cJSON_AddStringToObject(root, "projectId", projectId.c_str());
    cJSON_AddNumberToObject(root, "skuId", stoi(skuId));
    cJSON_AddNumberToObject(root, "screenId", stoi(screenId));
    char* json_str = cJSON_PrintUnformatted(root);
    string result(json_str);
    free(json_str);
    cJSON_Delete(root);
    return result;
}

inline string json_build_bark_payload(const string& title, const string& body, const string& group, bool is_stock) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return "";
    cJSON_AddStringToObject(root, "title", title.c_str());
    cJSON_AddStringToObject(root, "body", body.c_str());
    cJSON_AddStringToObject(root, "group", group.c_str());
    cJSON_AddStringToObject(root, "level", is_stock ? "critical" : "active");
    if (is_stock) {
        cJSON_AddStringToObject(root, "sound", "alarm");
    }
    char* json_str = cJSON_PrintUnformatted(root);
    string result(json_str);
    free(json_str);
    cJSON_Delete(root);
    return result;
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    ((string*)userp)->append((char*)contents, realsize);
    return realsize;
}

inline HttpResponse http_get(const string& url, const vector<string>& headers) {
    HttpResponse response;
    CURL* curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.data);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, Config::TIMEOUT);
        struct curl_slist* header_list = nullptr;
        for (const auto& header : headers) {
            header_list = curl_slist_append(header_list, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            response.error = curl_easy_strerror(res);
        } else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        }
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
    }
    return response;
}

inline HttpResponse http_post(const string& url, const string& json_body, const vector<string>& headers) {
    HttpResponse response;
    CURL* curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.data);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, Config::TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_body.size());
        struct curl_slist* header_list = nullptr;
        header_list = curl_slist_append(header_list, "Content-Type: application/json");
        for (const auto& header : headers) {
            header_list = curl_slist_append(header_list, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            response.error = curl_easy_strerror(res);
        } else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        }
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
    }
    return response;
}
