# AutoIP

延安职业技术学院专用 - 自动检测并设置空闲 IP 地址的 Windows 工具。专为教室/实验室等多人共用网段、IP 容易冲突的场景设计。

## 功能

- 随机生成 5 个候选 IP，先用 ARP 探测、再并发 ping 5 个包，任一有响应即判定被占用
- 从空闲 IP 中选一个，通过 `netsh` 设置为静态 IP
- 设置完成后 ping 百度 10 个包验证网络稳定性
- 如果有丢包，自动切换到下一个空闲 IP 重试
- 最多 3 轮，确保选出稳定可用的 IP
- 双击运行自动申请管理员权限，无黑色 cmd 窗口
- 实时日志窗口显示完整操作过程

## 检测原理

网段里没有 DHCP 服务器，每台机器都是手动配静态 IP，所以无法查租约表，只能主动探测。

判据分两层：

| 层次 | 方法 | 说明 |
|------|------|------|
| L2 | ARP 探测（`SendARP`） | 主判据。任何在用该 IP 的主机都必须应答 ARP，防火墙通常拦不住，能发现「关了 ICMP 回显」的机器 |
| L3 | ICMP ping 5 包 | 兜底。ARP 无应答时再确认一轮 |

任一有响应即视为「被占用」，全部无响应才判为空闲。ARP 需要与目标在同一广播域，本工具适用于同一网段。

## 网络配置（写死在源码中）

| 参数 | 值 |
|------|------|
| 适配器 | 以太网 |
| IP 段 | 10.172.201.x（只随机最后一位） |
| 子网掩码 | 255.255.254.0 |
| 网关 | 10.172.201.254 |
| DNS | 10.1.0.62 |

修改 `autoip.c` 顶部的 `#define` 即可适配你的网络。

## 两个版本

### 纯 C 版（单文件 exe）

```
autoip.c          主程序源码
autoip.rc         资源文件（嵌入图标）
autoip.ico        应用图标
autoip.manifest   UAC 管理员提权清单
```

编译：
```bash
rc /fo autoip.res autoip.rc
cl autoip.c autoip.res /Fe:AutoIP.exe /O2 /MT /utf-8
```

输出单个 `AutoIP.exe`，无需任何依赖。

### Qt 版（美化 UI）

```
AutoIPQt/
  main.cpp
  mainwindow.cpp / mainwindow.h
  pingworker.cpp / pingworker.h
  CMakeLists.txt
  autoip.qrc / autoip.rc / autoip.ico
```

编译：
```bash
cmake -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64
cmake --build build --config Release
```

需要 Qt 6.x + MSVC 2022+。运行时需搭配 `windeployqt` 部署 DLL。

## 截图

纯 C 版界面：
- Win32 原生控件，Consolas 等宽字体日志
- 彩色输出（绿色=成功，红色=失败，黄色=警告）

Qt 版界面：
- 深色主题（#1a1a2e）
- 渐变进度条（蓝→紫）
- 顶部信息卡片
- 彩色 HTML 富文本日志

## 许可证

MIT License
