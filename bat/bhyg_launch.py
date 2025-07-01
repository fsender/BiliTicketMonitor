def bhyg_launch(msg):
    from pyautogui import press, hotkey
    from pygetwindow import getWindowsWithTitle
    from time import sleep
    
    # 模拟按键
    try:
        press('y')
        print(msg)
        sleep(8)
        hotkey('ctrl', 'c')
        sleep(0.3)
        press('down')
        sleep(0.3)
        press('down')
        sleep(0.3)
        press('return')

    except Exception as e:
        print(f"press key failed!: {str(e)}")

if __name__ == "__main__":
    bhyg_launch('发现有票了, 启动BHYG抢票!')