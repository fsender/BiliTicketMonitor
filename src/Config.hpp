#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <format>

using namespace std;

extern const char* FILE_DATA;

struct TargetConfig {
    string screen_id;
    string sku_id;
    string label;
};

// 监控目标配置: ticket_no = 表格中的序号, script_command = 有票时执行的命令
struct MonitoredTargetConfig {
    int ticket_no;
    string script_command;  // 支持 {screen_id} 和 {sku_id} 占位符
};

class Config {
protected:
    bool configValid;
    vector<string> lines;
    string errorMsg;
public:
    static string TICKET_ID;
    static string project_name;
    static int REFRESH_INTERVAL;
    static int TIMEOUT;
    static vector<string> HEADERS;
    static vector<TargetConfig> TARGETS;             // 自动发现的全部票种
    static vector<MonitoredTargetConfig> MONITORED;  // config.txt中配置的要监控的目标
    static bool MONITOR_ALL;                         // true=监控全部+高亮, false=仅监控选定的
    static bool BARK_ENABLED;
    static string BARK_KEY;
    static string BARK_SERVER;
    static string BARK_GROUP;

    bool checkconf();
    void readconf();
    void writeConf();
};

extern const int DEFAULT_REFRESH;
extern const int DEFAULT_TIMEOUT;
extern const string DEFAULT_HEADER;

string trim(const string& str);
bool isValidPositiveInteger(const string& s);
bool isValidTargetConfig(const TargetConfig& tc);
string replace_vars(const string& tmpl, const string& screen_id, const string& sku_id);
