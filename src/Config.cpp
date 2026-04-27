#include "Config.hpp"

using namespace std;

const char* FILE_DATA = R"(
# 说明: 第一行为B站会员购的票务ID
# 第二行为API调用间隔, 单位ms
# 第三行为API超时时间, 单位ms
# 第四行为请求标识符 (User-Agent)
# 第五行为Bark推送Key (空=禁用)
# 第六行为监控目标数量 (0=监控全部自动发现的票种)
# 第七行起: 每行格式为 "序号 命令", 序号为启动时表格中显示的编号
#          命令中的 {screen_id} 和 {sku_id} 会被替换为实际值
# 示例: "5 python grab_ticket.py --screen {screen_id} --sku {sku_id}"
)";

const int DEFAULT_REFRESH = 300;
const int DEFAULT_TIMEOUT = 10000;
const string DEFAULT_HEADER = "User-Agent: Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36";

string Config::TICKET_ID = "102194";
string Config::project_name;
int Config::REFRESH_INTERVAL = DEFAULT_REFRESH;
int Config::TIMEOUT = DEFAULT_TIMEOUT;
vector<string> Config::HEADERS = { DEFAULT_HEADER };
vector<TargetConfig> Config::TARGETS;
vector<MonitoredTargetConfig> Config::MONITORED;
bool Config::BARK_ENABLED = false;
string Config::BARK_KEY = "";
string Config::BARK_SERVER = "https://api.day.app";
string Config::BARK_GROUP = "票务监控";

// 替换命令中的占位符
string replace_vars(const string& tmpl, const string& screen_id, const string& sku_id) {
    string result = tmpl;
    size_t pos;
    while ((pos = result.find("{screen_id}")) != string::npos)
        result.replace(pos, 11, screen_id);
    while ((pos = result.find("{sku_id}")) != string::npos)
        result.replace(pos, 7, sku_id);
    return result;
}

string trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool isValidPositiveInteger(const string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!isdigit(c)) return false;
    return true;
}

bool isValidTargetConfig(const TargetConfig& tc) {
    if (!isValidPositiveInteger(tc.screen_id)) return false;
    if (!isValidPositiveInteger(tc.sku_id)) return false;
    if (tc.label.empty()) return false;
    return true;
}

bool Config::checkconf() {
    configValid = true;
    lines.clear();
    ifstream configFile("config.txt");
    if (configFile.is_open()) {
        string line;
        while (getline(configFile, line)) lines.push_back(line);
        configFile.close();
    }

    // 最少需要6行 (TICKET_ID, REFRESH_INTERVAL, TIMEOUT, UA, BARK_KEY, TARGET_COUNT)
    if (lines.size() < 6) {
        configValid = false;
        errorMsg = "配置文件行数不足";
        return configValid;
    }

    int idx = 0;
    // Line 1: TICKET_ID
    string tid = trim(lines[idx++]);
    if (!isValidPositiveInteger(tid)) {
        configValid = false;
        errorMsg = "票务ID必须为正整数";
        return configValid;
    }
    TICKET_ID = tid;

    // Line 2: REFRESH_INTERVAL
    string ri = trim(lines[idx++]);
    if (isValidPositiveInteger(ri)) REFRESH_INTERVAL = stoi(ri);

    // Line 3: TIMEOUT
    string to = trim(lines[idx++]);
    if (isValidPositiveInteger(to)) TIMEOUT = stoi(to);

    // Line 4: User-Agent
    HEADERS[0] = trim(lines[idx++]);
    if (HEADERS[0].empty()) HEADERS[0] = DEFAULT_HEADER;

    // Line 5: BARK_KEY
    BARK_KEY = trim(lines[idx++]);
    BARK_ENABLED = !BARK_KEY.empty();

    // Line 6: Target count
    MONITORED.clear();
    if (idx < (int)lines.size()) {
        int target_count = 0;
        try { target_count = stoi(trim(lines[idx])); } catch (...) {}
        idx++;
        for (int ti = 0; ti < target_count && idx < (int)lines.size(); ti++, idx++) {
            string tl = trim(lines[idx]);
            if (tl.empty()) { ti--; continue; }
            size_t sp = tl.find(' ');
            MonitoredTargetConfig mtc;
            if (sp != string::npos && isValidPositiveInteger(tl.substr(0, sp))) {
                mtc.ticket_no = stoi(tl.substr(0, sp));
                mtc.script_command = tl.substr(sp + 1);
            } else if (isValidPositiveInteger(tl)) {
                mtc.ticket_no = stoi(tl);
                mtc.script_command = "";
            } else continue;
            MONITORED.push_back(mtc);
        }
    }

    return configValid;
}

void Config::readconf() {
    if (remove("config.txt") != 0) {
        cout << "注意：无法删除旧配置文件" << endl;
    }
    cout << errorMsg << endl;
    cout << "请重新输入配置（刷新间隔使用默认值 " << DEFAULT_REFRESH << "ms）" << endl;

    while (true) {
        string input;
        cout << "输入要监视的票务ID: ";
        getline(cin, input);
        input = trim(input);
        if (isValidPositiveInteger(input)) {
            TICKET_ID = input;
            break;
        }
        cout << "输入必须是正整数" << endl;
    }

    REFRESH_INTERVAL = DEFAULT_REFRESH;
    TIMEOUT = DEFAULT_TIMEOUT;
    cout << "请在 config.txt 中配置监控目标和脚本命令。" << endl;
    writeConf();
}

void Config::writeConf() {
    ofstream newConfig("config.txt");
    if (newConfig.is_open()) {
        newConfig << TICKET_ID << endl
                  << REFRESH_INTERVAL << endl
                  << TIMEOUT << endl
                  << HEADERS[0] << endl
                  << BARK_KEY << endl
                  << MONITORED.size() << endl;
        for (const auto& mt : MONITORED) {
            newConfig << mt.ticket_no << " " << mt.script_command << endl;
        }
        newConfig << FILE_DATA << endl;
        newConfig.close();
        cout << "配置已保存到 config.txt" << endl;
    } else {
        cout << "警告：无法保存配置文件" << endl;
    }
}
