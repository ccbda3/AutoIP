#pragma once

#include <QThread>
#include <QString>
#include <windows.h>

class PingWorker : public QThread {
    Q_OBJECT
public:
    explicit PingWorker(QObject *parent = nullptr);
    ~PingWorker();

    void cancel() { m_cancelled = true; }

protected:
    void run() override;

signals:
    void logMessage(const QString &msg, const QString &color);
    void progress(int value, const QString &status);
    void done(bool success);

private:
    bool m_cancelled = false;

    bool pingIp(DWORD ip, int count, int timeoutMs, int intervalMs, int &replies);
    bool runNetsh(const QString &cmd);
    bool setIP(int newLast);
    int verifyBaidu();
};
