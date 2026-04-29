/**  B站票务监控器 Monitor By FriendshipEnder
 * @file main.cpp
 * @author FriendshipEnder (https://space.bilibili.com/180327370)
 * @brief B站票务监控器
 * @date 2025-07-01
 * @date 2026-04-28
 * @copyright Copyright (c) 2025
 */

#define VERSION "3.1.1"
#define INIT_DELAY_SEC 1

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <format>
#include <cstdlib>
#include <curl/curl.h>

#include "Config.hpp"
#include "Utils.hpp"
#include "Monitor.hpp"

using namespace std;

const char *welcome = 
"····································································\n"
":\033[31m ____   ____  _      ____  ______  ____   __  __  _    ___ ______ \033[0m:\n"
":\033[31m|    \\ |    || |    |    ||      ||    | /  ]|  |/ ]  /  _]      |\033[0m:\n"
":\033[33m|  o  ) |  | | |     |  | |      | |  | /  / |  ' /  /  [_|      |\033[0m:\n"
":\033[32m|     | |  | | |___  |  | |_|  |_| |  |/  /  |    \\ |    _]_|  |_|\033[0m:\n"
":\033[36m|  O  | |  | |     | |  |   |  |   |  /   \\_ |     \\|   [_  |  |  \033[0m:\n"
":\033[34m|     | |  | |     | |  |   |  |   |  \\     ||  .  ||     | |  |  \033[0m:\n"
":\033[35m|_____||____||_____||____|  |__|  |____\\____||__|\\_||_____| |__|  \033[0m:\n"
":                   F R I E N D S H I P E N D E R                  :\n"
":\033[35m         ___ ___   ___   ____   ____  ______   ___   ____         \033[0m:\n"
":\033[35m        |   |   | /   \\ |    \\ |    ||      | /   \\ |    \\        \033[0m:\n"
":\033[34m        | _   _ ||     ||  _  | |  | |      ||     ||  D  )       \033[0m:\n"
":\033[36m        |  \\_/  ||  O  ||  |  | |  | |_|  |_||  O  ||    /        \033[0m:\n"
":\033[32m        |   |   ||     ||  |  | |  |   |  |  |     ||    \\        \033[0m:\n"
":\033[33m        |   |   ||     ||  |  | |  |   |  |  |     ||  .  \\       \033[0m:\n"
":\033[31m        |___|___| \\___/ |__|__||____|  |__|   \\___/ |__|\\_|       \033[0m:\n"
"····································································\n"
"\033[36mB站票务监控器：By FriendshipEnder V " VERSION "\033[0m\n"
"\033[32m" VERSION " 版本说明：并行请求架构, 更快请求, 更高性能!!\033[0m\n"
"\033[32m3.0.0 版本说明：新API, 降低风控风险!! 支持监视多个票种并单独配置自定义命令行抢票功能!!\033[0m\n"
"建议通过 \"在终端中打开\" 打开本程序, 然后运行时在后面加上 -h 参数看说明!\n"
"\033[33m汐❗加❗加❗编❗写❗, 性❗能❗更❗强❗大❗\033[0m\n"
"\033[31m超 强 性 能 😎😎😎无 所 畏 惧 ☝☝☝🤓🤓🤓👉👉👉\033[0m\n"
"\033[36m❤️ 🧡 💛 想听软件作者深夜肚子里消化的咕噜噜声音ASMR 💛 🧡 ❤️ 的+此QQ群: \033[33m👉👉1098897438 \033[0m\n"
"参数 \033[35m--id 票务ID\033[0m 来指定票务ID, 默认为102194 (BW2025)\033[0m\n"
"参数 \033[35m--ticket-no 票种标号\033[0m 来指定想蹲的票 (0代表不蹲票仅查看)\033[0m\n"
"本程序为 C++ 版, \033[33m性能更强大\033[0m, 免登录, 灵活性强,\n"
"若要进一步更改票务和票种信息可以 \033[32m可更改 config.txt 文件\033[0m 重新配置\n"
"\033[32m运行前使用参数 \033[35m-h\033[32m 来查看更多的帮助信息, 帮助信息很重要的\033[0m\n"
"项目github页面：\033[36mhttps://github.com/fsender/BiliTicketMonitor\033[0m\n"
"  \033[34m按 Control-C 退出本程序.\033[0m\n"
"监测模式";

const char *help = R"(
B站票务监控器

options:
  -h, --help            显示帮助信息并退出
  -v, --version         显示版本号信息并退出
  --id ID               要监控的票务ID.
  --interval INTERVAL   刷新间隔 单位毫秒. 默认为300ms.
  --bark-key KEY        启用Bark推送通知并设置Key.
  --bark-server URL     Bark服务器地址 (默认: https://api.day.app).
  --no-bark             禁用Bark推送通知.
  --bark-test           测试Bark推送配置.

默认读取 config.txt 配置文件。
监控目标在 config.txt 中配置: 第6行为目标数量, 后续每行 "序号 命令"
)";
const char *version = "BiliTicketMonitor Version " VERSION "\n作者: FriendshipEnder (B站同名)\n项目github页面：https://github.com/fsender/BiliTicketMonitor";

void show_welcome(){
    cout << welcome << endl;
    //for(int i = 0;i<INIT_DELAY_SEC;i++){
    //    cout << format("{} 秒后进入监测模式\r", INIT_DELAY_SEC-i);
    //    this_thread::sleep_for(chrono::seconds(1));
    //}
    //cout << endl;
}

int main(int argc, const char **argv) {
    
    clear_screen();
    Config config;
    // 先读取配置文件
    show_welcome();
    if(!config.checkconf()) {
        config.readconf();
    }
    
    // 再用CLI参数覆盖
    if(argc > 1){
        string first = trim(string(argv[1]));
        if(argc == 2 && (first == "-h" || first == "--help")){
            cout << help << endl;
            return 0;
        }
        if(argc == 2 && (first == "-v" || first == "--version")){
            cout << version << endl;
            return 0;
        }
        if(argc > 2){
            for(int i=1;i<argc;i++){
                string arg = string(argv[i]);
                if(arg == string("--id") && i+1 < argc){
                    Config::TICKET_ID = to_string(atoi(argv[++i]));
                }
                else if(arg == string("--interval") && i+1 < argc){
                    Config::REFRESH_INTERVAL = atoi(argv[++i]);
                }
                else if(arg == string("--bark-key") && i+1 < argc){
                    Config::BARK_KEY = string(argv[++i]);
                    Config::BARK_ENABLED = !Config::BARK_KEY.empty();
                }
                else if(arg == string("--bark-server") && i+1 < argc){
                    Config::BARK_SERVER = string(argv[++i]);
                }
                else if(arg == string("--no-bark")){
                    Config::BARK_ENABLED = false;
                }
            }
            config.writeConf();
        }
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    bool bark_test_mode = false;
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--bark-test") { bark_test_mode = true; break; }
    }
    
    cout << "\n\033[33m监控ID: " << Config::TICKET_ID 
         << " | 刷新间隔: " << Config::REFRESH_INTERVAL << "ms\n";
    cout << "===============================================================\033[0m" << "\n";
    
    if (bark_test_mode) {
        BarkClient::test();
        cout << "\n按回车键退出程序...\n";
        cin.ignore();
        curl_global_cleanup();
        return 0;
    }
    
    Monitor monitor;
    monitor.start();
    
    cout << "\n按回车键退出程序...\n";
    cin.ignore();
    
    curl_global_cleanup();
    return 0;
}
