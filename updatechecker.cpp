#include "updatechecker.h"
#include <QJsonArray>
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    
    // 从程序信息中获取当前版本
    currentVersion = QCoreApplication::applicationVersion();
    qDebug() << "Current version:" << currentVersion;
}

void UpdateChecker::checkForUpdates() {
    // TODO: 配置你的更新服务器地址
    QNetworkRequest request(QUrl("https://your-server.com/api/version.json"));
    QNetworkReply *reply = manager->get(request);
    
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            parseUpdateInfo(reply->readAll());
        } else {
            emit checkFailed(reply->errorString());
        }
        reply->deleteLater();
    });
}

void UpdateChecker::parseUpdateInfo(const QByteArray &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        emit checkFailed("无效的更新信息格式");
        return;
    }
    
    QJsonObject obj = doc.object();
    
    // 检查全局启用状态
    if (obj.contains("enabled") && !obj["enabled"].toBool()) {
        emit programDisabled(getDisableMessage(obj));
        return;
    }
    
    // 检查当前版本是否启用
    if (obj.contains("versions") && obj["versions"].isArray()) {
        QJsonArray versions = obj["versions"].toArray();
        for (const QJsonValue &value : versions) {
            QJsonObject versionObj = value.toObject();
            if (versionObj["version"].toString() == currentVersion) {
                if (!isVersionEnabled(versionObj, obj)) {
                    emit programDisabled(getDisableMessage(obj));
                    return;
                }
                break;
            }
        }
    }
    
    QString latestVersion = obj["latestVersion"].toString();
    
    // 检查是否有更新版本
    if (isNewerVersion(latestVersion)) {
        QJsonArray versions = obj["versions"].toArray();
        for (const QJsonValue &value : versions) {
            QJsonObject versionObj = value.toObject();
            if (versionObj["version"].toString() == latestVersion) {
                QString downloadUrl = versionObj["downloadUrl"].toString();
                QString changelog = versionObj["changeLog"].toString();
                emit updateAvailable(latestVersion, changelog, downloadUrl);
                return;
            }
        }
        emit checkFailed("找不到指定版本的下载信息");
    } else {
        emit noUpdateAvailable();
    }
}

bool UpdateChecker::isNewerVersion(const QString &newVersion) {
    QVersionNumber current = QVersionNumber::fromString(currentVersion);
    QVersionNumber latest = QVersionNumber::fromString(newVersion);
    
    return latest > current;
}

bool UpdateChecker::isVersionEnabled(const QJsonObject &versionObj, const QJsonObject &rootObj) {
    // 首先检查特定版本是否被禁用
    if (versionObj.contains("enabled") && !versionObj["enabled"].toBool()) {
        return false;
    }
    
    // 如果版本没有指定启用状态，则使用全局启用状态
    return !rootObj.contains("enabled") || rootObj["enabled"].toBool();
}

QString UpdateChecker::getDisableMessage(const QJsonObject &rootObj) {
    if (rootObj.contains("disableMessage")) {
        return rootObj["disableMessage"].toString();
    }
    return "程序已被禁用，请联系管理员。";
}