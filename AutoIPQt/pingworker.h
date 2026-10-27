#pragma once

#include <QThread>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <windows.h>
#include <atomic>

class PingWorker : public QThread {
    Q_OBJECT
public:
    explicit PingWorker(QObject *parent = nullptr);
    ~PingWorker();

    void cancel() {
        m_cancelled.store(true);
        QMutexLocker locker(&m_mutex);
        m_cond.wakeAll();
    }

    void answer(bool keep);

    // 供界面显示用，避免网段参数在两处各写一份
    static QString configSummary();

protected:
    void run() override;

signals:
    void logMessage(const QString &msg, const QString &color);
    void progress(int value, const QString &status);
    void askUser(const QString &ip, int replies, int total);
    void done(bool success);

private:
    std::atomic<bool> m_cancelled{false};
    QMutex m_mutex;
    QWaitCondition m_cond;
    bool m_waiting = false;
    bool m_keep = false;

    // --- 探测 ---
    bool arpProbe(DWORD ip, BYTE mac[6]);
    int  pingCount(DWORD ip, int count, int timeoutMs, int intervalMs);
    bool queryAdapter(const QString &name, DWORD &ifIndex, BYTE mac[8], ULONG &macLen);
    void purgeNeighbor(DWORD ifIndex, DWORD ip);

    // --- 配置与验证 ---
    bool runNetsh(const QString &cmd);
    bool setIP(int newLast);
    bool localHasAddress(DWORD target);
    bool waitForAddress(DWORD target, int maxMs);
    bool linkIsUp(DWORD ifIndex);
    bool addressIsPreferred(DWORD target, DWORD ifIndex, bool &duplicate);
    bool routeToGatewayOk(DWORD ifIndex, DWORD gateway);
    bool waitUntilReady(DWORD target, DWORD gateway, DWORD ifIndex, int maxMs, bool &conflict);
    void flushDns();
    bool confirmOwnership(DWORD target, const BYTE localMac[8], ULONG localMacLen);
    int  verifyBaidu();
};
