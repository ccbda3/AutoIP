#pragma once

#include <QThread>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <windows.h>

class PingWorker : public QThread {
    Q_OBJECT
public:
    explicit PingWorker(QObject *parent = nullptr);
    ~PingWorker();

    void cancel() {
        QMutexLocker locker(&m_mutex);
        m_cancelled = true;
        m_cond.wakeAll();
    }

    void answer(bool keep);

protected:
    void run() override;

signals:
    void logMessage(const QString &msg, const QString &color);
    void progress(int value, const QString &status);
    void askUser(const QString &ip, int replies, int total);
    void done(bool success);

private:
    bool m_cancelled = false;
    QMutex m_mutex;
    QWaitCondition m_cond;
    bool m_waiting = false;
    bool m_keep = false;

    bool pingIp(DWORD ip, int count, int timeoutMs, int intervalMs, int &replies);
    bool runNetsh(const QString &cmd);
    bool setIP(int newLast);
    int verifyBaidu();
};
