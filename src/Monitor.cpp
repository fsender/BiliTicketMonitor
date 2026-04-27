#include "Monitor.hpp"

// 从getV2响应中提取项目名称和所有票种目标
static pair<string, vector<TargetConfig>> discover_from_getv2(const string& json_str) {
    vector<TargetConfig> targets;
    string project_name;
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root) return {project_name, targets};
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (data) {
        cJSON* name_item = cJSON_GetObjectItemCaseSensitive(data, "name");
        if (cJSON_IsString(name_item) && name_item->valuestring) {
            project_name = name_item->valuestring;
        }
        cJSON* screen_list = cJSON_GetObjectItemCaseSensitive(data, "screen_list");
        if (screen_list && cJSON_IsArray(screen_list)) {
            for (int si = 0; si < cJSON_GetArraySize(screen_list); si++) {
                cJSON* screen = cJSON_GetArrayItem(screen_list, si);
                if (!screen) continue;
                // 获取screen_id (屏幕/场次级别，字段名为 id)
                cJSON* sid_item = cJSON_GetObjectItemCaseSensitive(screen, "id");
                int screen_id = cJSON_IsNumber(sid_item) ? (int)sid_item->valuedouble : 0;
                
                cJSON* ticket_list = cJSON_GetObjectItemCaseSensitive(screen, "ticket_list");
                if (ticket_list && cJSON_IsArray(ticket_list)) {
                    for (int tj = 0; tj < cJSON_GetArraySize(ticket_list); tj++) {
                        cJSON* ticket = cJSON_GetArrayItem(ticket_list, tj);
                        if (!ticket) continue;
                        // 获取票种ID (sku_id，字段名为 id)
                        cJSON* id_item = cJSON_GetObjectItemCaseSensitive(ticket, "id");
                        int sku_id = cJSON_IsNumber(id_item) ? (int)id_item->valuedouble : 0;
                        
                        // 获取票种名称
                        string screen_name = "", desc = "";
                        cJSON* sn = cJSON_GetObjectItemCaseSensitive(ticket, "screen_name");
                        if (cJSON_IsString(sn) && sn->valuestring) screen_name = sn->valuestring;
                        cJSON* di = cJSON_GetObjectItemCaseSensitive(ticket, "desc");
                        if (cJSON_IsString(di) && di->valuestring) desc = di->valuestring;
                        
                        if (screen_id > 0 && sku_id > 0) {
                            TargetConfig tc;
                            tc.screen_id = to_string(screen_id);
                            tc.sku_id = to_string(sku_id);
                            tc.label = screen_name;
                            if (!desc.empty()) tc.label += " " + desc;
                            targets.push_back(tc);
                        }
                    }
                }
            }
        }
    }
    cJSON_Delete(root);
    return {project_name, targets};
}

void Monitor::start() {
    if (Config::TARGETS.empty()) {
        // 自动发现: 调用一次getV2获取所有票种
        cout << "正在获取票务信息..." << endl;
        HttpResponse resp = http_get(Config::API_URL, Config::HEADERS);
        if (resp.status_code != 200) {
            cout << "\033[31m错误：获取票务信息失败 (HTTP " << resp.status_code << ")\033[0m" << endl;
            cout << "请使用 --target 手动指定监控目标。" << endl;
            return;
        }
        auto [project, targets] = discover_from_getv2(resp.data);
        if (targets.empty()) {
            cout << "\033[31m错误：未找到任何可监控的票种\033[0m" << endl;
            return;
        }
        Config::project_name = project;
        Config::TARGETS = targets;
        cout << "\033[32m已发现 " << targets.size() << " 个票种\033[0m" << endl;
    }
    run_multi_monitor();
}

void Monitor::handle_error(const string& msg, bool critical) {
    cout << "\n" << msg << endl;
    healthy = false;
    if (critical) stop = true;
}

// 多目标并发监控模式 (stock/check API) — 表格输出
void Monitor::run_multi_monitor() {
    size_t num_targets = Config::TARGETS.size();
    ThreadPool pool(num_targets);
    vector<pair<string, int>> current_status; // label, stock_code
    
    // 首次查询所有目标获取初始状态
    {
        vector<future<int>> init_futures;
        for (const auto& target : Config::TARGETS) {
            init_futures.push_back(pool.enqueue([this, &target]() {
                request_count++;
                return check_stock(target.screen_id, target.sku_id);
            }));
        }
        for (size_t i = 0; i < num_targets; i++) {
            int code = -1;
            try { code = init_futures[i].get(); } catch (...) {}
            current_status.push_back({Config::TARGETS[i].label, code});
            last_stock_status[Config::TARGETS[i].screen_id] = code;
        }
    }
    
    // 初始表格渲染 (带ANSI光标定位)
    {
        // 计算列宽: 寻找最长的标签
        size_t max_label_width = 0;
        for (const auto& [label, code] : current_status) {
            if (label.size() > max_label_width) max_label_width = label.size();
        }
        size_t col1_width = 8 + max_label_width; // "No.    " + label
        size_t status_col = col1_width + 2;       // 状态列起始位置
        
        string title = Config::project_name.empty() 
            ? format("B站票务监控器 - {} 个目标", num_targets)
            : format("{} - {} 个目标", Config::project_name, num_targets);
        cout << "\033[1m" << title << "\033[0m" << endl;
        cout << "\033[32m更新: " << get_ms_timestamp() << "\033[0m" << endl;
        cout << "\033[36mNo.   目标" << string(max_label_width > 4 ? max_label_width - 4 : 0, ' ') << "  状态\033[0m" << endl;
        cout << string(status_col + 8, '-') << endl;
        for (size_t i = 0; i < num_targets; i++) {
            auto& [label, code] = current_status[i];
            string color = stock_status_color(code);
            string text = code == -1 ? "查询失败" : stock_status_to_string(code);
            cout << format("\033[33m[{:>2}]\033[0m  ", i + 1) << label;
            cout << "\033[" << status_col << "G" << color << text << "\033[0m" << endl;
        }
        // 在表格下方记录列宽供重绘使用
        last_status_col = status_col;
        cout << "\033[" << (num_targets + 5) << "E"; // 移到表格下方
    }
    
    while (!stop) {
        // 显示当前时间和请求计数
        cout << "\033[32m当前时间: " << get_ms_timestamp() << " | 已发送: " << request_count << " 次\033[0m\r" << flush;
        
        // 并发检查所有目标的库存
        vector<future<int>> futures;
        for (const auto& target : Config::TARGETS) {
            futures.push_back(pool.enqueue([this, &target]() {
                request_count++;
                return check_stock(target.screen_id, target.sku_id);
            }));
        }
        
        // 收集结果，检测变化
        bool changed = false;
        for (size_t i = 0; i < num_targets; i++) {
            if (stop) break;
            try {
                int code = futures[i].get();
                const auto& target = Config::TARGETS[i];
                auto& last = last_stock_status[target.screen_id];
                
                if (code != -1 && code != last) {
                    last = code;
                    current_status[i].second = code;
                    changed = true;
                    healthy = true;
                    
                    // 触发Bark推送通知
                    string label_copy = target.label;
                    if (code == 3) {
                        ThreadPool bark_pool(1);
                        bark_pool.enqueue([label_copy]() {
                            BarkClient::send("有库存 - " + label_copy,
                                "赶紧去抢票！项目ID: " + Config::TICKET_ID, true);
                        });
                    } else if (code == 1) {
                        ThreadPool bark_pool(1);
                        bark_pool.enqueue([label_copy]() {
                            BarkClient::send("暂时售罄 - " + label_copy,
                                "可能有补票机会。项目ID: " + Config::TICKET_ID, false);
                        });
                    }
                }
            } catch (...) {
                // 跳过失败的检查
            }
        }
        
        // 状态变化时重新渲染表格
        if (changed) {
            cout << "\033[s"; // 保存光标位置 (表格底部)
            for (size_t i = 0; i < num_targets; i++) {
                auto& [label, code] = current_status[i];
                string color = stock_status_color(code);
                string text = code == -1 ? "查询失败" : stock_status_to_string(code);
                cout << "\033[1A\033[K"; // 上一行，清行
                cout << format("\033[33m[{:>2}]\033[0m  ", i + 1) << label;
                cout << "\033[" << last_status_col << "G" << color << text << "\033[0m" << endl;
            }
            cout << "\033[u"; // 恢复光标位置
            cout << flush;
            // 检查是否有余票触发BATPATH
            for (auto& [label, code] : current_status) {
                if (code == 3) {
                    if (Config::BATPATH == "") {
                        cout << "\033[32m您订阅的票种有票了! (" << label << ")\033[0m" << endl;
                    } else {
                        system(Config::BATPATH.c_str());
                    }
                }
            }
        }
        
        this_thread::sleep_for(chrono::milliseconds(Config::REFRESH_INTERVAL));
    }
}

// 检查单个目标库存
int Monitor::check_stock(const string& screen_id, const string& sku_id) {
    string json_body = json_build_stock_check(Config::TICKET_ID, sku_id, screen_id);
    string url = "https://show.bilibili.com/api/ticket/stock/check";
    HttpResponse resp = http_post(url, json_body, Config::HEADERS);
    if (resp.status_code != 200) return -1;
    return parse_stock_status(resp.data);
}

// 毫秒时间戳
string Monitor::get_ms_time() {
    return get_ms_timestamp();
}
