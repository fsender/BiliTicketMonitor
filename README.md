# B站票务监控器 3.1.1  By FriendshipEnder

A Bilibili Ticket Monitor use C++20. Extendable. Multi-platform.

> 🚀 **单轮 24 票种并发请求仅需 ~80ms**（实测平均延迟），零 CPU 空转，性能匹敌 Python 原生方案。

----

## 性能实测

24 票种并发轮询，`--interval 0` 连续模式：

```
当前时间: 18:51:24.003 | 已发送:         1 次 |        24 个请求 | 延迟: 86349 μs
当前时间: 18:51:24.076 | 已发送:         2 次 |        48 个请求 | 延迟: 73359 μs
当前时间: 18:51:24.146 | 已发送:         3 次 |        72 个请求 | 延迟: 69826 μs
```

- **24 并发请求 ≈ 80ms 完成**（Bilibili API 网络延迟）
- **零 CPU 空转**：`curl_multi_wait` 阻塞等待，不轮询
- **连接复用**：curl 句柄跨周期存活，无 TCP 重连开销
- **微秒级计时**：实时显示每轮最慢请求延迟 (μs)

----

## 下载与运行

### 预编译二进制 (推荐)

从 [Releases](https://github.com/fsender/BiliTicketMonitor/releases) 下载对应平台的版本：

| 文件 | 平台 | 依赖 |
|------|------|------|
| `BiliTicketMonitor-linux-x86_64` | Linux x86_64 | libcurl |
| `BiliTicketMonitor-linux-arm64` | Linux ARM64 | libcurl |
| `BiliTicketMonitor-macos-arm64` | macOS Apple Silicon | 无（系统自带） |
| `BiliTicketMonitor-win-x86_64.exe` | Windows x86_64 | 无（静态链接） |

下载后赋予执行权限：
```bash
chmod +x BiliTicketMonitor-linux-x86_64
./BiliTicketMonitor-linux-x86_64 --help
```

### Windows 注意事项

预编译的 Windows 版本已**静态链接 curl**，无需额外安装依赖，开箱即用。

如果要**自行编译** Windows 版本：

```powershell
# 方法 1: 使用 vcpkg（推荐）
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && .\bootstrap-vcpkg.bat
.\vcpkg install curl:x64-windows

# 然后编译：
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE="path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release

# 方法 2: 使用 MSYS2
pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles"
cmake --build build
```

### Linux 依赖

```bash
# Debian/Ubuntu
sudo apt install libcurl4-openssl-dev cmake g++

# Arch Linux
sudo pacman -S curl cmake gcc

# Gentoo
emerge -av net-misc/curl dev-util/cmake sys-devel/gcc
```

### macOS 依赖

```bash
brew install curl cmake
```

----

## 特点:

1. **免登录**, 双击即用, 实时监测B站项目票务信息. Login is not necessary!

2. **多目标并发监控**, 同时监控任意数量的票种.

3. **自定义脚本触发**, 每个目标可独立配置抢票命令, 支持 `{screen_id}` 和 `{sku_id}` 变量替换.

4. **stock/check API**, 比 getV2 更细粒度, 降低风控风险.

5. **Bark 推送通知**, 支持 iOS 推送.

6. 自由度高, 可以自己设置脚本启动批处理文件.

----

## 使用方法

### 首次运行

```bash
# 查看帮助
./BiliTicketMonitor --help

# 查看所有票种 (自动发现)
./BiliTicketMonitor --id 102194
```

### 配置文件

编辑当前目录下的 `config.txt`：

```
Line 1: 票务项目ID (如 102194)
Line 2: 轮询间隔(ms)
Line 3: HTTP超时(ms)
Line 4: User-Agent
Line 5: Bark推送Key (空=禁用)
Line 6: 监控目标数量 (>=0监控全部, <0仅监控选定的)
Line 7+: 序号 [命令]
```

示例——监控 No.5（无脚本，仅通知）和 No.7（有票时执行脚本）：
```
102194
300
10000
User-Agent: Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36

2
5
7 python grab_ticket.py --screen {screen_id} --sku {sku_id}
```

### CLI 参数

| 参数 | 说明 |
|------|------|
| `-h` | 帮助 |
| `--id ID` | 设置票务ID |
| `--interval MS` | 设置轮询间隔(ms) |
| `--bark-key KEY` | 启用 Bark 推送 |
| `--bark-test` | 测试 Bark 推送 |
| `--no-bark` | 禁用 Bark 推送 |

----

## 从源码编译

```bash
git clone https://github.com/fsender/BiliTicketMonitor.git
cd BiliTicketMonitor
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/BiliTicketMonitor --help
```

----

B站票务监控器 3.1.1 By FriendshipEnder