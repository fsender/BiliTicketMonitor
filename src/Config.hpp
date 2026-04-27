#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <format>

using namespace std;
namespace fs = std::filesystem;

extern const char* FILE_DATA;

struct TargetConfig {
    string screen_id;
    string sku_id;
    string label;
};

class Config {
protected:
    bool configValid;
    vector<string> lines;
    string errorMsg;
public:
    static string TICKET_ID;
    static string BATPATH;
    static string project_name;
    static int REFRESH_INTERVAL;
    static int TIMEOUT;
    static string API_BASE;
    static string API_URL;
    static vector<string> HEADERS;
    static vector<TargetConfig> TARGETS;
    static bool BARK_ENABLED;
    static string BARK_KEY;
    static string BARK_SERVER;
    static string BARK_GROUP;

    static void init() {
        API_URL = vformat(API_BASE, make_format_args(TICKET_ID));
    }
    bool checkconf();
    void readconf();
    void writeConf();
};

extern const int DEFAULT_REFRESH;
extern const int DEFAULT_TIMEOUT;
extern const string DEFAULT_API_BASE;
extern const string DEFAULT_HEADER;

string trim(const string& str);
bool isValidPositiveInteger(const string& s);
bool isValidBatPath(const string& path);
bool isValidTargetConfig(const TargetConfig& tc);
