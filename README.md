# AutoIP

延安职业技术学院专用 —— 自动检测并设置空闲 IP 地址的 Windows 工具。专为教室/实验室等多人共用网段、IP 容易冲突且需要手动配静态 IP 的场景设计。

![platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)
![Qt](https://img.shields.io/badge/Qt-6.9-green)
![license](https://img.shields.io/badge/license-MIT-orange)

## 功能

- 全段并发扫描（`10.172.201.1` – `10.172.201.253`，253 个地址同时探测），无需靠运气抽签
- 每个地址并发 ping 5 个包，有响应即判定被占用
- 从扫描出的空闲地址中随机挑一个，通过 `netsh` 设置为静态 IP
- 设置完成后 ping 百度 10 个包验证网络稳定性
- 如果有丢包，自动换下一个空闲地址重试
- 双击运行自动申请管理员权限，无黑色 cmd 窗口
- 深色 UI，渐变进度条，实时彩色日志

## 检测原理

网段里没有 DHCP 服务器，每台机器都是手动配静态 IP，所以无法查租约表，只能主动探测。

对每个地址并发发送 5 个 ICMP 回显请求（ping），只要收到任一响应就判定「被占用」，5 包全部无响应才判为空闲。

扫描范围由 `AutoIPQt/pingworker.cpp` 里的 `SCAN_FIRST`、`SCAN_LAST` 控制，共 253 个地址一次性并发探测，占用率高的教室网段也能立刻拿到完整空闲列表。

## 网络配置（写死在源码中）

| 参数 | 值 |
|------|------|
| 适配器 | 以太网 |
| IP 段 | 10.172.201.x（只随机最后一位） |
| 子网掩码 | 255.255.254.0 |
| 网关 | 10.172.201.254 |
| DNS | 10.1.0.62 |

修改 `AutoIPQt/pingworker.cpp` 顶部的常量即可适配你的网络。

## 源码结构

```
AutoIPQt/
  main.cpp          程序入口、UAC 管理员提权
  mainwindow.cpp/h  主窗口、日志区、进度条
  pingworker.cpp/h  扫描线程：ARP + ping 探测、netsh 设置、百度验证
  CMakeLists.txt    构建脚本（含 UAC 清单）
  autoip.qrc        图标资源
  autoip.rc         图标引用
  autoip.ico        应用图标
```

## 编译

环境要求：Qt 6.x（msvc2022_64）+ Visual Studio 2022 以上。

```bash
cmake -B AutoIPQt/build -G "Visual Studio 18 2026" -A x64 \
      -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64 AutoIPQt
cmake --build AutoIPQt/build --config Release
```

产物：`AutoIPQt/build/Release/AutoIP.exe`

## 部署

程序依赖 Qt 运行库，需用 `windeployqt` 把 DLL 复制到 exe 同级目录：

```bash
windeployqt --release --no-translations --no-opengl-sw AutoIP.exe
```

双击运行时会自动弹出 UAC 申请管理员权限（`netsh` 改 IP 需要）。

## 许可证

MIT License，详见 [LICENSE](LICENSE)
