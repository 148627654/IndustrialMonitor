#ifndef LOGDEF_H
#define LOGDEF_H

#include <QString>
#include <QDateTime>
#include <QThread>
#include <QMetaType>

/**
 * @brief 工业四级日志等级规约 (对齐 syslog / SCADA 标准)
 */
enum class LogLevel {
    Debug    = 0, // 底层通信报文与细粒度调试 (生产环境默认被过滤器关闭)
    Info     = 1, // 生产系统健康生命周期里程碑
    Warning  = 2, // 业务级非致命容错警告 (系统具备自愈能力)
    Critical = 3  // 严重故障/通信断绝/物理超标 (核心功能停摆)
};

/**
 * @brief 等级枚举 → 落盘文本标签 (统一补足 5 字符宽，保证 [LEVEL] 列严格对齐，
 *        便于 ELK/Python 等审计脚本按固定列宽正则清洗提取)
 */
inline const char* logLevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:    return "DEBUG";
    case LogLevel::Info:     return "INFO ";
    case LogLevel::Warning:  return "WARN ";
    case LogLevel::Critical: return "CRIT ";
    default:                 return "UNK  ";
    }
}

/**
 * @brief 等级枚举 → 终端 ANSI 彩色转义码 (仅用于控制台打印，严禁落盘)
 */
inline const char* logLevelToColorCode(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:    return "\033[36m"; // 青色
    case LogLevel::Info:     return "\033[32m"; // 绿色
    case LogLevel::Warning:  return "\033[33m"; // 黄色
    case LogLevel::Critical: return "\033[31m"; // 刺目红
    default:                 return "\033[0m";
    }
}

/**
 * @brief 绝对路径剥离：__FILE__ 展开为完整绝对路径 (冗长且浪费数倍磁盘)，
 *        仅保留最后的纯文件名，如 "D:/Cpp/.../AcquisitionWorker.cpp" → "AcquisitionWorker.cpp"
 */
inline QString stripSourcePath(const char* fullPath) {
    if (!fullPath) return QString();
    const QString path = QString::fromUtf8(fullPath);
    const int lastSlash = qMax(path.lastIndexOf('/'), path.lastIndexOf('\\'));
    return (lastSlash >= 0) ? path.mid(lastSlash + 1) : path;
}

/**
 * @brief 企业级结构化日志实体 (不可变纯值对象 / 工业黑匣子最小记录单元)
 * @details 每个日志事件在诞生时固化为强类型对象，封装全部诊断元数据：
 *          毫秒级绝对时间戳、物理线程上下文、日志等级、源码锚点 (文件/行号/函数)、
 *          业务正文 —— 为后续高速队列传输与异步落盘提供结构化契约保障。
 * @author Yuquan Guo
 * @date 2026-10
 */
struct LogMessage {
    QDateTime timestamp;                        // 生成时刻 (毫秒精度绝对时钟)
    LogLevel level{LogLevel::Info};
    QString threadId;                           // 物理线程 ID (形如 "0x1a8f")
    QString fileName;                           // 纯文件名 (已剥离绝对路径)
    int lineNumber{0};                          // 源码物理行号
    QString function;                           // 所在函数签名
    QString message;                            // 业务描述正文

    // 格式化为纯文本标准排版 (供磁盘落盘使用，严禁混入 ANSI 转义码)
    QString toString() const {
        return QString("[%1] [%2] [Thread:%3] [%4:%5] [%6] %7")
            .arg(timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(logLevelToString(level))
            .arg(threadId)
            .arg(fileName)
            .arg(lineNumber)
            .arg(function)
            .arg(message);
    }

    // 格式化为带 ANSI 转义码的彩色格式 (仅供终端打印使用，末尾强制复原颜色)
    QString toColorString() const {
        return QString("%1[%2] [%3] [Thread:%4] [%5:%6] [%7] %8\033[0m")
            .arg(logLevelToColorCode(level))
            .arg(timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(logLevelToString(level))
            .arg(threadId)
            .arg(fileName)
            .arg(lineNumber)
            .arg(function)
            .arg(message);
    }
};

// 注册 Qt 元类型系统：支持跨线程信号槽值语义拷贝与 QVariant 封装
Q_DECLARE_METATYPE(LogMessage)
Q_DECLARE_METATYPE(QList<LogMessage>)

#endif // LOGDEF_H
