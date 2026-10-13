#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QThread>

class PingWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLogMessage(const QString &msg, const QString &color);
    void onProgress(int value, const QString &status);
    void onAskUser(const QString &ip, int replies, int total);
    void onDone(bool success);

private:
    QTextEdit *m_log;
    QProgressBar *m_progress;
    QLabel *m_statusLabel;
    QLabel *m_infoLabel;
    QPushButton *m_closeBtn;
    QPushButton *m_retryBtn;
    QThread *m_workerThread;
    PingWorker *m_worker;
    bool m_done;

    void setupUI();
    void startWork();
    void appendLog(const QString &msg, const QString &color = "#e0e0e0");
};
