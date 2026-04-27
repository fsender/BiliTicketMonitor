/**  B站票务监控器 Monitor By FriendshipEnder
 * @file main.cpp
 * @author FriendshipEnder (https://space.bilibili.com/180327370)
 * @brief B站票务监控器
 * @version 2.0.0
 * @date 2025-07-01
 * @copyright Copyright (c) 2025
 */

#define VERSION "2.0.0"
#define INIT_DELAY_SEC 5

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
"\033[36mB站票务监控器：By FriendshipEnder V " VERSION "\033[0m\n"
"建议通过 \"在终端中打开\" 打开本程序\n"
"运行前使用参数 \033[35m-h\033[0m 来查看帮助信息\n"
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
        if(!config.checkconf()) {
            show_welcome();
            config.readconf();
        }
    }
    if(argc > 1){
        if(argc == 2){
            string strtrim = trim(string(argv[1]));
            if(strtrim == string("-h") || strtrim == string("--help")){
                cout << help << endl;
                return 0;
            }
            else if(strtrim == string("-v") || strtrim == string("--version")){
                cout << version << endl;
                return 0;
            }
            else {
                cout << "参数太少, 已无视参数." << endl;
                show_welcome();
                if(!config.checkconf()) config.readconf();
            }
        }
        else{
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
