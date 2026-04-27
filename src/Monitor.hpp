#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>
#include <iostream>
#include "Config.hpp"
#include "HttpClient.hpp"
#include "Utils.hpp"
#include "BarkClient.hpp"

using namespace std;

class Monitor {
private:
    bool stop;
    bool healthy;
    size_t last_status_col;
    unordered_map<string, int> last_stock_status;
    atomic<int> request_count;
    mutex print_mutex;
    // 每个目标对应的自定义脚本和是否高亮 (index 与 TARGETS 对应)
    vector<string> target_scripts;
    vector<bool> monitored_flags;

public:
    Monitor() : stop(false), healthy(true), last_status_col(0), request_count(0) {}

    void start();

private:
    void run_multi_monitor();
    void handle_error(const string& msg, bool critical);
};
