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
        if (cJSON_IsString(name_item) && name_item->valuestring) project_name = name_item->valuestring;
        cJSON* screen_list = cJSON_GetObjectItemCaseSensitive(data, "screen_list");
        if (screen_list && cJSON_IsArray(screen_list)) {
            for (int si = 0; si < cJSON_GetArraySize(screen_list); si++) {
                cJSON* screen = cJSON_GetArrayItem(screen_list, si);
                if (!screen) continue;
                cJSON* sid = cJSON_GetObjectItemCaseSensitive(screen, "id");
                int screen_id = cJSON_IsNumber(sid) ? (int)sid->valuedouble : 0;
                cJSON* ticket_list = cJSON_GetObjectItemCaseSensitive(screen, "ticket_list");
                if (ticket_list && cJSON_IsArray(ticket_list)) {
                    for (int tj = 0; tj < cJSON_GetArraySize(ticket_list); tj++) {
                        cJSON* ticket = cJSON_GetArrayItem(ticket_list, tj);
                        if (!ticket) continue;
                        cJSON* id_item = cJSON_GetObjectItemCaseSensitive(ticket, "id");
                        int sku_id = cJSON_IsNumber(id_item) ? (int)id_item->valuedouble : 0;
                        string sn = "", desc = "";
                        cJSON* i1 = cJSON_GetObjectItemCaseSensitive(ticket, "screen_name");
                        if (cJSON_IsString(i1) && i1->valuestring) sn = i1->valuestring;
                        cJSON* i2 = cJSON_GetObjectItemCaseSensitive(ticket, "desc");
                        if (cJSON_IsString(i2) && i2->valuestring) desc = i2->valuestring;
                        if (screen_id > 0 && sku_id > 0) {
                            TargetConfig tc;
                            tc.screen_id = to_string(screen_id);
                            tc.sku_id = to_string(sku_id);
                            tc.label = sn;
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
    // 硬编码API URL
    string getv2_url = "https://show.bilibili.com/api/ticket/project/getV2?version=134&id=" + Config::TICKET_ID;

    cout << "正在获取票务信息..." << endl;
    HttpResponse resp = http_get(getv2_url, Config::HEADERS);
    if (resp.status_code != 200) {
        cout << "\033[31m错误：获取票务信息失败 (HTTP " << resp.status_code << ")\033[0m" << endl;
        return;
    }
    auto [project, all_tickets] = discover_from_getv2(resp.data);
    if (all_tickets.empty()) {
        cout << "\033[31m错误：未找到任何可监控的票种\033[0m" << endl;
        return;
    }

    Config::project_name = project;
    cout << "\033[32m已发现 " << all_tickets.size() << " 个票种\033[0m" << endl;

    // 根据配置选择要监控的目标
    Config::TARGETS.clear();
    target_scripts.clear();
    monitored_flags.clear();
    
    if (Config::MONITOR_ALL) {
        // 监视全部票种, 有配置脚本的票种高亮执行
        Config::TARGETS = all_tickets;
        target_scripts.assign(Config::TARGETS.size(), "");
        monitored_flags.assign(Config::TARGETS.size(), false);
        for (const auto& mt : Config::MONITORED) {
            int idx = mt.ticket_no - 1;
            if (idx >= 0 && idx < (int)all_tickets.size()) {
                target_scripts[idx] = mt.script_command;
                monitored_flags[idx] = true;
            }
        }
    } else {
        // 仅监控选定的票种 (不高亮, 因为全都是)
        for (const auto& mt : Config::MONITORED) {
            int idx = mt.ticket_no - 1;
            if (idx >= 0 && idx < (int)all_tickets.size()) {
                Config::TARGETS.push_back(all_tickets[idx]);
                target_scripts.push_back(mt.script_command);
                monitored_flags.push_back(false); // 不高亮
            }
        }
        if (Config::TARGETS.empty()) {
            cout << "\033[31m错误：配置的监控目标序号无效\033[0m" << endl;
            return;
        }
    }
    
    run_multi_monitor();
}

void Monitor::handle_error(const string& msg, bool critical) {
    cout << "\n" << msg << endl;
    healthy = false;
    if (critical) stop = true;
}

void Monitor::run_multi_monitor() {
    size_t num_targets = Config::TARGETS.size();
    ThreadPool pool(num_targets);
    vector<pair<string, int>> current_status;

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

    // 初始表格渲染
    {
        size_t max_label_width = 0;
        for (const auto& [label, code] : current_status) {
            if (label.size() > max_label_width) max_label_width = label.size();
        }
        size_t col_gap = 8 + max_label_width + 2;

        string title = Config::project_name.empty()
            ? format("B站票务监控器 - {} 个目标", num_targets)
            : format("{} - {} 个目标", Config::project_name, num_targets);
        cout << "\033[1m" << title << "\033[0m" << endl;
        cout << "\033[32m更新: " << get_ms_timestamp() << "\033[0m" << endl;
        cout << "\033[36mNo.   目标" << string(max_label_width > 4 ? max_label_width - 4 : 0, ' ') << "  状态\033[0m" << endl;
        cout << string(col_gap + 8, '-') << endl;
        for (size_t i = 0; i < num_targets; i++) {
            auto& [label, code] = current_status[i];
            string color = stock_status_color(code);
            string text = code == -1 ? "查询失败" : stock_status_to_string(code);
            // 被选中的目标用紫红色粗体高亮
            if (monitored_flags[i])
                cout << format("\033[35m\033[1m[{:>2}]  ", i + 1) << label << "\033[0m";
            else
                cout << format("\033[33m[{:>2}]\033[0m  ", i + 1) << label;
            cout << "\033[" << col_gap << "G" << color << text << "\033[0m" << endl;
        }
        last_status_col = col_gap;
        cout << "\033[" << (num_targets + 5) << "E";
    }

    while (!stop) {
        cout << "\033[32m当前时间: " << get_ms_timestamp() << " | 已发送: " << request_count << " 次\033[0m\r" << flush;

        vector<future<int>> futures;
        for (const auto& target : Config::TARGETS) {
            futures.push_back(pool.enqueue([this, &target]() {
                request_count++;
                return check_stock(target.screen_id, target.sku_id);
            }));
        }

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

                    // 有库存时执行自定义脚本
                    if (code == 3 && !target_scripts[i].empty()) {
                        string cmd = replace_vars(target_scripts[i], target.screen_id, target.sku_id);
                        cout << "\033[32m执行: " << cmd << "\033[0m" << endl;
                        system(cmd.c_str());
                    }

                    // Bark推送通知
                    string label_copy = target.label;
                    if (code == 3) {
                        ThreadPool bp(1);
                        bp.enqueue([label_copy]() {
                            BarkClient::send("有库存 - " + label_copy,
                                "项目ID: " + Config::TICKET_ID, true);
                        });
                    } else if (code == 1) {
                        ThreadPool bp(1);
                        bp.enqueue([label_copy]() {
                            BarkClient::send("暂时售罄 - " + label_copy,
                                "项目ID: " + Config::TICKET_ID, false);
                        });
                    }
                }
            } catch (...) {}
        }

        if (changed) {
            cout << "\033[s";
            for (size_t i = 0; i < num_targets; i++) {
                auto& [label, code] = current_status[i];
                string color = stock_status_color(code);
                string text = code == -1 ? "查询失败" : stock_status_to_string(code);
                cout << "\033[1A\033[K";
                if (monitored_flags[i])
                    cout << format("\033[35m\033[1m[{:>2}]  ", i + 1) << label << "\033[0m";
                else
                    cout << format("\033[33m[{:>2}]\033[0m  ", i + 1) << label;
                cout << "\033[" << last_status_col << "G" << color << text << "\033[0m" << endl;
            }
            cout << "\033[u" << flush;
        }

        this_thread::sleep_for(chrono::milliseconds(Config::REFRESH_INTERVAL));
    }
}

int Monitor::check_stock(const string& screen_id, const string& sku_id) {
    string json_body = json_build_stock_check(Config::TICKET_ID, sku_id, screen_id);
    string url = "https://show.bilibili.com/api/ticket/stock/check";
    HttpResponse resp = http_post(url, json_body, Config::HEADERS);
    if (resp.status_code != 200) return -1;
    return parse_stock_status(resp.data);
}

string Monitor::get_ms_time() {
    return get_ms_timestamp();
}
