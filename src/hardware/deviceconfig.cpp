#include "deviceconfig.h"

#include <QJsonDocument>

DeviceConfig::DeviceConfig()
    : m_payload(TcpConfig{}) // 默认 TCP
{
}

DeviceConfig::DeviceConfig(const TcpConfig &tcp) : m_payload(tcp) {}
DeviceConfig::DeviceConfig(const SerialConfig &serial) : m_payload(serial) {}
DeviceConfig::DeviceConfig(const MockConfig &mock) : m_payload(mock) {}

QString DeviceConfig::toJsonString() const
{
    QJsonObject root;
    if (std::holds_alternative<TcpConfig>(m_payload)) {
        const auto &cfg = std::get<TcpConfig>(m_payload);
        root["timeoutMs"] = cfg.timeoutMs;
        root["keepAlive"] = cfg.keepAlive;
    } else if (std::holds_alternative<SerialConfig>(m_payload)) {
        const auto &cfg = std::get<SerialConfig>(m_payload);
        root["portName"] = cfg.portName;
        root["baudRate"] = cfg.baudRate;
        root["dataBits"] = cfg.dataBits;
        root["parity"]   = cfg.parity;
        root["stopBits"] = cfg.stopBits;
    } else if (std::holds_alternative<MockConfig>(m_payload)) {
        const auto &cfg = std::get<MockConfig>(m_payload);
        root["jitterMs"] = cfg.jitterMs;
        root["noise"]    = cfg.noiseAmplitude;
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

DeviceConfig DeviceConfig::fromJsonString(ProtocolType type, const QString &jsonStr)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    QJsonObject root = doc.object();

    switch (type) {
    case ProtocolType::ModbusTcp: {
        TcpConfig cfg;
        if (root.contains("timeoutMs")) cfg.timeoutMs = root["timeoutMs"].toInt();
        if (root.contains("keepAlive")) cfg.keepAlive = root["keepAlive"].toBool();
        return DeviceConfig(cfg);
    }
    case ProtocolType::ModbusRtu: {
        SerialConfig cfg;
        if (root.contains("portName")) cfg.portName = root["portName"].toString();
        if (root.contains("baudRate")) cfg.baudRate = root["baudRate"].toInt();
        if (root.contains("dataBits")) cfg.dataBits = root["dataBits"].toInt();
        if (root.contains("parity"))   cfg.parity   = root["parity"].toInt();
        if (root.contains("stopBits")) cfg.stopBits = root["stopBits"].toInt();
        return DeviceConfig(cfg);
    }
    case ProtocolType::Mock:
    default: {
        MockConfig cfg;
        if (root.contains("jitterMs")) cfg.jitterMs = root["jitterMs"].toInt();
        if (root.contains("noise"))    cfg.noiseAmplitude = root["noise"].toDouble();
        return DeviceConfig(cfg);
    }
    }
}

