# AutoIP

延安职业技术学院专用 —— 自动检测并设置空闲 IP 地址的 Windows 工具。专为教室/实验室等多人共用网段、IP 容易冲突且需要手动配静态 IP 的场景设计。

![platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)
![Qt](https://img.shields.io/badge/Qt-6.9-green)
![license](https://img.shields.io/badge/license-MIT-orange)

## 功能

- **每批随机抽 10 个地址探测**，有空的就用，全被占用再抽下一批（不再全段扫描）
- **ARP 为主判据**：任何在用该 IP 的主机都必须应答 ARP，防火墙拦不住
- 每个地址探测前清理它的邻居缓存条目，避免陈旧记录导致误判
- 从本批空闲地址中随机挑一个，通过 `netsh` 设置为静态 IP
- 设置后**主动检查网络就绪**（链路 Up / 地址过 DAD / 到网关路由 / 网关 ARP 可达），四项全满足才开始测丢包，不盲等固定秒数
- 静置后做冲突自检：ARP 回来的 MAC 必须是本机网卡，否则判定被抢
- 先测网关（判断本地链路），再测外网 10 包（判断稳定性）
- 如果有丢包，弹窗询问你「继续使用此 IP」还是「换一个 IP」，由你决定，不再强制切换
- 双击运行自动申请管理员权限，无黑色 cmd 窗口
- 深色 UI，渐变进度条，实时彩色日志

## 检测原理

网段里没有 DHCP 服务器，每台机器都是手动配静态 IP，所以无法查租约表，只能主动探测。

判据分两层：

| 层次 | 方法 | 说明 |
|------|------|------|
| L2 | ARP（`SendARP`） | 主判据。二层协议，Windows 防火墙不过滤，能发现「关了 ICMP 回显」的机器 |
| L3 | ICMP ping 1 包 | 兜底。ARP 无应答时再确认一轮，覆盖跨网段 / 代理 ARP 的情况 |

任一有响应即视为「被占用」，两层都无响应才判为空闲。

**为什么不再全段扫描**：这个教室的网关（`10.172.201.254`，MAC `00:00:5E:00:01:02`，VRRP 虚拟路由器）会替全网段代答 ARP，而且**有速率限制**。实测 253 个并发打过去时，121 个在线地址只探出 70 个，**漏判 51 个**——也就是把正在使用的 IP 判成空闲，撞上去的概率接近 40%。降到 6 并发复扫才拿到全部 121 个。

所以改成**小批量随机抽样**：每批只探 10 个，并发 10，量小不会触发限速。段内空闲率约 52% 时，一批 10 个全被占用的概率是 `0.48^10 ≈ 0.065%`，基本不会空手；真遇到了就再抽一批。

**为什么探测前要清邻居缓存**：`SendARP` 会先查本机邻居表，命中就直接返回旧 MAC，一个包都不发。教室机器频繁开关机，表里全是陈旧条目，会把空闲地址误判成占用（表现为「空闲表缩水」甚至「全段均被占用」）。所以每个地址探测前用 `DeleteIpNetEntry2` 删掉它的缓存条目，强制真实发包。

**为什么设置完不能立刻测**：`netsh` 返回成功只代表命令执行完了，地址可能还在下发过程中；即便地址已出现在网卡上，路由表、ARP 表、DNS 客户端缓存也还没收敛，此时立刻测丢包必然误判。

所以不盲等固定秒数，而是**主动检查四项就绪条件**，全满足立刻开测（通常 1.5~2 秒）：

| 条件 | 检查方式 | 说明 |
|------|----------|------|
| 地址已下发 | `GetAdaptersAddresses` | 地址是否已出现在网卡上 |
| 地址通过 DAD | `GetUnicastIpAddressEntry` → `DadState` | `Preferred` 才算可用；`Duplicate` 直接判冲突，这是 Windows 官方结论 |
| 链路 Up | `GetIfEntry2` → `OperStatus` | 网卡物理链路是否正常 |
| 到网关路由存在 | `GetBestRoute2` | 确认本接口上有出网路由 |
| 网关二层可达 | `SendARP(网关)` | 最实际的证据：二层通了、网关在线、本机地址可用 |

完整流程：

```
轮询等地址出现 → 逐项检查就绪(≤15s) → 清 DNS 缓存 → 冲突自检 → 测网关 → 测外网丢包
```

扫描范围、批次大小与等待时长由 `AutoIPQt/pingworker.cpp` 顶部的常量控制：`SCAN_FIRST`、`SCAN_LAST`、`BATCH_SIZE`、`MAX_BATCHES`、`MAX_ATTEMPTS`、`SETTLE_MAX_MS`、`SETTLE_MIN_MS`、`READY_TIMEOUT_MS`、`VERIFY_COUNT`。

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
  pingworker.cpp/h  扫描线程：ARP 探测、netsh 设置、就绪检查、网关/外网验证、丢包交互询问
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
