#include "Monitor.hpp"

void Monitor::start() {
    run_monitor();
}

// 显示表格（带颜色和优化布局）
void Monitor::show_table(const string& name, const vector<vector<string>>& tickets) {
    if (tickets.empty()) return;
    
    size_t col1_width = 0;
    size_t col2_width = 0;
    
    for (const auto& row : tickets) {
        if (row.size() >= 1) col1_width = max(col1_width, display_width(row[0]));
        if (row.size() >= 2) col2_width = max(col2_width, display_width(row[1]));
    }
    col1_width += 6;
    
    cout << "\033[s";
    cout << "\033[1m";
    cout << "\033[K\n" << name << " - ";
    cout << "\033[0m";

    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    tm now_tm = *localtime(&now_time);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &now_tm);
    cout << "\033[32m更新: " << time_str << "\033[0m\r" << endl;
    
    cout << "\033[36m";
    cout << "\033[K";
    string header1 = "No.   票种";
    size_t padding1 = col1_width - display_width(header1);
    cout << header1 << string(padding1, ' ');
    cout << "\033[" << (col1_width + 4) << "G";
    cout << "状态";
    cout << "\033[0m";
    cout << "\n";
    cout << "\033[K" << string(col1_width + col2_width + 4, '-') << "\n";
    
    size_t lines_printed = 0;
    int no = 0;
    for (const auto& row : tickets) {
        no++;
        if (row.size() < 2) continue;
        cout << "\033[K";
        lines_printed++;
        
        string ticket_name = format("\033[33m[{:>2}]\033[0m  ", no) + row[0];
        if(no == Config::TICKETNO){
            ticket_name = format("\033[35m\033[1m[{:>2}]  ", no) + row[0] + "\033[0m";
        }
        size_t ticket_width = display_width(ticket_name);
        
        if (ticket_width > col1_width) {
            size_t keep_chars = 0;
            size_t current_width = 0;
            bool last_char_double = false;
            for (size_t i = 0; i < ticket_name.size(); i++) {
                char c = ticket_name[i];
                size_t char_width = (static_cast<unsigned char>(c) > 191) ? 2 : ((static_cast<unsigned char>(c) > 127) ? 0 : 1);
                if (current_width + char_width > col1_width - 2) {
                    if (last_char_double && char_width == 2) keep_chars--;
                    ticket_name = ticket_name.substr(0, keep_chars) + "..";
                    break;
                }
                current_width += char_width;
                keep_chars++;
                last_char_double = (char_width == 2);
            }
        }
        
        cout << ticket_name;
        size_t padding = col1_width - display_width(ticket_name);
        if (padding > 0) cout << string(padding, ' ');
        cout << "\033[" << (col1_width + 4) << "G";
        
        auto it = StatusColor.find(row[1]);
        if (it != StatusColor.end()) {
            cout << it->second << row[1] << "\033[0m";
            if(no == Config::TICKETNO) selling = (row[1] == "预售中");
        } else {
            cout << row[1];
        }
        cout << "\n";
    }
    
    cout << "\033[K";
    int move_down = max(0, static_cast<int>(lines_printed + 4));
    cout << "\033[" << move_down << "E" << flush;
    if(selling) {
        if (Config::BATPATH == "") cout << "您订阅的票种有票了!" << endl;
        else system(Config::BATPATH.c_str());
    }
}

// 单目标监控模式 (legacy)
void Monitor::run_monitor() {
    while (!stop) {
        auto now = chrono::system_clock::now();
        time_t now_time = chrono::system_clock::to_time_t(now);
        tm now_tm = *localtime(&now_time);
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &now_tm);
        cout << "\033[32m当前时间: " << time_str << "\033[0m\r" << flush;
        
        try {
            HttpResponse resp = http_get(Config::API_URL, Config::HEADERS);
            if (resp.status_code != 200) {
                handle_error("HTTP错误: " + to_string(resp.status_code), resp.status_code == 412);
                continue;
            }
            auto [name, tickets] = process_data(resp.data);
            if (tickets.empty()) {
                this_thread::sleep_for(chrono::milliseconds(Config::REFRESH_INTERVAL));
                continue;
            }
            if (healthy && (tickets != last_data || selling)) {
                show_table(name, tickets);
                healthy = true;
            }
            last_data = tickets;
        } catch (const exception& e) {
            handle_error("请求异常: " + string(e.what()), false);
        }
        this_thread::sleep_for(chrono::milliseconds(Config::REFRESH_INTERVAL));
    }
}

void Monitor::handle_error(const string& msg, bool critical) {
    cout << "\n" << msg << endl;
    healthy = false;
    if (critical) stop = true;
}

// Stub: multi-target mode (Task 6)
void Monitor::run_multi_monitor() {
    // TODO: Task 6 will implement this
    cout << "多目标监控模式未实现" << endl;
}

int Monitor::check_stock(const string& screen_id, const string& sku_id) {
    // TODO: Task 6 will implement this
    return -1;
}

void Monitor::print_status_change(const string& label, int code) {
    // TODO: Task 6 will implement this
}

string Monitor::get_ms_time() {
    // TODO: Task 6 will implement this
    return "";
}
