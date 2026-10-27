#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <icmpapi.h>
#include "pingworker.h"
#include <QProcess>
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <QList>
#include <QSet>
#include <QRandomGenerator>
#include <QElapsedTimer>
#include <atomic>
#include <vector>
#include <cstring>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// ---- 网段与适配器 ----
#define SCAN_FIRST 1
#define SCAN_LAST 253
static const char *ADAPTER_NAME = "以太网";
static const int IP0 = 10, IP1 = 172, IP2 = 201;
static const int MASK0 = 255, MASK1 = 255, MASK2 = 254, MASK3 = 0;
static const int GW0 = 10, GW1 = 172, GW2 = 201, GW3 = 254;
static const int DNS0 = 10, DNS1 = 1, DNS2 = 0, DNS3 = 62;

// ---- 探测策略 ----
// 不再全段扫描：教室网关会替全网段代答 ARP，而且有速率限制。
// 253 个并发打过去会被丢包，实测漏判近一半（该占用的被判成空闲）。
// 改为每批随机抽 10 个，探完有空的就用，全被占用再抽下一批。
// 段内空闲率约 52% 时，一批全占的概率约 0.065%，基本不会空手。
#define BATCH_SIZE 10
#define MAX_BATCHES 8    // 最多抽 80 个地址
#define MAX_ATTEMPTS 10  // 最多真正配置 10 次，防止无限重试
// ARP 无响应时补一次 ping，只为兜住「跨网段 / 代理 ARP」的少数情况
#define PING_FALLBACK_COUNT 1
#define PING_FALLBACK_TIMEOUT 800

// ---- 配置生效等待 ----
// netsh 返回成功不代表地址已经落到网卡上，先轮询等它出现
#define SETTLE_POLL_MS 250
#define SETTLE_MAX_MS 15000
// 地址出现后不再盲等固定时长，而是逐项检查就绪状态：
//   链路 Up / 地址过 DAD(Preferred) / 到网关路由存在 / 网关 ARP 可达
// 四项全满足立刻开测，通常 1.5~2 秒就能开始，慢的时候自动等到条件满足
#define READY_POLL_MS 200
#define READY_TIMEOUT_MS 15000
// 最短静置：防止极端情况下"瞬间就绪"导致测得太早
#define SETTLE_MIN_MS 1500

// ---- 验证 ----
#define GW_CHECK_COUNT 3
#define GW_CHECK_TIMEOUT 1500
#define GW_CHECK_INTERVAL 300
#define VERIFY_COUNT 10
#define VERIFY_TIMEOUT 2000
#define VERIFY_INTERVAL 400
static const char *VERIFY_HOST_IP = "110.242.74.102";

// ------------------------------------------------------------------

static DWORD ipOf(int last) {
    char buf[32];
    sprintf(buf, "%d.%d.%d.%d", IP0, IP1, IP2, last);
    return inet_addr(buf);
}

static DWORD gatewayIp() {
    char buf[32];
    sprintf(buf, "%d.%d.%d.%d", GW0, GW1, GW2, GW3);
    return inet_addr(buf);
}

static QString ipToString(DWORD ip) {
    in_addr a;
    a.s_addr = ip;
    return QString::fromLatin1(inet_ntoa(a));
}

static QString macToString(const BYTE mac[6]) {
    return QString("%1:%2:%3:%4:%5:%6")
        .arg(mac[0], 2, 16, QChar('0')).arg(mac[1], 2, 16, QChar('0'))
        .arg(mac[2], 2, 16, QChar('0')).arg(mac[3], 2, 16, QChar('0'))
        .arg(mac[4], 2, 16, QChar('0')).arg(mac[5], 2, 16, QChar('0'))
        .toUpper();
}

static bool sameMac(const BYTE a[6], const BYTE b[6]) {
    return memcmp(a, b, 6) == 0;
}

// 取一张适配器地址表，返回第一条匹配名字的条目
static bool findAdapter(const QString &name, DWORD &ifIndex, BYTE mac[8], ULONG &macLen) {
    ULONG size = 16 * 1024;
    std::vector<BYTE> buf(size);
    auto *addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.data());
    ULONG ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addrs, &size);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf.resize(size);
        addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.data());
        ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addrs, &size);
    }
    if (ret != NO_ERROR) return false;

    for (auto *a = addrs; a; a = a->Next) {
        if (QString::fromWCharArray(a->FriendlyName) != name) continue;
        ifIndex = a->IfIndex;
        macLen = a->PhysicalAddressLength;
        if (macLen > 8) macLen = 8;
        memset(mac, 0, 8);
        memcpy(mac, a->PhysicalAddress, macLen);
        return true;
    }
    return false;
}

// ------------------------------------------------------------------

PingWorker::PingWorker(QObject *parent) : QThread(parent) {}

PingWorker::~PingWorker() {
    if (isRunning()) {
        cancel();
        wait(8000);
    }
}

QString PingWorker::configSummary() {
    auto ip = [](int a, int b, int c, int d) {
        return QString("%1.%2.%3.%4").arg(a).arg(b).arg(c).arg(d);
    };
    return QString("适配器: %1  |  IP段: %2.%3.%4.x  |  掩码: %5\n网关: %6  |  DNS: %7")
        .arg(QString::fromUtf8(ADAPTER_NAME))
        .arg(IP0).arg(IP1).arg(IP2)
        .arg(ip(MASK0, MASK1, MASK2, MASK3))
        .arg(ip(GW0, GW1, GW2, GW3))
        .arg(ip(DNS0, DNS1, DNS2, DNS3));
}

void PingWorker::answer(bool keep) {
    QMutexLocker locker(&m_mutex);
    m_keep = keep;
    m_waiting = false;
    m_cond.wakeAll();
}

// ---------------- 探测 ----------------

bool PingWorker::queryAdapter(const QString &name, DWORD &ifIndex, BYTE mac[8], ULONG &macLen) {
    return findAdapter(name, ifIndex, mac, macLen);
}

// 关键修复：SendARP 会先查本机邻居表，命中就返回旧 MAC 且一个包都不发。
// 教室机器频繁开关机，表里全是陈旧条目，会把空闲地址误判成占用。
// 所以每个地址探测前先删掉它的缓存条目，逼它真的发 ARP。
void PingWorker::purgeNeighbor(DWORD ifIndex, DWORD ip) {
    if (ifIndex == 0) return;

    MIB_IPNET_ROW2 row{};
    row.InterfaceIndex = ifIndex;
    row.Address.si_family = AF_INET;
    row.Address.Ipv4.sin_addr.s_addr = ip;
    DeleteIpNetEntry2(&row);
}

bool PingWorker::arpProbe(DWORD ip, BYTE mac[6]) {
    ULONG raw[2] = {0, 0};
    ULONG len = 6;
    if (SendARP(ip, 0, raw, &len) != NO_ERROR) return false;
    if (len != 6) return false;
    memcpy(mac, raw, 6);
    // 全零 MAC 视为无效响应
    for (int i = 0; i < 6; ++i)
        if (mac[i] != 0) return true;
    return false;
}

// 返回 -1 表示 ICMP 句柄创建失败（探测本身出错），调用方应按「占用」保守处理
int PingWorker::pingCount(DWORD ip, int count, int timeoutMs, int intervalMs) {
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) return -1;

    unsigned char data[32] = {0};
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8;
    void *buf = malloc(replySize);
    if (!buf) { IcmpCloseHandle(h); return -1; }

    int replies = 0;
    for (int i = 0; i < count; ++i) {
        if (m_cancelled.load()) break;
        if (IcmpSendEcho(h, ip, data, sizeof(data), nullptr, buf, replySize, timeoutMs) > 0) replies++;
        if (i < count - 1) Sleep(intervalMs);
    }

    free(buf);
    IcmpCloseHandle(h);
    return replies;
}

// ---------------- 配置 ----------------

bool PingWorker::runNetsh(const QString &cmd) {
    QProcess proc;
    proc.setProgram("netsh");
    QStringList args = QProcess::splitCommand(cmd);
    proc.setArguments(args.mid(1));
    proc.start();
    if (!proc.waitForStarted(5000)) return false;
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        proc.waitForFinished(2000);
        return false;
    }
    return proc.exitCode() == 0;
}

bool PingWorker::setIP(int newLast) {
    QString addrCmd = QString("netsh interface ip set address name=\"%1\" source=static addr=%2.%3.%4.%5 mask=%6.%7.%8.%9 gateway=%10.%11.%12.%13 gwmetric=1")
        .arg(QString::fromUtf8(ADAPTER_NAME))
        .arg(IP0).arg(IP1).arg(IP2).arg(newLast)
        .arg(MASK0).arg(MASK1).arg(MASK2).arg(MASK3)
        .arg(GW0).arg(GW1).arg(GW2).arg(GW3);
    bool ok = runNetsh(addrCmd);
    emit logMessage(QString("设置IP: %1").arg(ok ? "成功" : "失败"), ok ? "#4ade80" : "#f87171");

    QString dnsCmd = QString("netsh interface ip set dns name=\"%1\" source=static addr=%2.%3.%4.%5 register=primary")
        .arg(QString::fromUtf8(ADAPTER_NAME))
        .arg(DNS0).arg(DNS1).arg(DNS2).arg(DNS3);
    runNetsh(dnsCmd);
    emit logMessage("设置DNS: 完成", "#4ade80");
    return ok;
}

// ---------------- 生效等待 ----------------

bool PingWorker::localHasAddress(DWORD target) {
    ULONG size = 16 * 1024;
    std::vector<BYTE> buf(size);
    auto *addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.data());
    ULONG ret = GetAdaptersAddresses(AF_INET, 0, nullptr, addrs, &size);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf.resize(size);
        addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.data());
        ret = GetAdaptersAddresses(AF_INET, 0, nullptr, addrs, &size);
    }
    if (ret != NO_ERROR) return false;

    const QString want = QString::fromUtf8(ADAPTER_NAME);
    for (auto *a = addrs; a; a = a->Next) {
        if (QString::fromWCharArray(a->FriendlyName) != want) continue;
        for (auto *u = a->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr) continue;
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
            auto *sin = reinterpret_cast<sockaddr_in *>(u->Address.lpSockaddr);
            if (sin->sin_addr.s_addr == target) return true;
        }
    }
    return false;
}

// netsh 返回成功只代表命令执行完了，地址可能还在下发过程中。
// 这里轮询到地址真的出现在网卡上，再交给调用方静置。
bool PingWorker::waitForAddress(DWORD target, int maxMs) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < maxMs) {
        if (m_cancelled.load()) return false;
        if (localHasAddress(target)) {
            emit logMessage(QString("地址已生效 (耗时 %1 ms)").arg(timer.elapsed()), "#a0a0a0");
            return true;
        }
        emit progress(72, QString("等待地址生效 %1 秒").arg(timer.elapsed() / 1000));
        Sleep(SETTLE_POLL_MS);
    }
    return false;
}

bool PingWorker::linkIsUp(DWORD ifIndex) {
    MIB_IF_ROW2 row{};
    row.InterfaceIndex = ifIndex;
    if (GetIfEntry2(&row) != NO_ERROR) return false;
    return row.OperStatus == IfOperStatusUp;
}

// Windows 设静态地址时会自动做 DAD（重复地址检测）。
// DadState 直接给出官方结论：Preferred 可用 / Tentative 还在检测 / Duplicate 冲突。
bool PingWorker::addressIsPreferred(DWORD target, DWORD ifIndex, bool &duplicate) {
    duplicate = false;
    MIB_UNICASTIPADDRESS_ROW row{};
    InitializeUnicastIpAddressEntry(&row);
    row.InterfaceIndex = ifIndex;
    row.Address.si_family = AF_INET;
    row.Address.Ipv4.sin_addr.s_addr = target;

    if (GetUnicastIpAddressEntry(&row) != NO_ERROR) return false;
    duplicate = (row.DadState == IpDadStateDuplicate);
    return row.DadState == IpDadStatePreferred;
}

// 地址有了不等于能出去，还要确认本接口上存在到网关的路由
bool PingWorker::routeToGatewayOk(DWORD ifIndex, DWORD gateway) {
    MIB_IPFORWARD_ROW2 route{};
    SOCKADDR_INET src{};
    SOCKADDR_INET dest{};
    dest.si_family = AF_INET;
    dest.Ipv4.sin_addr.s_addr = gateway;

    if (GetBestRoute2(nullptr, ifIndex, nullptr, &dest, 0, &route, &src) != NO_ERROR)
        return false;
    return route.InterfaceIndex == ifIndex;
}

// 主动等待网络就绪：四项条件全满足就立刻返回，不再盲等固定秒数。
// 网关 ARP 可达是"二层通了、地址可用、网关在线"的实际证据。
bool PingWorker::waitUntilReady(DWORD target, DWORD gateway, DWORD ifIndex,
                               int maxMs, bool &conflict) {
    conflict = false;

    QElapsedTimer timer;
    timer.start();

    // 最短静置，避免极端情况下瞬间就绪就开测
    for (int e = 0; e < SETTLE_MIN_MS && !m_cancelled.load(); e += READY_POLL_MS)
        Sleep(READY_POLL_MS);
    if (m_cancelled.load()) return false;

    bool link = false, addr = false, route = false, gwArp = false, dup = false;
    QString lastDesc;
    auto mark = [](bool ok) { return ok ? QStringLiteral("√") : QStringLiteral("×"); };

    while (timer.elapsed() < maxMs) {
        if (m_cancelled.load()) return false;

        link = linkIsUp(ifIndex);
        addr = addressIsPreferred(target, ifIndex, dup);
        if (dup) {
            emit logMessage("*** 地址冲突: Windows DAD 判定为 Duplicate ***", "#f87171");
            conflict = true;
            return false;
        }
        route = routeToGatewayOk(ifIndex, gateway);

        BYTE gwMac[6] = {0};
        gwArp = arpProbe(gateway, gwMac);

        QString desc = QString("链路 %1 | 地址 %2 | 路由 %3 | 网关ARP %4")
                           .arg(mark(link)).arg(mark(addr))
                           .arg(mark(route)).arg(mark(gwArp));
        if (desc != lastDesc) {
            emit logMessage(QString("就绪检查 %1s: %2").arg(timer.elapsed() / 1000).arg(desc), "#a0a0a0");
            lastDesc = desc;
        }

        if (link && addr && route && gwArp) {
            emit logMessage(QString("网络已就绪 (耗时 %1 ms), 开始验证").arg(timer.elapsed()), "#4ade80");
            return true;
        }

        emit progress(76, QString("等待网络就绪 %1s").arg(timer.elapsed() / 1000));
        Sleep(READY_POLL_MS);
    }

    emit logMessage(QString("等待就绪超时, 最后状态: %1").arg(lastDesc), "#fbbf24");
    return false;
}

void PingWorker::flushDns() {
    QProcess p;
    p.setProgram("ipconfig");
    p.setArguments(QStringList() << "/flushdns");
    p.start();
    if (p.waitForStarted(3000)) {
        if (!p.waitForFinished(5000)) {
            p.kill();
            p.waitForFinished(1000);
        }
    }
}

// 设完地址后确认这个 IP 现在确实归本机：ARP 回来的 MAC 必须是本机网卡
bool PingWorker::confirmOwnership(DWORD target, const BYTE localMac[8], ULONG localMacLen) {
    if (localMacLen < 6) {
        emit logMessage("冲突自检: 取不到本机 MAC, 跳过", "#fbbf24");
        return true;
    }

    BYTE got[6] = {0};
    if (!arpProbe(target, got)) {
        // 自己的地址 ARP 不通, 说明二层还没起来, 不算冲突但也不该继续测
        emit logMessage("冲突自检: 本机地址 ARP 无响应", "#fbbf24");
        return true;
    }

    BYTE mine[6];
    memcpy(mine, localMac, 6);
    if (sameMac(got, mine)) {
        emit logMessage(QString("冲突自检: 通过 (本机 %1)").arg(macToString(mine)), "#4ade80");
        return true;
    }

    emit logMessage(QString("*** 冲突自检失败: %1 的 MAC 是 %2, 本机是 %3 ***")
                        .arg(ipToString(target), macToString(got), macToString(mine)),
                    "#f87171");
    return false;
}

// ---------------- 验证 ----------------

int PingWorker::verifyBaidu() {
    DWORD target = inet_addr(VERIFY_HOST_IP);
    int got = 0;
    for (int i = 0; i < VERIFY_COUNT; ++i) {
        if (m_cancelled.load()) break;
        int r = pingCount(target, 1, VERIFY_TIMEOUT, 0);
        if (r > 0) got++;
        emit progress(88 + (i + 1) * 12 / VERIFY_COUNT,
                      QString("验证外网 %1/%2").arg(i + 1).arg(VERIFY_COUNT));
        if (i < VERIFY_COUNT - 1) Sleep(VERIFY_INTERVAL);
    }
    return got;
}

// ------------------------------------------------------------------

void PingWorker::run() {
    emit logMessage("=== 自动设IP 启动 ===", "#60a5fa");
    emit logMessage(QString("适配器: %1").arg(QString::fromUtf8(ADAPTER_NAME)), "#a0a0a0");
    emit logMessage(QString("IP段: %1.%2.%3.x (只改最后一位)").arg(IP0).arg(IP1).arg(IP2), "#a0a0a0");
    emit logMessage(QString("掩码: %1.%2.%3.%4").arg(MASK0).arg(MASK1).arg(MASK2).arg(MASK3), "#a0a0a0");
    emit logMessage(QString("网关: %1.%2.%3.%4").arg(GW0).arg(GW1).arg(GW2).arg(GW3), "#a0a0a0");
    emit logMessage(QString("DNS:  %1.%2.%3.%4").arg(DNS0).arg(DNS1).arg(DNS2).arg(DNS3), "#a0a0a0");
    emit logMessage(QString("探测: ARP 为主, ping%1包兜底 (任一有响应即视为占用)").arg(PING_FALLBACK_COUNT), "#a0a0a0");
    emit logMessage(QString("策略: 每批随机抽 %1 个地址, 有空闲的就试, 全占用再抽下一批").arg(BATCH_SIZE), "#a0a0a0");
    emit logMessage(QString("等待: 地址生效后主动检查 链路/地址DAD/路由/网关ARP, 全就绪才测丢包"), "#a0a0a0");
    emit logMessage(QString("验证: 网关%1包 → 外网%2包").arg(GW_CHECK_COUNT).arg(VERIFY_COUNT), "#a0a0a0");
    emit logMessage("", "#a0a0a0");

    DWORD ifIndex = 0;
    BYTE localMac[8] = {0};
    ULONG localMacLen = 0;
    if (!queryAdapter(QString::fromUtf8(ADAPTER_NAME), ifIndex, localMac, localMacLen)) {
        emit logMessage(QString("*** 找不到适配器 \"%1\", 请检查网卡名称 ***")
                            .arg(QString::fromUtf8(ADAPTER_NAME)), "#f87171");
        emit progress(100, "失败");
        emit done(false);
        return;
    }
    emit logMessage(QString("适配器索引: %1").arg(ifIndex), "#a0a0a0");

    QSet<int> probed;
    QVector<int> queue;
    int batch = 0;
    int attempts = 0;
    bool success = false;
    int finalLast = 0;
    DWORD gwIp = gatewayIp();

    while (!success && attempts < MAX_ATTEMPTS) {
        if (m_cancelled.load()) break;

        // 队列空了就抽新的一批
        if (queue.isEmpty()) {
            if (batch >= MAX_BATCHES) {
                emit logMessage(QString("\n*** 已抽满 %1 批仍无可用地址 ***").arg(MAX_BATCHES), "#f87171");
                break;
            }
            ++batch;

            QVector<int> picks;
            int guard = 0;
            const int span = SCAN_LAST - SCAN_FIRST + 1;
            while (picks.size() < BATCH_SIZE && guard++ < 5000) {
                int last = SCAN_FIRST + int(QRandomGenerator::global()->bounded(span));
                if (probed.contains(last)) continue;
                probed.insert(last);
                picks.append(last);
            }
            if (picks.isEmpty()) break;

            emit logMessage(QString("\n--- 第 %1 批: 随机抽取 %2 个地址 ---").arg(batch).arg(picks.size()),
                            "#60a5fa");
            emit progress(12, QString("探测第 %1 批").arg(batch));

            struct Pick {
                DWORD ip;
                int last;
                bool occupied;
                BYTE mac[6];
            };

            std::vector<Pick> items(static_cast<size_t>(picks.size()));
            for (int i = 0; i < static_cast<int>(picks.size()); ++i) {
                items[i].ip = ipOf(picks[i]);
                items[i].last = picks[i];
                items[i].occupied = true;
                memset(items[i].mac, 0, 6);
            }

            // 只探这 10 个，并发 10，量小不会被网关限速
            QList<QThread *> ts;
            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                ts.append(QThread::create([this, &items, i, ifIndex]() {
                    purgeNeighbor(ifIndex, items[i].ip);
                    BYTE mac[6] = {0};
                    if (arpProbe(items[i].ip, mac)) {
                        items[i].occupied = true;
                        memcpy(items[i].mac, mac, 6);
                    } else {
                        // ARP 没应答再补 1 包 ping，兜住 ARP 静默的主机
                        int r = pingCount(items[i].ip, PING_FALLBACK_COUNT, PING_FALLBACK_TIMEOUT, 0);
                        // -1 说明 ICMP 句柄都建不起来，按占用保守处理
                        items[i].occupied = (r != 0);
                    }
                }));
            }
            for (auto *t : ts) t->start();
            for (auto *t : ts) { t->wait(); delete t; }

            if (m_cancelled.load()) break;

            QStringList detail;
            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                if (items[i].occupied) {
                    detail << QString(".%1 占").arg(items[i].last);
                } else {
                    queue.append(items[i].last);
                    detail << QString(".%1 空").arg(items[i].last);
                }
            }
            emit logMessage(QString("  结果: %1").arg(detail.join("  ")), "#a0a0a0");
            emit logMessage(QString("  本批空闲 %1/%2").arg(queue.size()).arg(items.size()),
                            queue.isEmpty() ? "#fbbf24" : "#4ade80");

            if (queue.isEmpty()) {
                emit logMessage("  本批全被占用, 抽下一批", "#fbbf24");
                continue;
            }

            // 洗牌，避免总是先试最小的那个
            const int qn = static_cast<int>(queue.size());
            for (int i = qn - 1; i > 0; --i) {
                int j = QRandomGenerator::global()->bounded(i + 1);
                int tmp = queue[i];
                queue[i] = queue[j];
                queue[j] = tmp;
            }
            emit progress(18, QString("第 %1 批待试 %2 个").arg(batch).arg(queue.size()));
        }

        int tryLast = queue.takeFirst();
        ++attempts;
        DWORD tryIp = ipOf(tryLast);
        QString tryStr = ipToString(tryIp);

        emit logMessage(QString("\n>> 尝试IP: %1").arg(tryStr), "#60a5fa");
        emit progress(70, QString("设置IP %1").arg(tryStr));

        if (!setIP(tryLast)) {
            emit logMessage("设置失败, 换下一个", "#f87171");
            continue;
        }

        // 1) 等地址真正下发到网卡
        emit logMessage("等待地址下发...", "#a0a0a0");
        if (!waitForAddress(tryIp, SETTLE_MAX_MS)) {
            emit logMessage(QString("地址 %1 秒内未生效, 换下一个").arg(SETTLE_MAX_MS / 1000), "#f87171");
            continue;
        }

        // 2) 主动检查就绪：链路 Up / 地址过 DAD / 路由就绪 / 网关 ARP 可达
        //    四项满足立刻开测，不再盲等固定秒数
        emit progress(74, "检查网络就绪状态");
        bool conflict = false;
        bool ready = waitUntilReady(tryIp, gwIp, ifIndex, READY_TIMEOUT_MS, conflict);
        if (conflict) {
            emit logMessage("该地址存在冲突, 换下一个", "#f87171");
            continue;
        }
        if (!ready) {
            emit logMessage("未完全就绪, 仍尝试验证 (结果可能偏悲观)", "#fbbf24");
        }
        if (m_cancelled.load()) break;

        flushDns();

        // 3) 先确认这个地址没被别人同时抢走
        emit progress(80, "冲突自检");
        if (!confirmOwnership(tryIp, localMac, localMacLen)) {
            emit logMessage("该地址已被他人占用, 换下一个", "#f87171");
            continue;
        }

        // 4) 先测网关：网关就丢包说明是本地链路问题，换 IP 也没用
        emit logMessage(QString("测网关 %1 (%2 包)...").arg(ipToString(gwIp)).arg(GW_CHECK_COUNT), "#a0a0a0");
        int gwReplies = pingCount(gwIp, GW_CHECK_COUNT, GW_CHECK_TIMEOUT, GW_CHECK_INTERVAL);
        if (gwReplies < GW_CHECK_COUNT) {
            emit logMessage(QString("网关丢包 %1/%2, 本地链路不稳").arg(GW_CHECK_COUNT - gwReplies).arg(GW_CHECK_COUNT),
                            "#fbbf24");
        } else {
            emit logMessage(QString("网关 %1/%2 全通").arg(gwReplies).arg(GW_CHECK_COUNT), "#4ade80");
        }

        // 5) 再测外网丢包
        emit progress(88, "验证外网稳定性");
        emit logMessage(QString("验证稳定性: 外网 %1 包, 间隔 %2 ms...").arg(VERIFY_COUNT).arg(VERIFY_INTERVAL),
                        "#fbbf24");
        int replies = verifyBaidu();
        int lost = VERIFY_COUNT - replies;
        emit logMessage(QString("外网验证: 收到 %1/%2 (丢 %3)").arg(replies).arg(VERIFY_COUNT).arg(lost),
                        lost == 0 ? "#4ade80" : "#f87171");

        if (replies == VERIFY_COUNT) {
            success = true;
            finalLast = tryLast;
            emit logMessage(">> 验证通过! IP稳定可用", "#4ade80");
            break;
        }

        // 丢包时把网关结果一起摆出来，便于判断是链路问题还是地址问题
        emit logMessage(QString(">> 有丢包 (丢失 %1 个包, 网关 %2/%3), 等待你的选择...")
                            .arg(lost).arg(gwReplies).arg(GW_CHECK_COUNT),
                        "#fbbf24");
        emit progress(96, QString("等待选择: 继续使用 %1 或换一个").arg(tryStr));

        {
            QMutexLocker locker(&m_mutex);
            m_waiting = true;
            m_keep = false;
        }
        emit askUser(tryStr, replies, VERIFY_COUNT);

        bool keep = false;
        {
            QMutexLocker locker(&m_mutex);
            while (m_waiting && !m_cancelled.load()) {
                m_cond.wait(&m_mutex, 200);
            }
            keep = m_keep;
        }

        if (keep) {
            success = true;
            finalLast = tryLast;
            emit logMessage(">> 已按你的选择继续使用该IP (存在丢包)", "#fbbf24");
        } else {
            emit logMessage(">> 已按你的选择换下一个IP", "#f87171");
        }
    }

    if (success) {
        emit logMessage("\n=== 设置完成 ===", "#4ade80");
        emit logMessage(QString("新IP:  %1.%2.%3.%4").arg(IP0).arg(IP1).arg(IP2).arg(finalLast), "#4ade80");
        emit logMessage(QString("掩码:  %1.%2.%3.%4").arg(MASK0).arg(MASK1).arg(MASK2).arg(MASK3), "#4ade80");
        emit logMessage(QString("网关:  %1.%2.%3.%4").arg(GW0).arg(GW1).arg(GW2).arg(GW3), "#4ade80");
        emit logMessage(QString("DNS:   %1.%2.%3.%4").arg(DNS0).arg(DNS1).arg(DNS2).arg(DNS3), "#4ade80");
        emit progress(100, "完成");
    } else {
        emit logMessage("\n*** 未能找到稳定可用IP, 请重试 ***", "#f87171");
        emit progress(100, "失败");
    }

    emit done(success);
}
