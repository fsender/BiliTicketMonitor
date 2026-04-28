# 3.0.3

## Release 3.0.3 - 2026/4/28

1. 修复Windows字体编码问题.

## Release 3.0.2 - 2026/4/28

1. 修复未订阅的票种也提示"您订阅的票种有票了"
2. Bark 推送仅对已订阅票种发送
3. request_count 计数修正为按轮次统计
4. Alpine 静态编译: 修复 CMake 静态库查找路径

## Release 3.0.1 - 2026/4/28

1. 修复长时间运行卡死：事件循环增加 `Config::TIMEOUT` 超时保护
2. 异步脚本执行：`system()` 改为 `std::thread` 异步调用，不阻塞轮询
3. 跨平台静态编译：新增 Alpine Linux 全静态构建（零依赖单文件）
4. 修复 release 文件命名：4 平台独立命名，无重复

## Release 3.0.0 - 2026/4/28

1. 全新异步 HTTP 轮询引擎 (curl_multi) — 替代多线程阻塞模型
2. SIMD JSON 解析 (simdjson) — 替代 cJSON，性能提升 10x+
3. stock/check API 替换 getV2 — 细粒度 SKU 级库存检测，降低风控风险
4. 多目标并发监控 — 同时监控任意数量的票种
5. 自定义脚本触发 — 每目标独立配置抢票命令，支持 {screen_id}/{sku_id} 变量替换
6. 配置系统重构 — config.txt 简化，支持按表格序号选择监控目标
7. 跨平台 CI 构建 — GitHub Actions 自动构建 Linux/macOS/Windows/ARM64
8. 附加说明：
   - 配置文件 {screen_id}/{sku_id} 变量替换修复 (长度计算错误)
   - Content-Type 请求头修复
   - curl 连接复用 + 句柄跨周期复用
   - 修复重渲染光标位置 bug

## Release 2.0.0 - 2025/7/2

1. C++重写

2. 加入自定义bat脚本执行器

## Release 1.1.0 - 2025/6/30

1. 重构较多代码, 为2.0做准备. 现在可以通过pyautogui来自动化控制bhyg

2. 修复bug

## Release 1.0.0 - 2025/6/30

1. 初次创建