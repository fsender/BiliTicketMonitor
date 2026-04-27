import sys
import time
import threading
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor
import requests
from urllib3 import disable_warnings
from urllib3.exceptions import InsecureRequestWarning
from colorama import Fore, Style, init

disable_warnings(InsecureRequestWarning)
requests.packages.urllib3.disable_warnings()
init(autoreset=True)

class Config:
    TICKET_ID = "115413"      # 漫展项目ID
        
    # 多目标配置 (输出顺序将严格遵循此列表顺序)
    TARGETS = [
        {"screen_id": "332913", "sku_id": "857648", "label": "Day 1"},
        {"screen_id": "332914", "sku_id": "857522", "label": "Day 2"},
        {"screen_id": "332915", "sku_id": "857686", "label": "Day 3"},
    ]

    # ========== Bark 推送配置 (默认关闭，由CMD输入决定) ==========
    BARK_ENABLED = False                            # 默认关闭
    BARK_SERVER = "https://api.day.app"             # Bark 服务器
    BARK_KEY = ""                                   # 默认为空
    BARK_GROUP = "票务监控"                         # 推送分组名称
    BARK_ICON = ""                                  # 推送图标（可选）
    BARK_SOUND = "alarm"                            # 推送声音（有库存时使用）
    # ===========================================================

    TIMEOUT = 5  
    STOCK_CHECK_URL = "https://show.bilibili.com/api/ticket/stock/check"
    
    HEADERS = {
        "User-Agent": "Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36",
        "Referer": f"https://show.bilibili.com/h5/detail/{TICKET_ID}",
        "Origin": "https://show.bilibili.com"
    }

STATUS_MAP = {
    1: ("暂时售罄", Fore.YELLOW),
    2: ("已售罄", Fore.RED),
    3: ("🚨 有库存 🚨", Fore.LIGHTGREEN_EX)
}

def clear_screen():
    print("\033c", end="")

class Monitor:
    def __init__(self):
        self.stop = threading.Event()
        self.last_stock_status = {t["screen_id"]: None for t in Config.TARGETS}
        self.healthy = True
        self.request_count = 0
        self.last_error_msg = ""
        self.print_lock = threading.Lock()
        self.status_bar_drawn = False

    def _safe_print(self, text: str, color: str = ""):
        with self.print_lock:
            if self.status_bar_drawn:
                sys.stdout.write(f"\r{' ' * 100}")
                sys.stdout.write(f"\r{color}{text}{Style.RESET_ALL}\n")
                self.status_bar_drawn = False
            else:
                sys.stdout.write(f"{color}{text}{Style.RESET_ALL}\n")
            sys.stdout.flush()

    def send_bark(self, title: str, body: str, is_stock: bool = False) -> bool:
        if not Config.BARK_ENABLED or not Config.BARK_KEY:
            return False

        try:
            url = f"{Config.BARK_SERVER}/{Config.BARK_KEY}"
            payload = {
                "title": title,
                "body": body,
                "group": Config.BARK_GROUP,
                "level": "critical" if is_stock else "active",
            }
            
            if is_stock and Config.BARK_SOUND:
                payload["sound"] = Config.BARK_SOUND
            if Config.BARK_ICON:
                payload["icon"] = Config.BARK_ICON

            resp = requests.post(url, json=payload, timeout=5, headers={"Content-Type": "application/json; charset=utf-8"})
            now_str = datetime.now().strftime('%H:%M:%S.%f')[:-3]
            
            if resp.status_code == 200:
                result = resp.json()
                if result.get("code") == 200:
                    self._safe_print(f"[{now_str}] [Bark] 推送成功 ✓", Fore.CYAN)
                    return True
                else:
                    self._safe_print(f"[{now_str}] [Bark] 返回错误: {result}", Fore.RED)
            else:
                self._safe_print(f"[{now_str}] [Bark] HTTP {resp.status_code}: {resp.text[:100]}", Fore.RED)
                
        except Exception as e:
            self._safe_print(f"[Bark] 异常: {type(e).__name__}: {e}", Fore.RED)
        return False

    def test_bark(self) -> bool:
        print(f"{Fore.YELLOW}正在发送测试推送...{Style.RESET_ALL}", flush=True)
        success = self.send_bark(
            title="🔔 Bark 测试",
            body="如果你看到这条消息，说明推送配置正确！",
            is_stock=True
        )
        if success:
            print(f"{Fore.GREEN}测试成功，请检查手机是否收到通知{Style.RESET_ALL}", flush=True)
        else:
            print(f"{Fore.RED}测试失败，请检查 Key 是否正确！{Style.RESET_ALL}", flush=True)
        return success

    def start(self):
        if not Config.TARGETS:
            print(f"{Fore.RED}错误：请在 TARGETS 中配置至少一个监控目标！")
            input("\n按回车键退出...")
            return

        # 【修复】删除了这里重复的 self.test_bark()

        time_thread = threading.Thread(target=self.show_time, daemon=True)
        monitor_thread = threading.Thread(target=self.run_monitor, daemon=True)
        
        time_thread.start()
        monitor_thread.start()

        try:
            while time_thread.is_alive() or monitor_thread.is_alive():
                time.sleep(1)
        except KeyboardInterrupt:
            self.stop.set()
            sys.exit(0)

    def show_time(self):
        current = ""
        while not self.stop.is_set():
            if self.healthy:
                new_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                if new_time != current:
                    with self.print_lock:
                        info = f"{Fore.GREEN}当前时间: {new_time}  |  已发送: {self.request_count} 次    {' ' * 20}"
                        sys.stdout.write(f"\r{info}")
                        sys.stdout.flush()
                        self.status_bar_drawn = True
                    current = new_time
            time.sleep(0.1)

    def check_stock(self, session: requests.Session, screen_id: str, sku_id: str) -> int:
        try:
            data = {
                "projectId": Config.TICKET_ID,
                "skuId": int(sku_id),
                "screenId": int(screen_id),
            }
            resp = session.post(
                Config.STOCK_CHECK_URL, 
                json=data, 
                headers=Config.HEADERS, 
                timeout=Config.TIMEOUT
            )
            
            try:
                res_json = resp.json()
            except requests.exceptions.JSONDecodeError:
                raise ValueError(f"返回非JSON(被风控): {resp.text[:80]}")
            
            stock_status = res_json.get("data", {}).get("stockStatus", -1)
            if stock_status == -1:
                raise ValueError(f"解析失败: {resp.text[:80]}")
                
            return stock_status
            
        except ValueError:
            return -1
        except requests.exceptions.HTTPError as e:
            if e.response.status_code == 412:
                self.handle_error("触发风控(412)！", critical=True)
            return -1
        except requests.exceptions.RequestException:
            return -1

    def fetch_stock(self, session: requests.Session, target: dict) -> int:
        self.request_count += 1
        return self.check_stock(session, target["screen_id"], target["sku_id"])

    def run_monitor(self):
        max_workers = len(Config.TARGETS)
        
        with requests.Session() as s:
            s.verify = False
            s.mount('https://', requests.adapters.HTTPAdapter(
                pool_connections=max_workers, 
                pool_maxsize=max_workers
            ))

            while not self.stop.is_set():
                round_results = []
                
                with ThreadPoolExecutor(max_workers=max_workers) as executor:
                    future_to_target = {
                        executor.submit(self.fetch_stock, s, target): target 
                        for target in Config.TARGETS
                    }
                    
                    for future in future_to_target.keys():
                        if self.stop.is_set():
                            break
                        try:
                            stock_code = future.result()
                            round_results.append((future_to_target[future], stock_code))
                        except Exception:
                            pass

                for target, stock_code in round_results:
                    if self.stop.is_set():
                        break
                        
                    sid = target["screen_id"]
                    last_status = self.last_stock_status.get(sid)

                    if stock_code != -1 and stock_code != last_status:
                        self.print_status_change(target["label"], stock_code)
                        self.last_stock_status[sid] = stock_code
                        self.healthy = True
                        
                        if stock_code == 1:
                            threading.Thread(target=self.send_bark, kwargs={
                                "title": f"⚡ {target['label']} 暂时售罄",
                                "body": f"可能有补票机会\n项目ID: {Config.TICKET_ID}",
                                "is_stock": False
                            }, daemon=True).start()
                        elif stock_code == 3:
                            threading.Thread(target=self.send_bark, kwargs={
                                "title": f"🎫 {target['label']} 有库存！",
                                "body": f"赶紧去抢票！\n项目ID: {Config.TICKET_ID}",
                                "is_stock": True
                            }, daemon=True).start()

    def get_ms_time(self) -> str:
        now = datetime.now()
        return now.strftime('%H:%M:%S.%f')[:-3]

    def print_status_change(self, label: str, code: int):
        now_str = self.get_ms_time()
        msg, color = STATUS_MAP.get(code, ("未知状态", Fore.WHITE))
        self._safe_print(f"[{now_str}] [{label}] {msg}", color)

    def handle_error(self, msg: str, critical: bool = False):
        now_str = self.get_ms_time()
        if msg != self.last_error_msg:
            self._safe_print(f"[{now_str}] {msg}", Fore.RED)
            self.last_error_msg = msg
            
        self.healthy = False
        if critical:
            self.stop.set()

if __name__ == "__main__":
    clear_screen()
    print(f"{Fore.MAGENTA}Acknowledgement: GLM5, ZianTT")
    print(f"{Fore.CYAN}HSR Land Stock Monitor")
    print(f"\033[31m关注火花喵，关注火花谢谢喵\033[0m")
    print(f"{Fore.YELLOW}项目: [{Config.TICKET_ID}] | 监控场次数量: {len(Config.TARGETS)}")
    for t in Config.TARGETS:
        print(f"{Fore.WHITE}  -> {t['label']} (场次:{t['screen_id']} | 票种:{t['sku_id']})")
    print("=" * 50)
    
    # ========== CMD 交互式配置 Bark (无痕模式) ==========
    lines_printed = 0
    monitor = Monitor() # 【修复】提前实例化，保证上下文是同一个对象
    
    sys.stdout.write(f"{Fore.YELLOW}是否启用 Bark 推送？(y/n){Style.RESET_ALL} ")
    sys.stdout.flush()
    enable_choice = input("").strip().lower()
    lines_printed += 1
    
    if enable_choice in ['y', 'yes']:
        sys.stdout.write(f"\r{Fore.YELLOW}请输入你的 Bark Key: {Style.RESET_ALL}          \r{Fore.YELLOW}请输入你的 Bark Key: {Style.RESET_ALL} ")
        sys.stdout.flush()
        input_key = input("").strip()
        lines_printed += 1
        
        if input_key:
            Config.BARK_KEY = input_key
            Config.BARK_ENABLED = True
            safe_key = f"{input_key[:3]}***{input_key[-3:]}" if len(input_key) > 6 else input_key
            print(f"{Fore.GREEN}Bark 推送: 已启用 (Key: {safe_key}){Style.RESET_ALL}")
            lines_printed += 1
            
            monitor.test_bark() # 【修复】使用同一个 monitor 实例
            lines_printed += 2
        else:
            print(f"{Fore.RED}输入为空，Bark 推送: 已禁用{Style.RESET_ALL}")
            lines_printed += 1
    else:
        print(f"{Fore.RED}Bark 推送: 已禁用{Style.RESET_ALL}")
        lines_printed += 1

    for _ in range(lines_printed):
        sys.stdout.write("\033[1A\033[2K")
    sys.stdout.flush()
    # ========================================
    
    print("=" * 50 + "\n")
    monitor.start() # 【修复】启动时不再重复测试