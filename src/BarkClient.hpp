#pragma once

#include <string>
#include <iostream>
#include "Config.hpp"
#include "HttpClient.hpp"

using namespace std;

namespace BarkClient {
    // 发送Bark推送通知
    inline bool send(const string& title, const string& body, bool is_stock = false) {
        if (!Config::BARK_ENABLED || Config::BARK_KEY.empty()) return false;
        string json = json_build_bark_payload(title, body, Config::BARK_GROUP, is_stock);
        string url = Config::BARK_SERVER + "/" + Config::BARK_KEY;
        auto resp = http_post(url, json, Config::HEADERS);
        if (resp.status_code == 200) {
            cout << "[Bark] 推送成功: " << title << endl;
            return true;
        } else {
            cout << "[Bark] 推送失败: HTTP " << resp.status_code << endl;
            return false;
        }
    }

    // 发送Bark测试推送
    inline bool test() {
        cout << "正在发送Bark测试推送..." << endl;
        return send("Bark 测试", "如果你看到这条消息，说明推送配置正确！", true);
    }
}
