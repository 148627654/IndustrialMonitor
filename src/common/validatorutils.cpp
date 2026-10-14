#include "validatorutils.h"

// IPv4 单个字节 0~255 正则片段
static const QString IP_BYTE_REGEX = "(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])";

// 完整的 IPv4 正则表达式
static const QString IP_FULL_REGEX = QString("^%1\\.%1\\.%1\\.%1$")
                                         .arg(IP_BYTE_REGEX);

// 设备唯一编号统一格式：DEV- 加 4 位连续数字
static const QString DEV_ID_REGEX = "^DEV-\\d{4}$";



QRegularExpressionValidator *ValidatorUtils::createIpValidator(QObject *parent)
{
    QRegularExpression regex(IP_FULL_REGEX);
    return new QRegularExpressionValidator(regex, parent);
}

QRegularExpressionValidator *ValidatorUtils::createDeviceIdValidator(QObject *parent)
{
    QRegularExpression regex(DEV_ID_REGEX);
    return new QRegularExpressionValidator(regex, parent);
}

bool ValidatorUtils::isValidIp(const QString &ip)
{
    static QRegularExpression regex(IP_BYTE_REGEX);
    return regex.match(ip.trimmed()).hasMatch();
}

bool ValidatorUtils::isValidDeviceId(const QString &id)
{
    static QRegularExpression regex(DEV_ID_REGEX);
    return regex.match(id.trimmed()).hasMatch();
}

bool ValidatorUtils::isValidPort(int port)
{
    return port >= 1 && port <= 65535;
}
