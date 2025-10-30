#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

class UpdateDownloader : public QObject {
    Q_OBJECT
public:
    explicit UpdateDownloader(QObject *parent = nullptr);
    void downloadUpdate(const QString &url);
    
signals:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString &filePath);
    void downloadFailed(const QString &errorMessage);
    
private:
    QNetworkAccessManager *manager;
    QNetworkReply *currentDownload;
    QFile *downloadFile;
    
    QString getSaveFilePath(const QString &url);
}; 