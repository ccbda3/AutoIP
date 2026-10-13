#include "mainwindow.h"
#include "pingworker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QFile>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_worker(nullptr), m_done(false)
{
    setWindowTitle("AutoIP - 自动设IP");
    setWindowIcon(QIcon(":/icons/autoip.ico"));
    setFixedSize(620, 560);
    setupUI();

    m_worker = new PingWorker();
    m_worker->moveToThread(new QThread(this));
    m_workerThread = qobject_cast<QThread*>(m_worker->thread());

    connect(m_worker, &PingWorker::logMessage, this, &MainWindow::onLogMessage, Qt::QueuedConnection);
    connect(m_worker, &PingWorker::progress, this, &MainWindow::onProgress, Qt::QueuedConnection);
    connect(m_worker, &PingWorker::done, this, &MainWindow::onDone, Qt::QueuedConnection);

    startWork();
}

MainWindow::~MainWindow() {
    if (m_worker) {
        m_worker->cancel();
    }
}

void MainWindow::setupUI() {
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setStyleSheet(
        "QLabel {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e293b, stop:1 #0f172a);"
        "  color: #94a3b8;"
        "  padding: 10px;"
        "  border-radius: 6px;"
        "  font-size: 12px;"
        "}"
    );
    m_infoLabel->setText(
        "适配器: 以太网  |  IP段: 10.172.201.x  |  掩码: 255.255.254.0\n"
        "网关: 10.172.201.254  |  DNS: 10.1.0.62"
    );
    layout->addWidget(m_infoLabel);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setStyleSheet(
        "QTextEdit {"
        "  background-color: #0f0f0f;"
        "  color: #e0e0e0;"
        "  border: 1px solid #2d2d2d;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 13px;"
        "}"
    );
    layout->addWidget(m_log, 1);

    m_statusLabel = new QLabel("准备中...", this);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "  color: #94a3b8;"
        "  font-size: 12px;"
        "  padding-left: 4px;"
        "}"
    );
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setStyleSheet(
        "QProgressBar {"
        "  background-color: #1e1e2e;"
        "  border: 1px solid #2d2d2d;"
        "  border-radius: 6px;"
        "  text-align: center;"
        "  color: #e0e0e0;"
        "  height: 24px;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #8b5cf6);"
        "  border-radius: 5px;"
        "}"
    );
    layout->addWidget(m_progress);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_retryBtn = new QPushButton("重试", this);
    m_retryBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #1e293b;"
        "  color: #94a3b8;"
        "  border: 1px solid #334155;"
        "  border-radius: 6px;"
        "  padding: 8px 24px;"
        "  font-size: 13px;"
        "  min-width: 80px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #334155;"
        "  color: #e0e0e0;"
        "}"
        "QPushButton:disabled {"
        "  color: #475569;"
        "}"
    );
    m_retryBtn->setEnabled(false);
    connect(m_retryBtn, &QPushButton::clicked, this, [this]() {
        m_log->clear();
        m_progress->setValue(0);
        m_done = false;
        m_closeBtn->setEnabled(false);
        m_retryBtn->setEnabled(false);
        startWork();
    });
    btnLayout->addWidget(m_retryBtn);

    m_closeBtn = new QPushButton("关闭", this);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #8b5cf6);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 24px;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "  min-width: 80px;"
        "}"
        "QPushButton:hover {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563eb, stop:1 #7c3aed);"
        "}"
        "QPushButton:disabled {"
        "  background-color: #1e293b;"
        "  color: #475569;"
        "}"
    );
    m_closeBtn->setEnabled(false);
    connect(m_closeBtn, &QPushButton::clicked, this, &QMainWindow::close);
    btnLayout->addWidget(m_closeBtn);

    layout->addLayout(btnLayout);

    setCentralWidget(central);

    setStyleSheet("QMainWindow { background-color: #1a1a2e; }");
}

void MainWindow::startWork() {
    m_done = false;
    if (m_worker) {
        if (m_worker->isRunning()) {
            m_worker->cancel();
            m_worker->wait(5000);
        }
        m_worker->start();
    }
}

void MainWindow::onLogMessage(const QString &msg, const QString &color) {
    if (msg.isEmpty()) {
        m_log->append("");
    } else {
        m_log->append(QString("<span style='color:%1;'>%2</span>").arg(color).arg(msg.toHtmlEscaped()));
    }
    QScrollBar *bar = m_log->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void MainWindow::onProgress(int value, const QString &status) {
    m_progress->setValue(value);
    m_statusLabel->setText(status);
}

void MainWindow::onDone(bool success) {
    m_done = true;
    m_closeBtn->setEnabled(true);
    m_retryBtn->setEnabled(true);
    m_closeBtn->setText(success ? "完成" : "关闭");
    m_statusLabel->setText(success ? "成功" : "失败");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_worker && m_worker->isRunning()) {
        if (!m_done) {
            event->ignore();
            return;
        }
        m_worker->cancel();
        m_worker->wait(3000);
    }
    event->accept();
}
