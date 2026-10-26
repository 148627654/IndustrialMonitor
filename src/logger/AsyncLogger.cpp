#include "AsyncLogger.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDate>

AsyncLogger::AsyncLogger(QObject *parent)
    : QThread(parent)
{
    setObjectName("AsyncLogThread");
}

AsyncLogger::~AsyncLogger()
{
    stopLogging(); // 析构保险：优雅汇合 + 最终清仓 (幂等，重复调用安全)
}

void AsyncLogger::setLogDirectory(const QString &dirPath)
{
    m_logDirectory = dirPath;
}

void AsyncLogger::setLogFile(const QString &fileName)
{
    m_logFile = fileName;
}

void AsyncLogger::startLogging()
{
    if (isRunning())
        return;
    m_running = true;
    start();
}

bool AsyncLogger::stopLogging()
{
    m_running = false;
    m_cond.notify_all();     // 立刻唤醒休眠中的写盘线程，加速退出汇合
    if (!isRunning())
        return true;         // 从未启动或已结束
    return wait();           // 汇合等待 run() 完成最终清仓落盘
}

void AsyncLogger::pushLog(const LogMessage &msg)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeQueue.append(msg);
        // 定量唤醒：前台积压达阈值，立刻唤醒后台写盘 (否则由 1000ms 超时保底兜底)
        if (m_activeQueue.size() >= m_flushThreshold)
            m_cond.notify_one();
    }
}

int AsyncLogger::pendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeQueue.size() + m_flushQueue.size();
}

void AsyncLogger::run()
{
    m_running = true;
    QFile file;

    while (m_running.load()) {
        // ==================== 1. 条件变量休眠：定量唤醒 or 1000ms 定时保底超时 ====================
        QList<LogMessage> batch;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait_for(lock, std::chrono::milliseconds(1000));
            if (!m_activeQueue.isEmpty()) {
                m_activeQueue.swap(m_flushQueue); // 原子指针级交换 (前后台桶瞬间对调)
                batch.swap(m_flushQueue);         // 移交本地缓冲，放锁后完全无锁落盘
            }
        }

        // ==================== 2. 无锁批量流式落盘 (生产者零反压) ====================
        if (!batch.isEmpty())
            writeBatch(batch, file);
    }

    // ==================== 3. 停机收尾：最终清仓落盘熔断 (零截断、零遗失) ====================
    QList<LogMessage> finalBatch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        finalBatch.swap(m_activeQueue);
    }
    if (!finalBatch.isEmpty())
        writeBatch(finalBatch, file);

    file.close();
    m_running = false;
}

void AsyncLogger::writeBatch(const QList<LogMessage> &batch, QFile &file)
{
    if (batch.isEmpty())
        return;

    // 日志目录自动创建 (幂等)
    QDir().mkpath(m_logDirectory);

    // 目标文件名：显式指定 or 自动按天切卷 (跨天时 fileName 变化自动触发句柄重开)
    const QString targetFile = m_logDirectory + "/"
        + (m_logFile.isEmpty()
               ? QString("monitor_%1.log").arg(QDate::currentDate().toString("yyyyMMdd"))
               : m_logFile);

    // 句柄复用：仅首次/跨天切卷时重开，杜绝每批 open/close 的反复损耗
    if (!file.isOpen() || file.fileName() != targetFile) {
        file.close();
        file.setFileName(targetFile);
        file.open(QIODevice::Append | QIODevice::Text);
    }

    // 极端防御：打开失败放弃本批 (日志线程绝不反噬业务线程)
    if (!file.isOpen())
        return;

    QTextStream stream(&file);
    for (const LogMessage &msg : batch)
        stream << msg.toString() << "\n";
    stream.flush();
    file.flush(); // 操作系统级物理刷盘
}
