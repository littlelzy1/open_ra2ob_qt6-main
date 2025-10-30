#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);
    void checkForUpdates();
    
signals:
    void updateAvailable(const QString &newVersion, const QString &changelog, const QString &downloadUrl);
    void noUpdateAvailable();
    void checkFailed(const QString &errorMessage);
    void programDisabled(const QString &message);
    
private:
    QNetworkAccessManager *manager;
    QString currentVersion;
    
    void parseUpdateInfo(const QByteArray &data);
    bool isNewerVersion(const QString &newVersion);
    bool isVersionEnabled(const QJsonObject &versionObj, const QJsonObject &rootObj);
    QString getDisableMessage(const QJsonObject &rootObj);
}; 