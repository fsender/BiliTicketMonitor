#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <mutex>
#include <algorithm>
#include "cJSON.h"
#include "Config.hpp"

using namespace std;

// ==================== 状态映射 ====================

inline const unordered_map<string, string> StatusColor = {
    {"已售罄", "\033[31m"},
    {"已停售", "\033[31m"},
    {"不可售", "\033[31m"},
    {"未开售", "\033[36m"},
    {"暂时售罄", "\033[33m"},
    {"预售中", "\033[32m"},
};

inline const unordered_map<int, string> StockStatusMap = {
    {1, "暂时售罄"},
    {2, "已售罄"},
    {3, "有库存"},
};

inline const unordered_map<int, string> StockStatusColor = {
    {1, "\033[33m"},
    {2, "\033[31m"},
    {3, "\033[32m"},
};

// ==================== 库存状态解析 ====================

inline int parse_stock_status(const string& json_str) {
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root) return -1;
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!data) { cJSON_Delete(root); return -1; }
    cJSON* status = cJSON_GetObjectItemCaseSensitive(data, "stockStatus");
    int result = -1;
    if (cJSON_IsNumber(status)) {
        result = (int)status->valuedouble;
    }
    cJSON_Delete(root);
    return result;
}

// ==================== 项目信息解析 (getV2) ====================

inline pair<string, vector<vector<string>>> process_data(const string& json_str) {
    vector<vector<string>> tickets;
    string name;
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root) return make_pair("", tickets);
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (data) {
        cJSON* name_item = cJSON_GetObjectItemCaseSensitive(data, "name");
        if (cJSON_IsString(name_item) && name_item->valuestring) name = name_item->valuestring;
        cJSON* screen_list = cJSON_GetObjectItemCaseSensitive(data, "screen_list");
        if (screen_list && cJSON_IsArray(screen_list)) {
            for (int i = 0; i < std::min(cJSON_GetArraySize(screen_list), 1000); i++) {
                cJSON* screen = cJSON_GetArrayItem(screen_list, i);
                if (!screen) continue;
                cJSON* tl = cJSON_GetObjectItemCaseSensitive(screen, "ticket_list");
                if (tl && cJSON_IsArray(tl)) {
                    for (int j = 0; j < std::min(cJSON_GetArraySize(tl), 10); j++) {
                        cJSON* ticket = cJSON_GetArrayItem(tl, j);
                        if (!ticket) continue;
                        string sn = "", desc = "", status = "";
                        cJSON* i1 = cJSON_GetObjectItemCaseSensitive(ticket, "screen_name");
                        if (cJSON_IsString(i1) && i1->valuestring) sn = i1->valuestring;
                        cJSON* i2 = cJSON_GetObjectItemCaseSensitive(ticket, "desc");
                        if (cJSON_IsString(i2) && i2->valuestring) desc = i2->valuestring;
                        cJSON* sf = cJSON_GetObjectItemCaseSensitive(ticket, "sale_flag");
                        if (sf) {
                            cJSON* dn = cJSON_GetObjectItemCaseSensitive(sf, "display_name");
                            if (cJSON_IsString(dn) && dn->valuestring) status = dn->valuestring;
                        }
                        if (!sn.empty() || !desc.empty() || !status.empty()) {
                            string tn = sn;
                            if (!desc.empty()) tn += " " + desc;
                            tickets.push_back({tn, status});
                        }
                    }
                }
            }
        }
    }
    cJSON_Delete(root);
    return make_pair(name, tickets);
}

// ==================== 显示工具 ====================

inline size_t display_width(const string& str) {
    size_t width = 0;
    bool wflag = 0;
    for (char c : str) {
        if (static_cast<unsigned char>(c) >= 192) { width += 2; }
        else if (static_cast<unsigned char>(c) == '\033') { wflag = 1; }
        else if (static_cast<unsigned char>(c) <= 127) {
            if(wflag && c=='m') wflag=0;
            else if(!wflag) width += 1;
        }
    }
    return width;
}

inline void clear_screen() {
#ifdef _WIN32
    system("chcp 65001");
    system("cls");
#else
    system("clear");
#endif
}

// ==================== 时间戳 ====================

inline string get_ms_timestamp() {
    auto now = chrono::system_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = chrono::system_clock::to_time_t(now);
    tm bt = *localtime(&timer);
    ostringstream oss;
    oss << put_time(&bt, "%H:%M:%S") << '.' << setfill('0') << setw(3) << ms.count();
    return oss.str();
}

// ==================== 线程安全输出 ====================

inline mutex global_print_mutex;

inline void safe_print(const string& text, const string& color = "") {
    lock_guard<mutex> lock(global_print_mutex);
    if (!color.empty()) cout << color;
    cout << text;
    if (!color.empty()) cout << "\033[0m";
    cout << endl;
}

// ==================== 状态码辅助 ====================

inline string stock_status_to_string(int code) {
    auto it = StockStatusMap.find(code);
    if (it != StockStatusMap.end()) return it->second;
    return "未知状态";
}

inline string stock_status_color(int code) {
    auto it = StockStatusColor.find(code);
    if (it != StockStatusColor.end()) return it->second;
    return "\033[0m";
}
