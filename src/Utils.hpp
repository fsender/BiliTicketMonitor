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
#include "Config.hpp"

using namespace std;

// ==================== 库存状态映射 ====================

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

// ==================== 工具函数 ====================

inline void clear_screen() {
#ifdef _WIN32
    system("chcp 65001");
    system("cls");
#else
    //system("clear");
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
