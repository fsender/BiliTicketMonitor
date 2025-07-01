/**  B站票务监控器 Monitor By FriendshipEnder
 * @file main.cpp
 * @author FriendshipEnder (https://space.bilibili.com/180327370)
 * @brief B站票务监控器
 * @version 2.0.0
 * @date 2025-07-01
 * @copyright Copyright (c) 2025
 */

//该项目还可能会在Arduino-ESP32上运行. ESP32端可以使用cJSON,但是web请求需要使用WiFiClient库

#define VERSION "2.0.0"
#define INIT_DELAY_SEC 5 //无参数启动后等待秒数

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <format>
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


const char *welcome = 
"\033[36mB站票务监控器：By FriendshipEnder V " VERSION "\033[0m\n"
"建议通过 \"在终端中打开\" 打开本程序, 然后运行时在后面加上 -h 参数看说明!\n"
"\033[33m建议通过 \"在终端中打开\" 打开本程序, 然后运行时在后面加上 -h 参数看说明!\033[0m\n"
"\033[31m建议通过 \"在终端中打开\" 打开本程序, 然后运行时在后面加上 -h 参数看说明!\033[0m\n"
"参数 \033[35m--id 票务ID\033[0m 来指定票务ID, 默认为102194 (BW2025)\033[0m\n"
"参数 \033[35m--ticket-no 票种标号\033[0m 来指定想蹲的票 (0代表不蹲票仅查看)\033[0m\n"
"本程序为 C++ 版, \033[33m性能更强大\033[0m, 免登录, 灵活性强,\n"
"若要进一步更改票务和票种信息可以 \033[32m可更改 config.txt 文件\033[0m 重新配置\n"
"运行前使用参数 \033[35m-h\033[0m 来查看更多的帮助信息, 帮助信息很重要的\033[0m\n"
"项目github页面：\033[36mhttps://github.com/fsender/BiliTicketMonitor\033[0m\n"
"  \033[34m按 Control-C 退出本程序.\033[0m\n"
"监测模式";

const char *help = R"(
B站票务监控器

options:
  -h, --help            显示帮助信息并退出
  -v, --version         显示版本号信息并退出
  --id ID               要监控的票务ID. BW2025 为 102194, BML2025 为 102626.
  --ticket-no TICKET_NO
                        需要蹲票的票种代号, 0表示不蹲票, 1~票种个数代表蹲对应的票
  --interval INTERVAL   刷新间隔 单位秒. 默认为0.3s. 作者未测试设为0.3s以下的检测速率会导致什么后果.
                        本程序不需要登录b站, 但同时运行本程序与BHYG, 较低间隔仍可能导致同IP风控. 多机模式
                        (本程序和BHYG不在同一IP地址) 下, 可以适当降低到更低的刷新间隔.
  --script SCRIPT       辅助脚本, 输入批处理文件 (*.bat, *.sh, *.ps1) 的路径后,
                        如果发现监视的票种如果有余票, 则会启动该批处理脚本.
                        仓库内自带的 bhyg.bat 文件可以用于启动 bhyg , 只需要将本程序 bhyg 目录内即可使用

                        当前 暂不支持网页端抢票 (web), 只支持 BHYG By ZianTT (bhyg)
                        当配合 BHYG 使用时, 请先在BHYG中确定需要购买的票种, 在 BHYG
                        预填写抢票票种和购买人等参数, 并把抢票延时设置为 1 毫秒, 然后选择 '开始抢票' 选项,
                        直到弹出 '请确认信息，以进入抢票进程 (倒计时) ' 的时候, 以正常模式运行该脚本
                        (记得带要抢的票的ticket-no 参数), 最下方以绿色显示当前时间后, 设置 BHYG 为活跃窗口,
                        即可开始蹲票默认设定下, 本脚本检测到 "预售中" 时会自动激活BHYG抢票, 持续8秒,
                        随后自动关闭 BHYG 的抢票模式, 重新进入等待模式.
)";
const char *version = "BiliTicketMonitor Version " VERSION "\n作者: FriendshipEnder (B站同名)\n项目github页面：https://github.com/fsender/BiliTicketMonitor";

const char *FILE_DATA = R"(
# 说明: 第一行为B站会员购的票务ID, 即链接 https://show.bilibili.com/platform/detail.html?id=102194 后面的ID数字
# 第二行为票种No. 具体对应哪个票种 (时间、票档) 需要依照票务ID
# 第三行为脚本批处理文件路径吗支持 bat, sh 格式的批处理程序. 批处理程序可以调用python等程序, 此处的自定义程度极高
# 第四行为票务信息 (余票监测) Get API 的调用间隔, 单位ms
# 第五行不建议修改, 为 Get API 的超时时间, 单位ms
# 第六行不建议修改, 为 Get API 的请求链接, 用 {0} 来代表抢票ID数据段
# 第七行不建议修改, 为请求标识符, 就是浏览器请求标识.
)";

// 配置类
class Config {
    protected: 
        bool configValid;
        vector<string> lines;
        string errorMsg;
    public:
        static string TICKET_ID;
        static string BATPATH;
        static int REFRESH_INTERVAL;
        static int TIMEOUT;
        static string API_BASE;
        static string API_URL;
        static vector<string> HEADERS;
        static int TICKETNO;

        static void init() {
            API_URL = vformat(API_BASE, make_format_args(TICKET_ID));
        }
        bool checkconf();
        void readconf();
        void writeConf();
};

const int DEFAULT_REFRESH = 300;
const int DEFAULT_TIMEOUT = 10000;
const string DEFAULT_API_BASE = "https://show.bilibili.com/api/ticket/project/getV2?version=134&id={0}";
const string DEFAULT_HEADER = "User-Agent: Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36";
string Config::TICKET_ID = "102194"; // 修改为 102194 (BW)
string Config::BATPATH = ""; // 修改为 102194 (BW)
int Config::REFRESH_INTERVAL = DEFAULT_REFRESH;
int Config::TIMEOUT = DEFAULT_TIMEOUT;
int Config::TICKETNO = 0;
string Config::API_BASE = DEFAULT_API_BASE;
string Config::API_URL;
vector<string> Config::HEADERS = { DEFAULT_HEADER };

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
bool Config::checkconf(){
    configValid = true;
    // 尝试读取配置文件
    ifstream configFile("config.txt");
    if (configFile.is_open()) {
        string line;
        while (getline(configFile, line)) {
            lines.push_back(line);
        }
        configFile.close();
        
        // 检查行数
        if (lines.size() < 7) {
            configValid = false;
            errorMsg = "错误：配置文件行数不足";
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
                    TICKET_ID = tid_str;
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
            API_BASE=lines[5];
            HEADERS[0]=lines[6];
        }
    } else {
        configValid = false;
        errorMsg = "错误：找不到 config.txt 文件。";
    }
    return configValid;
}
void Config::readconf(){
    
    // 如果配置无效，删除文件并提示用户输入
    //if (!configValid) {
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
                    TICKET_ID = input;
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

        REFRESH_INTERVAL = DEFAULT_REFRESH;
        TIMEOUT = DEFAULT_TIMEOUT;
        writeConf();
    //}
    
}
void Config::writeConf(){
    // 保存新的配置文件
    ofstream newConfig("config.txt");
    if (newConfig.is_open()) {
        newConfig << TICKET_ID << endl
                    << TICKETNO << endl
                    << BATPATH << endl
                    << REFRESH_INTERVAL << endl
                    << TIMEOUT << endl
                    << API_BASE << endl
                    << HEADERS[0] << endl
                    << FILE_DATA << endl;
        newConfig.close();
        cout << "配置已保存到 config.txt" << endl;
    } else {
        cout << "警告：无法保存新配置文件" << endl;
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
    bool wflag = 0;
    for (char c : str) {
        // 简单处理：中文字符宽度为2，英文字符宽度为1
        if (static_cast<unsigned char>(c) >= 192) {
            width += 2;
        } else if (static_cast<unsigned char>(c) == '\033') { //是转义字符, 直到读取到 'm'
            wflag = 1;
        } else if (static_cast<unsigned char>(c) <= 127) {
            if(wflag && c=='m') wflag=0;
            else if(!wflag) width += 1;
        }
    }
    return width;
}

// 清屏函数
void clear_screen() {
#ifdef _WIN32
    system("chcp 65001"); //设置终端编码 UTF-8
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


// 监控器类
class Monitor {
private:
    bool stop;
    bool healthy;
    bool selling;
public:
    Monitor() : stop(false), healthy(true), selling(false){
        Config::init();
    }
    
    void start() {
        run_monitor();
    }
    
private:
    vector<vector<string>> last_data;
    void show_table(const string& name, const vector<vector<string>>& tickets);
    
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
                if (healthy && (tickets != last_data || selling)) {
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

// 显示表格（带颜色和优化布局）// 显示表格（优化宽度和光标定位）
void Monitor::show_table(const string& name, const vector<vector<string>>& tickets) {
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
    col1_width += 6; // 票档 No.
    
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
    string header1 = "No.   票种";
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
        no++;
        if (row.size() < 2) continue;
        
        cout << "\033[K"; // 清除当前行
        lines_printed++;
        
        // 处理票种文本（限制宽度）
        string ticket_name = format("\033[33m[{:>2}]\033[0m  ", no) + row[0];
        //是你想抢的票种
        if(no == Config::TICKETNO){
            ticket_name = format("\033[35m\033[1m[{:>2}]  ", no) + row[0] + "\033[0m";
        }
        size_t ticket_width = display_width(ticket_name);
        
        // 如果票种文本过长，进行截断处理
        if (ticket_width > col1_width) {
            // 计算需要保留的字符数
            size_t keep_chars = 0;
            size_t current_width = 0;
            bool last_char_double = false; // 记录上一个字符是否是双宽度
            
            for (size_t i = 0; i < ticket_name.size(); i++) {
                char c = ticket_name[i];
                size_t char_width = (static_cast<unsigned char>(c) > 191) ? 2 : ((static_cast<unsigned char>(c) > 127) ? 0 : 1);
                
                // 检查是否超过可用宽度（预留2字符用于省略号）
                if (current_width + char_width > col1_width - 2) {
                    // 如果当前字符是双字节字符且只显示了一半，需要回退
                    if (last_char_double && char_width == 2) {
                        keep_chars--;
                    }
                    
                    ticket_name = ticket_name.substr(0, keep_chars) + "..";
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
            if(no == Config::TICKETNO) { //命中了 想抢的票种 当前状态为预售中
                selling = (row[1] == "预售中");
            }
        } else {
            cout << row[1]; // 默认颜色
        }
        
        // 移动到下一行
        cout << "\n";
    }
    
    // 清除最后一行
    cout << "\033[K";
    
    // 移动光标到左下角位置
    // 计算需要下移的行数（终端高度 - 已打印行数 - 标题和表头行数）
    int move_down = max(0, static_cast<int>(lines_printed + 4));
    cout << "\033[" << move_down << "E" << flush; // 向下移动并回到行首
    if(selling) {
        if (Config::BATPATH == "") cout << "您订阅的票种有票了!" << endl;
        else system(Config::BATPATH.c_str()); //预售中, 执行bat
    }
}

void show_welcome(){
    cout << welcome << endl;
    for(int i = 0;i<INIT_DELAY_SEC;i++){
        cout << format("{} 秒后进入监测模式\r", INIT_DELAY_SEC-i);
        this_thread::sleep_for(chrono::seconds(1));
    }
    cout << endl;
}

int main(int argc, const char **argv) {
    
    clear_screen();
    Config config;
    if(argc == 1){
        show_welcome();
        if(!config.checkconf()) config.readconf();//读取配置文件
    }
    if(argc > 1){
        if(argc == 2){ //只有一个参数
            string strtrim = trim(string(argv[1]));
            if(strtrim == string("-h") || strtrim == string("--help")){ //帮助
                cout << help << endl;
                return 0;
            }
            else if(strtrim == string("-v") || strtrim == string("--version")){ //版本
                cout << version << endl;
                return 0;
            }
            else {
                cout << "参数太少, 已无视参数." << endl;
                show_welcome();
                if(!config.checkconf()) config.readconf();//读取配置文件
            }
        }
        else{ //argv >=3
            //处理argc参数, 最后写入Conf文件并开始运行
            for(int i=0;i<argc/2;i++){
                if(string(argv[i*2+1]) == string("--id")){
                    long tid = atoi(argv[i*2+2]);
                    Config::TICKET_ID = to_string(tid);
                }
                if(string(argv[i*2+1]) == string("--ticket-no")){
                    Config::TICKETNO = atoi(argv[i*2+2]);
                }
                if(string(argv[i*2+1]) == string("--interval")){ //间隔
                    Config::REFRESH_INTERVAL = atoi(argv[i*2+2]);
                }
                if(string(argv[i*2+1]) == string("--script")){ //脚本bat地址
                    Config::BATPATH = string(argv[i*2+2]);
                }
            }
            config.writeConf();
        }
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    //clear_screen();
    cout << "\n\033[33m监控ID: " << Config::TICKET_ID 
         << " | 刷新间隔: " << Config::REFRESH_INTERVAL << "ms\n";
    cout << "===============================================================\033[0m" << "\n";
    
    Monitor monitor;
    monitor.start();
    
    cout << "\n按回车键退出程序...\n";
    cin.ignore();
    
    curl_global_cleanup();
    return 0;
}