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

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define NUM_CANDIDATES 5
#define PING_COUNT 5
#define PING_TIMEOUT 2000
#define PING_INTERVAL 500
#define VERIFY_COUNT 10
#define VERIFY_TIMEOUT 3000
#define VERIFY_INTERVAL 1000
#define MAX_ROUNDS 3

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
        emit progress(70 + (i + 1) * 3, QString("验证百度 %1/%2").arg(i + 1).arg(VERIFY_COUNT));
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
    emit logMessage(QString("验证: ping百度%1包, 全通才算成功").arg(VERIFY_COUNT), "#a0a0a0");
    emit logMessage("", "#a0a0a0");

    int used[256] = {0};
    used[GW3] = 1;
    used[0] = 1;
    used[255] = 1;

    bool success = false;
    int finalLast = 0;

    for (int round = 0; round < MAX_ROUNDS && !success; round++) {
        if (m_cancelled) break;
        emit progress(round * 20, QString("第%1轮").arg(round + 1));
        emit logMessage(QString("[第%1轮] 随机生成5个候选IP:").arg(round + 1), "#fbbf24");

        struct Cand { DWORD ip; int last; bool reachable; int replies; };
        Cand cands[NUM_CANDIDATES];
        QList<int> candidates;

        for (int i = 0; i < NUM_CANDIDATES; i++) {
            int last;
            do { last = 1 + QRandomGenerator::global()->bounded(253); } while (used[last]);
            used[last] = 1;
            candidates.append(last);

            char ipBuf[32];
            sprintf(ipBuf, "%d.%d.%d.%d", IP0, IP1, IP2, last);
            cands[i].ip = inet_addr(ipBuf);
            cands[i].last = last;
            cands[i].reachable = true;
            cands[i].replies = 0;

            emit logMessage(QString("  候选%1: %2.%3.%4.%5").arg(i + 1).arg(IP0).arg(IP1).arg(IP2).arg(last), "#a0a0a0");
        }

        emit logMessage(QString("\n正在并发Ping (每个IP发%1包, 间隔%2ms, 超时%3ms)...").arg(PING_COUNT).arg(PING_INTERVAL).arg(PING_TIMEOUT), "#a0a0a0");
        emit progress(round * 20 + 5, "Ping候选IP...");

        QList<QThread*> threads;
        struct PingArg { Cand *c; bool *cancelled; };
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            auto t = QThread::create([&, i]() {
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
        for (auto *t : threads) { t->wait(60000); delete t; }

        emit logMessage("Ping结果:", "#a0a0a0");
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            QString status = cands[i].reachable ? "(被占用)" : "(空闲)";
            QString color = cands[i].reachable ? "#f87171" : "#4ade80";
            emit logMessage(QString("  %1.%2.%3.%4 -> 收到%5/%6 %7")
                .arg(IP0).arg(IP1).arg(IP2).arg(cands[i].last)
                .arg(cands[i].replies).arg(PING_COUNT).arg(status), color);
        }

        QList<int> freeCands;
        for (int i = 0; i < NUM_CANDIDATES; i++) {
            if (!cands[i].reachable) freeCands.append(i);
        }

        if (freeCands.isEmpty()) {
            emit logMessage("\n本轮5个IP全部被占用, 继续下一轮...", "#fbbf24");
            continue;
        }

        for (int idx = 0; idx < freeCands.size() && !success; idx++) {
            if (m_cancelled) break;
            int pick = freeCands[idx];
            int tryLast = cands[pick].last;
            emit logMessage(QString("\n>> 尝试IP: %1.%2.%3.%4").arg(IP0).arg(IP1).arg(IP2).arg(tryLast), "#60a5fa");
            emit progress(round * 20 + 30, QString("设置IP %1.%2.%3.%4").arg(IP0).arg(IP1).arg(IP2).arg(tryLast));

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
                emit logMessage(">> 有丢包, 放弃此IP, 换下一个", "#f87171");
                used[tryLast] = 1;
            }
        }

        if (!success) {
            emit logMessage("\n本轮空闲IP均不稳定, 继续下一轮...", "#fbbf24");
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
