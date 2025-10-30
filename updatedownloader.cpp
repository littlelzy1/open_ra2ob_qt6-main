#include "updatedownloader.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

UpdateDownloader::UpdateDownloader(QObject *parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    downloadFile = nullptr;
    currentDownload = nullptr;
}

void UpdateDownloader::downloadUpdate(const QString &url) {
    QNetworkRequest request(url);
    currentDownload = manager->get(request);
    
    QString saveFilePath = getSaveFilePath(url);
    downloadFile = new QFile(saveFilePath);
    
    if (!downloadFile->open(QIODevice::WriteOnly)) {
        emit downloadFailed("无法保存安装程序: " + downloadFile->errorString());
        delete downloadFile;
        downloadFile = nullptr;
        currentDownload->abort();
        currentDownload->deleteLater();
        return;
    }
    
    connect(currentDownload, &QNetworkReply::readyRead, [this]() {
        if (downloadFile) {
            downloadFile->write(currentDownload->readAll());
        }
    });
    
    connect(currentDownload, &QNetworkReply::downloadProgress, this, &UpdateDownloader::downloadProgress);
    
    connect(currentDownload, &QNetworkReply::finished, [this]() {
        if (currentDownload->error() == QNetworkReply::NoError) {
            if (downloadFile) {
                downloadFile->write(currentDownload->readAll());
                downloadFile->close();
                QString filePath = downloadFile->fileName();
                delete downloadFile;
                downloadFile = nullptr;
                
                qDebug() << "Download finished, installer saved to:" << filePath;
                emit downloadFinished(filePath);
            }
        } else {
            if (downloadFile) {
                downloadFile->close();
                downloadFile->remove();
                delete downloadFile;
                downloadFile = nullptr;
            }
            
            emit downloadFailed(currentDownload->errorString());
        }
        
        currentDownload->deleteLater();
        currentDownload = nullptr;
    });
}

QString UpdateDownloader::getSaveFilePath(const QString &url) {
    QFileInfo fileInfo(url);
    QString fileName = fileInfo.fileName();
    
    QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return downloadPath + QDir::separator() + fileName;
} 