#ifndef VALIDATORUTILS_H
#define VALIDATORUTILS_H

#include <QRegularExpressionValidator>


class ValidatorUtils
{
public:
    // （IPv4 强正则）
    static QRegularExpressionValidator* createIpValidator(QObject *parent = nullptr);
    // 固定格式 DEV-[0-9]{4}
    static QRegularExpressionValidator* createDeviceIdValidator(QObject *parent = nullptr);

    // 静态快捷布尔判定
    static bool isValidIp(const QString &ip);
    static bool isValidDeviceId(const QString &id);
    static bool isValidPort(int port);
private:
    ValidatorUtils()=delete;
};

#endif // VALIDATORUTILS_H
