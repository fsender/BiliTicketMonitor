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
  --interval INTERVAL   刷新间隔 单位秒. 默认为0.3s.
  --script SCRIPT       辅助脚本, 输入批处理文件 (*.bat, *.sh, *.ps1) 的路径后,
                        如果发现监视的票种如果有余票, 则会启动该批处理脚本.
  --target SCREEN_ID:SKU_ID:LABEL
                        多目标监控模式 (可多次使用). 格式: 场次ID:票种ID:标签.
                        例如: --target 332913:857648:Day1 --target 332914:857522:Day2
  --bark-key KEY        启用Bark推送通知并设置Key.
  --bark-server URL     Bark服务器地址 (默认: https://api.day.app).
  --no-bark             禁用Bark推送通知.

                        当配合 BHYG 使用时, 请先在BHYG中确定需要购买的票种, 在 BHYG
                        预填写抢票票种和购买人等参数, 并把抢票延时设置为 1 毫秒, 然后选择 '开始抢票' 选项,
                        直到弹出 '请确认信息，以进入抢票进程 (倒计时) ' 的时候, 以正常模式运行该脚本
                        (记得带要抢的票的ticket-no 参数), 最下方以绿色显示当前时间后, 设置 BHYG 为活跃窗口,
                        即可开始蹲票默认设定下, 本脚本检测到 "预售中" 时会自动激活BHYG抢票, 持续8秒,
                        随后自动关闭 BHYG 的抢票模式, 重新进入等待模式.
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
            config.readconf();//读取配置文件
        }
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
            for(int i=1;i<argc;i++){
                string arg = string(argv[i]);
                
                if(arg == string("--id") && i+1 < argc){
                    long tid = atoi(argv[++i]);
                    Config::TICKET_ID = to_string(tid);
                }
                else if(arg == string("--ticket-no") && i+1 < argc){
                    Config::TICKETNO = atoi(argv[++i]);
                }
                else if(arg == string("--interval") && i+1 < argc){
                    Config::REFRESH_INTERVAL = atoi(argv[++i]);
                }
                else if(arg == string("--script") && i+1 < argc){
                    Config::BATPATH = string(argv[++i]);
                }
                else if(arg == string("--target") && i+1 < argc){
                    string val = string(argv[++i]);
                    // 格式: screen_id:sku_id:label
                    size_t c1 = val.find(':');
                    size_t c2 = val.rfind(':');
                    if (c1 != string::npos && c2 != string::npos && c1 != c2) {
                        TargetConfig tc;
                        tc.screen_id = val.substr(0, c1);
                        tc.sku_id = val.substr(c1 + 1, c2 - c1 - 1);
                        tc.label = val.substr(c2 + 1);
                        if (isValidTargetConfig(tc)) {
                            Config::TARGETS.push_back(tc);
                        }
                    }
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
                else if(arg == string("--bark-test")){
                    // 将在curl初始化后执行
                }
            }
            config.writeConf();
        }
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // 检查是否有--bark-test参数
    bool bark_test_mode = false;
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--bark-test") {
            bark_test_mode = true;
            break;
        }
    }
    
    //clear_screen();
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
