#ifndef ASYNCLOGGER_H
#define ASYNCLOGGER_H

#include <QThread>
#include <QList>
#include <QString>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "common/LogDef.h"

class QFile;

/**
 * @brief 企业级异步后台写盘工作者 (Async Logger + 双缓冲阻塞队列)
 * @details 继承 QThread 独占物理文件句柄。生产者 (UI/采集/任意业务线程) 经 pushLog
 *          微秒级非阻塞投递至前台缓冲桶；写盘线程以 1000ms 周期超时保底或
 *          积压阈值 (50 条) 定量唤醒，仅通过 std::swap 原子级交换前后台缓冲桶，
 *          随后在完全无锁状态下批量流式落盘 —— 磁盘 I/O 对生产者零反压。
 *          停机时强制执行最终清仓落盘熔断：零截断、零遗失。
 * @author Yuquan Guo
 * @date 2026-10
 */
class AsyncLogger : public QThread
{
    Q_OBJECT
public:
    explicit AsyncLogger(QObject *parent = nullptr);
    ~AsyncLogger() override;

    void setLogDirectory(const QString &dirPath);   // 日志目录 (默认 "logs"，自动创建)
    void setLogFile(const QString &fileName);       // 显式文件名；空 = 自动按天切卷 monitor_yyyyMMdd.log
    void startLogging();                            // 启动专有写盘线程
    bool stopLogging();                             // 优雅停机 + 最终清仓落盘 (返回线程汇合结果)
    void pushLog(const LogMessage &msg);            // 微秒级非阻塞投递 (停机期间仍可投递，最终清仓兜底)
    int pendingCount() const;                       // 未落盘条目数 (前台 + 后台，供单测断言)

protected:
    void run() override;                            // 消费者落盘事件循环

private:
    void writeBatch(const QList<LogMessage> &batch, QFile &file); // 无锁批量流式落盘 (句柄复用)

    QString m_logDirectory{"logs"};
    QString m_logFile;                              // 空 = 自动按天命名
    QList<LogMessage> m_activeQueue;                // 前台接收桶 (生产者写入)
    QList<LogMessage> m_flushQueue;                 // 后台落盘桶 (仅消费者独占访问)
    mutable std::mutex m_mutex;                     // 仅保护两桶交换与前台压入 (临界区纳秒级)
    std::condition_variable m_cond;                 // 休眠唤醒 (定量 notify + 定时超时双机制)
    std::atomic<bool> m_running{false};
    int m_flushThreshold{50};                       // 前台积压达阈值即刻唤醒写盘
};

#endif // ASYNCLOGGER_H
