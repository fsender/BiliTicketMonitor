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
#include "ThreadPool.hpp"
#include "BarkClient.hpp"

using namespace std;

class Monitor {
private:
    bool stop;
    bool healthy;
    bool selling;
    vector<vector<string>> last_data;
    unordered_map<string, int> last_stock_status;
    atomic<int> request_count;
    mutex print_mutex;

public:
    Monitor() : stop(false), healthy(true), selling(false), request_count(0) {
        Config::init();
    }

    void start();

private:
    void run_monitor();
    void show_table(const string& name, const vector<vector<string>>& tickets);
    void handle_error(const string& msg, bool critical);

    // Multi-target concurrent methods (Task 6 will implement fully)
    void run_multi_monitor();
    int check_stock(const string& screen_id, const string& sku_id);
    void print_status_change(const string& label, int code);
    string get_ms_time();
};
