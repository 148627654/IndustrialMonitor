#include "ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QKeyEvent>
#include <QDebug>

QString ThemeManager::s_currentQssPath = "";

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

void ThemeManager::applyTheme(const QString &qssPath) {
    s_currentQssPath = qssPath;
    QFile file(qssPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString styleSheet = QString::fromUtf8(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();
        qDebug() << "[ThemeManager] Theme loaded successfully from:" << qssPath;
    } else {
        qWarning() << "[ThemeManager] Failed to load theme from:" << qssPath;
    }
}

void ThemeManager::reloadTheme() {
    if (!s_currentQssPath.isEmpty()) {
        applyTheme(s_currentQssPath);
        qDebug() << "[ThemeManager] Theme hot-reloaded.";
    }
}

void ThemeManager::enableHotReload() {
    qApp->installEventFilter(this);
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_F5) {
            reloadTheme();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}