//Monitor By FriendshipEnder
//该项目还可能会在Arduino-ESP32上运行. ESP32端可以使用cJSON,但是web请求需要使用WiFiClient库

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <curl/curl.h>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <cstdlib>

//#include <consoleapi2.h>
#include "cJSON.h"

using namespace std;
namespace fs = std::filesystem;

// 配置类
class Config {
public:
    static string TICKET_ID;
    static string BATPATH;
    static int REFRESH_INTERVAL;
    static int TIMEOUT;
    static string API_URL;
    static const vector<string> HEADERS;
    static int TICKETNO;

    static void init() {
        API_URL = "https://show.bilibili.com/api/ticket/project/getV2?version=134&id=" + TICKET_ID;
    }
    static void readconf();
};

const int DEFAULT_REFRESH = 300;
const int DEFAULT_TIMEOUT = 10000;
string Config::TICKET_ID = "102194"; // 修改为 102194 (BW)
string Config::BATPATH = ""; // 修改为 102194 (BW)
int Config::REFRESH_INTERVAL = DEFAULT_REFRESH;
int Config::TIMEOUT = DEFAULT_TIMEOUT;
int Config::TICKETNO = 0;
string Config::API_URL;
const vector<string> Config::HEADERS = {
    "User-Agent: Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36"
};

// 移除前后空格
string trim(const string& str);

// 检查字符串是否为有效正整数
bool isValidPositiveInteger(const string& s);
bool isValidBatPath(const string& path) {
    if (path.empty()) return true; // 允许空路径
    
    // 检查文件扩展名
    string ext = fs::path(path).extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){
        return tolower(c); 
    });
    
    const vector<string> validExts = {".bat", ".sh", ".ps1"};
    if (find(validExts.begin(), validExts.end(), ext) == validExts.end()) {
        cout << "错误：不支持的文件扩展名 " << ext << endl;
        return false;
    }
    
    // 检查文件是否存在
    if (!fs::exists(path)) {
        cout << "错误：文件不存在: " << path << endl;
        return false;
    }
    
    return true;
}
void Config::readconf(){
    
    vector<string> lines;
    bool configValid = true;
    string errorMsg;
    
    // 尝试读取配置文件
    ifstream configFile("config.txt");
    if (configFile.is_open()) {
        string line;
        while (getline(configFile, line)) {
            lines.push_back(line);
        }
        configFile.close();
        
        // 检查行数
        if (lines.size() < 5) {
            configValid = false;
            errorMsg = "错误：配置文件不足5行";
        } 
        // 验证并解析内容
        else {
            // 检查并转换整数参数
            try {
                string tid_str = trim(lines[0]);
                if (!isValidPositiveInteger(tid_str)) {
                    configValid = false;
                    errorMsg = "错误：票务ID必须为正整数";
                } else {
                    TICKET_ID = stoi(tid_str);
                }
            } catch (...) {
                configValid = false;
                errorMsg = "错误：票务ID不是整型";
            }

            if (configValid) {
                try {
                    string tno_str = trim(lines[1]);
                    if (!isValidPositiveInteger(tno_str)) {
                        configValid = false;
                        errorMsg = "错误：票档编号必须为正整数";
                    } else {
                        TICKETNO = stoi(tno_str);
                    }
                } catch (...) {
                    configValid = false;
                    errorMsg = "错误：票档编号不是整型";
                }
            }
            
            // BATPATH 不需要转换/ 验证BATPATH
            if (configValid) {
                BATPATH = trim(lines[2]);
                if (!isValidBatPath(BATPATH)) {
                    configValid = false;
                    errorMsg = "错误：批处理文件的路径无效";
                }
            }
            
            if (configValid) {
                try {
                    string ri_str = trim(lines[3]);
                    if (!isValidPositiveInteger(ri_str)) {
                        configValid = false;
                        errorMsg = "错误：刷新间隔 必须为正整数。";
                    } else {
                        REFRESH_INTERVAL = stoi(ri_str);
                    }
                } catch (...) {
                    configValid = false;
                    errorMsg = "错误：刷新间隔 无法转换为int。";
                }
            }
            
            if (configValid) {
                try {
                    string to_str = trim(lines[4]);
                    if (!isValidPositiveInteger(to_str)) {
                        configValid = false;
                        errorMsg = "错误：超时时间 必须为正整数。";
                    } else {
                        TIMEOUT = stoi(to_str);
                    }
                } catch (...) {
                    configValid = false;
                    errorMsg = "错误：超时时间 转换失败。";
                }
            }
        }
    } else {
        configValid = false;
        errorMsg = "错误：找不到 config.txt 文件。";
    }
    
    // 如果配置无效，删除文件并提示用户输入
    if (!configValid) {
        // 删除存在的配置文件
        if (remove("config.txt") != 0 && lines.size() >= 5) {
            cout << "注意：无法删除无效配置文件，但将继续请求新输入。" << endl;
        }
        
        cout << errorMsg << endl;
        cout << "请重新输入以下配置：" << endl;
        cout << "刷新间隔使用默认值 300ms 超时时间使用默认值 10000ms。" << endl;
        
        // 获取用户输入
        while (true) {
            string input;
            cout << "输入要监视的票务ID. BW2025为102194, BML2025为102626: ";
            getline(cin, input);
            input = trim(input);
            if (isValidPositiveInteger(input)) {
                try {
                    TICKET_ID = stoi(input);
                    break;
                } catch (...) {
                    cout << "数值不合法，请重新输入。" << endl;
                }
            } else {
                cout << "输入必须是正整数，请重新输入。" << endl;
            }
        }
        
        while (true) {
            string input;
            cout << "输入想抢票的票档 (日期, 票种等), 输入0代表不需要抢票, 仅查看票务信息:";
            getline(cin, input);
            input = trim(input);
            if (isValidPositiveInteger(input)) {
                try {
                    TICKETNO = stoi(input);
                    break;
                } catch (...) {
                    cout << "数值不合法，请重新输入" << endl;
                }
            } else {
                cout << "输入必须是正整数，请重新输入" << endl;
            }
        }
        
        cout << "输入监测到对应票档有票时, 执行的批处理路径 (建议绝对路径): ";
        getline(cin, BATPATH);
        BATPATH = trim(BATPATH);
        
        // 保存新的配置文件
        ofstream newConfig("config.txt");
        if (newConfig.is_open()) {
            newConfig << TICKET_ID << endl
                      << TICKETNO << endl
                      << BATPATH << endl
                      << DEFAULT_REFRESH << endl
                      << DEFAULT_TIMEOUT << endl;
            newConfig.close();
            cout << "配置已保存到 config.txt" << endl;
        } else {
            cout << "警告：无法保存新配置文件" << endl;
        }
    }
    
}


// HTTP响应结构体
struct HttpResponse {
    string data;
    long status_code;
    string error;
};

// 回调函数用于接收HTTP响应数据
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    ((string*)userp)->append((char*)contents, realsize);
    return realsize;
}

// 去除字符串前后空白字符
string trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// 检查字符串是否为有效正整数
bool isValidPositiveInteger(const string& s) {
    if (s.empty()) return false;
    
    for (char c : s) {
        if (!isdigit(c)) 
            return false;
    }
    return true;
}

// 执行HTTP GET请求
HttpResponse http_get(const string& url, const vector<string>& headers) {
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

// 处理JSON数据
pair<string, vector<vector<string>>> process_data(const string& json_str) {
    vector<vector<string>> tickets;
    string name;
    
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root) {
        return make_pair("", tickets);
    }
    
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (data) {
        cJSON* name_item = cJSON_GetObjectItemCaseSensitive(data, "name");
        if (cJSON_IsString(name_item) && name_item->valuestring) {
            name = name_item->valuestring;
        }
        
        cJSON* screen_list = cJSON_GetObjectItemCaseSensitive(data, "screen_list");
        if (screen_list && cJSON_IsArray(screen_list)) {
            int screen_count = cJSON_GetArraySize(screen_list);
            //cout << "Debug screen_count: " << screen_count << endl;
            
            for (int i = 0; i < std::min(screen_count, 1000); i++) {
                cJSON* screen = cJSON_GetArrayItem(screen_list, i);
                if (!screen) continue;
                
                cJSON* ticket_list = cJSON_GetObjectItemCaseSensitive(screen, "ticket_list");
                if (ticket_list && cJSON_IsArray(ticket_list)) {
                    int ticket_count = cJSON_GetArraySize(ticket_list);
                    
                    //cout << "  Debug ticket_count: " << ticket_count << endl;
                    for (int j = 0; j < std::min(ticket_count, 10); j++) {
                        cJSON* ticket = cJSON_GetArrayItem(ticket_list, j);
                        if (!ticket) continue;
                        
                        string screen_name = "";
                        string desc = "";
                        string status = "";
                        
                        cJSON* screen_name_item = cJSON_GetObjectItemCaseSensitive(ticket, "screen_name");
                        if (cJSON_IsString(screen_name_item) && screen_name_item->valuestring) {
                            screen_name = screen_name_item->valuestring;
                        }
                        
                        cJSON* desc_item = cJSON_GetObjectItemCaseSensitive(ticket, "desc");
                        if (cJSON_IsString(desc_item) && desc_item->valuestring) {
                            desc = desc_item->valuestring;
                        }
                        
                        cJSON* sale_flag = cJSON_GetObjectItemCaseSensitive(ticket, "sale_flag");
                        if (sale_flag) {
                            cJSON* display_name = cJSON_GetObjectItemCaseSensitive(sale_flag, "display_name");
                            if (cJSON_IsString(display_name) && display_name->valuestring) {
                                status = display_name->valuestring;
                            }
                        }
                        
                        if (!screen_name.empty() || !desc.empty() || !status.empty()) {
                            string ticket_name = screen_name;
                            if (!desc.empty()) {
                                ticket_name += " " + desc;
                            }
                            tickets.push_back({ticket_name, status});
                        }
                    }
                }
            }
        }
    }
    
    cJSON_Delete(root);
    return make_pair(name, tickets);
}

// 计算字符串显示宽度（近似）
size_t display_width(const string& str) {
    size_t width = 0;
    for (char c : str) {
        // 简单处理：中文字符宽度为2，英文字符宽度为1
        if (static_cast<unsigned char>(c) >= 192) {
            width += 2;
        } else if (static_cast<unsigned char>(c) <= 127) {
            width += 1;
        }
    }
    return width;
}

// 清屏函数
void clear_screen() {
#ifdef _WIN32
    system("chcp 65001");
    system("cls");
#else
    system("clear");
#endif
}

/* 显示表格
void show_table(const string& name, const vector<vector<string>>& tickets) {
    if (tickets.empty()) return;
    
    // 计算列宽
    size_t col1_width = 0;
    size_t col2_width = 0;
    
    for (const auto& row : tickets) {
        if (row.size() >= 1) {
            col1_width = max(col1_width, display_width(row[0]));
        }
        if (row.size() >= 2) {
            col2_width = max(col2_width, display_width(row[1]));
        }
    }
    
    // 打印表头
    cout << "\n" << name << "\n";
    cout << string(64, '-') << "\n";
    
    // 打印表体
    for (const auto& row : tickets) {
        if (row.size() < 2) continue;
        
        string first_col = row[0];
        // 填充第一列
        size_t current_width = display_width(first_col);
        if (current_width < col1_width) {
            first_col += string(col1_width - current_width, ' ');
        }
        
        // 第二列右对齐
        string second_col = row[1];
        size_t second_width = display_width(second_col);
        if (second_width < col2_width) {
            // 注意：由于有中文字符，简单的空格填充可能不完美
            second_col = string(col2_width - second_width, ' ') + second_col;
        }
        
        cout << first_col << "  " << second_col << "\n";
    }
    cout << endl;
}*/
// 状态颜色映射
const unordered_map<string, string> StatusColor = {
    {"已售罄", "\033[31m"},   // 红色
    {"已停售", "\033[31m"},      // 红色
    {"不可售", "\033[31m"},      // 红色
    {"未开售", "\033[36m"},      // 青色
    {"暂时售罄", "\033[33m"},  // 黄色
    {"预售中", "\033[32m"},      // 绿色
};

// 显示表格（带颜色和优化布局）// 显示表格（优化宽度和光标定位）
void show_table(const string& name, const vector<vector<string>>& tickets) {
    if (tickets.empty()) return;
    
    /* 计算终端宽度（如果获取失败则使用默认值）
    int term_width = 80;
#ifdef TIOCGSIZE
    struct ttysize ts;
    if (ioctl(STDOUT_FILENO, TIOCGSIZE, &ts) == 0) {
        term_width = ts.ts_cols;
    }
#elif defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        term_width = csbi.dwSize.X;
    }
#endif */
    
    // 计算最大可用宽度（保留20字符用于状态栏和间距）
    //const int max_col1_width = std::max(20, term_width - 25);
    
    // 计算列宽（以英文字符宽度为单位）
    size_t col1_width = 0;
    size_t col2_width = 0;
    
    // 计算票种列实际需要的宽度（限制最大宽度）
    for (const auto& row : tickets) {
        if (row.size() >= 1) {
            size_t width = display_width(row[0]);
            col1_width = max(col1_width, width);
        }
        if (row.size() >= 2) {
            col2_width = max(col2_width, display_width(row[1]));
        }
    }
    
    // 保存当前光标位置
    cout << "\033[s";
    
    // 打印表格标题（粗体）
    cout << "\033[1m"; // 设置粗体
    cout << "\033[K\n" << name << "\n";
    cout << "\033[0m"; // 重置样式
    
    // 打印表头（青色）
    cout << "\033[36m"; // 设置青色
    cout << "\033[K";
    
    // 打印"票种"并填充
    string header1 = "票种";
    size_t padding1 = col1_width - display_width(header1);
    cout << header1 << string(padding1, ' ');
    
    // 移动光标到第二列位置并打印"状态"
    cout << "\033[" << (col1_width + 4) << "G";
    cout << "状态";
    
    cout << "\033[0m"; // 重置颜色
    cout << "\n";
    
    // 打印分隔线
    cout << "\033[K" << string(col1_width + col2_width + 4, '-') << "\n";
    
    // 记录打印的行数
    size_t lines_printed = 0;
    
    // 打印表体
    int no = 0;
    for (const auto& row : tickets) {
        if (row.size() < 2) continue;
        
        cout << "\033[K"; // 清除当前行
        lines_printed++;
        
        // 处理票种文本（限制宽度）
        string ticket_name = row[0];
        size_t ticket_width = display_width(ticket_name);
        
        // 如果票种文本过长，进行截断处理
        if (ticket_width > col1_width) {
            // 计算需要保留的字符数
            size_t keep_chars = 0;
            size_t current_width = 0;
            bool last_char_double = false; // 记录上一个字符是否是双宽度
            
            for (size_t i = 0; i < ticket_name.size(); i++) {
                char c = ticket_name[i];
                size_t char_width = (static_cast<unsigned char>(c) > 127) ? 2 : 1;
                
                // 检查是否超过可用宽度（预留3字符用于省略号）
                if (current_width + char_width > col1_width - 3) {
                    // 如果当前字符是双字节字符且只显示了一半，需要回退
                    if (last_char_double && char_width == 2) {
                        keep_chars--;
                    }
                    
                    ticket_name = ticket_name.substr(0, keep_chars) + "...";
                    break;
                }
                
                current_width += char_width;
                keep_chars++;
                last_char_double = (char_width == 2);
            }
        }
        
        // 打印第一列（默认颜色）
        cout << ticket_name;
        size_t padding = col1_width - display_width(ticket_name);
        if (padding > 0) {
            cout << string(padding, ' ');
        }
        
        // 移动光标到第二列位置
        cout << "\033[" << (col1_width + 4) << "G";
        
        // 打印第二列（带颜色）
        auto it = StatusColor.find(row[1]);
        if (it != StatusColor.end()) {
            cout << it->second << row[1] << "\033[0m"; // 应用颜色并重置
            //if(row[1] == "预售中" && no == Config::TICKETNO - 1) { //命中了 想抢的票种 当前状态为预售中
            //    if (Config::BATPATH == "") cout << "您订阅的票种有票了!" << endl;
            //    else system(Config::BATPATH.c_str()); //预售中, 执行bat
            //}
        } else {
            cout << row[1]; // 默认颜色
        }
        
        // 移动到下一行
        cout << "\n";
        no++;
    }
    
    // 清除最后一行
    cout << "\033[K";
    
    // 移动光标到左下角位置
    // 计算需要下移的行数（终端高度 - 已打印行数 - 标题和表头行数）
    int move_down = max(0, static_cast<int>(lines_printed + 4));
    cout << "\033[" << move_down << "E" << flush; // 向下移动并回到行首
}


// 监控器类
class Monitor {
public:
    Monitor() : stop(false), healthy(true) {
        Config::init();
    }
    
    void start() {
        run_monitor();
    }
    
private:
    bool stop;
    bool healthy;
    vector<vector<string>> last_data;
    
    void run_monitor() {
        while (!stop) {
            auto now = chrono::system_clock::now();
            time_t now_time = chrono::system_clock::to_time_t(now);
            tm now_tm = *localtime(&now_time);
            
            // 显示当前时间
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
                
                // 检查数据是否发生变化
                if (healthy && tickets != last_data) {
                    //clear_screen();
                    //cout << "项目github页面：https://github.com/fsender/BiliTicketMonitor \n\n";
                    //cout << "监控ID: " << Config::TICKET_ID 
                    //     << " | 刷新间隔: " << Config::REFRESH_INTERVAL << "s\n";
                    //cout << "=" << string(38, '=') << "\n";
                    
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
    
    void handle_error(const string& msg, bool critical) {
        cout << "\n" << msg << endl;
        healthy = false;
        if (critical) {
            stop = true;
        }
    }
};

int main() {
    
    clear_screen();
    Config::readconf();//读取配置文件

    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    //clear_screen();
    cout << "项目github页面：https://github.com/fsender/BiliTicketMonitor \n\n";
    cout << "监控ID: " << Config::TICKET_ID 
         << " | 刷新间隔: " << Config::REFRESH_INTERVAL << "ms\n";
    cout << "=" << string(38, '=') << "\n";
    
    Monitor monitor;
    monitor.start();
    
    cout << "\n按回车键退出程序...\n";
    cin.ignore();
    
    curl_global_cleanup();
    return 0;
}