#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include "pingworker.h"
#include <QProcess>
#include <QByteArray>
#include <QStringList>
#include <QRandomGenerator>
#include <QVector>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define SCAN_FIRST 1
#define SCAN_LAST 253
#define PING_COUNT 5
#define PING_TIMEOUT 2000
#define PING_INTERVAL 500
#define VERIFY_COUNT 5
#define VERIFY_TIMEOUT 3000
#define VERIFY_INTERVAL 1000

static const char *ADAPTER_NAME = "以太网";
static const int IP0 = 10, IP1 = 172, IP2 = 201;
static const int MASK0 = 255, MASK1 = 255, MASK2 = 254, MASK3 = 0;
static const int GW0 = 10, GW1 = 172, GW2 = 201, GW3 = 254;
static const int DNS0 = 10, DNS1 = 1, DNS2 = 0, DNS3 = 62;

PingWorker::PingWorker(QObject *parent) : QThread(parent) {}

PingWorker::~PingWorker() {
    if (isRunning()) {
        cancel();
        wait(5000);
    }
}

bool PingWorker::pingIp(DWORD ip, int count, int timeoutMs, int intervalMs, int &replies) {
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) { replies = count; return true; }

    unsigned char data[32] = {0};
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8;
    void *buf = malloc(replySize);
    if (!buf) { IcmpCloseHandle(h); replies = count; return true; }

    replies = 0;
    for (int i = 0; i < count; i++) {
        if (m_cancelled) break;
        DWORD ret = IcmpSendEcho(h, ip, data, sizeof(data), NULL, buf, replySize, timeoutMs);
        if (ret > 0) replies++;
        if (i < count - 1) Sleep(intervalMs);
    }

    free(buf);
    IcmpCloseHandle(h);
    return replies > 0;
}

bool PingWorker::runNetsh(const QString &cmd) {
    QProcess proc;
    proc.setProgram("netsh");
    QStringList args = QProcess::splitCommand(cmd);
    proc.setArguments(args.mid(1));
    proc.start();
    proc.waitForFinished(30000);
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

int PingWorker::verifyBaidu() {
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) return 0;

    unsigned char data[32] = {0};
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8;
    void *buf = malloc(replySize);
    if (!buf) { IcmpCloseHandle(h); return 0; }

    DWORD baiduIp = inet_addr("110.242.74.102");
    int got = 0;
    for (int i = 0; i < VERIFY_COUNT; i++) {
        if (m_cancelled) break;
        DWORD ret = IcmpSendEcho(h, baiduIp, data, sizeof(data), NULL, buf, replySize, VERIFY_TIMEOUT);
        if (ret > 0) got++;
        emit progress(70 + (i + 1) * (30 / VERIFY_COUNT), QString("验证百度 %1/%2").arg(i + 1).arg(VERIFY_COUNT));
        if (i < VERIFY_COUNT - 1) Sleep(VERIFY_INTERVAL);
    }

    free(buf);
    IcmpCloseHandle(h);
    return got;
}

void PingWorker::run() {
    emit logMessage("=== 自动设IP 启动 ===", "#60a5fa");
    emit logMessage(QString("适配器: %1").arg(QString::fromUtf8(ADAPTER_NAME)), "#a0a0a0");
    emit logMessage(QString("IP段: %1.%2.%3.x (只改最后一位)").arg(IP0).arg(IP1).arg(IP2), "#a0a0a0");
    emit logMessage(QString("掩码: %1.%2.%3.%4").arg(MASK0).arg(MASK1).arg(MASK2).arg(MASK3), "#a0a0a0");
    emit logMessage(QString("网关: %1.%2.%3.%4").arg(GW0).arg(GW1).arg(GW2).arg(GW3), "#a0a0a0");
    emit logMessage(QString("DNS:  %1.%2.%3.%4").arg(DNS0).arg(DNS1).arg(DNS2).arg(DNS3), "#a0a0a0");
    emit logMessage(QString("探测: 每个地址 ping%1包 (有响应即视为占用)").arg(PING_COUNT), "#a0a0a0");
    emit logMessage(QString("验证: ping百度%1包, 全通才算成功").arg(VERIFY_COUNT), "#a0a0a0");
    emit logMessage("", "#a0a0a0");

    struct Cand { DWORD ip; int last; bool reachable; int replies; };
    const int total = SCAN_LAST - SCAN_FIRST + 1;
    std::vector<Cand> cands(total);

    emit progress(10, "并发扫描地址中...");
    emit logMessage(QString("并发扫描 %1.%2.%3.%4 - %5.%6.%7.%8 (共%9个地址)...")
        .arg(IP0).arg(IP1).arg(IP2).arg(SCAN_FIRST)
        .arg(IP0).arg(IP1).arg(IP2).arg(SCAN_LAST)
        .arg(total), "#fbbf24");

    for (int i = 0; i < total; i++) {
        int last = SCAN_FIRST + i;
        char ipBuf[32];
        sprintf(ipBuf, "%d.%d.%d.%d", IP0, IP1, IP2, last);
        cands[i].ip = inet_addr(ipBuf);
        cands[i].last = last;
        cands[i].reachable = true;
        cands[i].replies = 0;
    }

    QList<QThread*> threads;
    for (int i = 0; i < total; i++) {
        auto t = QThread::create([&cands, i]() {
            HANDLE h = IcmpCreateFile();
            if (h == INVALID_HANDLE_VALUE) { cands[i].reachable = true; cands[i].replies = PING_COUNT; return; }
            unsigned char data[32] = {0};
            DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 8;
            void *buf = malloc(replySize);
            if (!buf) { IcmpCloseHandle(h); cands[i].reachable = true; cands[i].replies = PING_COUNT; return; }
            int replies = 0;
            for (int j = 0; j < PING_COUNT; j++) {
                DWORD ret = IcmpSendEcho(h, cands[i].ip, data, sizeof(data), NULL, buf, replySize, PING_TIMEOUT);
                if (ret > 0) replies++;
                if (j < PING_COUNT - 1) Sleep(PING_INTERVAL);
            }
            cands[i].replies = replies;
            cands[i].reachable = (replies > 0);
            free(buf);
            IcmpCloseHandle(h);
        });
        threads.append(t);
        t->start();
    }
    for (auto *t : threads) { t->wait(120000); delete t; }

    int occupied = 0;
    QVector<int> freeLasts;
    for (int i = 0; i < total; i++) {
        if (cands[i].reachable) occupied++;
        else freeLasts.append(cands[i].last);
    }

    emit progress(65, "扫描完成");
    emit logMessage(QString("扫描完成: 共%1个地址, 被占用 %2 个, 空闲 %3 个")
        .arg(total).arg(occupied).arg(freeLasts.size()), "#a0a0a0");

    if (freeLasts.isEmpty()) {
        emit logMessage("\n*** 全段地址均被占用, 请稍后重试 ***", "#f87171");
        emit progress(100, "失败");
        emit done(false);
        return;
    }

    QStringList listed;
    int show = freeLasts.size() < 40 ? freeLasts.size() : 40;
    for (int i = 0; i < show; i++)
        listed << QString("%1.%2.%3.%4").arg(IP0).arg(IP1).arg(IP2).arg(freeLasts[i]);
    emit logMessage(QString("空闲地址(前%1个): %2").arg(show).arg(listed.join("  ")), "#4ade80");
    emit logMessage("", "#a0a0a0");

    for (int i = freeLasts.size() - 1; i > 0; i--) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        int tmp = freeLasts[i];
        freeLasts[i] = freeLasts[j];
        freeLasts[j] = tmp;
    }

    bool success = false;
    int finalLast = 0;

    for (int idx = 0; idx < freeLasts.size() && !success; idx++) {
        if (m_cancelled) break;
        int tryLast = freeLasts[idx];
        emit logMessage(QString("\n>> 尝试IP: %1.%2.%3.%4").arg(IP0).arg(IP1).arg(IP2).arg(tryLast), "#60a5fa");
        emit progress(70, QString("设置IP %1.%2.%3.%4").arg(IP0).arg(IP1).arg(IP2).arg(tryLast));

        if (!setIP(tryLast)) {
            emit logMessage("设置失败, 换下一个", "#f87171");
            continue;
        }

        emit logMessage("等待网络生效 (3秒)...", "#a0a0a0");
        for (int s = 0; s < 3; s++) {
            if (m_cancelled) break;
            Sleep(1000);
        }

        emit logMessage(QString("验证稳定性: ping百度 %1 包...").arg(VERIFY_COUNT), "#fbbf24");
        int replies = verifyBaidu();
        emit logMessage(QString("百度验证结果: 收到 %1/%2").arg(replies).arg(VERIFY_COUNT),
                        replies == VERIFY_COUNT ? "#4ade80" : "#f87171");

        if (replies == VERIFY_COUNT) {
            success = true;
            finalLast = tryLast;
            emit logMessage(">> 验证通过! IP稳定可用", "#4ade80");
        } else {
            emit logMessage(">> 有丢包, 换下一个", "#f87171");
        }
    }

    if (success) {
        emit logMessage("\n=== 设置完成 ===", "#4ade80");
        emit logMessage(QString("新IP:  %1.%2.%3.%4").arg(IP0).arg(IP1).arg(IP2).arg(finalLast), "#4ade80");
        emit logMessage(QString("掩码:  %1.%2.%3.%4").arg(MASK0).arg(MASK1).arg(MASK2).arg(MASK3), "#4ade80");
        emit logMessage(QString("网关:  %1.%2.%3.%4").arg(GW0).arg(GW1).arg(GW2).arg(GW3), "#4ade80");
        emit logMessage(QString("DNS:   %1.%2.%3.%4").arg(DNS0).arg(DNS1).arg(DNS2).arg(DNS3), "#4ade80");
        emit logMessage(QString("百度验证: %1/%2 全通").arg(VERIFY_COUNT).arg(VERIFY_COUNT), "#4ade80");
        emit progress(100, "完成");
    } else {
        emit logMessage("\n*** 未能找到稳定可用IP, 请重试 ***", "#f87171");
        emit progress(100, "失败");
    }

    emit done(success);
}
