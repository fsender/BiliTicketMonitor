#include "Config.hpp"

using namespace std;
namespace fs = std::filesystem;

const char* FILE_DATA = R"(
# 说明: 第一行为B站会员购的票务ID, 即链接 https://show.bilibili.com/platform/detail.html?id=102194 后面的ID数字
# 第二行为票种No. 具体对应哪个票种 (时间、票档) 需要依照票务ID
# 第三行为脚本批处理文件路径吗支持 bat, sh 格式的批处理程序. 批处理程序可以调用python等程序, 此处的自定义程度极高
# 第四行为票务信息 (余票监测) Get API 的调用间隔, 单位ms
# 第五行不建议修改, 为 Get API 的超时时间, 单位ms
# 第六行不建议修改, 为 Get API 的请求链接, 用 {0} 来代表抢票ID数据段
# 第七行不建议修改, 为请求标识符, 就是浏览器请求标识.
)";

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
vector<TargetConfig> Config::TARGETS;
bool Config::BARK_ENABLED = false;
string Config::BARK_KEY = "";
string Config::BARK_SERVER = "https://api.day.app";
string Config::BARK_GROUP = "票务监控";

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

// 检查目标配置是否有效
bool isValidTargetConfig(const TargetConfig& tc) {
    if (!isValidPositiveInteger(tc.screen_id)) return false;
    if (!isValidPositiveInteger(tc.sku_id)) return false;
    if (tc.label.empty()) return false;
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
            
            // 解析扩展配置 (第8行起, 可选)
            if (lines.size() >= 8) {
                BARK_KEY = trim(lines[7]);
                BARK_ENABLED = !BARK_KEY.empty();
            }
            if (lines.size() >= 9) {
                try {
                    int target_count = stoi(trim(lines[8]));
                    if (target_count > 0 && lines.size() >= (size_t)(9 + target_count)) {
                        TARGETS.clear();
                        for (int ti = 0; ti < target_count; ti++) {
                            string tl = trim(lines[9 + ti]);
                            // 格式: screen_id,sku_id,label (逗号分隔)
                            size_t c1 = tl.find(',');
                            size_t c2 = tl.rfind(',');
                            if (c1 != string::npos && c2 != string::npos && c1 != c2) {
                                TargetConfig tc;
                                tc.screen_id = tl.substr(0, c1);
                                tc.sku_id = tl.substr(c1 + 1, c2 - c1 - 1);
                                tc.label = tl.substr(c2 + 1);
                                if (isValidTargetConfig(tc)) {
                                    TARGETS.push_back(tc);
                                }
                            }
                        }
                    }
                } catch (...) {
                    // targets解析失败不是致命错误，保持TARGETS为空
                }
            }
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
    // 保存新的配置文件 (扩展格式: 第8行起为可选扩展配置)
    ofstream newConfig("config.txt");
    if (newConfig.is_open()) {
        newConfig << TICKET_ID << endl
                  << TICKETNO << endl
                  << BATPATH << endl
                  << REFRESH_INTERVAL << endl
                  << TIMEOUT << endl
                  << API_BASE << endl
                  << HEADERS[0] << endl;
        // 第8行: Bark Key (空=禁用)
        newConfig << BARK_KEY << endl;
        // 第9行: 目标数量
        newConfig << TARGETS.size() << endl;
        // 第10+行: 每个目标一行 screen_id,sku_id,label
        for (const auto& target : TARGETS) {
            newConfig << target.screen_id << ","
                      << target.sku_id << ","
                      << target.label << endl;
        }
        newConfig << FILE_DATA << endl;
        newConfig.close();
        cout << "配置已保存到 config.txt" << endl;
    } else {
        cout << "警告：无法保存新配置文件" << endl;
    }
}
